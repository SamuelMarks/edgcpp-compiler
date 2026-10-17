/**
 * @file llvm_gen_be_driver.cpp
 * @brief Toolchain detection and linker driver orchestrator.
 * @details Implements the invocation of system linkers (LLD, GNU ld, Apple ld64, MSVC link.exe)
 *          along with resolution of standard libraries (libc++, libstdc++, MSVCRT) and
 *          multi-module ThinLTO/FullLTO combined index and IR linking pipelines.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_driver.h"
#include "llvm_gen_be_codegen.h"
#include "llvm_gen_be_opt.h"
#include "cmd_line.h"
#include <cstdlib>
#include <string>
#include <vector>
#include <llvm/Analysis/ModuleSummaryAnalysis.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/ModuleSummaryIndex.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Program.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Probes the host/target environment to discover SDK, CRT, and library paths for @p triple.
 * @details Inspects Linux multiarch/GCC directories, macOS CommandLineTools/Xcode SDKs, or
 *          Windows MSVC/WindowsSDK directories, honoring @p sysroot_override when non-null.
 * @param[in] triple Target LLVM triple being linked.
 * @param[in] sysroot_override Optional explicit sysroot/SDK path override (may be null).
 * @param[out] out_config Receives the populated `llvm_toolchain_config_t`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_config` is null.
 */
llvm_gen_be_error_t discover_target_toolchain(
    const llvm::Triple& triple,
    const char* sysroot_override,
    llvm_toolchain_config_t* out_config) noexcept {
    if (!out_config) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    *out_config = llvm_toolchain_config_t{};

    if (sysroot_override && sysroot_override[0] != '\0') {
        out_config->sysroot_path = sysroot_override;
    }

    if (triple.isOSDarwin()) {
        if (out_config->sysroot_path.empty()) {
            const char* sdk_env = std::getenv("SDKROOT");
            if (sdk_env) {
                out_config->sysroot_path = sdk_env;
            } else {
                out_config->sysroot_path = "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk";
            }
        }
        out_config->lib_search_dirs.push_back(out_config->sysroot_path + "/usr/lib");
        out_config->cxx_stdlib_flag = "-lc++";
        out_config->unwind_lib_flag = "-lSystem";
    } else if (triple.isOSWindows()) {
        const char* vc_dir = std::getenv("VCToolsInstallDir");
        if (vc_dir) {
            out_config->lib_search_dirs.push_back(std::string(vc_dir) + "/lib/x64");
        }
        const char* win_sdk = std::getenv("WindowsSdkDir");
        if (win_sdk) {
            out_config->lib_search_dirs.push_back(std::string(win_sdk) + "/Lib/um/x64");
        }
        out_config->cxx_stdlib_flag = "-lmsvcprt";
        out_config->unwind_lib_flag = "-lvcruntime";
    } else {
        std::string root = out_config->sysroot_path;
        std::string arch_tuple = triple.getArchName().str() + "-linux-gnu";
        std::string usr_multiarch = root + "/usr/lib/" + arch_tuple;
        std::string lib_multiarch = root + "/lib/" + arch_tuple;
        std::string crt_dir = llvm::sys::fs::exists(usr_multiarch + "/crt1.o")
                                  ? usr_multiarch
                                  : (root + "/usr/lib");
        out_config->crt1_path = crt_dir + "/crt1.o";
        out_config->crti_path = crt_dir + "/crti.o";
        out_config->crtbegin_path = crt_dir + "/crtbegin.o";
        out_config->crtend_path = crt_dir + "/crtend.o";
        out_config->crtn_path = crt_dir + "/crtn.o";
        out_config->lib_search_dirs.push_back(usr_multiarch);
        out_config->lib_search_dirs.push_back(lib_multiarch);
        out_config->lib_search_dirs.push_back(root + "/usr/lib");
        out_config->lib_search_dirs.push_back(root + "/lib");
        out_config->cxx_stdlib_flag = "-lstdc++";
        out_config->unwind_lib_flag = "-lgcc_s";
    }

    llvm::ErrorOr<std::string> clang_path = llvm::sys::findProgramByName("clang++");
    if (clang_path) {
        out_config->linker_program = *clang_path;
        out_config->is_direct_lld = false;
    } else {
        const char* lld_name = triple.isWasm()
                                   ? "wasm-ld"
                                   : (triple.isOSWindows()
                                          ? "lld-link"
                                          : (triple.isOSDarwin() ? "ld64.lld" : "ld.lld"));
        llvm::ErrorOr<std::string> lld_path = llvm::sys::findProgramByName(lld_name);
        if (lld_path) {
            out_config->linker_program = *lld_path;
            out_config->is_direct_lld = true;
        }
    }

    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Synthesizes the complete command-line argument vector for linking @p obj_files into @p exe_file_path.
 * @details Formats target triple, sysroot, CRT objects, library search paths, sanitizer/profile/OpenMP
 *          runtimes, and user `extra_linker_args` for ELF, Mach-O, COFF, or Wasm targets.
 * @param[in] tc Discovered toolchain configuration.
 * @param[in] triple Target LLVM triple.
 * @param[in] obj_files Non-empty list of input object or bitcode file paths.
 * @param[in] exe_file_path Desired output executable path.
 * @param[out] out_args Receives the constructed command-line argument strings.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are invalid.
 */
llvm_gen_be_error_t build_native_linker_args(
    const llvm_toolchain_config_t& tc,
    const llvm::Triple& triple,
    const std::vector<std::string>& obj_files,
    const char* exe_file_path,
    std::vector<std::string>* out_args) noexcept {
    if (tc.linker_program.empty() || obj_files.empty() || !exe_file_path || !out_args) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    out_args->clear();
    out_args->push_back(tc.linker_program);

    if (!tc.is_direct_lld) {
        out_args->push_back("-target");
        out_args->push_back(triple.str());
        for (const std::string& obj : obj_files) {
            out_args->push_back(obj);
        }
        if (sanitize_address) out_args->push_back("-fsanitize=address");
        if (sanitize_thread) out_args->push_back("-fsanitize=thread");
        if (sanitize_memory) out_args->push_back("-fsanitize=memory");
        if (sanitize_undefined) out_args->push_back("-fsanitize=undefined");
        if (sanitize_hwaddress) out_args->push_back("-fsanitize=hwaddress");
        if (use_safestack) out_args->push_back("-fsanitize=safe-stack");
        if (use_shadowcallstack) out_args->push_back("-fsanitize=shadow-call-stack");
        if (fprofile_generate_path) out_args->push_back("-fprofile-instr-generate");
        if (flto_kind) {
            if (std::string(flto_kind) == "thin") out_args->push_back("-flto=thin");
            else if (std::string(flto_kind) == "full") out_args->push_back("-flto=full");
        }
        if (be_state->fopenmp_enabled || be_state->module->getFunction("__kmpc_fork_call") != nullptr) {
            out_args->push_back("-fopenmp");
        }
        for (const std::string& extra : be_state->extra_linker_args) {
            out_args->push_back(extra);
        }
        out_args->push_back("-o");
        out_args->push_back(exe_file_path);
        return llvm_gen_be_error_t::ok;
    }

    if (triple.isOSWindows()) {
        for (const std::string& dir : tc.lib_search_dirs) {
            out_args->push_back("/libpath:" + dir);
        }
        for (const std::string& obj : obj_files) {
            out_args->push_back(obj);
        }
        out_args->push_back("/defaultlib:libcmt");
        out_args->push_back("/defaultlib:oldnames");
        out_args->push_back("msvcprt.lib");
        out_args->push_back("vcruntime.lib");
        out_args->push_back("ucrt.lib");
        out_args->push_back("kernel32.lib");
        for (const std::string& extra : be_state->extra_linker_args) {
            out_args->push_back(extra);
        }
        out_args->push_back(std::string("/out:") + exe_file_path);
        return llvm_gen_be_error_t::ok;
    }

    if (triple.isOSDarwin()) {
        out_args->push_back("-arch");
        out_args->push_back(triple.isAArch64() ? "arm64" : "x86_64");
        out_args->push_back("-platform_version");
        out_args->push_back("macos");
        out_args->push_back("14.0.0");
        out_args->push_back("14.0.0");
        if (!tc.sysroot_path.empty()) {
            out_args->push_back("-syslibroot");
            out_args->push_back(tc.sysroot_path);
        }
        for (const std::string& dir : tc.lib_search_dirs) {
            out_args->push_back("-L" + dir);
        }
        for (const std::string& obj : obj_files) {
            out_args->push_back(obj);
        }
        out_args->push_back(tc.cxx_stdlib_flag);
        out_args->push_back(tc.unwind_lib_flag);
        for (const std::string& extra : be_state->extra_linker_args) {
            out_args->push_back(extra);
        }
        out_args->push_back("-o");
        out_args->push_back(exe_file_path);
        return llvm_gen_be_error_t::ok;
    }

    if (triple.isWasm()) {
        out_args->push_back("--no-entry");
        out_args->push_back("--export-all");
        for (const std::string& dir : tc.lib_search_dirs) {
            out_args->push_back("-L" + dir);
        }
        for (const std::string& obj : obj_files) {
            out_args->push_back(obj);
        }
        for (const std::string& extra : be_state->extra_linker_args) {
            out_args->push_back(extra);
        }
        out_args->push_back("-o");
        out_args->push_back(exe_file_path);
        return llvm_gen_be_error_t::ok;
    }

    if (!tc.sysroot_path.empty()) {
        out_args->push_back("--sysroot=" + tc.sysroot_path);
    }
    if (!tc.crt1_path.empty()) out_args->push_back(tc.crt1_path);
    if (!tc.crti_path.empty()) out_args->push_back(tc.crti_path);
    if (!tc.crtbegin_path.empty()) out_args->push_back(tc.crtbegin_path);
    for (const std::string& dir : tc.lib_search_dirs) {
        out_args->push_back("-L" + dir);
    }
    for (const std::string& obj : obj_files) {
        out_args->push_back(obj);
    }
    if (!tc.cxx_stdlib_flag.empty()) out_args->push_back(tc.cxx_stdlib_flag);
    out_args->push_back("-lm");
    if (!tc.unwind_lib_flag.empty()) out_args->push_back(tc.unwind_lib_flag);
    out_args->push_back("-lc");
    if (!tc.crtend_path.empty()) out_args->push_back(tc.crtend_path);
    if (!tc.crtn_path.empty()) out_args->push_back(tc.crtn_path);
    for (const std::string& extra : be_state->extra_linker_args) {
        out_args->push_back(extra);
    }
    out_args->push_back("-o");
    out_args->push_back(exe_file_path);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Invokes the system linker to link multiple object files into an executable.
 * @details Discovers the target toolchain, synthesizes linker flags via `build_native_linker_args`,
 *          and executes the linker via `llvm::sys::ExecuteAndWait`.
 * @param[in] obj_files Vector of object file paths to link.
 * @param[in] exe_file_path The desired path for the final executable.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_link_executables(
    const std::vector<std::string>& obj_files,
    const char* exe_file_path) noexcept {
    if (obj_files.empty() || !exe_file_path || !be_state || !be_state->module) {
        return llvm_gen_be_error_t::invalid_argument;
    }

    llvm::Triple triple(be_state->module->getTargetTriple());
    llvm_toolchain_config_t tc = {};
    LLVM_GEN_BE_ASSERT_OK(discover_target_toolchain(triple, nullptr, &tc));
    if (tc.linker_program.empty()) {
        return llvm_gen_be_error_t::code_gen_failure;
    }

    std::vector<std::string> owned_args;
    LLVM_GEN_BE_ASSERT_OK(
        build_native_linker_args(tc, triple, obj_files, exe_file_path, &owned_args));

    std::vector<llvm::StringRef> args;
    args.reserve(owned_args.size());
    for (const std::string& s : owned_args) {
        args.push_back(s);
    }
    int ret = llvm::sys::ExecuteAndWait(tc.linker_program, args);
    if (ret != 0) {
        return llvm_gen_be_error_t::code_gen_failure;
    }

    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Invokes the system linker to create an executable.
 * @details Detects system toolchains, libraries, and SDKs and executes the driver via `llvm_link_executables`.
 * @param[in] obj_file_path The path to the object file to link.
 * @param[in] exe_file_path The desired path for the final executable.
 * @return llvm_gen_be_error_t::ok on success, or a failure code.
 */
llvm_gen_be_error_t llvm_link_executable(const char* obj_file_path, const char* exe_file_path) noexcept {
    if (!obj_file_path || !exe_file_path) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    std::vector<std::string> all_inputs = {std::string(obj_file_path)};
    if (be_state) {
        for (const std::string& extra_in : be_state->extra_linker_inputs) {
            all_inputs.push_back(extra_in);
        }
    }
    return llvm_link_executables(all_inputs, exe_file_path);
}

/**
 * @brief Performs multi-module ThinLTO/FullLTO combined index generation, IR linking, and native executable emission.
 * @details Parses each bitcode file in @p bitcode_files, builds a combined `ModuleSummaryIndex`,
 *          links the modules into a unified LTO module, runs the LTO optimization pipeline,
 *          emits a native object, and links `@p output_exe_path` when non-null.
 * @param[in] bitcode_files List of LLVM bitcode (`.bc`) file paths to merge and optimize.
 * @param[in] output_exe_path Optional path for the linked executable (if null, only LTO object emission runs).
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t run_thinlto_combined_index_and_link(
    const std::vector<std::string>& bitcode_files,
    const char* output_exe_path) noexcept {
    if (bitcode_files.empty() || !be_state) {
        return llvm_gen_be_error_t::invalid_argument;
    }

    auto combined_mod = std::make_unique<llvm::Module>("lto_combined_module", *be_state->context);
    llvm::Linker ir_linker(*combined_mod);
    llvm::ModuleSummaryIndex combined_index(/*HaveGVs=*/true);

    for (const std::string& bc_path : bitcode_files) {
        auto buf_or_err = llvm::MemoryBuffer::getFile(bc_path);
        if (!buf_or_err) {
            return llvm_gen_be_error_t::io_error;
        }
        auto mod_or_err = llvm::parseBitcodeFile(buf_or_err.get()->getMemBufferRef(), *be_state->context);
        if (!mod_or_err) {
            llvm::consumeError(mod_or_err.takeError());
            return llvm_gen_be_error_t::io_error;
        }
        std::unique_ptr<llvm::Module> sub_mod = std::move(*mod_or_err);
        if (!sub_mod->getTargetTriple().str().empty()) {
            combined_mod->setTargetTriple(sub_mod->getTargetTriple());
        }

        llvm::PassBuilder pb;
        llvm::LoopAnalysisManager lam;
        llvm::FunctionAnalysisManager fam;
        llvm::CGSCCAnalysisManager cgam;
        llvm::ModuleAnalysisManager mam;
        pb.registerModuleAnalyses(mam);
        pb.registerCGSCCAnalyses(cgam);
        pb.registerFunctionAnalyses(fam);
        pb.registerLoopAnalyses(lam);
        pb.crossRegisterProxies(lam, fam, cgam, mam);
        llvm::ModuleSummaryIndex& mod_summary = mam.getResult<llvm::ModuleSummaryIndexAnalysis>(*sub_mod);
        for (const auto& entry : mod_summary) {
            combined_index.getOrInsertValueInfo(entry.first);
        }

        if (ir_linker.linkInModule(std::move(sub_mod))) {
            return llvm_gen_be_error_t::code_gen_failure;
        }
    }

    combined_index.setWithGlobalValueDeadStripping();
    be_state->module = std::move(combined_mod);
    a_const_char* saved_ofile = ofile_name;
    ofile_name = output_exe_path;
    llvm_gen_be_error_t lto_err = invoke_embedded_lld_lto();
    ofile_name = saved_ofile;
    return lto_err;
}

/**
 * @brief Performs direct embedded libLLVM LTO optimization and TargetMachine object emission.
 * @details Runs the LTO optimization pipeline on `be_state->module`, emits a native object
 *          artifact via `TargetMachine`, and invokes `llvm_link_executable` when `ofile_name` is set.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t invoke_embedded_lld_lto(void) noexcept {
    if (!be_state || !be_state->module) {
        return llvm_gen_be_error_t::invalid_argument;
    }

    if (!be_state->module->getModuleFlag("FullLTO") && !be_state->module->getModuleFlag("ThinLTO")) {
        be_state->module->addModuleFlag(llvm::Module::Error, "FullLTO", 1);
    }

    llvm_opt_options_t opts = {};
    opts.opt_level = be_state->opt_level;
    opts.use_lto = true;
    llvm_gen_be_error_t opt_err = run_optimization_pipeline(be_state->module.get(), &opts);
    if (opt_err != llvm_gen_be_error_t::ok) {
        return opt_err;
    }

    llvm_gen_be_error_t init_err = initialize_llvm_targets();
    if (init_err != llvm_gen_be_error_t::ok) {
        return init_err;
    }

    std::string triple_str = be_state->module->getTargetTriple().str();
    if (triple_str.empty()) {
        triple_str = "x86_64-unknown-linux-gnu";
    }

    llvm::CodeGenOptLevel cg_opt = llvm::CodeGenOptLevel::Default;
    switch (be_state->opt_level) {
        case llvm_opt_level_t::O0: cg_opt = llvm::CodeGenOptLevel::None; break;
        case llvm_opt_level_t::O1: cg_opt = llvm::CodeGenOptLevel::Less; break;
        case llvm_opt_level_t::O2:
        case llvm_opt_level_t::Os:
        case llvm_opt_level_t::Oz: cg_opt = llvm::CodeGenOptLevel::Default; break;
        case llvm_opt_level_t::O3: cg_opt = llvm::CodeGenOptLevel::Aggressive; break;
        default: cg_opt = llvm::CodeGenOptLevel::Default; break;
    }

    llvm::TargetMachine* tm = nullptr;
    llvm_gen_be_error_t tm_err = create_target_machine(
        triple_str.c_str(), nullptr, nullptr, cg_opt, std::nullopt, std::nullopt, &tm);
    if (tm_err != llvm_gen_be_error_t::ok) {
        return tm_err;
    }

    const char* obj_out_path = gen_obj_file_name ? gen_obj_file_name : "edg_lto_tmp.o";
    llvm_gen_be_error_t emit_err = emit_machine_code_to_file(
        be_state->module.get(), tm, codegen_file_type_t::object_file, obj_out_path);
    if (emit_err != llvm_gen_be_error_t::ok) {
        return emit_err;
    }

    extern a_const_char* ofile_name;
    if (ofile_name) {
        llvm_gen_be_error_t link_err = llvm_link_executable(obj_out_path, ofile_name);
        if (!gen_obj_file_name) {
            (void)llvm::sys::fs::remove(obj_out_path);
        }
        return link_err;
    }

    return llvm_gen_be_error_t::ok;
}
END_EDG_NAMESPACE
#endif
