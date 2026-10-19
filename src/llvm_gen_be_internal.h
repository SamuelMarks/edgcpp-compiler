/**
 * @file llvm_gen_be_internal.h
 * @brief Internal state and shared declarations for the EDG LLVM backend.
 * @details Defines `LLVMBackendState`, `ScopeCleanup`, `ScopeCleanupStack`, and internal
 *          cross-module lowering entry points.
 */
#ifndef LLVM_GEN_BE_INTERNAL_H
#define LLVM_GEN_BE_INTERNAL_H

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be.h"
#include "llvm_gen_be_error.h"
#include "host_envir.h"
#include "error.h"
#include "il.h"
#include "targ_def.h"
#include "types.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_const.h"
#include "llvm_gen_be_expr.h"
#include "llvm_gen_be_debug.h"
#include "llvm_gen_be_opt.h"

#include <llvm/Config/llvm-config.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/TargetParser/Triple.h>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if LLVM_VERSION_MAJOR >= 21
#define EDG_LLVM_SET_TARGET_TRIPLE(mod, triple_str) \
  (mod)->setTargetTriple(llvm::Triple(triple_str))
#else
#define EDG_LLVM_SET_TARGET_TRIPLE(mod, triple_str) \
  (mod)->setTargetTriple(triple_str)
#endif

#if LLVM_VERSION_MAJOR >= 20
#define EDG_LLVM_GET_INTRINSIC_DECL(...) \
  llvm::Intrinsic::getOrInsertDeclaration(__VA_ARGS__)
#define EDG_LLVM_GET_TERMINATOR(bb) \
  ((bb)->getTerminatorOrNull())
#define EDG_LLVM_FIRST_INSERTION_PT(bb) \
  ((bb)->getFirstNonPHIIt())
#else
#define EDG_LLVM_GET_INTRINSIC_DECL(...) \
  llvm::Intrinsic::getDeclaration(__VA_ARGS__)
#define EDG_LLVM_GET_TERMINATOR(bb) \
  ((bb)->getTerminator())
#define EDG_LLVM_FIRST_INSERTION_PT(bb) \
  ((bb)->getFirstInsertionPt())
#endif

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
#define EDG_INT_VAL_LOW64(val) static_cast<uint64_t>(val)
#if INT128_EXTENSIONS_ALLOWED
#define EDG_INT_VAL_HIGH64(val) \
  ((sizeof(val) > 8) ? static_cast<uint64_t>((val) >> 64) : 0ULL)
#else
#define EDG_INT_VAL_HIGH64(val) (0ULL)
#endif
#else
#define EDG_INT_VAL_LOW64(val) \
  (static_cast<uint64_t>((val).part[0]) | \
   (static_cast<uint64_t>((val).part[1]) << 16) | \
   (static_cast<uint64_t>((val).part[2]) << 32) | \
   (static_cast<uint64_t>((val).part[3]) << 48))
#define EDG_INT_VAL_HIGH64(val) \
  ((INT_VALUE_PARTS_PER_INTEGER_VALUE >= 8) \
       ? (static_cast<uint64_t>((val).part[4]) | \
          (static_cast<uint64_t>((val).part[5]) << 16) | \
          (static_cast<uint64_t>((val).part[6]) << 32) | \
          (static_cast<uint64_t>((val).part[7]) << 48)) \
       : 0ULL)
#endif

#if !GNU_EXTENSIONS_ALLOWED && !MICROSOFT_EXTENSIONS_ALLOWED
#define EDG_ALIGN_OF_TYPE(tp) alignment_of_type(tp)
#else
#define EDG_ALIGN_OF_TYPE(tp) f_alignment_of_type(tp)
#endif

#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
#define EDG_ASM_OP_IS_OUTPUT(aop) ((aop)->is_output_operand != 0)
#define EDG_ASM_OP_CONSTRAINT_STR(aop) \
  ((aop)->constraints_string ? std::string((aop)->constraints_string) : std::string(""))
#else
#define EDG_ASM_OP_IS_OUTPUT(aop) (((aop)->modifiers & aom_output) != 0)
#define EDG_ASM_OP_CONSTRAINT_STR(aop) \
  ([](an_asm_operand_ptr _op) -> std::string { \
    std::string _s; \
    if (_op->modifiers & aom_output) { \
      _s += (_op->modifiers & aom_input) ? "+" : "="; \
    } \
    for (an_asm_operand_constraint_ptr _c = _op->constraints; _c != nullptr; _c = _c->next) { \
      _s += asm_operand_constraint_letters[static_cast<int>(_c->kind)]; \
    } \
    return _s; \
  }(aop))
#endif

BEGIN_EDG_NAMESPACE

struct llvm_gen_be_debug_state_t;

/**
 * @struct ScopeCleanup
 * @brief Represents an active automatic variable requiring RAII cleanup.
 * @details Tracks destructor routines, outlined SEH `__finally` funclets, or static guard aborts.
 */
struct ScopeCleanup {
    a_routine_ptr dtor_routine; ///< The destructor routine to invoke.
    llvm::Value* object_ptr;    ///< Pointer to the object to be destroyed.
    llvm::Function* finally_func = nullptr; ///< The outlined __finally funclet to call.
    llvm::BasicBlock* normal_dest; ///< The basic block to branch to after normal cleanup.
    bool is_active;             ///< Whether this cleanup is currently active in the scope.
    bool is_guard_abort = false;///< True if this is a thread-safe static guard abort.
    llvm::Value* guard_abort_ptr = nullptr; ///< Guard variable pointer to pass to __cxa_guard_abort.
    llvm::Value* saved_stack_ptr = nullptr; ///< Saved stack pointer from `llvm.stacksave` for VLA scope restoration.
};

/**
 * @struct ScopeCleanupStack
 * @brief Manages the stack of active cleanups for RAII.
 * @details Stores `ScopeCleanup` entries in lexical activation order.
 */
struct ScopeCleanupStack {
    std::vector<ScopeCleanup> cleanups; ///< Active scope cleanup entries.
};

/**
 * @struct GlobalDynamicInitEntry
 * @brief Tracks a namespace-scope or class-static variable requiring dynamic initialization or destruction.
 * @details Used by `llvm_lower_global_ctors_and_dtors` and `__tls_init` synthesis when EDG IL lowering
 *          has not pre-synthesized an initialization routine (`__sti__*`).
 */
struct GlobalDynamicInitEntry {
    a_variable_ptr var;       ///< The EDG global, static, or thread_local variable.
    a_dynamic_init_ptr dip;   ///< The dynamic initialization record for the variable.
};

/**
 * @brief Pushes a cleanup entry onto a `ScopeCleanupStack`.
 * @details Validates pointers and appends @p entry to @p stack.
 * @param[in,out] stack Pointer to the target cleanup stack.
 * @param[in] entry Pointer to the cleanup entry to push.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if null.
 */
inline llvm_gen_be_error_t llvm_cleanup_stack_push(
    ScopeCleanupStack* stack,
    const ScopeCleanup* entry) noexcept {
  stack->cleanups.push_back(*entry);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Retrieves the current depth of a `ScopeCleanupStack`.
 * @details Writes the number of active cleanups in @p stack to @p out_size.
 * @param[in] stack Pointer to the cleanup stack to query.
 * @param[out] out_size Receives the current cleanup stack size.
 * @return llvm_gen_be_error_t::ok on success.
 */
inline llvm_gen_be_error_t llvm_cleanup_stack_size(
    const ScopeCleanupStack* stack,
    size_t* out_size) noexcept {
  *out_size = stack->cleanups.size();
  return llvm_gen_be_error_t::ok;
}

/**
 * @struct LLVMBackendState
 * @brief Global state for the LLVM generation backend.
 * @details Encapsulates all persistent data structures, caches, and context needed
 *          during the lowering of EDG IL to LLVM IR.
 */
struct LLVMBackendState {
  std::unique_ptr<llvm::LLVMContext> context;       ///< LLVM context instance.
  std::unique_ptr<llvm::Module> module;             ///< Top-level LLVM module.
  std::unique_ptr<llvm::IRBuilder<>> builder;       ///< IR instruction builder.
  std::unordered_map<a_type_ptr, llvm::Type*> type_cache; ///< Cache mapping EDG a_type_ptr to llvm::Type*.
  std::unordered_map<a_variable_ptr, llvm::Value*> local_vars; ///< Symbol table mapping EDG variables to allocas/values.
  std::unordered_map<a_variable_ptr, llvm::Value*> vla_saved_stacks; ///< VLA stack pointer storage for \@llvm.stacksave.
  std::unordered_map<a_type_ptr, a_vla_dimension_ptr> array_to_vla_dim; ///< Dimension map for variable-length arrays.
  std::unordered_map<a_vla_dimension_ptr, llvm::Value*> vla_dimension_values; ///< Evaluated dimension values.
  std::unordered_map<a_dynamic_init_ptr, llvm::Value*> reused_values; ///< Map for enk_reuse_value cache mappings.

  std::vector<llvm::BasicBlock*> break_blocks;      ///< Stack of target basic blocks for break.
  std::vector<llvm::BasicBlock*> continue_blocks;   ///< Stack of target basic blocks for continue.
  std::vector<size_t> break_cleanup_depths;         ///< Cleanup stack depths at break target scopes.
  std::vector<size_t> continue_cleanup_depths;      ///< Cleanup stack depths at continue target scopes.
  std::vector<llvm::BasicBlock*> current_landing_pads; ///< Stack of active exception landing pads.
  std::vector<size_t> landing_pad_cleanup_depths;   ///< Cleanup stack depths at enclosing landing pad scopes.
  std::vector<llvm::Value*> active_funclet_pads;    ///< Stack of active Windows SEH funclet pads (catchpad/cleanuppad).
  std::vector<llvm::BasicBlock*> seh_leave_blocks;  ///< Stack of target basic blocks for MSVC `__leave` statements.
  std::vector<size_t> stmt_expr_cleanup_depths;     ///< Cleanup stack depths at enclosing GNU statement-expression (`({ ... })`) entries.
  bool enable_async_seh;                            ///< True when MSVC `/EHa` asynchronous SEH mode is enabled.
  std::vector<GlobalDynamicInitEntry> global_dynamic_inits; ///< Tracked global/static/thread_local variables requiring dynamic init.
  bool current_routine_is_ctor_or_dtor = false;     ///< True when the currently lowered routine is a constructor or destructor.
  std::unordered_map<a_statement_ptr, const char*> omp_pragmas; ///< Associated OpenMP pragma directive strings by statement.
  
  std::vector<llvm::Value*> escaped_allocas;        ///< Allocas that need llvm.localescape.
  std::unordered_map<llvm::Value*, int> escaped_var_indices; ///< Indices of escaped allocas.

  ScopeCleanupStack cleanup_stack; ///< RAII scope cleanup stack.
  std::unordered_map<a_label_ptr, llvm::BasicBlock*> label_blocks; ///< Map from EDG labels to LLVM basic blocks.
  std::unordered_map<a_label_ptr, size_t> label_cleanup_depths;    ///< Map from EDG labels to cleanup depths.
  llvm::Value* current_coro_handle = nullptr;       ///< Active coroutine frame handle (`coro.begin`).
  llvm::Value* current_coro_promise = nullptr;      ///< Active coroutine promise alloca pointer.
  llvm::Value* current_coro_gro_slot = nullptr;     ///< Active coroutine ramp `get_return_object` stack slot.
  llvm::BasicBlock* current_coro_cleanup_bb = nullptr; ///< Active coroutine frame cleanup/destroy block.
  llvm::BasicBlock* current_coro_suspend_bb = nullptr; ///< Active coroutine final suspend/end block.
  bool sanitize_undefined = false;                  ///< True to emit UBSan runtime checks on arithmetic/pointer operations.
  std::unordered_map<a_switch_case_entry_ptr, llvm::BasicBlock*> case_blocks; ///< Map from switch case entries to LLVM basic blocks.
  std::unordered_map<std::string, llvm::GlobalVariable*> string_literal_cache; ///< Deduplication cache for string literal globals.
  std::unordered_map<a_constant_ptr, llvm::GlobalVariable*> temporary_constant_cache; ///< Deduplication cache for `abk_temporary` address constants.
  std::vector<llvm::Value*> current_function_params; ///< Parameter allocas/values for the currently lowered function (for `abk_param_ref`).
  
  llvm_gen_be_debug_state_t* dbg_state = nullptr;   ///< Active DWARF debug state handle.
  llvm_gen_be_error_context_t err_context;          ///< Persistent diagnostic context buffer.
  
  llvm::Value* current_stmt_expr_result_ptr = nullptr; ///< Current temporary allocation for GNU statement expressions.
  llvm_opt_level_t opt_level = llvm_opt_level_t::O0;   ///< Configured LLVM optimization level.
  bool emit_debug_info = true;                         ///< Whether to emit DWARF/CodeView debug metadata.
  std::string target_triple_override;                  ///< Optional target triple override string.
  std::string split_dwarf_file;                        ///< Optional Split DWARF (.dwo) output filename.
  std::string target_cpu;                              ///< Optional target CPU override (`-mcpu=` / `-march=`).
  std::string target_features;                         ///< Optional target feature string (`-mattr=`).
  std::optional<llvm::Reloc::Model> reloc_model;       ///< Optional relocation model (`-fPIC` / `-fno-pic`).
  std::optional<llvm::CodeModel::Model> code_model;    ///< Optional code model (`-mcmodel=`).
  std::unordered_set<a_scope_ptr> visited_scopes;      ///< Deduplication set of visited scopes during TU traversal.
  std::vector<std::string> extra_linker_args;          ///< User-supplied linker flags (`-L`, `-l`, `-Wl,`).
  std::vector<std::string> extra_linker_inputs;        ///< Extra object/archive file paths forwarded to `llvm_link_executable`.
  bool defer_final_link = false;                       ///< True when multi-TU compilation defers final executable linking to `llvm_finalize_multi_tu_link`.
  bool fopenmp_enabled = false;                        ///< Whether `-fopenmp` was requested via CLI.
  bool enable_ubsan_checks = false;                    ///< Whether front-end UBSan IR checks are enabled.
  a_pragma_ptr active_pragma_list = nullptr;           ///< Optional active EDG pragma AST list for OpenMP directive lookup.
};

extern LLVMBackendState* be_state; ///< Global pointer to the active LLVM backend state.

/**
 * @brief Mangles an unlowered C++ entity (variable, function, method, or special member) under Itanium or MSVC ABI.
 * @details Encodes nested namespace/class scopes, cv-qualifiers, reference qualifiers, template
 *          arguments (`I...E`), standard substitutions (`St`), and function parameter types
 *          when `DO_IL_LOWERING` did not pre-mangle `scp->name`.
 * @param[in] scp Pointer to the entity's `a_source_correspondence`.
 * @param[in] entity_type Pointer to the entity's EDG type (`tk_routine` for functions, or data type).
 * @param[in] is_msvc `true` to synthesize Microsoft Visual C++ (`?name@...`) mangling, `false` for Itanium (`_Z...`).
 * @param[out] out_mangled Receives the synthesized mangled symbol name.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t llvm_mangle_unlowered_entity(
    a_source_correspondence* scp,
    a_type_ptr entity_type,
    bool is_msvc,
    std::string* out_mangled) noexcept;

/**
 * @brief Resets all per-translation-unit maps and state in an `LLVMBackendState`.
 * @details Clears `type_cache`, `local_vars`, `global_dynamic_inits`, `omp_pragmas`,
 *          `escaped_allocas`, label/case/cleanup stacks, and scope caches while preserving
 *          CLI configuration options across multi-TU compilation (`COMPILE_MULTIPLE_SOURCE_FILES`).
 * @param[in,out] state Pointer to the backend state to reset.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `state` is null.
 */
llvm_gen_be_error_t reset_llvm_backend_tu_state(LLVMBackendState* state) noexcept;

/**
 * @brief Legacy compatibility helper redirecting to llvm_type_from_edg_type.
 * @details Convenience wrapper returning llvm::Type* while inspecting error codes internally.
 * @param[in] edg_type Pointer to the EDG type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t get_llvm_type(a_type_ptr edg_type, llvm::Type** out_type) noexcept;

/**
 * @brief Generates or retrieves the LLVM global variable for a given type's RTTI type_info.
 * @details Generates `type_info` structures for fundamental types, pointers, classes, and derived classes,
 *          including Itanium ABI `__si_class_type_info` and `__vmi_class_type_info` descriptors,
 *          as well as MSVC TypeDescriptors.
 * @param[in] type The EDG AST type pointer.
 * @param[out] out_const Receives the resulting LLVM Constant pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_const) noexcept;

/**
 * @brief Dispatches an EDG statement node (`a_statement_ptr`) to the corresponding LLVM IR lowering routine.
 * @details Attaches source debug location metadata when debug info is active and lowers control flow,
 *          expressions, initializations, VLAs, exception blocks, coroutines, and inline assembly.
 * @param[in] stmt Pointer to the EDG statement node to lower.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept;

/**
 * @brief Recursively precomputes cleanup stack depths for all labels in a statement tree.
 * @details Traverses nested blocks, `if`/`else`, loops, `switch`, and `try` bodies starting at
 *          @p base_cleanup_depth, recording the active RAII/VLA cleanup depth at each `stmk_label`
 *          in `be_state->label_cleanup_depths`.
 * @param[in] root_stmt Root statement list or block to scan.
 * @param[in] base_cleanup_depth Cleanup stack depth at entry to @p root_stmt.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t llvm_precompute_function_labels(
    a_statement_ptr root_stmt,
    size_t base_cleanup_depth) noexcept;

/**
 * @brief Lowers all EDG global variables in the translation unit.
 * @details Traverses the primary file scope and all nested namespace and class scopes.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_global_variables(void) noexcept;

/**
 * @brief Lowers all EDG routine declarations in the translation unit.
 * @details Traverses the primary file scope and all nested namespace and class scopes.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_function_declarations(void) noexcept;

/**
 * @brief Computes return value `sret` and coerced-type information across all 11 supported target ABIs.
 * @details Inspects the active module's target triple and dispatches to the matching target ABI
 *          return classification routine (SysV x86_64, Win64, AAPCS64, x86_32, ARM32, RISC-V,
 *          LoongArch64, MIPS/MIPS64, PPC64, SystemZ s390x, SPARC V8/V9, or WebAssembly).
 * @param[in] ret_ty Pointer to the EDG return type.
 * @param[in] is_vectorcall True when the routine uses `X86_VectorCall` calling convention.
 * @param[out] out_sret Receives `true` if the return value requires a hidden `sret` pointer parameter.
 * @param[out] out_coerced_ty Receives the coerced return `llvm::Type*` (or `nullptr` if uncoerced).
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_compute_target_return_info(
    a_type_ptr ret_ty,
    bool is_vectorcall,
    bool* out_sret,
    llvm::Type** out_coerced_ty) noexcept;

/**
 * @brief Lowers all EDG routine definitions in the translation unit.
 * @details Traverses the primary file scope and all nested namespace and class scopes.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_function_definitions(void) noexcept;

/**
 * @brief Lowers C++20 Module interfaces, partitions, and global linkage visibility settings.
 * @details Emits module flags (`C++20 Module`, `C++20 Module Partition`), named metadata nodes
 *          (`!edg.cpp20.module`, `!edg.cpp20.imports`), a module initialization guard variable
 *          (`__module_init_guard_<name>`), and an idempotent module initializer routine
 *          (`__module_init_<name>`) that invokes imported module initializers and local dynamic
 *          initialization routines.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_module_metadata(void) noexcept;

/**
 * @brief Lowers global constructors and destructors into llvm.global_ctors/dtors or MSVC .CRT sections.
 * @details Implements or declares `llvm_lower_global_ctors_and_dtors` for the EDG LLVM backend.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_global_ctors_and_dtors(void) noexcept;

/**
 * @brief Constructs the target LLVM data layout string based on EDG configuration.
 * @details Encodes endianness, pointer size, and primitive alignments into the data layout string.
 * @param[out] out_dl Receives the generated data layout string on success.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointer is null.
 */
llvm_gen_be_error_t build_data_layout(std::string* out_dl) noexcept;

/**
 * @brief Emits the configured output files (.ll, .bc, .s, .o) for the compiled module.
 * @details Runs optimization pipelines, verifies module consistency, and invokes TargetMachine emission.
 * @param[in] base_name The base filename to use for generated output artifacts.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t generate_llvm_output_file(const char* base_name) noexcept;

/**
 * @brief Verifies that all basic blocks in the module possess proper terminators.
 * @details Traverses all non-declaration functions and inspects each basic block.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_gen_be_verify_state(void) noexcept;

/**
 * @brief Emits LLVM IR for a dynamic initialization entry into a target memory pointer.
 * @details Handles `dik_none`, `dik_zero`, `dik_constant`, `dik_expression`,
 *          `dik_class_result_via_ctor`, `dik_nonconstant_aggregate`, `dik_bitwise_copy`,
 *          and `dik_constructor` (including default argument evaluation and array element loops).
 * @param[in] dip Pointer to the EDG dynamic initialization entry.
 * @param[in] ptr Destination memory pointer where the initialized value is stored.
 * @param[in] obj_type Lowered LLVM type of the object being initialized.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_dynamic_init(a_dynamic_init_ptr dip, llvm::Value* ptr, llvm::Type* obj_type) noexcept;

/**
 * @brief Resolves the symbol name for an EDG routine.
 * @details Validates @p routine and @p out_name and returns `source_corresp.name` (or `"anon_fn"`).
 * @param[in] routine Pointer to the EDG routine.
 * @param[out] out_name Receives the resolved null-terminated routine symbol name.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if null.
 */
inline llvm_gen_be_error_t llvm_get_routine_symbol_name(
    a_routine_ptr routine,
    const char** out_name) noexcept {
  *out_name = (routine->source_corresp.name && routine->source_corresp.name[0] != '\0')
                  ? routine->source_corresp.name
                  : "anon_fn";
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves the symbol name for an EDG variable.
 * @details Returns `source_corresp.name` when non-empty (or `"anon_var"`).
 * @param[in] var Pointer to the EDG variable.
 * @param[out] out_name Receives the resolved null-terminated variable symbol name.
 * @return llvm_gen_be_error_t::ok on success.
 */
inline llvm_gen_be_error_t llvm_get_variable_symbol_name(
    a_variable_ptr var,
    const char** out_name) noexcept {
  *out_name = (var->source_corresp.name && var->source_corresp.name[0] != '\0')
                  ? var->source_corresp.name
                  : "anon_var";
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE

#endif
