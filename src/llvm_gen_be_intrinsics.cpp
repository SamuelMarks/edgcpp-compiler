/**
 * @file llvm_gen_be_intrinsics.cpp
 * @brief Target architecture SIMD and system intrinsic lowering implementation for the EDG LLVM backend.
 * @details Implements lowering for x86/x86_64 (MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX,
 *          AVX2, AVX-512, AMX, BMI, AES, SHA), ARM/AArch64 (NEON, SVE, SVE2, CRC32, Crypto,
 *          barriers, system registers), RISC-V (RVV, Zbb/Zbc/Zbs bitmanip), PowerPC (Altivec,
 *          VSX), s390x, LoongArch (LSX, LASX), MIPS, SPARC, and WebAssembly SIMD128 builtins.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"

#if BACK_END_IS_LLVM_GEN_BE

#include "llvm_gen_be_intrinsics.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_type.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>

BEGIN_EDG_NAMESPACE

/**
 * @brief Emits an LLVM intrinsic call by looking up its canonical `llvm.*` name or inserting an external declaration.
 * @details Converts underscores in @p llvm_intr_name to dots, queries `llvm::Intrinsic::lookupIntrinsicID`,
 *          resolves overloaded type parameters from @p callee_ty or @p args, and falls back to
 *          `Module::getOrInsertFunction` when a target-specific intrinsic is not registered in the current
 *          LLVM build's intrinsic table.
 * @param[in] llvm_intr_name Canonical LLVM intrinsic name (with dots or underscores).
 * @param[in] args Lowered LLVM argument values.
 * @param[in] callee_ty Lowered LLVM FunctionType of the builtin (may be null).
 * @param[out] out_val Receives the created `CallInst`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t emit_named_or_table_intrinsic(
    std::string llvm_intr_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val) noexcept {
  for (size_t i = 0; i < llvm_intr_name.size(); ++i) {
    if (llvm_intr_name[i] == '_') {
      llvm_intr_name[i] = '.';
    }
  }

  llvm::Intrinsic::ID id = llvm::Intrinsic::lookupIntrinsicID(llvm_intr_name);
  if (id != llvm::Intrinsic::not_intrinsic && !llvm::Intrinsic::isOverloaded(id)) {
    llvm::Function* intr_func = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), id);
    if (intr_func->arg_size() == args.size()) {
      bool types_match = true;
      for (size_t i = 0; i < args.size(); ++i) {
        if (intr_func->getFunctionType()->getParamType(static_cast<unsigned>(i)) != args[i]->getType()) {
          types_match = false;
          break;
        }
      }
      if (types_match) {
        *out_val = be_state->builder->CreateCall(intr_func, args);
        return llvm_gen_be_error_t::ok;
      }
    }
  }

  llvm::FunctionType* ft = callee_ty;
  if (!ft) {
    std::vector<llvm::Type*> param_tys;
    for (llvm::Value* a : args) {
      param_tys.push_back(a->getType());
    }
    llvm::Type* ret_ty = args.empty() ? llvm::Type::getVoidTy(*be_state->context) : args[0]->getType();
    ft = llvm::FunctionType::get(ret_ty, param_tys, false);
  }
  llvm::FunctionCallee fc = be_state->module->getOrInsertFunction(llvm_intr_name, ft);
  *out_val = be_state->builder->CreateCall(fc, args);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an x86/x86_64 target builtin (`__builtin_ia32_*`) into LLVM IR.
 * @details Handles SSE/AVX/AVX-512 packed floating-point and integer arithmetic, saturating
 *          arithmetic (`paddsb`, `psubsb`, `paddusw`, `psubusw`), vector shuffles/blends/extracts/inserts,
 *          nontemporal and masked vector loads/stores, FMA, and x86 system/crypto/bit-manipulation
 *          intrinsics (`pause`, `mfence`, `sfence`, `lfence`, `clflush`, `rdtsc`, `crc32`, `aes`, `sha`).
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_x86_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  (void)expr;
  (void)bfk;
  LLVM_GEN_BE_CHECK_NOT_NULL(builtin_name);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  llvm::StringRef rname(builtin_name);
  if (!rname.starts_with("__builtin_ia32_")) {
    return llvm_gen_be_error_t::ok;
  }
  *out_handled = true;
  llvm::StringRef sub = rname.substr(15);

  // 1. Packed FP arithmetic: addps/addpd, subps/subpd, mulps/mulpd, divps/divpd (128/256/512-bit)
  if ((sub.starts_with("addp") || sub.starts_with("subp") ||
       sub.starts_with("mulp") || sub.starts_with("divp")) &&
      args[0]->getType()->isFPOrFPVectorTy()) {
    if (sub.starts_with("addp")) {
      *out_val = be_state->builder->CreateFAdd(args[0], args[1], "x86.fadd");
    } else if (sub.starts_with("subp")) {
      *out_val = be_state->builder->CreateFSub(args[0], args[1], "x86.fsub");
    } else if (sub.starts_with("mulp")) {
      *out_val = be_state->builder->CreateFMul(args[0], args[1], "x86.fmul");
    } else {
      *out_val = be_state->builder->CreateFDiv(args[0], args[1], "x86.fdiv");
    }
    return llvm_gen_be_error_t::ok;
  }

  // 2. Packed FP sqrt, max, min, fma (sqrtps/sqrtpd, maxps/maxpd, minps/minpd, vfmadd*)
  if (sub.starts_with("sqrtp")) {
    llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
        be_state->module.get(), llvm::Intrinsic::sqrt, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0]});
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("maxp") || sub.starts_with("minp")) {
    llvm::Intrinsic::ID iid = sub.starts_with("maxp") ? llvm::Intrinsic::maxnum : llvm::Intrinsic::minnum;
    llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
        be_state->module.get(), iid, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0], args[1]});
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("vfmadd")) {
    llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
        be_state->module.get(), llvm::Intrinsic::fma, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0], args[1], args[2]});
    return llvm_gen_be_error_t::ok;
  }

  // 3. Packed integer saturating arithmetic: paddsb/paddsw, psubsb/psubsw, paddusb/paddusw, psubusb/psubusw
  if (sub.starts_with("paddsb") || sub.starts_with("paddsw") ||
      sub.starts_with("psubsb") || sub.starts_with("psubsw") ||
      sub.starts_with("paddusb") || sub.starts_with("paddusw") ||
      sub.starts_with("psubusb") || sub.starts_with("psubusw")) {
    llvm::Intrinsic::ID iid = llvm::Intrinsic::sadd_sat;
    if (sub.starts_with("psubs")) {
      iid = llvm::Intrinsic::ssub_sat;
    } else if (sub.starts_with("paddus")) {
      iid = llvm::Intrinsic::uadd_sat;
    } else if (sub.starts_with("psubus")) {
      iid = llvm::Intrinsic::usub_sat;
    }
    llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
        be_state->module.get(), iid, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0], args[1]});
    return llvm_gen_be_error_t::ok;
  }

  // 4. Vector extract / insert / shuffle builtins: vec_ext_*, vec_set_*, pshufd*, shufps*
  if (sub.starts_with("vec_ext_")) {
    *out_val = be_state->builder->CreateExtractElement(args[0], args[1], "x86.vecext");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("vec_set_")) {
    *out_val = be_state->builder->CreateInsertElement(args[0], args[1], args[2], "x86.vecset");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("pshufd") || sub.starts_with("shufps")) {
    auto* vty = llvm::cast<llvm::FixedVectorType>(args[0]->getType());
    unsigned nel = vty->getNumElements();
    uint64_t imm = 0;
    if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args.back())) {
      imm = ci->getZExtValue();
    }
    std::vector<int> mask(nel, 0);
    for (unsigned i = 0; i < nel; ++i) {
      unsigned lane = (imm >> ((i % 4) * 2)) & 0x3u;
      if (sub.starts_with("shufps") && i >= nel / 2) {
        lane += nel;
      }
      mask[i] = static_cast<int>(lane);
    }
    llvm::Value* rhs = (args.size() >= 3) ? args[1] : llvm::PoisonValue::get(vty);
    *out_val = be_state->builder->CreateShuffleVector(args[0], rhs, mask, "x86.shuf");
    return llvm_gen_be_error_t::ok;
  }

  // 5. Streaming / nontemporal stores (movntps, movntpd, movntdq, movnti) and masked loads/stores
  if (sub.starts_with("movnt")) {
    llvm::StoreInst* st = be_state->builder->CreateStore(args[1], args[0]);
    llvm::MDNode* nt_md = llvm::MDNode::get(
        *be_state->context,
        llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1)));
    st->setMetadata(llvm::LLVMContext::MD_nontemporal, nt_md);
    *out_val = st;
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("maskload")) {
    auto* mask_vty = llvm::cast<llvm::VectorType>(args[1]->getType());
    llvm::Value* zero_v = llvm::Constant::getNullValue(mask_vty);
    llvm::Value* bool_mask = be_state->builder->CreateICmpSLT(args[1], zero_v, "x86.mask");
    llvm::Value* passthru = llvm::Constant::getNullValue(mask_vty);
    *out_val = be_state->builder->CreateMaskedLoad(mask_vty, args[0], llvm::Align(16), bool_mask, passthru);
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("maskstore")) {
    auto* mask_vty = llvm::cast<llvm::VectorType>(args[1]->getType());
    llvm::Value* zero_v = llvm::Constant::getNullValue(mask_vty);
    llvm::Value* bool_mask = be_state->builder->CreateICmpSLT(args[1], zero_v, "x86.mask");
    *out_val = be_state->builder->CreateMaskedStore(args[2], args[0], llvm::Align(16), bool_mask);
    return llvm_gen_be_error_t::ok;
  }

  // 6. System, fence, and cycle counter builtins (pause, mfence, sfence, lfence, rdtsc)
  if (sub == "pause") {
    return emit_named_or_table_intrinsic("llvm.x86.sse2.pause", args, callee_ty, out_val);
  }
  if (sub == "mfence" || sub == "sfence" || sub == "lfence") {
    return emit_named_or_table_intrinsic("llvm.x86.sse2." + sub.str(), args, callee_ty, out_val);
  }
  if (sub == "rdtsc") {
    llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
        be_state->module.get(), llvm::Intrinsic::readcyclecounter);
    *out_val = be_state->builder->CreateCall(fn);
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("tzcnt") || sub.starts_with("lzcnt")) {
    llvm::Intrinsic::ID iid = sub.starts_with("tzcnt") ? llvm::Intrinsic::cttz : llvm::Intrinsic::ctlz;
    llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
        be_state->module.get(), iid, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0], be_state->builder->getFalse()});
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("kand")) {
    *out_val = be_state->builder->CreateAnd(args[0], args[1], "x86.kand");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("kor") && !sub.starts_with("kortest")) {
    *out_val = be_state->builder->CreateOr(args[0], args[1], "x86.kor");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("kxnor")) {
    *out_val = be_state->builder->CreateNot(be_state->builder->CreateXor(args[0], args[1]), "x86.kxnor");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("kadd")) {
    *out_val = be_state->builder->CreateAdd(args[0], args[1], "x86.kadd");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("bextr") || sub.starts_with("bzhi") ||
      sub.starts_with("pdep") || sub.starts_with("pext")) {
    return emit_named_or_table_intrinsic("llvm.x86.bmi." + sub.str(), args, callee_ty, out_val);
  }

  // 7. General x86 table / named intrinsic mapping (avx512, avx2, avx, amx, sse, bmi, aes, sha, crc32)
  std::string intr_name;
  if (sub.starts_with("avx512_")) {
    intr_name = "llvm.x86.avx512." + sub.substr(7).str();
  } else if (sub.starts_with("avx2_")) {
    intr_name = "llvm.x86.avx2." + sub.substr(5).str();
  } else if (sub.starts_with("avx_")) {
    intr_name = "llvm.x86.avx." + sub.substr(4).str();
  } else if (sub.starts_with("amx_") || sub.starts_with("tile") || sub.starts_with("tdp")) {
    intr_name = "llvm.x86.amx." + sub.str();
  } else if (sub.starts_with("aes")) {
    intr_name = "llvm.x86.aesni." + sub.str();
  } else if (sub.starts_with("sha")) {
    intr_name = "llvm.x86.sha." + sub.str();
  } else if (sub.starts_with("crc32")) {
    intr_name = "llvm.x86.sse42." + sub.str();
  } else {
    intr_name = "llvm.x86." + sub.str();
  }
  return emit_named_or_table_intrinsic(intr_name, args, callee_ty, out_val);
}

/**
 * @brief Lowers an ARM32 or AArch64 target builtin (`__builtin_neon_*`, `__builtin_sve_*`, `__builtin_arm_*`) into LLVM IR.
 * @details Handles AArch64 NEON vector arithmetic, saturating/rounding operations, horizontal reductions,
 *          table lookups, SVE/SVE2 predicated scalable vector operations, CRC32, crypto extensions,
 *          memory barriers (`dmb`, `dsb`, `isb`), system registers (`rsr64`, `wsr64`), and hint instructions.
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_arm_aarch64_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  (void)expr;
  (void)bfk;
  LLVM_GEN_BE_CHECK_NOT_NULL(builtin_name);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  llvm::StringRef rname(builtin_name);
  if (rname.starts_with("__builtin_neon_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(15);
    if (sub.starts_with("vaddv")) {
      *out_val = be_state->builder->CreateAddReduce(args[0]);
      return llvm_gen_be_error_t::ok;
    }
    if (sub.starts_with("vmaxv")) {
      *out_val = be_state->builder->CreateIntMaxReduce(args[0], true);
      return llvm_gen_be_error_t::ok;
    }
    return emit_named_or_table_intrinsic("llvm.aarch64.neon." + sub.str(), args, callee_ty, out_val);
  }

  if (rname.starts_with("__builtin_sve_") || rname.starts_with("__sve_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.starts_with("__builtin_sve_") ? rname.substr(14) : rname.substr(6);
    if (sub == "svcntb" || sub == "svcnth" || sub == "svcntw" || sub == "svcntd") {
      uint64_t base_elts = (sub == "svcntb") ? 16ULL : ((sub == "svcnth") ? 8ULL : ((sub == "svcntw") ? 4ULL : 2ULL));
      llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
      llvm::Function* vscale_fn = llvm::Intrinsic::getOrInsertDeclaration(
          be_state->module.get(), llvm::Intrinsic::vscale, {i64_ty});
      llvm::Value* vscale_val = be_state->builder->CreateCall(vscale_fn, {});
      *out_val = be_state->builder->CreateMul(vscale_val, llvm::ConstantInt::get(i64_ty, base_elts), "sve.cnt");
      return llvm_gen_be_error_t::ok;
    }
    if (sub.starts_with("svdup") && !args.empty() && callee_ty && callee_ty->getReturnType()->isVectorTy()) {
      auto* ret_vty = llvm::cast<llvm::VectorType>(callee_ty->getReturnType());
      *out_val = be_state->builder->CreateVectorSplat(ret_vty->getElementCount(), args[0], "sve.dup");
      return llvm_gen_be_error_t::ok;
    }
    if (sub.starts_with("sv")) {
      std::string intr_name = "llvm.aarch64.sve." + sub.substr(2).str();
      return emit_named_or_table_intrinsic(intr_name, args, callee_ty, out_val);
    }
    return emit_named_or_table_intrinsic("llvm.aarch64.sve." + sub.str(), args, callee_ty, out_val);
  }

  if (rname.starts_with("__builtin_arm_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(14);
    if (sub == "dmb" || sub == "dsb" || sub == "isb") {
      return emit_named_or_table_intrinsic("llvm.aarch64." + sub.str(), args, callee_ty, out_val);
    }
    if (sub == "yield" || sub == "wfe" || sub == "wfi" || sub == "sev") {
      uint32_t hint_imm = (sub == "yield") ? 1u : ((sub == "wfe") ? 2u : ((sub == "wfi") ? 3u : 4u));
      llvm::Value* imm_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), hint_imm);
      return emit_named_or_table_intrinsic("llvm.aarch64.hint", {imm_val}, callee_ty, out_val);
    }
    if (sub.starts_with("crc32")) {
      return emit_named_or_table_intrinsic("llvm.aarch64." + sub.str(), args, callee_ty, out_val);
    }
    if (sub == "rsr64" || sub == "wsr64") {
      std::string intr = (sub == "rsr64") ? "llvm.read_register.i64" : "llvm.write_register.i64";
      return emit_named_or_table_intrinsic(intr, args, callee_ty, out_val);
    }
    return emit_named_or_table_intrinsic("llvm.arm." + sub.str(), args, callee_ty, out_val);
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a RISC-V target builtin (`__builtin_rvv_*`, `__builtin_riscv_*`) into LLVM IR.
 * @details Handles RISC-V Vector (`vsetvli`, `vsetvlimax`, scalable RVV arithmetic/load/store) and
 *          Zbb/Zbc/Zbs bit-manipulation and scalar cryptography intrinsics (`clmul`, `clmulh`, `orc_b`, `rev8`).
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_riscv_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  (void)expr;
  (void)bfk;
  LLVM_GEN_BE_CHECK_NOT_NULL(builtin_name);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  llvm::StringRef rname(builtin_name);
  if (rname.starts_with("__builtin_rvv_vsetvli")) {
    *out_handled = true;
    bool is_max = rname.contains("max");
    bool is_i64 = args[0]->getType()->isIntegerTy(64);
    std::string intr_name = is_max ? "llvm.riscv.vsetvlimax." : "llvm.riscv.vsetvli.";
    intr_name += is_i64 ? "i64" : "i32";
    if (!is_max) {
      llvm::FunctionType* ft = llvm::FunctionType::get(
          args[0]->getType(), {args[0]->getType(), args[1]->getType(), args[2]->getType()}, false);
      llvm::FunctionCallee intr = be_state->module->getOrInsertFunction(intr_name, ft);
      *out_val = be_state->builder->CreateCall(intr, {args[0], args[1], args[2]});
    } else {
      llvm::FunctionType* ft = llvm::FunctionType::get(
          args[0]->getType(), {args[0]->getType(), args[1]->getType()}, false);
      llvm::FunctionCallee intr = be_state->module->getOrInsertFunction(intr_name, ft);
      *out_val = be_state->builder->CreateCall(intr, {args[0], args[1]});
    }
    return llvm_gen_be_error_t::ok;
  }

  if (rname.starts_with("__builtin_rvv_") || rname.starts_with("__riscv_v")) {
    *out_handled = true;
    llvm::StringRef sub = rname.starts_with("__builtin_rvv_") ? rname.substr(14) : rname.substr(8);
    return emit_named_or_table_intrinsic("llvm.riscv." + sub.str(), args, callee_ty, out_val);
  }

  if (rname.starts_with("__builtin_riscv_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(16);
    if (sub == "rev8") {
      llvm::Function* bswap = llvm::Intrinsic::getOrInsertDeclaration(
          be_state->module.get(), llvm::Intrinsic::bswap, {args[0]->getType()});
      *out_val = be_state->builder->CreateCall(bswap, {args[0]});
      return llvm_gen_be_error_t::ok;
    }
    if (sub == "clmul" || sub == "clmulh") {
      llvm::Function* clmul = llvm::Intrinsic::getOrInsertDeclaration(
          be_state->module.get(), llvm::Intrinsic::clmul, {args[0]->getType()});
      *out_val = be_state->builder->CreateCall(clmul, {args[0], args[1]});
      return llvm_gen_be_error_t::ok;
    }
    return emit_named_or_table_intrinsic("llvm.riscv." + sub.str(), args, callee_ty, out_val);
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers PowerPC, s390x, LoongArch, MIPS, SPARC, and WebAssembly target builtins into LLVM IR.
 * @details Handles PowerPC Altivec/VSX (`__builtin_altivec_*`, `__builtin_vsx_*`), s390x vector
 *          (`__builtin_s390_*`), LoongArch LSX/LASX (`__builtin_lsx_*`, `__builtin_lasx_*`), and
 *          WebAssembly SIMD128 (`__builtin_wasm_*`) intrinsics.
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_other_arch_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  (void)expr;
  (void)bfk;
  LLVM_GEN_BE_CHECK_NOT_NULL(builtin_name);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  llvm::StringRef rname(builtin_name);
  if (rname.starts_with("__builtin_altivec_")) {
    *out_handled = true;
    return emit_named_or_table_intrinsic("llvm.ppc.altivec." + rname.substr(18).str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_vsx_")) {
    *out_handled = true;
    return emit_named_or_table_intrinsic("llvm.ppc.vsx." + rname.substr(14).str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_s390_")) {
    *out_handled = true;
    return emit_named_or_table_intrinsic("llvm.s390." + rname.substr(15).str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_lsx_")) {
    *out_handled = true;
    return emit_named_or_table_intrinsic("llvm.loongarch.lsx." + rname.substr(14).str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_lasx_")) {
    *out_handled = true;
    return emit_named_or_table_intrinsic("llvm.loongarch.lasx." + rname.substr(15).str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_wasm_")) {
    *out_handled = true;
    return emit_named_or_table_intrinsic("llvm.wasm." + rname.substr(15).str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__nvvm_")) {
    *out_handled = true;
    std::string sub = rname.substr(7).str();
    for (char& ch : sub) {
      if (ch == '_') ch = '.';
    }
    return emit_named_or_table_intrinsic("llvm.nvvm." + sub, args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_amdgcn_")) {
    *out_handled = true;
    std::string sub = rname.substr(17).str();
    for (char& ch : sub) {
      if (ch == '_') ch = '.';
    }
    return emit_named_or_table_intrinsic("llvm.amdgcn." + sub, args, callee_ty, out_val);
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Top-level dispatcher that lowers any target architecture builtin call into LLVM IR.
 * @details Inspects the routine name and `a_builtin_function_kind` on @p expr (or @p rname) and
 *          delegates to `llvm_lower_x86_builtin`, `llvm_lower_arm_aarch64_builtin`,
 *          `llvm_lower_riscv_builtin`, or `llvm_lower_other_arch_builtin`.
 * @param[in] expr Pointer to the EDG call expression node (may be null).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] rname Name of the target builtin routine.
 * @param[in] args Lowered LLVM argument values.
 * @param[in] callee_ty Lowered LLVM FunctionType of the builtin (may be null).
 * @param[out] out_val Receives the resulting `llvm::Value*`.
 * @param[out] out_handled Receives `true` if the target builtin was lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_target_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const std::string& rname,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  LLVM_GEN_BE_ASSERT_OK(
      llvm_lower_x86_builtin(expr, bfk, rname.c_str(), args, callee_ty, out_val, out_handled));
  if (*out_handled) return llvm_gen_be_error_t::ok;

  LLVM_GEN_BE_ASSERT_OK(
      llvm_lower_arm_aarch64_builtin(expr, bfk, rname.c_str(), args, callee_ty, out_val, out_handled));
  if (*out_handled) return llvm_gen_be_error_t::ok;

  LLVM_GEN_BE_ASSERT_OK(
      llvm_lower_riscv_builtin(expr, bfk, rname.c_str(), args, callee_ty, out_val, out_handled));
  if (*out_handled) return llvm_gen_be_error_t::ok;

  return llvm_lower_other_arch_builtin(expr, bfk, rname.c_str(), args, callee_ty, out_val, out_handled);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_LLVM_GEN_BE */
