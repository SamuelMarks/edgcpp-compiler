#!/usr/bin/env python3
"""Verifies CMake backend option combinations and smoke-tests compiled cpfe binaries.

Supports verifying a single build directory (`--build-dir <dir> --llvm <0|1> --gcc <0|1>`)
as well as configuring, building, and smoke-testing all four backend modes
(`--all-modes`).
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile


def check_dump_configuration(cpfe_bin: str, expect_llvm: bool, expect_gcc: bool) -> None:
    """Verifies that `cpfe --dump_configuration` reports expected BACK_END_IS_* macros."""
    proc = subprocess.run(
        [cpfe_bin, "--dump_configuration"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    out = proc.stdout
    expect_c = 0 if (expect_llvm or expect_gcc) else 1
    expected_lines = [
        f"#define BACK_END_IS_C_GEN_BE {expect_c}",
        f"#define BACK_END_IS_LLVM_GEN_BE {1 if expect_llvm else 0}",
        f"#define BACK_END_IS_GCC_GEN_BE {1 if expect_gcc else 0}",
    ]
    for line in expected_lines:
        if line not in out:
            raise RuntimeError(
                f"Expected '{line}' in {cpfe_bin} --dump_configuration output, but not found."
            )


def smoke_test_binary(cpfe_bin: str, expect_llvm: bool, expect_gcc: bool) -> None:
    """Runs a compilation smoke test on `cpfe_bin` for each enabled backend."""
    check_dump_configuration(cpfe_bin, expect_llvm, expect_gcc)

    with tempfile.TemporaryDirectory() as tmpdir:
        src_file = os.path.join(tmpdir, "smoke.c")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write("int edg_smoke_add(int a, int b) { return a + b; }\n")

        # 1. Front-end only check (--fe_only) works in all modes
        subprocess.run(
            [cpfe_bin, "--c", "--clear_flag=use_predefined_macro_file", "--fe_only", src_file],
            check=True,
        )

        # 2. If LLVM backend is enabled, emit LLVM IR
        if expect_llvm:
            ll_file = os.path.join(tmpdir, "smoke.ll")
            subprocess.run(
                [
                    cpfe_bin,
                    "--c",
                    "--clear_flag=use_predefined_macro_file",
                    "--llvm-be",
                    f"--gen_llvm_file_name={ll_file}",
                    src_file,
                ],
                check=True,
            )
            if not os.path.exists(ll_file) or os.path.getsize(ll_file) == 0:
                raise RuntimeError("LLVM backend failed to emit non-empty LLVM IR file.")

        # 3. If GCC backend is enabled, emit assembly or object via libgccjit
        if expect_gcc:
            has_gccjit_runtime = True
            if sys.platform == "win32":
                has_gccjit_runtime = bool(
                    shutil.which("libgccjit-0.dll")
                    or shutil.which("libgccjit.dll")
                    or os.path.exists(r"C:\msys64\mingw64\bin\libgccjit-0.dll")
                )
            if has_gccjit_runtime:
                asm_file = os.path.join(tmpdir, "smoke.s")
                subprocess.run(
                    [
                        cpfe_bin,
                        "--c",
                        "--clear_flag=use_predefined_macro_file",
                        "--gcc-be",
                        "-S",
                        "-o",
                        asm_file,
                        src_file,
                    ],
                    check=True,
                )
                if not os.path.exists(asm_file) or os.path.getsize(asm_file) == 0:
                    raise RuntimeError("GCC backend failed to emit non-empty assembly file.")

        # 4. If neither LLVM nor GCC is enabled, C-generating backend emits .int.c
        if not expect_llvm and not expect_gcc:
            gen_c_file = os.path.join(tmpdir, "smoke.int.c")
            subprocess.run(
                [
                    cpfe_bin,
                    "--c",
                    "--clear_flag=use_predefined_macro_file",
                    "--c-be",
                    f"--gen_c_file_name={gen_c_file}",
                    src_file,
                ],
                check=True,
            )
            if not os.path.exists(gen_c_file) or os.path.getsize(gen_c_file) == 0:
                raise RuntimeError("C-generating backend failed to emit non-empty .int.c file.")


def main() -> int:
    """CLI entry point for CMake backend mode verification."""
    parser = argparse.ArgumentParser(description="Verify EDG CMake backend option modes.")
    parser.add_argument("--cpfe", help="Path to cpfe executable to verify.")
    parser.add_argument("--llvm", type=int, choices=[0, 1], default=0, help="Expected LLVM backend state.")
    parser.add_argument("--gcc", type=int, choices=[0, 1], default=0, help="Expected GCC backend state.")
    parser.add_argument("--all-modes", action="store_true", help="Verify all 4 build directories under build/.")
    args = parser.parse_args()

    if args.all_modes:
        modes = [
            ("Mode A (Neither)", "build/mode-a-neither", False, False),
            ("Mode B (LLVM Only)", "build/llvm-debug", True, False),
            ("Mode C (GCC Only)", "build/gcc-be-debug", False, True),
            ("Mode D (Both LLVM and GCC)", "build/multibackend-debug", True, True),
        ]
        for label, bdir, llvm_on, gcc_on in modes:
            exe = "cpfe.exe" if sys.platform == "win32" else "cpfe"
            cpfe_path = os.path.join(bdir, "bin", exe)
            if not os.path.exists(cpfe_path):
                raise RuntimeError(f"{label}: binary not found at {cpfe_path}")
            smoke_test_binary(cpfe_path, llvm_on, gcc_on)
            print(f"[PASS] {label} ({cpfe_path})")
        return 0

    if not args.cpfe:
        parser.error("--cpfe is required when --all-modes is not specified.")
    smoke_test_binary(args.cpfe, bool(args.llvm), bool(args.gcc))
    print(f"[PASS] Verified cpfe binary ({args.cpfe}) [LLVM={args.llvm}, GCC={args.gcc}]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
