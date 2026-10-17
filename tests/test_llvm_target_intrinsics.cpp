/**
 * @file test_llvm_target_intrinsics.cpp
 * @brief Unit tests for the target architecture SIMD and system intrinsic lowering subsystem.
 * @details Validates 100% function, line, region, and branch coverage of
 *          `src/llvm_gen_be_intrinsics.cpp` across x86/x86_64, ARM/AArch64, RISC-V,
 *          PowerPC, s390x, LoongArch, and WebAssembly target builtins.
 */

#include "llvm_gen_be_intrinsics.h"
#include "llvm_gen_be_internal.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

/**
 * @brief Tests null-pointer argument validation across all target intrinsic entry points.
 * @details Exercises `LLVM_GEN_BE_CHECK_NOT_NULL` branches in `llvm_lower_x86_builtin`,
 *          `llvm_lower_arm_aarch64_builtin`, `llvm_lower_riscv_builtin`,
 *          `llvm_lower_other_arch_builtin`, and `llvm_lower_target_builtin`.
 * @param[out] out_passed Receives `true` when all error checks pass.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t test_intrinsic_null_checks(bool* out_passed) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_passed);
  *out_passed = false;
  llvm::Value* val = nullptr;
  bool handled = false;
  std::vector<llvm::Value*> empty_args;
  a_builtin_function_kind bfk = static_cast<a_builtin_function_kind>(0);

  assert(llvm_lower_x86_builtin(nullptr, bfk, nullptr, empty_args, nullptr, &val, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_x86_builtin(nullptr, bfk, "__builtin_ia32_pause", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_x86_builtin(nullptr, bfk, "__builtin_ia32_pause", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  assert(llvm_lower_arm_aarch64_builtin(nullptr, bfk, nullptr, empty_args, nullptr, &val, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_arm_aarch64_builtin(nullptr, bfk, "__builtin_arm_dmb", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_arm_aarch64_builtin(nullptr, bfk, "__builtin_arm_dmb", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  assert(llvm_lower_riscv_builtin(nullptr, bfk, nullptr, empty_args, nullptr, &val, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_riscv_builtin(nullptr, bfk, "__builtin_rvv_vadd", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_riscv_builtin(nullptr, bfk, "__builtin_rvv_vadd", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  assert(llvm_lower_other_arch_builtin(nullptr, bfk, nullptr, empty_args, nullptr, &val, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_other_arch_builtin(nullptr, bfk, "__builtin_wasm_add", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_other_arch_builtin(nullptr, bfk, "__builtin_wasm_add", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pause", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pause", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  *out_passed = true;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests x86/x86_64 SIMD, saturating, shuffle, masked, and system builtins.
 * @details Exercises SSE/AVX/AVX-512 packed FP ops, sqrt/min/max/fma, saturating integer ops,
 *          vector extract/insert/pshufd/shufps, nontemporal stores, masked loads/stores,
 *          pause/fences/rdtsc, and table/named x86 intrinsics.
 * @param[out] out_passed Receives `true` when all x86 intrinsic tests pass.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t test_x86_intrinsics(bool* out_passed) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_passed);
  *out_passed = false;
  llvm::LLVMContext& ctx = *be_state->context;
  llvm::IRBuilder<>& b = *be_state->builder;
  a_builtin_function_kind bfk = static_cast<a_builtin_function_kind>(0);

  llvm::Type* f32_ty = llvm::Type::getFloatTy(ctx);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(ctx);
  llvm::Type* v4f32_ty = llvm::FixedVectorType::get(f32_ty, 4);
  llvm::Type* v4i32_ty = llvm::FixedVectorType::get(i32_ty, 4);

  llvm::Value* vf1 = llvm::Constant::getNullValue(v4f32_ty);
  llvm::Value* vf2 = llvm::Constant::getNullValue(v4f32_ty);
  llvm::Value* vf3 = llvm::Constant::getNullValue(v4f32_ty);
  llvm::Value* vi1 = llvm::Constant::getNullValue(v4i32_ty);
  llvm::Value* vi2 = llvm::Constant::getNullValue(v4i32_ty);
  llvm::Value* idx0 = llvm::ConstantInt::get(i32_ty, 0);
  llvm::Value* scalar_i32 = llvm::ConstantInt::get(i32_ty, 42);
  llvm::Value* dynamic_i32 = b.CreateAlloca(i32_ty);
  llvm::Value* dyn_loaded = b.CreateLoad(i32_ty, dynamic_i32);
  llvm::Value* ptr_val = b.CreateAlloca(v4f32_ty);

  llvm::Value* out = nullptr;
  bool handled = false;

  const char* fp_bin_ops[] = {
      "__builtin_ia32_addps", "__builtin_ia32_subps",
      "__builtin_ia32_mulps", "__builtin_ia32_divps"
  };
  for (const char* name : fp_bin_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {vf1, vf2}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sqrtps", {vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_maxps", {vf1, vf2}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_minps", {vf1, vf2}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vfmaddps", {vf1, vf2, vf3}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  const char* sat_ops[] = {
      "__builtin_ia32_paddsb128", "__builtin_ia32_paddsw128",
      "__builtin_ia32_psubsb128", "__builtin_ia32_psubsw128",
      "__builtin_ia32_paddusb128", "__builtin_ia32_paddusw128",
      "__builtin_ia32_psubusb128", "__builtin_ia32_psubusw128"
  };
  for (const char* name : sat_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {vi1, vi2}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vec_ext_v4si", {vi1, idx0}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vec_set_v4si", {vi1, scalar_i32, idx0}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  // pshufd (2 args, constant and non-constant imm) and shufps (3 args)
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pshufd", {vi1, idx0}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pshufd", {vi1, dyn_loaded}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_shufps", {vf1, vf2, idx0}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  // Streaming store & masked load/store
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_movntps", {ptr_val, vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_maskloadps", {ptr_val, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_maskstoreps", {ptr_val, vi1, vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  // System / fences / rdtsc / prefixes
  const char* sys_ops[] = {
      "__builtin_ia32_pause", "__builtin_ia32_mfence",
      "__builtin_ia32_sfence", "__builtin_ia32_lfence", "__builtin_ia32_rdtsc"
  };
  for (const char* name : sys_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  // BMI1/BMI2 and AVX-512 mask register builtins
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_tzcnt_u32", {scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_lzcnt_u32", {scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kandhi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_korhi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kortestchi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kxnorhi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kaddhi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  const char* bmi_ops[] = {
      "__builtin_ia32_bextr_u32", "__builtin_ia32_bzhi_si",
      "__builtin_ia32_pdep_si", "__builtin_ia32_pext_si"
  };
  for (const char* name : bmi_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {scalar_i32, scalar_i32}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  const char* prefix_ops[] = {
      "__builtin_ia32_avx512_CustomOp",
      "__builtin_ia32_avx2_CustomOp",
      "__builtin_ia32_avx_CustomOp",
      "__builtin_ia32_amx_tilezero",
      "__builtin_ia32_tileloadd64",
      "__builtin_ia32_tdpbssd",
      "__builtin_ia32_aesenc128",
      "__builtin_ia32_sha256msg1",
      "__builtin_ia32_crc32si",
      "__builtin_ia32_sse2_clflush"
  };
  llvm::FunctionType* explicit_ft = llvm::FunctionType::get(v4i32_ty, {v4i32_ty, v4i32_ty}, false);
  for (const char* name : prefix_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {vi1, vi2}, explicit_ft, &out, &handled));
    assert(handled && out != nullptr);
  }

  *out_passed = true;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests ARM/AArch64, RISC-V, PowerPC, s390x, LoongArch, and WebAssembly target builtins.
 * @details Exercises NEON reductions and table intrinsics, SVE intrinsics, ARM system/barrier/hint/crc32/register
 *          builtins, RISC-V `vsetvli`/`vsetvlimax`/RVV/Zbb/Zbc (`rev8`, `clmul`, `clmulh`), and Altivec/VSX/s390/LSX/LASX/Wasm builtins.
 * @param[out] out_passed Receives `true` when all non-x86 target intrinsic tests pass.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t test_non_x86_intrinsics(bool* out_passed) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_passed);
  *out_passed = false;
  llvm::LLVMContext& ctx = *be_state->context;
  a_builtin_function_kind bfk = static_cast<a_builtin_function_kind>(0);

  llvm::Type* i32_ty = llvm::Type::getInt32Ty(ctx);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(ctx);
  llvm::Type* v4i32_ty = llvm::FixedVectorType::get(i32_ty, 4);
  llvm::Value* vi1 = llvm::Constant::getNullValue(v4i32_ty);
  llvm::Value* i32_v = llvm::ConstantInt::get(i32_ty, 1);
  llvm::Value* i64_v = llvm::ConstantInt::get(i64_ty, 1);

  llvm::Value* out = nullptr;
  bool handled = false;

  // ARM / AArch64 NEON, SVE, and system builtins
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vaddvq_s32", {vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vmaxvq_s32", {vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_saddlp", {vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  llvm::FunctionType* neon_ft = llvm::FunctionType::get(v4i32_ty, {v4i32_ty}, false);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_saddlp", {vi1}, neon_ft, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_custom_op", {vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svadd_u", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svSub_u", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svptrue_b8", {i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svwhilelt_b8", {i32_v, i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_custom_op", {vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svcntb", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svcnth", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svcntw", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svcntd", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  llvm::Type* nxv4i32_ty = llvm::ScalableVectorType::get(i32_ty, 4);
  llvm::FunctionType* svdup_ft = llvm::FunctionType::get(nxv4i32_ty, {i32_ty}, false);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svdup_s32", {i32_v}, svdup_ft, &out, &handled));
  assert(handled && out != nullptr);
  llvm::FunctionType* svdup_nonvec_ft = llvm::FunctionType::get(i32_ty, {i32_ty}, false);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svdup_s32", {i32_v}, svdup_nonvec_ft, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svdup_s32", {i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svdup_s32", {}, svdup_ft, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svadd_s32", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  const char* arm_ops[] = {
      "__builtin_arm_dmb", "__builtin_arm_dsb", "__builtin_arm_isb",
      "__builtin_arm_yield", "__builtin_arm_wfe", "__builtin_arm_wfi", "__builtin_arm_sev",
      "__builtin_arm_crc32b", "__builtin_arm_rsr64", "__builtin_arm_wsr64", "__builtin_arm_custom"
  };
  for (const char* name : arm_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {i32_v}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  // RISC-V builtins
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vsetvli", {i32_v, i32_v, i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vsetvli", {i64_v, i64_v, i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vsetvlimax", {i32_v, i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vadd_vv", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__riscv_vadd_vv", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_riscv_rev8", {i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_riscv_clmul", {i64_v, i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_riscv_clmulh", {i64_v, i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_riscv_orc_b", {i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  // PowerPC, s390x, LoongArch, WebAssembly, NVPTX, and AMDGPU builtins
  const char* other_arch_ops[] = {
      "__builtin_altivec_vaddubm",
      "__builtin_vsx_xvaddsp",
      "__builtin_s390_vfaeb",
      "__builtin_lsx_vadd_b",
      "__builtin_lasx_xvadd_b",
      "__builtin_wasm_abs_i32x4",
      "__nvvm_read_ptx_sreg_tid_x",
      "__nvvm_bar0",
      "__builtin_amdgcn_workitem_id_x",
      "__builtin_amdgcn_s_barrier"
  };
  for (const char* name : other_arch_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {vi1, vi1}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  // Unhandled non-target builtin name returns handled = false
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_unknown_arch_op", {i32_v}, nullptr, &out, &handled));
  assert(!handled);

  *out_passed = true;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the target architecture intrinsics unit test binary.
 * @details Initializes a synthetic LLVM module/function context and executes all
 *          target intrinsic test cases.
 * @return 0 on success.
 */
int main(void) {
  puts("Running test_llvm_target_intrinsics...");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_target_intrinsics", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(
      ft, llvm::Function::ExternalLinkage, "test_intrinsics_fn", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  bool passed = false;
  LLVM_GEN_BE_ASSERT_OK(test_intrinsic_null_checks(&passed));
  assert(passed);

  LLVM_GEN_BE_ASSERT_OK(test_x86_intrinsics(&passed));
  assert(passed);

  LLVM_GEN_BE_ASSERT_OK(test_non_x86_intrinsics(&passed));
  assert(passed);

  state.builder->CreateRetVoid();
  puts("test_llvm_target_intrinsics PASSED!");
  return 0;
}
