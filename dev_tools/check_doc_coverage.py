#!/usr/bin/env python3
"""Verifies 100% Doxygen documentation coverage across all LLVM backend and test files."""

import glob
import re
import sys


def check_file_coverage(filepath: str) -> list[str]:
    """Checks file, struct/enum, field/enumerator, function, and parameter Doxygen coverage."""
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()
    lines = content.splitlines()

    errors: list[str] = []

    # 1. File-level check (@file, @brief, @details)
    if "@file" not in content:
        errors.append("Missing @file Doxygen tag")
    if "@brief" not in content:
        errors.append("Missing @brief Doxygen tag")
    if "@details" not in content:
        errors.append("Missing @details Doxygen tag")

    if filepath.endswith("llvm_gen_be_error.h"):
        if not re.search(
            r"typedef\s+enum\s+class\s+\[\[nodiscard\]\]\s+llvm_gen_be_error_t\s*:\s*int32_t",
            content,
        ):
            errors.append(
                "llvm_gen_be_error.h must define 'typedef enum class [[nodiscard]] llvm_gen_be_error_t : int32_t'"
            )

    # 2. Struct / Class / Enum and Field / Enumerator check
    in_struct = False
    struct_name = ""
    brace_depth = 0
    for i, line in enumerate(lines, 1):
        s = line.strip()
        m = re.match(
            r"^(?:typedef\s+)?(struct|class|enum(?:\s+class)?)\s+(?:\[\[nodiscard\]\]\s+)?([a-zA-Z0-9_]+)?[^{;]*\{",
            s,
        )
        if m and not in_struct:
            kind, name = m.group(1), m.group(2) or "<anon>"
            pre = chr(10).join(lines[max(0, i - 18) : i - 1]).rstrip()
            if not pre.endswith("*/"):
                errors.append(
                    f"Line {i}: {kind} '{name}' is missing preceding /** ... */ Doxygen block"
                )
            else:
                c_start = pre.rfind("/**")
                blk = pre[c_start:] if c_start != -1 else ""
                if "@brief" not in blk:
                    errors.append(f"Line {i}: {kind} '{name}' Doxygen block missing @brief")
            in_struct = True
            struct_name = name
            brace_depth = s.count("{") - s.count("}")
            continue
        if in_struct:
            brace_depth += s.count("{") - s.count("}")
            if brace_depth <= 0:
                in_struct = False
                continue
            if (
                s
                and not s.startswith("//")
                and not s.startswith("/*")
                and not s.startswith("*")
                and not s.startswith("public:")
                and not s.startswith("private:")
                and not s.startswith("protected:")
                and not s.startswith("}")
            ):
                # Skip multi-line constructor/destructor body lines inside inline helper structs
                if s.startswith("if ") or s.startswith("return "):
                    continue
                prev_line = lines[i - 2].strip() if i >= 2 else ""
                if "///<" not in s and "/**" not in s and not prev_line.endswith("*/"):
                    errors.append(
                        f"Line {i}: Field/enumerator in '{struct_name}' missing Doxygen comment: {s}"
                    )

    # 3. Function and function parameter check
    fn_pattern = re.compile(
        r"^[ \t]*(?:static\s+|inline\s+|extern\s+(?:\"C\"\s+)?)*"
        r"(llvm_gen_be_error_t|int|void|bool|a_boolean|char\*|a_targ_alignment|"
        r"a_targ_size_t|a_routine_ptr|a_source_file_ptr|a_type_qualifier_set|"
        r"a_scope_ptr|a_number_buffer|RunResult)\s+"
        r"([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*(?:noexcept)?\s*(?:\{|;)",
        re.MULTILINE,
    )
    for m in fn_pattern.finditer(content):
        ret_ty = m.group(1)
        fn_name = m.group(2)
        if fn_name in ("if", "while", "for", "switch", "return"):
            continue
        allowed_non_err_fns = {
            "main",
            "LLVMFuzzerTestOneInput",
            "is_bool_type",
            "f_alignment_of_type",
            "is_trivially_copyable_type",
            "is_class_struct_union_type",
            "type_has_nontrivial_destructor",
            "conv_seq_to_file_and_line",
            "mangled_typeinfo_name",
            "f_get_type_qualifiers",
            "is_reference_type",
            "is_empty_class_type",
            "get_variable_initializer",
            "scope_for_routine",
            "mangled_vtable_name",
            "mangled_vtbl_name",
            "mangled_vtt_name",
            "mangled_virtual_table_table_name",
            "mangled_function_name",
            "get_mangled_function_name",
            "key_function_for_class",
            "vtbl_decider_function_for_class",
            "skip_typerefs",
            "int_type_is_signed",
            "int_constant_value",
            "uns_constant_value",
            "il_read",
            "make_number_buffer",
            "fp_to_string",
            "free_general",
            "alloc_general",
            "assertion_failed",
            "insufficient_address_space",
            "derived_name",
        }
        if ret_ty != "llvm_gen_be_error_t" and fn_name not in allowed_non_err_fns:
            errors.append(
                f"Function '{fn_name}' returns '{ret_ty}' instead of 'llvm_gen_be_error_t'"
            )
        params_raw = m.group(3).strip()
        pre = content[: m.start()].rstrip()
        if not pre.endswith("*/"):
            errors.append(f"Function '{fn_name}' is missing preceding /** ... */ Doxygen block")
            continue
        c_start = pre.rfind("/**")
        comment = pre[c_start:] if c_start != -1 else ""
        if "@brief" not in comment:
            errors.append(f"Function '{fn_name}' is missing @brief")
        if "@details" not in comment:
            errors.append(f"Function '{fn_name}' is missing @details")
        if ret_ty != "void" and "@return" not in comment:
            errors.append(f"Function '{fn_name}' is missing @return")

        if params_raw and params_raw != "void":
            for p in params_raw.split(","):
                p_clean = p.strip()
                if p_clean == "..." or not p_clean:
                    continue
                p_no_arr = re.sub(r"\[[^\]]*\]", "", p_clean.split("=")[0]).strip()
                tokens = re.findall(r"[a-zA-Z0-9_]+", p_no_arr)
                if not tokens:
                    continue
                pname = tokens[-1]
                if not re.search(r"@param(?:\[[a-z,]+\])?\s+" + re.escape(pname) + r"\b", comment):
                    errors.append(
                        f"Function '{fn_name}' parameter '{pname}' is missing @param documentation"
                    )

    return errors


def main() -> None:
    """Entry point scanning all LLVM backend and test C++ files."""
    files = sorted(
        glob.glob("src/llvm_gen_be*.h")
        + glob.glob("src/llvm_gen_be*.cpp")
        + glob.glob("tests/*llvm*.cpp")
        + glob.glob("tests/llvm_ir_backend_tests/*.cpp")
    )

    if not files:
        print("Error: No LLVM backend or test files found.")
        sys.exit(1)

    all_passed = True
    total_errors = 0

    for f in files:
        errs = check_file_coverage(f)
        if errs:
            all_passed = False
            total_errors += len(errs)
            print(f"Documentation Coverage Failed for {f}:")
            for e in errs:
                print(f"  - {e}")

    if all_passed:
        print(f"100% Documentation Coverage Verified across {len(files)} files!")
        sys.exit(0)
    else:
        print(f"Failed: {total_errors} documentation issues found across {len(files)} files.")
        sys.exit(1)


if __name__ == "__main__":
    main()
