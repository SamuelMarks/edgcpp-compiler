/**
 * @file llvm_gen_be_expr.cpp
 * @brief Expression lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end expression nodes (an_expr_node_ptr) into LLVM IR
 * values (llvm::Value*).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_expr.h"
#include <type_traits>
#include <vector>

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_vtable.h"
#include "llvm_gen_be_rtti.h"
#include "llvm_gen_be_ptrmem.h"
#include "llvm_gen_be_coro.h"
#include "llvm_gen_be_eh.h"
#include "llvm_gen_be_intrinsics.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_win64.h"
#include "llvm_gen_be_abi_x86_32.h"
#include "llvm_gen_be_abi_arm32.h"
#include "llvm_gen_be_abi_wasm.h"
#include "llvm_gen_be_abi_riscv64.h"
#include "llvm_gen_be_abi_loongarch64.h"
#include "llvm_gen_be_abi_mips.h"
#include "llvm_gen_be_abi_ppc64.h"
#include "llvm_gen_be_abi_s390x.h"
#include "llvm_gen_be_abi_sparc.h"
#include "target.h"
#include "lower_name.h"
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Emits a compiler-rt / libgcc complex multiplication or division runtime call.
 * @details Extracts the real and imaginary components of `v1` and `v2` and calls
 *          `__mulsc3`/`__divsc3` (`float`), `__muldc3`/`__divdc3` (`double`),
 *          `__mulxc3`/`__divxc3` (`x86_fp80`), or `__multc3`/`__divtc3` (`fp128`/`ppc_fp128`).
 * @param[in] sc_name Name of the single-precision runtime function (e.g., `__mulsc3` or `__divsc3`).
 * @param[in] dc_name Name of the double-precision runtime function (e.g., `__muldc3` or `__divdc3`).
 * @param[in] v1 First complex struct operand.
 * @param[in] v2 Second complex struct operand.
 * @param[out] out_val Receives the resulting complex struct LLVM Value.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
static llvm_gen_be_error_t emit_complex_libcall(
    const char* sc_name,
    const char* dc_name,
    llvm::Value* v1,
    llvm::Value* v2,
    llvm::Value** out_val) noexcept {
    llvm::Type* struct_ty = v1->getType();
    llvm::Type* elem_ty = struct_ty->getStructElementType(0);
    bool is_mul = (sc_name[2] == 'm');
    const char* func_name = sc_name;
    if (elem_ty->isDoubleTy()) {
      func_name = dc_name;
    } else if (elem_ty->isX86_FP80Ty()) {
      func_name = is_mul ? "__mulxc3" : "__divxc3";
    } else if (elem_ty->isFP128Ty() || elem_ty->isPPC_FP128Ty()) {
      func_name = is_mul ? "__multc3" : "__divtc3";
    }

    llvm::FunctionType* fty = llvm::FunctionType::get(struct_ty, {elem_ty, elem_ty, elem_ty, elem_ty}, false);
    llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(func_name, fty);

    llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
    llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
    llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
    llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);

    *out_val = be_state->builder->CreateCall(callee, {r1, i1, r2, i2});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Checks whether an EDG type is `volatile`-qualified.
 * @details Queries `f_get_type_qualifiers` directly to determine volatile qualification.
 * @param[in] tp The EDG type pointer to inspect.
 * @param[out] out_vol Receives `true` if `tp` is volatile-qualified.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t check_type_is_volatile(a_type_ptr tp, bool* out_vol) noexcept {
    *out_vol = (f_get_type_qualifiers(tp, TRUE) & TQ_VOLATILE) != 0;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes the integer container bit width for a bitfield load or store.
 * @details Preserves declared byte width for volatile or MSVC bitfields and widens spanning
 *          non-volatile bitfields to 8/16/32-bit storage units.
 * @param[in] field Pointer to the EDG bitfield descriptor.
 * @param[in] is_vol Whether the access is volatile-qualified.
 * @param[out] out_bits Receives the computed container bit width.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t compute_bitfield_container_bits(
    a_field_ptr field,
    bool is_vol,
    unsigned* out_bits) noexcept {
    unsigned bit_offset = field->offset_bit_remainder;
    unsigned bit_size = field->bit_size;
    llvm::Triple triple(be_state->module->getTargetTriple());
    bool is_msvc = triple.isWindowsMSVCEnvironment();
    unsigned container_bits = static_cast<unsigned>(skip_typerefs(field->type)->size * 8);
    if ((!is_vol && !is_msvc) || container_bits == 0) {
        container_bits = ((bit_offset + bit_size + 7) / 8) * 8;
        if (container_bits == 24) container_bits = 32;
    }
    *out_bits = container_bits;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a bitfield load and sign/zero extension from a container address.
 * @details Loads the underlying container integer (preserving declared width for volatile bitfields),
 *          shifts and masks the bitfield bits, and sign- or zero-extends to `expr_type`.
 * @param[in] ptr Pointer to the bitfield storage unit.
 * @param[in] field Pointer to the EDG bitfield descriptor.
 * @param[in] expr_type The target EDG expression type.
 * @param[out] out_val Receives the extracted bitfield value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_bitfield_load(
    llvm::Value* ptr,
    a_field_ptr field,
    a_type_ptr expr_type,
    llvm::Value** out_val) noexcept {
    unsigned bit_offset = field->offset_bit_remainder;
    unsigned bit_size = field->bit_size;
    bool is_vol = false;
    LLVM_GEN_BE_ASSERT_OK(check_type_is_volatile(expr_type, &is_vol));
    unsigned container_bits = 0;
    LLVM_GEN_BE_ASSERT_OK(compute_bitfield_container_bits(field, is_vol, &container_bits));
    
    llvm::Type* container_ty = llvm::Type::getIntNTy(*be_state->context, container_bits);
    llvm::LoadInst* load = be_state->builder->CreateLoad(container_ty, ptr);
    if (is_vol) load->setVolatile(true);
    
    unsigned shift = targ_little_endian ? bit_offset : (container_bits - (bit_offset + bit_size));
    llvm::Value* val = load;
    if (shift > 0) val = be_state->builder->CreateLShr(val, shift);
    
    llvm::APInt mask_ap = llvm::APInt::getLowBitsSet(container_bits, bit_size);
    val = be_state->builder->CreateAnd(val, llvm::ConstantInt::get(container_ty, mask_ap));
    
    if (field->bit_field_is_signed) {
        unsigned shift_up = container_bits - bit_size;
        val = be_state->builder->CreateShl(val, shift_up);
        val = be_state->builder->CreateAShr(val, shift_up);
    }
    
    llvm::Type* target_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(expr_type, &target_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    *out_val = be_state->builder->CreateIntCast(val, target_ty, field->bit_field_is_signed);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a bitfield read-modify-write store into a container address.
 * @details Masks and shifts `new_val` into the bitfield position and updates the container
 *          (using an atomic `cmpxchg` loop when volatile-qualified).
 * @param[in] ptr Pointer to the bitfield storage unit.
 * @param[in] new_val The new value to store into the bitfield.
 * @param[in] field Pointer to the EDG bitfield descriptor.
 * @param[in] expr_type The EDG expression type (checked for volatile qualification).
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_bitfield_store(
    llvm::Value* ptr,
    llvm::Value* new_val,
    a_field_ptr field,
    a_type_ptr expr_type) noexcept {
    unsigned bit_offset = field->offset_bit_remainder;
    unsigned bit_size = field->bit_size;
    bool is_vol = false;
    LLVM_GEN_BE_ASSERT_OK(check_type_is_volatile(expr_type, &is_vol));
    unsigned container_bits = 0;
    LLVM_GEN_BE_ASSERT_OK(compute_bitfield_container_bits(field, is_vol, &container_bits));
    
    llvm::Type* container_ty = llvm::Type::getIntNTy(*be_state->context, container_bits);
    llvm::Value* val = be_state->builder->CreateIntCast(new_val, container_ty, false);
    
    unsigned shift = targ_little_endian ? bit_offset : (container_bits - (bit_offset + bit_size));
    llvm::APInt val_mask_ap = llvm::APInt::getLowBitsSet(container_bits, bit_size);
    val = be_state->builder->CreateAnd(val, llvm::ConstantInt::get(container_ty, val_mask_ap));
    if (shift > 0) val = be_state->builder->CreateShl(val, shift);
    
    llvm::APInt container_mask_ap = ~(val_mask_ap << shift);
    llvm::Constant* container_mask = llvm::ConstantInt::get(container_ty, container_mask_ap);
    
    
    if (is_vol) {
        llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* pre_bb = be_state->builder->GetInsertBlock();
        llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(*be_state->context, "bf.atomic.loop", func);
        llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "bf.atomic.end");
        
        llvm::LoadInst* initial_load = be_state->builder->CreateLoad(container_ty, ptr);
        initial_load->setVolatile(true);
        be_state->builder->CreateBr(loop_bb);
        
        be_state->builder->SetInsertPoint(loop_bb);
        llvm::PHINode* old_val = be_state->builder->CreatePHI(container_ty, 2);
        old_val->addIncoming(initial_load, pre_bb);
        
        llvm::Value* masked_old = be_state->builder->CreateAnd(old_val, container_mask);
        llvm::Value* new_container = be_state->builder->CreateOr(masked_old, val);
        
        llvm::AtomicCmpXchgInst* cmpxchg = be_state->builder->CreateAtomicCmpXchg(
            ptr, old_val, new_container, llvm::MaybeAlign(),
            llvm::AtomicOrdering::SequentiallyConsistent, llvm::AtomicOrdering::SequentiallyConsistent);
        cmpxchg->setVolatile(true);
        
        llvm::Value* success = be_state->builder->CreateExtractValue(cmpxchg, 1);
        llvm::Value* actual_old = be_state->builder->CreateExtractValue(cmpxchg, 0);
        old_val->addIncoming(actual_old, loop_bb);
        
        be_state->builder->CreateCondBr(success, end_bb, loop_bb);
        
        func->insert(func->end(), end_bb);
        be_state->builder->SetInsertPoint(end_bb);
    } else {
        llvm::LoadInst* load = be_state->builder->CreateLoad(container_ty, ptr);
        llvm::Value* masked_container = be_state->builder->CreateAnd(load, container_mask);
        llvm::Value* new_container = be_state->builder->CreateOr(masked_container, val);
        be_state->builder->CreateStore(new_container, ptr);
    }
    
    return llvm_gen_be_error_t::ok;
}



/**
 * @brief Resolves and extracts the LLVM FunctionType from an EDG routine or pointer-to-routine type.
 * @details Unwraps pointers, references, and typerefs, lowering the underlying routine type.
 * @param[in] ty The EDG AST type pointer representing the routine or function pointer.
 * @param[out] out_fn_ty Receives the lowered LLVM FunctionType pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_llvm_function_type(a_type_ptr ty, llvm::FunctionType** out_fn_ty) noexcept {
  *out_fn_ty = nullptr;
  while (ty) {
    if (ty->kind == tk_pointer || is_reference_type(ty)) {
      ty = ty->variant.pointer.type;
    } else if (ty->kind == tk_typeref) {
      ty = ty->variant.typeref.type;
    } else {
      break;
    }
  }
  if (ty && ty->kind == tk_routine) {
    llvm::Type* lt = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(ty, &lt);
    if (err != llvm_gen_be_error_t::ok) return err;
    *out_fn_ty = llvm::cast<llvm::FunctionType>(lt);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a structured UndefinedBehaviorSanitizer (UBSan) `TypeDescriptor` global constant.
 * @details Constructs `{ i16 TypeKind, i16 TypeInfo, [N x i8] TypeName }` describing @p edg_ty
 *          for UBSan type-mismatch, overflow, and float-cast-overflow diagnostics.
 * @param[in] edg_ty Pointer to the EDG type to describe (may be nullptr for unknown type).
 * @param[out] out_desc Receives the created `llvm::GlobalVariable*` for the UBSan `TypeDescriptor`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p out_desc is null.
 */
llvm_gen_be_error_t llvm_emit_ubsan_type_descriptor(
    a_type_ptr edg_ty,
    llvm::GlobalVariable** out_desc) noexcept {
  if (!out_desc) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  uint16_t tk_val = 0xffff;
  uint16_t ti_val = 0;
  const char* tname = "<unknown>";
  if (edg_ty) {
    if (edg_ty->source_corresp.name) {
      tname = edg_ty->source_corresp.name;
    }
    if (edg_ty->kind == tk_integer) {
      tk_val = 0x0000;
      ti_val = static_cast<uint16_t>((edg_ty->size > 0 ? 6 : 0) | 1);
    } else if (edg_ty->kind == tk_float) {
      tk_val = 0x0001;
      ti_val = static_cast<uint16_t>(edg_ty->size * 8);
    }
  }
  llvm::Type* i16_ty = llvm::Type::getInt16Ty(*be_state->context);
  llvm::Constant* name_arr = llvm::ConstantDataArray::getString(*be_state->context, tname);
  llvm::StructType* desc_ty = llvm::StructType::get(
      *be_state->context, {i16_ty, i16_ty, name_arr->getType()});
  llvm::Constant* desc_init = llvm::ConstantStruct::get(
      desc_ty, {llvm::ConstantInt::get(i16_ty, tk_val), llvm::ConstantInt::get(i16_ty, ti_val), name_arr});
  *out_desc = new llvm::GlobalVariable(
      *be_state->module, desc_ty, true, llvm::GlobalValue::PrivateLinkage, desc_init, ".ubsan.type");
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a conditional UndefinedBehaviorSanitizer (UBSan) runtime handler call when @p cond is true.
 * @details Splits the current basic block into a `ubsan.fail` block that invokes @p handler_name
 *          with a source descriptor pointer and @p handler_args, followed by a `ubsan.cont` continuation block.
 * @param[in] handler_name Null-terminated name of the `__ubsan_handle_*` runtime function.
 * @param[in] cond Boolean fault condition (`i1` `llvm::Value*`).
 * @param[in] handler_args Additional `llvm::Value*` arguments passed to the UBSan handler after the descriptor pointer.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `handler_name` or `cond` is null.
 */
llvm_gen_be_error_t llvm_emit_ubsan_check(
    const char* handler_name,
    llvm::Value* cond,
    const std::vector<llvm::Value*>& handler_args) noexcept {
  if (!handler_name || !cond) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::BasicBlock* fail_bb = llvm::BasicBlock::Create(*be_state->context, "ubsan.fail", func);
  llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*be_state->context, "ubsan.cont", func);
  be_state->builder->CreateCondBr(cond, fail_bb, cont_bb);

  be_state->builder->SetInsertPoint(fail_bb);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  uint32_t dbg_line = 1;
  uint32_t dbg_col = 1;
  if (llvm::DebugLoc dl = be_state->builder->getCurrentDebugLocation()) {
    dbg_line = dl.getLine();
    dbg_col = dl.getCol();
  }
  llvm::StructType* ubsan_loc_ty = llvm::StructType::get(*be_state->context, {ptr_ty, i32_ty, i32_ty});
  const std::string& src_fn = be_state->module->getSourceFileName();
  llvm::Constant* fn_str = be_state->builder->CreateGlobalString(
      !src_fn.empty() ? src_fn.c_str() : "<file>", ".ubsan.file");
  llvm::Constant* loc_init = llvm::ConstantStruct::get(
      ubsan_loc_ty, {fn_str, llvm::ConstantInt::get(i32_ty, dbg_line), llvm::ConstantInt::get(i32_ty, dbg_col)});
  llvm::GlobalVariable* loc_gv = new llvm::GlobalVariable(
      *be_state->module, ubsan_loc_ty, true, llvm::GlobalValue::PrivateLinkage, loc_init, ".ubsan.loc");
  std::vector<llvm::Type*> param_tys = {ptr_ty};
  std::vector<llvm::Value*> call_args = {loc_gv};
  for (llvm::Value* a : handler_args) {
    param_tys.push_back(a->getType());
    call_args.push_back(a);
  }
  llvm::FunctionType* fty = llvm::FunctionType::get(
      llvm::Type::getVoidTy(*be_state->context), param_tys, false);
  llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(handler_name, fty);
  be_state->builder->CreateCall(fn, call_args);
  be_state->builder->CreateBr(cont_bb);

  be_state->builder->SetInsertPoint(cont_bb);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a conditional UBSan runtime handler call when @p cond is true.
 * @details Delegates to `llvm_emit_ubsan_check` with an empty extra-argument list.
 * @param[in] cond Boolean fault condition (`i1`).
 * @param[in] handler_name Name of the `__ubsan_handle_*` runtime function.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t emit_ubsan_check_branch(
    llvm::Value* cond,
    const char* handler_name) noexcept {
  return llvm_emit_ubsan_check(handler_name, cond, {});
}

/**
 * @brief Loads an rvalue from an lvalue pointer, preserving volatile, atomic, UBSan, and TBAA semantics.
 * @details Lowers `edg_ty` to an `llvm::Type*`, emits a `CreateLoad`, and attaches volatile/atomic/TBAA metadata.
 * @param[in] ptr The source memory address value.
 * @param[in] edg_ty The EDG type of the loaded value.
 * @param[out] out_val Receives the loaded `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t load_from_lvalue_ptr(
    llvm::Value* ptr,
    a_type_ptr edg_ty,
    llvm::Value** out_val) noexcept {
  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(edg_ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (be_state->enable_ubsan_checks) {
    llvm::Value* is_null = be_state->builder->CreateIsNull(ptr, "ubsan.null");
    LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(is_null, "__ubsan_handle_type_mismatch_v1"));
  }
  a_type_qualifier_set quals = f_get_type_qualifiers(edg_ty, TRUE);
  if ((quals & TQ_C11_ATOMIC) != 0) {
    uint64_t size = be_state->module->getDataLayout().getTypeStoreSize(llvm_ty);
    if ((llvm_ty->isIntegerTy() || llvm_ty->isPointerTy()) &&
        (size == 1 || size == 2 || size == 4 || size == 8 || size == 16)) {
      llvm::LoadInst* load = be_state->builder->CreateLoad(llvm_ty, ptr);
      load->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);
      load->setAlignment(llvm::Align(size));
      if ((quals & TQ_VOLATILE) != 0) load->setVolatile(true);
      *out_val = load;
      return llvm_gen_be_error_t::ok;
    }
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::AllocaInst* tmp = be_state->builder->CreateAlloca(llvm_ty, nullptr, "atomic.load.tmp");
    llvm::FunctionType* fty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
    llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("__atomic_load", fty);
    be_state->builder->CreateCall(
        fn, {llvm::ConstantInt::get(i64_ty, size), ptr, tmp, llvm::ConstantInt::get(i32_ty, 5)});
    *out_val = be_state->builder->CreateLoad(llvm_ty, tmp);
    return llvm_gen_be_error_t::ok;
  }
  llvm::LoadInst* load = be_state->builder->CreateLoad(llvm_ty, ptr);
  if ((quals & TQ_VOLATILE) != 0) load->setVolatile(true);
  if (be_state->enable_ubsan_checks && is_bool_type(edg_ty) && llvm_ty->isIntegerTy(8)) {
    llvm::Value* is_invalid_bool = be_state->builder->CreateICmpUGT(
        load, llvm::ConstantInt::get(llvm_ty, 1), "ubsan.bool.invalid");
    LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(is_invalid_bool, "__ubsan_handle_load_invalid_value"));
  }
  if (be_state->opt_level != llvm_opt_level_t::O0 &&
      (llvm_ty->isIntegerTy() || llvm_ty->isFloatingPointTy() || llvm_ty->isPointerTy())) {
    llvm::MDNode* tbaa_tag = nullptr;
    LLVM_GEN_BE_ASSERT_OK(llvm_get_tbaa_access_tag(edg_ty, edg_ty, 0, &tbaa_tag));
    load->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_tag);
  }
  *out_val = load;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers the first operand `op1` and optional second operand `op1->next` of an operation.
 * @details Evaluates `op1` into `*out_v1` and, if `op1->next` is non-null, evaluates `op1->next` into `*out_v2`.
 * @param[in] op1 Pointer to the first EDG operand expression node.
 * @param[out] out_v1 Receives the lowered first operand value.
 * @param[out] out_v2 Receives the lowered second operand value (or `nullptr` if unary).
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
/**
 * @brief Coerces a scalar value (`sc`) to the element type of `vec_ty` and splats it into a vector.
 * @details Handles floating-point (`FPCast` / `SIToFP`) and integer (`IntCast`) element conversions.
 * @param[in] sc Scalar `llvm::Value*` to broadcast.
 * @param[in] vec_ty Target `llvm::VectorType*`.
 * @param[out] out_splat Receives the broadcasted vector value.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t broadcast_scalar_to_vector(
    llvm::Value* sc,
    llvm::VectorType* vec_ty,
    llvm::Value** out_splat) noexcept {
  llvm::Type* elem_ty = vec_ty->getElementType();
  if (sc->getType() != elem_ty) {
    if (elem_ty->isFloatingPointTy()) {
      sc = sc->getType()->isFloatingPointTy()
               ? be_state->builder->CreateFPCast(sc, elem_ty)
               : be_state->builder->CreateSIToFP(sc, elem_ty);
    } else {
      sc = be_state->builder->CreateIntCast(sc, elem_ty, true);
    }
  }
  *out_splat = be_state->builder->CreateVectorSplat(vec_ty->getElementCount(), sc);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers the first and (optional) second operands of a binary EDG expression.
 * @details Evaluates `op1` into `out_v1` and `op1->next` (if present) into `out_v2`.
 * @param[in] op1 Pointer to the first operand node.
 * @param[out] out_v1 Receives the lowered first operand value.
 * @param[out] out_v2 Receives the lowered second operand value (or nullptr if unary).
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t lower_binary_operands(
    an_expr_node_ptr op1,
    llvm::Value** out_v1,
    llvm::Value** out_v2) noexcept {
  *out_v1 = nullptr;
  *out_v2 = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, out_v1);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (op1->next) {
    err = llvm_lower_expression(op1->next, out_v2);
    if (err != llvm_gen_be_error_t::ok) return err;
    llvm::Type* t1 = (*out_v1)->getType();
    llvm::Type* t2 = (*out_v2)->getType();
    if (t1->isVectorTy() && !t2->isVectorTy()) {
      LLVM_GEN_BE_ASSERT_OK(broadcast_scalar_to_vector(
          *out_v2, llvm::cast<llvm::VectorType>(t1), out_v2));
    } else if (!t1->isVectorTy() && t2->isVectorTy()) {
      LLVM_GEN_BE_ASSERT_OK(broadcast_scalar_to_vector(
          *out_v1, llvm::cast<llvm::VectorType>(t2), out_v1));
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an lvalue expression into its corresponding memory address (`llvm::Value*`).
 * @details Handles local/global variables, array subscripts (including VLAs), struct/union field
 *          selections, pointer-to-member field accesses, dereferences, and complex real/imag parts.
 * @param[in] expr Pointer to the EDG lvalue expression node.
 * @param[out] out_ptr Receives the resulting address pointer value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_lvalue_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_ptr) noexcept {
  if (!expr || !out_ptr) return llvm_gen_be_error_t::invalid_argument;
  
  switch (expr->kind) {
    case enk_constant: {
       if (!expr->variant.constant.ptr || expr->variant.constant.ptr->kind != ck_string) {
           return llvm_gen_be_error_t::unsupported_expr;
       }
       llvm::Constant* out_const = nullptr;
       LLVM_GEN_BE_ASSERT_OK(evaluate_constant(
           expr->variant.constant.ptr,
           llvm::PointerType::getUnqual(*be_state->context),
           &out_const));
       *out_ptr = out_const;
       return llvm_gen_be_error_t::ok;
    }
    case enk_variable: {
       a_variable_ptr var = expr->variant.variable.ptr;
       if (!var) return llvm_gen_be_error_t::unsupported_expr;
       llvm::Value* ptr = nullptr;
       auto it = be_state->local_vars.find(var);
       if (it != be_state->local_vars.end()) {
         ptr = it->second;
       } else if (var->source_corresp.name) {
         const char* name = nullptr;
         LLVM_GEN_BE_ASSERT_OK(llvm_get_variable_symbol_name(var, &name));
         ptr = be_state->module->getNamedGlobal(name);
         if (!ptr) {
           llvm::Type* var_ty = nullptr;
           llvm_gen_be_error_t err = get_llvm_type(expr->type, &var_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           ptr = new llvm::GlobalVariable(
             *be_state->module,
             var_ty,
             false, // isConstant
             llvm::GlobalValue::ExternalLinkage,
             nullptr,
             name
           );
         }
       } else {
         return llvm_gen_be_error_t::unsupported_expr;
       }
       *out_ptr = ptr;
       return llvm_gen_be_error_t::ok;
    }
    case enk_temp_init:
    case enk_lambda: {
       llvm::Type* expr_llvm_ty = nullptr;
       LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &expr_llvm_ty));
       a_dynamic_init_ptr dip = expr->variant.init.dynamic_init;
       llvm::Value* temp_storage = nullptr;
       if (dip && dip->static_temp) {
         temp_storage = new llvm::GlobalVariable(
             *be_state->module, expr_llvm_ty, false,
             llvm::GlobalValue::InternalLinkage,
             llvm::Constant::getNullValue(expr_llvm_ty), ".compoundliteral");
       } else {
         llvm::Function* parent_func = be_state->builder->GetInsertBlock()->getParent();
         llvm::IRBuilder<> tmp_builder(&parent_func->getEntryBlock(), parent_func->getEntryBlock().begin());
         temp_storage = tmp_builder.CreateAlloca(expr_llvm_ty, nullptr, "temp.lval");
       }
       if (dip) {
         be_state->reused_values[dip] = temp_storage;
         LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(dip, temp_storage, expr_llvm_ty));
         if (dip->destructor && dip->has_temporary_lifetime) {
           LLVM_GEN_BE_ASSERT_OK(llvm_emit_eh_cleanup(dip->destructor, temp_storage));
         }
       }
       *out_ptr = temp_storage;
       return llvm_gen_be_error_t::ok;
    }
    case enk_operation: {
       an_expr_node_ptr op1 = expr->variant.operation.operands;
       an_expr_node_ptr op2 = op1->next;
       an_expr_operator_kind k = expr->variant.operation.kind;

       if (k == eok_parens || k == eok_lvalue || k == eok_lvalue_adjust ||
           k == eok_lvalue_cast || k == eok_array_to_pointer) {
         return llvm_lower_lvalue_expression(op1, out_ptr);
       }

       if (k == eok_question) {
         an_expr_node_ptr true_expr = op2;
         an_expr_node_ptr false_expr = true_expr->next;
         llvm::Value* cond = nullptr;
         LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &cond));
         if (!cond->getType()->isIntegerTy(1)) {
           cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
         }
         llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
         llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(*be_state->context, "lcond.true", func);
         llvm::BasicBlock* false_bb = llvm::BasicBlock::Create(*be_state->context, "lcond.false");
         llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "lcond.end");
         be_state->builder->CreateCondBr(cond, true_bb, false_bb);

         be_state->builder->SetInsertPoint(true_bb);
         llvm::Value* true_ptr = nullptr;
         LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(true_expr, &true_ptr));
         llvm::BasicBlock* true_end_bb = be_state->builder->GetInsertBlock();
         be_state->builder->CreateBr(end_bb);

         func->insert(func->end(), false_bb);
         be_state->builder->SetInsertPoint(false_bb);
         llvm::Value* false_ptr = nullptr;
         LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(false_expr, &false_ptr));
         llvm::BasicBlock* false_end_bb = be_state->builder->GetInsertBlock();
         be_state->builder->CreateBr(end_bb);

         func->insert(func->end(), end_bb);
         be_state->builder->SetInsertPoint(end_bb);
         llvm::PHINode* phi = be_state->builder->CreatePHI(
             llvm::PointerType::getUnqual(*be_state->context), 2, "lcond.phi");
         phi->addIncoming(true_ptr, true_end_bb);
         phi->addIncoming(false_ptr, false_end_bb);
         *out_ptr = phi;
         return llvm_gen_be_error_t::ok;
       }

       if (k == eok_pm_field || k == eok_pm_points_to_field) {
         return llvm_lower_pm_field_access(expr, out_ptr);
       }
       if (k == eok_real_part || k == eok_imag_part) {
         bool is_imag = (k == eok_imag_part);
         llvm::Value* v1 = nullptr;
         llvm_gen_be_error_t err = llvm_lower_lvalue_expression(op1, &v1);
         if (err != llvm_gen_be_error_t::ok) return err;
         llvm::Type* pty = nullptr;
         err = get_llvm_type(op1->type, &pty);
         if (err != llvm_gen_be_error_t::ok) return err;
         if (pty->isStructTy()) {
           *out_ptr = be_state->builder->CreateStructGEP(pty, v1, is_imag ? 1 : 0);
         } else if (!is_imag) {
           *out_ptr = v1;
         } else if (expr->type) {
           llvm::Type* elem_ty = nullptr;
           err = get_llvm_type(expr->type, &elem_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (elem_ty->isVoidTy()) return llvm_gen_be_error_t::unsupported_expr;
           *out_ptr = be_state->builder->CreateInBoundsGEP(
               elem_ty, v1, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 1));
         } else {
           return llvm_gen_be_error_t::unsupported_expr;
         }
         return llvm_gen_be_error_t::ok;
       }
       if (k == eok_dot_static || k == eok_points_to_static) {
         llvm::Value* v1 = nullptr;
         LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &v1));
         return llvm_lower_lvalue_expression(op2, out_ptr);
       }
       if (k != eok_subscript && k != eok_dot_field && k != eok_points_to_field && k != eok_indirect) {
         return llvm_gen_be_error_t::unsupported_expr;
       }

       llvm::Value* v1 = nullptr;
       bool op1_is_array = (k == eok_subscript && skip_typerefs(op1->type)->kind == tk_array);
       if (k == eok_dot_field || op1_is_array) {
         LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(op1, &v1));
       } else {
         LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &v1));
       }
       if (!v1->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
       if (k == eok_indirect) {
         *out_ptr = v1;
         return llvm_gen_be_error_t::ok;
       }
       if (k == eok_dot_field || k == eok_points_to_field) {
         if (op2) {
           llvm::Value* offset_val = llvm::ConstantInt::get(
               llvm::Type::getInt64Ty(*be_state->context), op2->variant.field.ptr->offset);
           *out_ptr = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), v1, offset_val);
           return llvm_gen_be_error_t::ok;
         }
         return llvm_gen_be_error_t::unsupported_expr;
       }

       llvm::Value* v2 = nullptr;
       llvm_gen_be_error_t err = llvm_lower_expression(op2, &v2);
       if (err != llvm_gen_be_error_t::ok) return err;
       llvm::Type* elem_ty = nullptr;
       err = get_llvm_type(expr->type, &elem_ty);
       if (err != llvm_gen_be_error_t::ok) return err;

       a_type_ptr unwrapped_ty = skip_typerefs(expr->type);
       if (unwrapped_ty->kind == tk_array) {
           llvm::Value* total_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 1);
           a_type_ptr ty = unwrapped_ty;
           while (ty->kind == tk_array) {
               llvm::Value* dim_size = nullptr;
               if (ty->variant.array.is_vla) {
                   a_vla_dimension_ptr dim = be_state->array_to_vla_dim[ty];
                   dim_size = be_state->builder->CreateZExtOrTrunc(
                       be_state->vla_dimension_values[dim], llvm::Type::getInt64Ty(*be_state->context));
               } else {
                   uint64_t n = ty->variant.array.bound_is_zero ? 0 : (uint64_t)ty->variant.array.variant.number_of_elements;
                   dim_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), n);
               }
               total_size = be_state->builder->CreateMul(total_size, dim_size);
               ty = skip_typerefs(ty->variant.array.element_type);
           }
           llvm::Type* inner_llvm_ty = nullptr;
           err = get_llvm_type(ty, &inner_llvm_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           const llvm::DataLayout &DL = be_state->module->getDataLayout();
           uint64_t elem_size = DL.getTypeAllocSize(inner_llvm_ty);
           llvm::Value* elem_size_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), elem_size);
           llvm::Value* byte_stride = be_state->builder->CreateMul(total_size, elem_size_val);
           
           llvm::Value* index = be_state->builder->CreateZExtOrTrunc(v2, llvm::Type::getInt64Ty(*be_state->context));
           llvm::Value* byte_offset = be_state->builder->CreateMul(index, byte_stride);
           
           *out_ptr = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), v1, byte_offset);
           return llvm_gen_be_error_t::ok;
       }

       *out_ptr = be_state->builder->CreateInBoundsGEP(elem_ty, v1, v2);
       return llvm_gen_be_error_t::ok;
    }
    default:
       return llvm_gen_be_error_t::unsupported_expr;
  }
}

/**
 * @brief Lowers an arithmetic, bitwise, pointer arithmetic, or complex arithmetic expression.
 * @details Supports integer, floating-point, vector, pointer, and C99 `_Complex` operations.
 * @param[in] expr Pointer to the EDG arithmetic operation expression node.
 * @param[out] out_val Receives the computed `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_arithmetic_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  llvm::Value* v1 = nullptr;
  llvm::Value* v2 = nullptr;
  LLVM_GEN_BE_CHECK(lower_binary_operands(op1, &v1, &v2));

#if FIXED_POINT_ALLOWED
  if (expr->type && skip_typerefs(expr->type)->kind == tk_fixed_point && v2 != nullptr) {
    unsigned frac_bits = 15;
    bool is_signed = true;
    bool is_sat = false;
    LLVM_GEN_BE_ASSERT_OK(llvm_get_fixed_point_scale(expr->type, &frac_bits, &is_signed, &is_sat));
    llvm::Type* fx_ty = v1->getType();
    llvm::Value* scale_arg = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), frac_bits);
    an_expr_operator_kind k = expr->variant.operation.kind;
    if (k == eok_add || k == eok_subtract) {
      if (is_sat) {
        llvm::Intrinsic::ID sat_id = (k == eok_add)
            ? (is_signed ? llvm::Intrinsic::sadd_sat : llvm::Intrinsic::uadd_sat)
            : (is_signed ? llvm::Intrinsic::ssub_sat : llvm::Intrinsic::usub_sat);
        llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), sat_id, {fx_ty});
        *out_val = be_state->builder->CreateCall(fn, {v1, v2});
      } else {
        *out_val = (k == eok_add) ? be_state->builder->CreateAdd(v1, v2)
                                  : be_state->builder->CreateSub(v1, v2);
      }
      return llvm_gen_be_error_t::ok;
    }
    if (k == eok_multiply || k == eok_divide) {
      llvm::Intrinsic::ID fx_id = (k == eok_multiply)
          ? (is_sat ? (is_signed ? llvm::Intrinsic::smul_fix_sat : llvm::Intrinsic::umul_fix_sat)
                    : (is_signed ? llvm::Intrinsic::smul_fix : llvm::Intrinsic::umul_fix))
          : (is_sat ? (is_signed ? llvm::Intrinsic::sdiv_fix_sat : llvm::Intrinsic::udiv_fix_sat)
                    : (is_signed ? llvm::Intrinsic::sdiv_fix : llvm::Intrinsic::udiv_fix));
      llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), fx_id, {fx_ty});
      *out_val = be_state->builder->CreateCall(fn, {v1, v2, scale_arg});
      return llvm_gen_be_error_t::ok;
    }
  }
#endif

  switch (expr->variant.operation.kind) {
    case eok_negate:
      if (v1->getType()->isStructTy()) {
        llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
        llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
        llvm::Value* r = be_state->builder->CreateFNeg(r1);
        llvm::Value* i = be_state->builder->CreateFNeg(i1);
        llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r, 0);
        *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      } else if (v1->getType()->isFPOrFPVectorTy()) {
        *out_val = be_state->builder->CreateFNeg(v1);
      } else if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy() &&
                 int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) {
        llvm::Value* zero = llvm::ConstantInt::get(v1->getType(), 0);
        llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::ssub_with_overflow, {v1->getType()});
        llvm::Value* pair = be_state->builder->CreateCall(intr, {zero, v1});
        llvm::Value* ovf = be_state->builder->CreateExtractValue(pair, 1);
        LLVM_GEN_BE_ASSERT_OK(llvm_emit_ubsan_check("__ubsan_handle_negate_overflow", ovf, {v1}));
        *out_val = be_state->builder->CreateExtractValue(pair, 0);
      } else {
        *out_val = be_state->builder->CreateNeg(v1);
      }
      break;
    case eok_unary_plus:
      *out_val = v1;
      break;
    case eok_add:
      if (v1->getType()->isStructTy()) {
        llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
        llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
        llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
        llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);
        llvm::Value* r = be_state->builder->CreateFAdd(r1, r2);
        llvm::Value* i = be_state->builder->CreateFAdd(i1, i2);
        llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r, 0);
        *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      } else if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFAdd(v1, v2);
      else if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy() &&
               int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) {
        llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::sadd_with_overflow, {v1->getType()});
        llvm::Value* pair = be_state->builder->CreateCall(intr, {v1, v2});
        llvm::Value* ovf = be_state->builder->CreateExtractValue(pair, 1);
        LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(ovf, "__ubsan_handle_add_overflow"));
        *out_val = be_state->builder->CreateExtractValue(pair, 0);
      } else *out_val = be_state->builder->CreateAdd(v1, v2);
      break;
    case eok_subtract:
      if (v1->getType()->isStructTy()) {
        llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
        llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
        llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
        llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);
        llvm::Value* r = be_state->builder->CreateFSub(r1, r2);
        llvm::Value* i = be_state->builder->CreateFSub(i1, i2);
        llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r, 0);
        *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      } else if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFSub(v1, v2);
      else if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy() &&
               int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) {
        llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::ssub_with_overflow, {v1->getType()});
        llvm::Value* pair = be_state->builder->CreateCall(intr, {v1, v2});
        llvm::Value* ovf = be_state->builder->CreateExtractValue(pair, 1);
        LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(ovf, "__ubsan_handle_sub_overflow"));
        *out_val = be_state->builder->CreateExtractValue(pair, 0);
      } else *out_val = be_state->builder->CreateSub(v1, v2);
      break;
    case eok_multiply:
      if (v1->getType()->isStructTy()) {
        LLVM_GEN_BE_ASSERT_OK(emit_complex_libcall("__mulsc3", "__muldc3", v1, v2, out_val));
      } else if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFMul(v1, v2);
      else if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy() &&
               int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) {
        llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::smul_with_overflow, {v1->getType()});
        llvm::Value* pair = be_state->builder->CreateCall(intr, {v1, v2});
        llvm::Value* ovf = be_state->builder->CreateExtractValue(pair, 1);
        LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(ovf, "__ubsan_handle_mul_overflow"));
        *out_val = be_state->builder->CreateExtractValue(pair, 0);
      } else *out_val = be_state->builder->CreateMul(v1, v2);
      break;
    case eok_divide:
      if (v1->getType()->isStructTy()) {
        LLVM_GEN_BE_ASSERT_OK(emit_complex_libcall("__divsc3", "__divdc3", v1, v2, out_val));
      } else if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFDiv(v1, v2);
      else {
        if (be_state->enable_ubsan_checks && v2->getType()->isIntegerTy()) {
          llvm::Value* is_zero = be_state->builder->CreateICmpEQ(
              v2, llvm::ConstantInt::get(v2->getType(), 0));
          LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(is_zero, "__ubsan_handle_divrem_overflow"));
        }
        if (int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) *out_val = be_state->builder->CreateSDiv(v1, v2);
        else *out_val = be_state->builder->CreateUDiv(v1, v2);
      }
      break;
    case eok_remainder:
      if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFRem(v1, v2);
      else {
        if (be_state->enable_ubsan_checks && v2->getType()->isIntegerTy()) {
          llvm::Value* is_zero = be_state->builder->CreateICmpEQ(
              v2, llvm::ConstantInt::get(v2->getType(), 0));
          LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(is_zero, "__ubsan_handle_divrem_overflow"));
        }
        if (!int_kind_is_signed[skip_typerefs(expr->type)->variant.integer.int_kind]) *out_val = be_state->builder->CreateURem(v1, v2);
        else *out_val = be_state->builder->CreateSRem(v1, v2);
      }
      break;
    case eok_gnu_min:
    case eok_gnu_max: {
      bool is_min = (expr->variant.operation.kind == eok_gnu_min);
      llvm::Value* cmp = nullptr;
      if (v1->getType()->isFPOrFPVectorTy()) {
        cmp = is_min ? be_state->builder->CreateFCmpOLT(v1, v2)
                     : be_state->builder->CreateFCmpOGT(v1, v2);
      } else {
        bool is_signed = int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind];
        llvm::CmpInst::Predicate pred = is_min
            ? (is_signed ? llvm::CmpInst::ICMP_SLT : llvm::CmpInst::ICMP_ULT)
            : (is_signed ? llvm::CmpInst::ICMP_SGT : llvm::CmpInst::ICMP_UGT);
        cmp = be_state->builder->CreateICmp(pred, v1, v2);
      }
      *out_val = be_state->builder->CreateSelect(cmp, v1, v2);
      break;
    }
    case eok_padd:
    case eok_psubtract:
    case eok_pdiff: {
      a_type_ptr ptr_edg_ty = skip_typerefs(op1->type);
      llvm::Value* base_ptr = v1;
      llvm::Value* idx_val = v2;
      if (ptr_edg_ty->kind != tk_pointer) {
        ptr_edg_ty = skip_typerefs(op1->next->type);
        base_ptr = v2;
        idx_val = v1;
      }
      if (ptr_edg_ty->kind == tk_pointer) {
        llvm::Type* elem_ty = nullptr;
        if (get_llvm_type(ptr_edg_ty->variant.pointer.type, &elem_ty) != llvm_gen_be_error_t::ok) return llvm_gen_be_error_t::unsupported_expr;
        if (expr->variant.operation.kind == eok_pdiff) {
          *out_val = be_state->builder->CreatePtrDiff(elem_ty, v1, v2);
        } else {
          llvm::Value* idx = (expr->variant.operation.kind == eok_psubtract) ? be_state->builder->CreateNeg(idx_val) : idx_val;
          *out_val = be_state->builder->CreateGEP(elem_ty, base_ptr, idx);
        }
      } else if (expr->variant.operation.kind == eok_pdiff) {
        return llvm_gen_be_error_t::unsupported_expr;
      } else {
        *out_val = v1;
      }
      break;
    }
    case eok_fjadd: {
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v1->getType(), v2->getType())), v1, 0);
      *out_val = be_state->builder->CreateInsertValue(res, v2, 1);
      break;
    }
    case eok_jfadd: {
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v2->getType(), v1->getType())), v2, 0);
      *out_val = be_state->builder->CreateInsertValue(res, v1, 1);
      break;
    }
    case eok_fjsubtract: {
      llvm::Value* neg_v2 = be_state->builder->CreateFNeg(v2);
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v1->getType(), v2->getType())), v1, 0);
      *out_val = be_state->builder->CreateInsertValue(res, neg_v2, 1);
      break;
    }
    case eok_jfsubtract: {
      llvm::Value* neg_v1 = be_state->builder->CreateFNeg(v1);
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v2->getType(), v1->getType())), neg_v1, 0);
      *out_val = be_state->builder->CreateInsertValue(res, v1, 1);
      break;
    }
    case eok_jmultiply: {
      llvm::Value* mul = be_state->builder->CreateFMul(v1, v2);
      *out_val = be_state->builder->CreateFNeg(mul);
      break;
    }
    case eok_jdivide: {
      llvm::Value* div = be_state->builder->CreateFDiv(v1, v2);
      *out_val = be_state->builder->CreateFNeg(div);
      break;
    }
    case eok_xconj: {
      if (v1->getType()->isStructTy()) {
          llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
          llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
          llvm::Value* neg_i1 = be_state->builder->CreateFNeg(i1);
          llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r1, 0);
          *out_val = be_state->builder->CreateInsertValue(res, neg_i1, 1);
      } else {
          *out_val = v1;
      }
      break;
    }
    case eok_complement: *out_val = be_state->builder->CreateNot(v1); break;
    case eok_and: *out_val = be_state->builder->CreateAnd(v1, v2); break;
    case eok_or: *out_val = be_state->builder->CreateOr(v1, v2); break;
    case eok_xor: *out_val = be_state->builder->CreateXor(v1, v2); break;
    case eok_shiftl:
      if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy()) {
        unsigned bw = v1->getType()->getIntegerBitWidth();
        llvm::Value* oob = be_state->builder->CreateICmpUGE(v2, llvm::ConstantInt::get(v2->getType(), bw));
        LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(oob, "__ubsan_handle_shift_out_of_bounds"));
      }
      *out_val = be_state->builder->CreateShl(v1, be_state->builder->CreateZExtOrTrunc(v2, v1->getType()));
      break;
    case eok_shiftr: {
      if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy()) {
        unsigned bw = v1->getType()->getIntegerBitWidth();
        llvm::Value* oob = be_state->builder->CreateICmpUGE(v2, llvm::ConstantInt::get(v2->getType(), bw));
        LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(oob, "__ubsan_handle_shift_out_of_bounds"));
      }
      llvm::Value* sh_v2 = be_state->builder->CreateZExtOrTrunc(v2, v1->getType());
      if (!int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) *out_val = be_state->builder->CreateLShr(v1, sh_v2);
      else *out_val = be_state->builder->CreateAShr(v1, sh_v2);
      break;
    }
    default: return llvm_gen_be_error_t::unsupported_expr;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a logical (`&&`, `||`, `!`) or relational (`==`, `!=`, `<`, `<=`, `>`, `>=`, `<=>`) expression.
 * @details Emits short-circuit basic blocks and PHI nodes for `&&`/`||`, three-way comparison selects for `<=>`,
 *          and `icmp`/`fcmp` instructions for scalar, pointer, and vector comparisons.
 * @param[in] expr Pointer to the EDG logical/relational expression node.
 * @param[out] out_val Receives the computed boolean or integer comparison result.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_logical_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1->next;
  
  if (skip_typerefs(op1->type)->kind == tk_ptr_to_member) {
    return llvm_lower_pm_comparison(expr, out_val);
  }

  llvm::Type* expr_llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (expr->variant.operation.kind == eok_land || expr->variant.operation.kind == eok_lor) {
    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(*be_state->context, 
      (expr->variant.operation.kind == eok_land) ? "land.rhs" : "lor.rhs", func);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, 
      (expr->variant.operation.kind == eok_land) ? "land.end" : "lor.end");

    llvm::Value* lhs_val = nullptr;
    err = llvm_lower_expression(op1, &lhs_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::Value* lhs_cond = be_state->builder->CreateICmpNE(lhs_val, llvm::Constant::getNullValue(lhs_val->getType()));
    llvm::BasicBlock* lhs_end_bb = be_state->builder->GetInsertBlock();
    
    if (expr->variant.operation.kind == eok_land) be_state->builder->CreateCondBr(lhs_cond, rhs_bb, end_bb);
    else be_state->builder->CreateCondBr(lhs_cond, end_bb, rhs_bb);

    be_state->builder->SetInsertPoint(rhs_bb);
    llvm::Value* rhs_val = nullptr;
    err = llvm_lower_expression(op2, &rhs_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::Value* rhs_cond = be_state->builder->CreateICmpNE(rhs_val, llvm::Constant::getNullValue(rhs_val->getType()));
    llvm::BasicBlock* rhs_end_bb = be_state->builder->GetInsertBlock();
    be_state->builder->CreateBr(end_bb);

    func->insert(func->end(), end_bb);
    be_state->builder->SetInsertPoint(end_bb);

    llvm::PHINode* phi = be_state->builder->CreatePHI(be_state->builder->getInt1Ty(), 2, 
      (expr->variant.operation.kind == eok_land) ? "land.phi" : "lor.phi");
    phi->addIncoming((expr->variant.operation.kind == eok_land) ? llvm::ConstantInt::getFalse(*be_state->context) : llvm::ConstantInt::getTrue(*be_state->context), lhs_end_bb);
    phi->addIncoming(rhs_cond, rhs_end_bb);
    
    *out_val = be_state->builder->CreateZExt(phi, expr_llvm_ty);
    return llvm_gen_be_error_t::ok;
  }

  llvm::Value* v1 = nullptr;
  llvm::Value* v2 = nullptr;
  err = lower_binary_operands(op1, &v1, &v2);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  if (v1->getType()->isStructTy()) {
    llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
    llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
    llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
    llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);
    bool elem_fp = r1->getType()->isFloatingPointTy();
    if (expr->variant.operation.kind == eok_eq) {
      llvm::Value* r_eq = elem_fp ? be_state->builder->CreateFCmpOEQ(r1, r2) : be_state->builder->CreateICmpEQ(r1, r2);
      llvm::Value* i_eq = elem_fp ? be_state->builder->CreateFCmpOEQ(i1, i2) : be_state->builder->CreateICmpEQ(i1, i2);
      *out_val = be_state->builder->CreateAnd(r_eq, i_eq);
    } else {
      llvm::Value* r_ne = elem_fp ? be_state->builder->CreateFCmpUNE(r1, r2) : be_state->builder->CreateICmpNE(r1, r2);
      llvm::Value* i_ne = elem_fp ? be_state->builder->CreateFCmpUNE(i1, i2) : be_state->builder->CreateICmpNE(i1, i2);
      *out_val = be_state->builder->CreateOr(r_ne, i_ne);
    }
    return llvm_gen_be_error_t::ok;
  }

  bool is_fp = v1->getType()->isFPOrFPVectorTy();
  bool is_ptr = v1->getType()->isPointerTy();
  a_type_ptr raw_op1_ty = skip_typerefs(op1->type);
  bool is_unsigned = !is_fp && !is_ptr && raw_op1_ty->kind == tk_integer &&
                     !int_kind_is_signed[raw_op1_ty->variant.integer.int_kind];

  if (expr->variant.operation.kind == eok_spaceship) {
     llvm::Value* res = nullptr;
     llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
     llvm::Value* lt_val = llvm::ConstantInt::get(i8_ty, -1, true);
     llvm::Value* eq_val = llvm::ConstantInt::get(i8_ty, 0, true);
     llvm::Value* gt_val = llvm::ConstantInt::get(i8_ty, 1, true);
     if (is_fp) {
        llvm::Value* cmp_lt = be_state->builder->CreateFCmpOLT(v1, v2);
        llvm::Value* cmp_eq = be_state->builder->CreateFCmpOEQ(v1, v2);
        llvm::Value* cmp_gt = be_state->builder->CreateFCmpOGT(v1, v2);
        llvm::Value* un_val = llvm::ConstantInt::get(i8_ty, -127, true);
        llvm::Value* sel1 = be_state->builder->CreateSelect(cmp_eq, eq_val, be_state->builder->CreateSelect(cmp_gt, gt_val, un_val));
        res = be_state->builder->CreateSelect(cmp_lt, lt_val, sel1);
     } else {
        llvm::Value* cmp_lt = is_ptr ? be_state->builder->CreateICmpULT(v1, v2) : be_state->builder->CreateICmp(is_unsigned ? llvm::CmpInst::ICMP_ULT : llvm::CmpInst::ICMP_SLT, v1, v2);
        llvm::Value* cmp_eq = be_state->builder->CreateICmpEQ(v1, v2);
        res = be_state->builder->CreateSelect(cmp_lt, lt_val, be_state->builder->CreateSelect(cmp_eq, eq_val, gt_val));
     }

     if (expr_llvm_ty->isStructTy()) {
         *out_val = be_state->builder->CreateInsertValue(llvm::UndefValue::get(expr_llvm_ty), res, 0);
     } else {
         *out_val = be_state->builder->CreateSExtOrTrunc(res, expr_llvm_ty);
     }
     return llvm_gen_be_error_t::ok;
  }

  llvm::CmpInst::Predicate pred;
  switch (expr->variant.operation.kind) {
     case eok_eq: pred = is_fp ? llvm::CmpInst::FCMP_OEQ : llvm::CmpInst::ICMP_EQ; break;
     case eok_ne: pred = is_fp ? llvm::CmpInst::FCMP_ONE : llvm::CmpInst::ICMP_NE; break;
     case eok_lt: pred = is_fp ? llvm::CmpInst::FCMP_OLT : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_ULT : llvm::CmpInst::ICMP_SLT); break;
     case eok_le: pred = is_fp ? llvm::CmpInst::FCMP_OLE : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_ULE : llvm::CmpInst::ICMP_SLE); break;
     case eok_gt: pred = is_fp ? llvm::CmpInst::FCMP_OGT : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_UGT : llvm::CmpInst::ICMP_SGT); break;
     case eok_ge: pred = is_fp ? llvm::CmpInst::FCMP_OGE : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_UGE : llvm::CmpInst::ICMP_SGE); break;
     case eok_not: pred = llvm::CmpInst::ICMP_EQ; v2 = llvm::Constant::getNullValue(v1->getType()); is_fp = false; break;
     default: return llvm_gen_be_error_t::unsupported_expr;
  }
  if (is_fp) *out_val = be_state->builder->CreateFCmp(pred, v1, v2);
  else *out_val = be_state->builder->CreateICmp(pred, v1, v2);
  
  if ((*out_val)->getType()->isVectorTy()) {
     if (expr->variant.operation.kind != eok_not) {
        *out_val = be_state->builder->CreateSExt(*out_val, expr_llvm_ty);
     }
  } else if (expr_llvm_ty->getIntegerBitWidth() > 1) {
     *out_val = be_state->builder->CreateZExt(*out_val, expr_llvm_ty);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a type cast expression (`eok_cast`, `eok_Pass_by_value`, etc.) into LLVM IR.
 * @details Handles int-to-ptr, ptr-to-int, fpext/fptrunc, fptosi/fptoui, sitofp/uitofp, sext/zext/trunc, and bitcast.
 * @param[in] expr Pointer to the EDG cast expression node.
 * @param[out] out_val Receives the converted `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_cast_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  
  llvm::Value* v1 = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  llvm::Type* dest_ty = nullptr;
  err = get_llvm_type(expr->type, &dest_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (dest_ty->isVoidTy()) {
      *out_val = nullptr;
      return llvm_gen_be_error_t::ok;
  }
  llvm::Type* src_ty = v1->getType();
#if FIXED_POINT_ALLOWED
  a_type_ptr raw_src_edg = op1->type ? skip_typerefs(op1->type) : nullptr;
  a_type_ptr raw_dst_edg = expr->type ? skip_typerefs(expr->type) : nullptr;
  bool src_is_fx = (raw_src_edg && raw_src_edg->kind == tk_fixed_point);
  bool dst_is_fx = (raw_dst_edg && raw_dst_edg->kind == tk_fixed_point);
  if (src_is_fx || dst_is_fx) {
    unsigned src_f = 0, dst_f = 0;
    bool src_s = true, dst_s = true, src_sat = false, dst_sat = false;
    if (src_is_fx) LLVM_GEN_BE_ASSERT_OK(llvm_get_fixed_point_scale(raw_src_edg, &src_f, &src_s, &src_sat));
    if (dst_is_fx) LLVM_GEN_BE_ASSERT_OK(llvm_get_fixed_point_scale(raw_dst_edg, &dst_f, &dst_s, &dst_sat));

    if (src_is_fx && dst_is_fx) {
      unsigned common_bw = std::max(src_ty->getIntegerBitWidth(), dest_ty->getIntegerBitWidth());
      llvm::Type* wide_ty = llvm::IntegerType::get(*be_state->context, common_bw);
      llvm::Value* w = be_state->builder->CreateIntCast(v1, wide_ty, src_s);
      if (dst_f > src_f) {
        w = be_state->builder->CreateShl(w, llvm::ConstantInt::get(wide_ty, dst_f - src_f));
      } else if (src_f > dst_f) {
        w = src_s ? be_state->builder->CreateAShr(w, llvm::ConstantInt::get(wide_ty, src_f - dst_f))
                  : be_state->builder->CreateLShr(w, llvm::ConstantInt::get(wide_ty, src_f - dst_f));
      }
      *out_val = be_state->builder->CreateIntCast(w, dest_ty, src_s);
      return llvm_gen_be_error_t::ok;
    }
    if (src_is_fx && dest_ty->isFloatingPointTy()) {
      llvm::Value* fp_v = src_s ? be_state->builder->CreateSIToFP(v1, dest_ty)
                                : be_state->builder->CreateUIToFP(v1, dest_ty);
      llvm::Value* scale_fp = llvm::ConstantFP::get(dest_ty, static_cast<double>(1ULL << src_f));
      *out_val = be_state->builder->CreateFDiv(fp_v, scale_fp, "fx.to.fp");
      return llvm_gen_be_error_t::ok;
    }
    if (dst_is_fx && src_ty->isFloatingPointTy()) {
      llvm::Value* scale_fp = llvm::ConstantFP::get(src_ty, static_cast<double>(1ULL << dst_f));
      llvm::Value* scaled = be_state->builder->CreateFMul(v1, scale_fp, "fp.to.fx");
      *out_val = dst_s ? be_state->builder->CreateFPToSI(scaled, dest_ty)
                       : be_state->builder->CreateFPToUI(scaled, dest_ty);
      return llvm_gen_be_error_t::ok;
    }
    if (src_is_fx) {
      llvm::Value* unscaled = src_s
          ? be_state->builder->CreateAShr(v1, llvm::ConstantInt::get(src_ty, src_f))
          : be_state->builder->CreateLShr(v1, llvm::ConstantInt::get(src_ty, src_f));
      *out_val = be_state->builder->CreateIntCast(unscaled, dest_ty, src_s);
      return llvm_gen_be_error_t::ok;
    }
    bool int_s = int_kind_is_signed[raw_src_edg->variant.integer.int_kind];
    llvm::Value* cast_v = be_state->builder->CreateIntCast(v1, dest_ty, int_s);
    *out_val = be_state->builder->CreateShl(cast_v, llvm::ConstantInt::get(dest_ty, dst_f));
    return llvm_gen_be_error_t::ok;
  }
#endif
  if (src_ty == dest_ty) {
      *out_val = v1;
      return llvm_gen_be_error_t::ok;
  }

  // _Complex conversions ({ elem_ty, elem_ty })
  bool src_is_complex = src_ty->isStructTy();
  bool dest_is_complex = dest_ty->isStructTy();
  if (src_is_complex && dest_is_complex) {
      llvm::Type* dst_elem_ty = dest_ty->getStructElementType(0);
      llvm::Value* r = be_state->builder->CreateExtractValue(v1, 0);
      llvm::Value* i = be_state->builder->CreateExtractValue(v1, 1);
      r = be_state->builder->CreateFPCast(r, dst_elem_ty);
      i = be_state->builder->CreateFPCast(i, dst_elem_ty);
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(dest_ty), r, 0);
      *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      return llvm_gen_be_error_t::ok;
  }
  if (dest_is_complex) {
      llvm::Type* dst_elem_ty = dest_ty->getStructElementType(0);
      llvm::Value* r = src_ty->isFloatingPointTy()
                           ? be_state->builder->CreateFPCast(v1, dst_elem_ty)
                           : be_state->builder->CreateSIToFP(v1, dst_elem_ty);
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(dest_ty), r, 0);
      *out_val = be_state->builder->CreateInsertValue(res, llvm::Constant::getNullValue(dst_elem_ty), 1);
      return llvm_gen_be_error_t::ok;
  }
  if (src_is_complex) {
      v1 = be_state->builder->CreateExtractValue(v1, 0);
      src_ty = v1->getType();
      if (src_ty == dest_ty) {
          *out_val = v1;
          return llvm_gen_be_error_t::ok;
      }
  }
  
  if (src_ty->isPointerTy()) {
     if (dest_ty->isPointerTy()) {
        *out_val = be_state->builder->CreateAddrSpaceCast(v1, dest_ty, "addrspace.cast");
        return llvm_gen_be_error_t::ok;
     }
     *out_val = be_state->builder->CreatePtrToInt(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  if (dest_ty->isPointerTy()) {
     *out_val = be_state->builder->CreateIntToPtr(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isFloatingPointTy()) {
     if (dest_ty->isFloatingPointTy()) {
        *out_val = be_state->builder->CreateFPCast(v1, dest_ty);
     } else {
        if (be_state->enable_ubsan_checks) {
           llvm::Value* is_nan = be_state->builder->CreateFCmpUNO(v1, v1, "ubsan.fpcast.nan");
           LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(is_nan, "__ubsan_handle_float_cast_overflow"));
        }
        if (!int_kind_is_signed[skip_typerefs(expr->type)->variant.integer.int_kind]) {
           *out_val = be_state->builder->CreateFPToUI(v1, dest_ty);
        } else {
           *out_val = be_state->builder->CreateFPToSI(v1, dest_ty);
        }
     }
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isIntegerTy()) {
     bool src_is_unsigned = !int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind];
     if (dest_ty->isFloatingPointTy()) {
        *out_val = src_is_unsigned ? be_state->builder->CreateUIToFP(v1, dest_ty)
                                   : be_state->builder->CreateSIToFP(v1, dest_ty);
     } else {
        *out_val = be_state->builder->CreateIntCast(v1, dest_ty, !src_is_unsigned);
     }
     return llvm_gen_be_error_t::ok;
  }
  
  *out_val = be_state->builder->CreateBitCast(v1, dest_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Decodes a C11/C++11 memory model integer constant into an `llvm::AtomicOrdering`.
 * @details Maps `0..5` to `Monotonic`, `Acquire`, `Release`, `AcquireRelease`, or `SequentiallyConsistent`,
 *          falling back to `SequentiallyConsistent` for release/acq_rel on load or failure orderings.
 * @param[in] order_val The lowered LLVM value for the memory order argument.
 * @param[in] is_load_or_fail True if decoding a load or cmpxchg failure ordering.
 * @param[out] out_order Receives the decoded `llvm::AtomicOrdering`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t decode_atomic_ordering(
    llvm::Value* order_val,
    bool is_load_or_fail,
    llvm::AtomicOrdering* out_order) noexcept {
  *out_order = llvm::AtomicOrdering::SequentiallyConsistent;
  if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(order_val)) {
    switch (ci->getZExtValue()) {
      case 0: *out_order = llvm::AtomicOrdering::Monotonic; break;
      case 1:
      case 2: *out_order = llvm::AtomicOrdering::Acquire; break;
      case 3:
        if (!is_load_or_fail) *out_order = llvm::AtomicOrdering::Release;
        break;
      case 4:
        if (!is_load_or_fail) *out_order = llvm::AtomicOrdering::AcquireRelease;
        break;
      default: break;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a runtime switch over a non-constant `memory_order` value for `__atomic_thread_fence` / `__atomic_signal_fence`.
 * @details Dispatches runtime `order_val` (`1`/`2` = `Acquire`, `3` = `Release`, `4` = `AcquireRelease`, default = `SequentiallyConsistent`)
 *          into dedicated basic blocks with the corresponding `llvm::FenceInst`.
 * @param[in] order_val Runtime integer `memory_order` value.
 * @param[in] scope Synchronization scope (`System` or `SingleThread`).
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t emit_dynamic_atomic_ordering_fence(
    llvm::Value* order_val,
    llvm::SyncScope::ID scope) noexcept {
  llvm::Function* fn = be_state->builder->GetInsertBlock()->getParent();
  llvm::BasicBlock* acq_bb = llvm::BasicBlock::Create(*be_state->context, "fence.acq", fn);
  llvm::BasicBlock* rel_bb = llvm::BasicBlock::Create(*be_state->context, "fence.rel", fn);
  llvm::BasicBlock* ar_bb = llvm::BasicBlock::Create(*be_state->context, "fence.acqrel", fn);
  llvm::BasicBlock* sc_bb = llvm::BasicBlock::Create(*be_state->context, "fence.seqcst", fn);
  llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(*be_state->context, "fence.done", fn);

  llvm::Type* ord_ty = order_val->getType();
  llvm::SwitchInst* sw = be_state->builder->CreateSwitch(order_val, sc_bb, 4);
  sw->addCase(llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(ord_ty, 1)), acq_bb);
  sw->addCase(llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(ord_ty, 2)), acq_bb);
  sw->addCase(llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(ord_ty, 3)), rel_bb);
  sw->addCase(llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(ord_ty, 4)), ar_bb);

  be_state->builder->SetInsertPoint(acq_bb);
  be_state->builder->CreateFence(llvm::AtomicOrdering::Acquire, scope);
  be_state->builder->CreateBr(done_bb);

  be_state->builder->SetInsertPoint(rel_bb);
  be_state->builder->CreateFence(llvm::AtomicOrdering::Release, scope);
  be_state->builder->CreateBr(done_bb);

  be_state->builder->SetInsertPoint(ar_bb);
  be_state->builder->CreateFence(llvm::AtomicOrdering::AcquireRelease, scope);
  be_state->builder->CreateBr(done_bb);

  be_state->builder->SetInsertPoint(sc_bb);
  be_state->builder->CreateFence(llvm::AtomicOrdering::SequentiallyConsistent, scope);
  be_state->builder->CreateBr(done_bb);

  be_state->builder->SetInsertPoint(done_bb);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a unary or binary overloaded math/bit-manipulation builtin into an LLVM intrinsic call.
 * @details Matches `rname` against known math and bit-manipulation builtin prefixes and emits the corresponding intrinsic call.
 * @param[in] rname Name of the builtin routine.
 * @param[in] args Lowered LLVM argument values.
 * @param[out] out_val Receives the resulting call instruction value.
 * @param[out] out_handled Receives `true` if the builtin matched and was lowered.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t llvm_lower_overloaded_intrinsic_builtin(
    llvm::StringRef rname,
    const std::vector<llvm::Value*>& args,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  *out_handled = false;
  static const std::pair<const char*, llvm::Intrinsic::ID> kUnaryEntries[] = {
    {"__builtin_sqrt", llvm::Intrinsic::sqrt},
    {"__builtin_sin", llvm::Intrinsic::sin},
    {"__builtin_cos", llvm::Intrinsic::cos},
    {"__builtin_exp", llvm::Intrinsic::exp},
    {"__builtin_log", llvm::Intrinsic::log},
    {"__builtin_round", llvm::Intrinsic::round},
    {"__builtin_trunc", llvm::Intrinsic::trunc},
    {"__builtin_floor", llvm::Intrinsic::floor},
    {"__builtin_ceil", llvm::Intrinsic::ceil},
    {"__builtin_fabs", llvm::Intrinsic::fabs},
    {"__builtin_bswap", llvm::Intrinsic::bswap},
    {"__builtin_constant_p", llvm::Intrinsic::is_constant},
  };
  for (const auto& entry : kUnaryEntries) {
    if (rname.starts_with(entry.first) && !rname.starts_with("__builtin_expect")) {
      llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), entry.second, {args[0]->getType()});
      *out_val = be_state->builder->CreateCall(intr, {args[0]});
      *out_handled = true;
      return llvm_gen_be_error_t::ok;
    }
  }
  static const std::pair<const char*, llvm::Intrinsic::ID> kBinaryEntries[] = {
    {"__builtin_pow", llvm::Intrinsic::pow},
    {"__builtin_copysign", llvm::Intrinsic::copysign},
  };
  for (const auto& entry : kBinaryEntries) {
    if (rname.starts_with(entry.first)) {
      llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), entry.second, {args[0]->getType()});
      *out_val = be_state->builder->CreateCall(intr, {args[0], args[1]});
      *out_handled = true;
      return llvm_gen_be_error_t::ok;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a direct, indirect, virtual, or builtin function call expression into LLVM IR.
 * @details Handles target ABI parameter/return lowering (`sret`, `byval`, register coercion),
 *          compiler builtins, C11/C++11 atomics, virtual dispatch, and `invoke` unwinding.
 * @param[in] expr Pointer to the EDG call expression node.
 * @param[out] out_val Receives the call return `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_call_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  
  llvm::FunctionType* callee_ty = nullptr;
  llvm::AttributeList callee_attrs;
  if (op1->type) {
     a_type_ptr func_ty_edg = skip_typerefs(op1->type);
     if (func_ty_edg->kind == tk_pointer) {
       func_ty_edg = skip_typerefs(func_ty_edg->variant.pointer.type);
     }
     if (func_ty_edg->kind == tk_routine) {
         llvm::Triple triple(be_state->module->getTargetTriple());
         llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
         if (triple.isAArch64()) {
           err = build_aapcs64_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.isOSWindows()) {
           err = build_win64_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.getArch() == llvm::Triple::x86) {
           err = build_x86_32_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.isARM() || triple.isThumb()) {
           err = build_arm32_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.isRISCV()) {
           err = build_riscv64_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.isLoongArch()) {
           err = build_loongarch64_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.isMIPS()) {
           err = build_mips_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.isPPC()) {
           err = build_ppc64_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.isSystemZ()) {
           err = build_s390x_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.isSPARC()) {
           err = build_sparc_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else if (triple.isWasm()) {
           err = build_wasm_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else {
           err = build_sysv_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         }
         if (err != llvm_gen_be_error_t::ok) return err;
         if (op1->kind == enk_routine) {
           llvm::Function* existing_fn = be_state->module->getFunction(
               op1->variant.routine.ptr->source_corresp.name);
           if (existing_fn && existing_fn->getFunctionType()->getNumParams() > callee_ty->getNumParams()) {
             callee_ty = existing_fn->getFunctionType();
             callee_attrs = existing_fn->getAttributes();
           }
         }
     }
  }

  std::vector<llvm::Value*> args;
  unsigned callee_arg_idx = 0;
  llvm::AllocaInst* sret_alloc = nullptr;
  llvm::Type* sret_ty = nullptr;
  
  if (callee_ty && callee_attrs.hasParamAttr(0, llvm::Attribute::StructRet)) {
      sret_ty = callee_attrs.getParamStructRetType(0);
      sret_alloc = be_state->builder->CreateAlloca(sret_ty, nullptr, "sret");
      args.push_back(sret_alloc);
      callee_arg_idx++;
  }

  llvm::Value* this_val = nullptr;

  for (an_expr_node_ptr arg = op1->next; arg != nullptr; arg = arg->next) {
    llvm::Value* arg_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(arg, &arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    if (arg == op1->next) this_val = arg_val; // Save 'this' pointer for virtual dispatch
    
    if (callee_ty && callee_arg_idx < callee_ty->getNumParams()) {
        llvm::Type* expected_ty = callee_ty->getParamType(callee_arg_idx);
        if (callee_attrs.hasParamAttr(callee_arg_idx, llvm::Attribute::ByVal)) {
            llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(arg_val->getType());
            be_state->builder->CreateStore(arg_val, alloc);
            args.push_back(alloc);
            callee_arg_idx++;
            continue;
        } else if (arg_val->getType() != expected_ty) {
            llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(arg_val->getType());
            be_state->builder->CreateStore(arg_val, alloc);
            llvm::Value* first_piece = be_state->builder->CreateLoad(expected_ty, alloc);
            args.push_back(first_piece);
            callee_arg_idx++;

            const llvm::DataLayout& dl = be_state->module->getDataLayout();
            uint64_t packed_bytes = dl.getTypeAllocSize(expected_ty);
            uint64_t total_arg_bytes = dl.getTypeAllocSize(arg_val->getType());
            while (packed_bytes < total_arg_bytes) {
                llvm::Type* piece_ty = callee_ty->getParamType(callee_arg_idx++);
                llvm::Value* piece_ptr = be_state->builder->CreateInBoundsGEP(
                    llvm::Type::getInt8Ty(*be_state->context), alloc,
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), packed_bytes), "call.arg.piece");
                args.push_back(be_state->builder->CreateLoad(piece_ty, piece_ptr));
                packed_bytes += dl.getTypeAllocSize(piece_ty);
            }
            continue;
        }
    }
    
    args.push_back(arg_val);
    callee_arg_idx++;
  }

  llvm::Value* v1 = nullptr;
  if (expr->variant.operation.is_virtual_call) {
      a_routine_ptr r = op1->variant.routine.ptr;
      llvm::Triple triple(be_state->module->getTargetTriple());
      llvm_gen_be_error_t v_err;
      if (triple.isWindowsMSVCEnvironment()) {
          v_err = llvm_lower_msvc_virtual_dispatch(this_val, r, &v1);
      } else {
          v_err = llvm_lower_virtual_dispatch(this_val, r, &v1);
      }
      if (v_err != llvm_gen_be_error_t::ok) return v_err;
  } else {
      if (op1->kind == enk_routine && callee_ty &&
          !be_state->module->getFunction(op1->variant.routine.ptr->source_corresp.name)) {
          llvm::Function::Create(
              callee_ty, llvm::GlobalValue::ExternalLinkage,
              op1->variant.routine.ptr->source_corresp.name, be_state->module.get());
      }
      llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
      if (err != llvm_gen_be_error_t::ok) return err;
      
      // Check for builtins in call expressions
      if (op1->kind == enk_routine) {
             a_routine_ptr r = op1->variant.routine.ptr;
             std::string rname = r->source_corresp.name;
             bool ov_handled = false;
             LLVM_GEN_BE_ASSERT_OK(llvm_lower_overloaded_intrinsic_builtin(rname, args, out_val, &ov_handled));
             if (ov_handled) return llvm_gen_be_error_t::ok;
             if (rname == "__builtin_expect") {
                 llvm::Function* exp = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::expect, {llvm::Type::getInt64Ty(*be_state->context)});
                 *out_val = be_state->builder->CreateCall(exp, {args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_expect_with_probability") {
                 llvm::Function* exp_prob = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::expect_with_probability, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(exp_prob, {args[0], args[1], args[2]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_frame_address") {
                 llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::frameaddress, {llvm::PointerType::getUnqual(*be_state->context)});
                 *out_val = be_state->builder->CreateCall(intr, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_return_address") {
                 llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::returnaddress, {llvm::PointerType::getUnqual(*be_state->context)});
                 *out_val = be_state->builder->CreateCall(intr, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_alloca") {
                 *out_val = be_state->builder->CreateAlloca(llvm::Type::getInt8Ty(*be_state->context), args[0]);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_alloca_with_align") {
                 llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(llvm::Type::getInt8Ty(*be_state->context), args[0]);
                 if (auto* align_ci = llvm::dyn_cast<llvm::ConstantInt>(args[1])) {
                     alloc->setAlignment(llvm::Align(align_ci->getZExtValue() / 8));
                 }
                 *out_val = alloc;
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_shufflevector") {
                 std::vector<int> mask;
                 for (size_t i = 2; i < args.size(); ++i) {
                     if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args[i])) {
                         mask.push_back(ci->getSExtValue());
                     } else {
                         mask.push_back(-1);
                     }
                 }
                 *out_val = be_state->builder->CreateShuffleVector(args[0], args[1], mask);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_convertvector") {
                 llvm::Type* dst_ty = nullptr;
                 llvm_gen_be_error_t err_ty = get_llvm_type(expr->type, &dst_ty);
                 if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                 
                 llvm::Type* src_ty = args[0]->getType();
                 if (src_ty->isIntOrIntVectorTy() && dst_ty->isIntOrIntVectorTy()) {
                     *out_val = be_state->builder->CreateIntCast(args[0], dst_ty, true);
                 } else if (src_ty->isFPOrFPVectorTy() && dst_ty->isFPOrFPVectorTy()) {
                     *out_val = be_state->builder->CreateFPCast(args[0], dst_ty);
                 } else if (src_ty->isIntOrIntVectorTy()) {
                     *out_val = be_state->builder->CreateSIToFP(args[0], dst_ty);
                 } else {
                     *out_val = be_state->builder->CreateFPToSI(args[0], dst_ty);
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_assume") {
                 llvm::Function* assume = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::assume);
                 *out_val = be_state->builder->CreateCall(assume, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_unreachable") {
                 if (be_state->enable_ubsan_checks) {
                     LLVM_GEN_BE_ASSERT_OK(emit_ubsan_check_branch(
                         be_state->builder->getTrue(), "__ubsan_handle_builtin_unreachable"));
                 }
                 *out_val = be_state->builder->CreateUnreachable();
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_trap") {
                 llvm::Function* trap = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::trap);
                 *out_val = be_state->builder->CreateCall(trap);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).ends_with("setjmp")) {
                 llvm::Function* setjmp_func = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::eh_sjlj_setjmp);
                 *out_val = be_state->builder->CreateCall(setjmp_func, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).ends_with("longjmp")) {
                 llvm::Function* longjmp_func = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::eh_sjlj_longjmp);
                 *out_val = be_state->builder->CreateCall(longjmp_func, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_cabs")) {
                 llvm::Value* v_cplx = args[0];
                 if (v_cplx->getType()->isStructTy()) {
                     llvm::Value* r = be_state->builder->CreateExtractValue(v_cplx, 0);
                     llvm::Value* i = be_state->builder->CreateExtractValue(v_cplx, 1);
                     llvm::Value* r2 = be_state->builder->CreateFMul(r, r);
                     llvm::Value* i2 = be_state->builder->CreateFMul(i, i);
                     llvm::Value* sum = be_state->builder->CreateFAdd(r2, i2);
                     llvm::Function* sqrt_func = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::sqrt, {r->getType()});
                     *out_val = be_state->builder->CreateCall(sqrt_func, {sum});
                 } else {
                     llvm::Function* fabs_func = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::fabs, {v_cplx->getType()});
                     *out_val = be_state->builder->CreateCall(fabs_func, {v_cplx});
                 }
                 return llvm_gen_be_error_t::ok;
             }
             bool target_handled = false;
#if BUILTIN_FUNCTIONS_ENABLED
             a_builtin_function_kind bfk = static_cast<a_builtin_function_kind>(r->variant.builtin_function_kind);
#else
             a_builtin_function_kind bfk = 0;
#endif
             LLVM_GEN_BE_ASSERT_OK(llvm_lower_target_builtin(
                 expr, bfk, rname, args, callee_ty, out_val, &target_handled));
             if (target_handled) {
                 return llvm_gen_be_error_t::ok;
             }
             if (llvm::StringRef(rname).starts_with("__builtin_") &&
                        llvm::StringRef(rname).ends_with("_overflow")) {
                 llvm::Type* op_ty = args[0]->getType();
                 a_type_ptr first_edg_ty = skip_typerefs(op1->next->type);
                 bool generic_signed = int_kind_is_signed[first_edg_ty->variant.integer.int_kind];
                 bool is_signed = (rname.find("sadd") != std::string::npos) || 
                                  (rname.find("ssub") != std::string::npos) || 
                                  (rname.find("smul") != std::string::npos) || 
                                  ((rname == "__builtin_add_overflow" || rname == "__builtin_sub_overflow" || rname == "__builtin_mul_overflow") && generic_signed);
                                  
                 llvm::Intrinsic::ID iid;
                 if (rname.find("add") != std::string::npos) iid = is_signed ? llvm::Intrinsic::sadd_with_overflow : llvm::Intrinsic::uadd_with_overflow;
                 else if (rname.find("sub") != std::string::npos) iid = is_signed ? llvm::Intrinsic::ssub_with_overflow : llvm::Intrinsic::usub_with_overflow;
                 else iid = is_signed ? llvm::Intrinsic::smul_with_overflow : llvm::Intrinsic::umul_with_overflow;

                 llvm::Function* ofunc = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), iid, {op_ty});
                 llvm::Value* res = be_state->builder->CreateCall(ofunc, {args[0], args[1]});
                 
                 // Store result
                 llvm::Value* val = be_state->builder->CreateExtractValue(res, 0);
                 be_state->builder->CreateStore(val, args[2]);
                 
                 // Return overflow flag
                 *out_val = be_state->builder->CreateExtractValue(res, 1);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_") &&
                        llvm::StringRef(rname).ends_with("_overflow_p")) {
                 llvm::Type* op_ty = args[0]->getType();
                 llvm::Intrinsic::ID iid = (rname.find("add") != std::string::npos)
                                               ? llvm::Intrinsic::sadd_with_overflow
                                               : ((rname.find("sub") != std::string::npos)
                                                      ? llvm::Intrinsic::ssub_with_overflow
                                                      : llvm::Intrinsic::smul_with_overflow);
                 llvm::Function* ofunc = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), iid, {op_ty});
                 llvm::Value* res = be_state->builder->CreateCall(ofunc, {args[0], args[1]});
                 *out_val = be_state->builder->CreateExtractValue(res, 1);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_popcount") ||
                        llvm::StringRef(rname).starts_with("__builtin_stdc_count_ones")) {
                 llvm::Function* pop = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctpop, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(pop, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_has_single_bit")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Function* pop = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctpop, {arg_ty});
                 llvm::Value* cnt = be_state->builder->CreateCall(pop, {args[0]});
                 *out_val = be_state->builder->CreateICmpEQ(cnt, llvm::ConstantInt::get(arg_ty, 1));
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_bit_floor")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 unsigned bw = arg_ty->getIntegerBitWidth();
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {args[0], be_state->builder->getFalse()});
                 llvm::Value* shift = be_state->builder->CreateSub(llvm::ConstantInt::get(arg_ty, bw - 1), lz);
                 llvm::Value* pow2 = be_state->builder->CreateShl(llvm::ConstantInt::get(arg_ty, 1), shift);
                 llvm::Value* is_zero = be_state->builder->CreateICmpEQ(args[0], llvm::ConstantInt::get(arg_ty, 0));
                 *out_val = be_state->builder->CreateSelect(is_zero, llvm::ConstantInt::get(arg_ty, 0), pow2);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_bit_ceil")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 unsigned bw = arg_ty->getIntegerBitWidth();
                 llvm::Value* sub1 = be_state->builder->CreateSub(args[0], llvm::ConstantInt::get(arg_ty, 1));
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {sub1, be_state->builder->getFalse()});
                 llvm::Value* shift = be_state->builder->CreateSub(llvm::ConstantInt::get(arg_ty, bw), lz);
                 llvm::Value* pow2 = be_state->builder->CreateShl(llvm::ConstantInt::get(arg_ty, 1), shift);
                 llvm::Value* le1 = be_state->builder->CreateICmpULE(args[0], llvm::ConstantInt::get(arg_ty, 1));
                 *out_val = be_state->builder->CreateSelect(le1, llvm::ConstantInt::get(arg_ty, 1), pow2);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_count_zeros")) {
                 llvm::Value* inv = be_state->builder->CreateNot(args[0]);
                 llvm::Function* pop = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctpop, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(pop, {inv});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_bit_width")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {args[0], be_state->builder->getFalse()});
                 *out_val = be_state->builder->CreateSub(
                     llvm::ConstantInt::get(arg_ty, arg_ty->getIntegerBitWidth()), lz);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_first_leading_one") ||
                        llvm::StringRef(rname).starts_with("__builtin_stdc_first_leading_zero")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Value* in_v = llvm::StringRef(rname).ends_with("zero")
                                         ? be_state->builder->CreateNot(args[0])
                                         : args[0];
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {in_v, be_state->builder->getFalse()});
                 llvm::Value* is_zero = be_state->builder->CreateICmpEQ(in_v, llvm::ConstantInt::get(arg_ty, 0));
                 llvm::Value* idx1 = be_state->builder->CreateAdd(lz, llvm::ConstantInt::get(arg_ty, 1));
                 *out_val = be_state->builder->CreateSelect(is_zero, llvm::ConstantInt::get(arg_ty, 0), idx1);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_first_trailing_one") ||
                        llvm::StringRef(rname).starts_with("__builtin_stdc_first_trailing_zero")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Value* in_v = llvm::StringRef(rname).ends_with("zero")
                                         ? be_state->builder->CreateNot(args[0])
                                         : args[0];
                 llvm::Function* ctz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::cttz, {arg_ty});
                 llvm::Value* tz = be_state->builder->CreateCall(ctz, {in_v, be_state->builder->getFalse()});
                 llvm::Value* is_zero = be_state->builder->CreateICmpEQ(in_v, llvm::ConstantInt::get(arg_ty, 0));
                 llvm::Value* idx1 = be_state->builder->CreateAdd(tz, llvm::ConstantInt::get(arg_ty, 1));
                 *out_val = be_state->builder->CreateSelect(is_zero, llvm::ConstantInt::get(arg_ty, 0), idx1);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_clear_padding") {
                 llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
                 a_type_ptr ptee_ty = op1->next->type->variant.pointer.type;
                 std::vector<uint8_t> pad_mask;
                 LLVM_GEN_BE_ASSERT_OK(llvm_compute_type_padding_mask(ptee_ty, &pad_mask));
                 for (size_t b = 0; b < pad_mask.size(); ++b) {
                     uint8_t m = pad_mask[b];
                     if (m == 0xFFu) continue;
                     llvm::Value* byte_ptr = be_state->builder->CreateInBoundsGEP(
                         i8_ty, args[0], llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), b));
                     if (m == 0x00u) {
                         be_state->builder->CreateStore(llvm::ConstantInt::get(i8_ty, 0), byte_ptr);
                     } else {
                         llvm::Value* old_b = be_state->builder->CreateLoad(i8_ty, byte_ptr);
                         llvm::Value* masked_b = be_state->builder->CreateAnd(old_b, llvm::ConstantInt::get(i8_ty, m));
                         be_state->builder->CreateStore(masked_b, byte_ptr);
                     }
                 }
                 be_state->builder->CreateMemSet(
                     args[0], llvm::ConstantInt::get(i8_ty, 0),
                     llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 0), llvm::MaybeAlign());
                 *out_val = args[0];
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_FILE") {
                 const std::string& mod_fn = be_state->module->getSourceFileName();
                 *out_val = be_state->builder->CreateGlobalString(
                     !mod_fn.empty() ? mod_fn.c_str() : "<file>", ".file");
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_FUNCTION") {
                 llvm::Function* cur_fn = be_state->builder->GetInsertBlock()->getParent();
                 *out_val = be_state->builder->CreateGlobalString(cur_fn->getName(), ".func");
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_LINE") {
                 *out_val = llvm::ConstantInt::get(
                     llvm::Type::getInt32Ty(*be_state->context), expr->position.seq ? expr->position.seq : 1);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_COLUMN") {
                 *out_val = llvm::ConstantInt::get(
                     llvm::Type::getInt32Ty(*be_state->context), expr->position.column ? expr->position.column : 1);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_clz")) {
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(clz, {args[0], be_state->builder->getTrue()});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_ctz")) {
                 llvm::Function* ctz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::cttz, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(ctz, {args[0], be_state->builder->getTrue()});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_ffs")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Function* ctz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::cttz, {arg_ty});
                 llvm::Value* tz = be_state->builder->CreateCall(ctz, {args[0], be_state->builder->getFalse()});
                 llvm::Value* tz_plus_1 = be_state->builder->CreateAdd(tz, llvm::ConstantInt::get(arg_ty, 1));
                 llvm::Value* is_zero = be_state->builder->CreateICmpEQ(args[0], llvm::ConstantInt::get(arg_ty, 0));
                 *out_val = be_state->builder->CreateSelect(is_zero, llvm::ConstantInt::get(arg_ty, 0), tz_plus_1);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_clrsb")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 unsigned bw = arg_ty->getIntegerBitWidth();
                 llvm::Value* sign_mask = be_state->builder->CreateAShr(args[0], llvm::ConstantInt::get(arg_ty, bw - 1));
                 llvm::Value* xored = be_state->builder->CreateXor(args[0], sign_mask);
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {xored, be_state->builder->getFalse()});
                 *out_val = be_state->builder->CreateSub(lz, llvm::ConstantInt::get(arg_ty, 1));
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_launder") {
                 llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                 llvm::FunctionCallee launder = be_state->module->getOrInsertFunction(
                     "llvm.launder.invariant.group.p0", llvm::FunctionType::get(ptr_ty, {ptr_ty}, false));
                 *out_val = be_state->builder->CreateCall(launder, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_dump_struct") {
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                 llvm::FunctionType* printf_fty = llvm::FunctionType::get(i32_ty, {ptr_ty}, true);
                 llvm::Constant* fmt_str = be_state->builder->CreateGlobalString("struct {}\n", ".dump_fmt");
                 *out_val = be_state->builder->CreateCall(printf_fty, args[1], {fmt_str});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_assume_aligned") {
                 llvm::Function* assume_fn = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::assume);
                 llvm::Value* cond = be_state->builder->getTrue();
                 std::vector<llvm::Value*> bundle_args = {args[0], args[1]};
                 if (args.size() > 2) {
                     bundle_args.push_back(args[2]);
                 }
                 llvm::OperandBundleDef align_bundle("align", bundle_args);
                 be_state->builder->CreateCall(assume_fn, {cond}, {align_bundle});
                 *out_val = args[0];
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_choose_expr") {
                 bool cond_true = false;
                 if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args[0])) {
                     cond_true = !ci->isZero();
                 }
                 *out_val = cond_true ? args[1] : args[2];
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_va_arg_pack" || rname == "__builtin_va_arg_pack_len" ||
                        rname == "__builtin_is_constant_evaluated") {
                 llvm::Type* ret_ty = llvm::Type::getInt32Ty(*be_state->context);
                 if (expr->type && skip_typerefs(expr->type)->variant.integer.bool_type) {
                     ret_ty = llvm::Type::getInt1Ty(*be_state->context);
                 }
                 *out_val = llvm::ConstantInt::get(ret_ty, 0);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_nondeterministic_value") {
                 *out_val = be_state->builder->CreateFreeze(llvm::PoisonValue::get(args[0]->getType()));
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_addressof") {
                 return llvm_lower_lvalue_expression(op1->next, out_val);
             } else if (rname == "__builtin_complex") {
                 llvm::Type* cplx_ty = llvm::StructType::get(*be_state->context, {args[0]->getType(), args[1]->getType()});
                 llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(cplx_ty), args[0], 0);
                 *out_val = be_state->builder->CreateInsertValue(res, args[1], 1);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_source_location") {
                 llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::StructType* sl_ty = llvm::StructType::get(*be_state->context, {ptr_ty, ptr_ty, i32_ty, i32_ty});
                 const std::string& mod_fn = be_state->module->getSourceFileName();
                 llvm::Constant* fn_str = be_state->builder->CreateGlobalString(
                     !mod_fn.empty() ? mod_fn.c_str() : "<file>", ".file");
                 llvm::Function* cur_fn = be_state->builder->GetInsertBlock()->getParent();
                 llvm::Constant* func_str = be_state->builder->CreateGlobalString(cur_fn->getName(), ".func");
                 llvm::Constant* sl_init = llvm::ConstantStruct::get(
                     sl_ty,
                     {fn_str, func_str,
                      llvm::ConstantInt::get(i32_ty, expr->position.seq ? expr->position.seq : 1),
                      llvm::ConstantInt::get(i32_ty, expr->position.column ? expr->position.column : 1)});
                 llvm::GlobalVariable* sl_gv = new llvm::GlobalVariable(
                     *be_state->module, sl_ty, true, llvm::GlobalValue::PrivateLinkage, sl_init, ".src_loc");
                 *out_val = sl_gv;
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_classify_type") {
                 an_expr_node_ptr arg_node = op1->next;
                 a_type_ptr target_ty = (arg_node && arg_node->kind == enk_type_operand)
                                            ? arg_node->variant.type_operand.type
                                            : (arg_node ? arg_node->type : nullptr);
                 int32_t cls_code = -1;
                 if (target_ty) {
                     a_type_ptr ut = skip_typerefs(target_ty);
                     llvm::Type* lt = nullptr;
                     llvm_gen_be_error_t l_err = get_llvm_type(ut, &lt);
                     if (l_err == llvm_gen_be_error_t::ok && lt->isVectorTy()) cls_code = 19;
                     else if (ut->kind == tk_void) cls_code = 0;
                     else if (ut->kind == tk_integer) {
                         if (ut->variant.integer.bool_type) cls_code = 4;
                         else if (ut->variant.integer.enum_type) cls_code = 3;
                         else if (ut->variant.integer.int_kind == ik_char ||
                                  ut->variant.integer.int_kind == ik_signed_char ||
                                  ut->variant.integer.int_kind == ik_unsigned_char) cls_code = 2;
                         else cls_code = 1;
                     } else if (ut->kind == tk_pointer) cls_code = ut->variant.pointer.is_reference ? 6 : 5;
                     else if (ut->kind == tk_ptr_to_member) {
                         a_type_ptr mty = ut->variant.ptr_to_member.type ? skip_typerefs(ut->variant.ptr_to_member.type) : nullptr;
                         cls_code = (mty && mty->kind == tk_routine) ? 11 : 7;
                     } else if (ut->kind == tk_float || ut->kind == tk_imaginary) cls_code = 8;
                     else if (ut->kind == tk_complex) cls_code = 9;
                     else if (ut->kind == tk_routine) cls_code = 10;
                     else if (ut->kind == tk_struct || ut->kind == tk_class) cls_code = 12;
                     else if (ut->kind == tk_union) cls_code = 13;
                     else if (ut->kind == tk_array) cls_code = 14;
                 }
                 *out_val = llvm::ConstantInt::getSigned(llvm::Type::getInt32Ty(*be_state->context), cls_code);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__uuidof") {
                 an_expr_node_ptr arg_node = op1->next;
                 a_type_ptr target_ty = (arg_node && arg_node->kind == enk_type_operand)
                                            ? arg_node->variant.type_operand.type
                                            : (arg_node ? arg_node->type : nullptr);
                 a_constant uuid_con = {};
                 uuid_con.kind = ck_address;
                 uuid_con.type = expr->type;
                 uuid_con.variant.address.kind = abk_uuidof;
                 uuid_con.variant.address.variant.type = target_ty;
                 llvm::Constant* guid_gv = nullptr;
                 LLVM_GEN_BE_ASSERT_OK(evaluate_constant(
                     &uuid_con, llvm::PointerType::getUnqual(*be_state->context), &guid_gv));
                 *out_val = guid_gv;
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_prefetch") {
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                 llvm::Value* rw = (args.size() > 1) ? be_state->builder->CreateIntCast(args[1], i32_ty, false) : llvm::ConstantInt::get(i32_ty, 0);
                 llvm::Value* loc = (args.size() > 2) ? be_state->builder->CreateIntCast(args[2], i32_ty, false) : llvm::ConstantInt::get(i32_ty, 3);
                 llvm::FunctionCallee pref = be_state->module->getOrInsertFunction(
                     "llvm.prefetch.p0",
                     llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {ptr_ty, i32_ty, i32_ty, i32_ty}, false));
                 *out_val = be_state->builder->CreateCall(pref, {args[0], rw, loc, llvm::ConstantInt::get(i32_ty, 1)});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_clear_cache") {
                 llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                 llvm::FunctionCallee cc = be_state->module->getOrInsertFunction(
                     "llvm.clear_cache",
                     llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {ptr_ty, ptr_ty}, false));
                 *out_val = be_state->builder->CreateCall(cc, {args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_readcyclecounter") {
                 llvm::Function* rcc = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::readcyclecounter);
                 *out_val = be_state->builder->CreateCall(rcc);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_isnan") {
                 llvm::Value* is_unord = be_state->builder->CreateFCmpUNO(args[0], args[0]);
                 *out_val = be_state->builder->CreateZExt(is_unord, llvm::Type::getInt32Ty(*be_state->context));
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_isinf" || rname == "__builtin_isfinite" || rname == "__builtin_isnormal") {
                 unsigned mask = (rname == "__builtin_isinf") ? 516u : ((rname == "__builtin_isfinite") ? 504u : 264u);
                 llvm::Function* fpcls = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::is_fpclass, {args[0]->getType()});
                 llvm::Value* res = be_state->builder->CreateCall(fpcls, {args[0], llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), mask)});
                 *out_val = be_state->builder->CreateZExt(res, llvm::Type::getInt32Ty(*be_state->context));
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_signbit")) {
                 unsigned bw = args[0]->getType()->getPrimitiveSizeInBits();
                 llvm::Type* int_ty = llvm::Type::getIntNTy(*be_state->context, bw);
                 llvm::Value* bits = be_state->builder->CreateBitCast(args[0], int_ty);
                 llvm::Value* is_neg = be_state->builder->CreateICmpSLT(bits, llvm::ConstantInt::get(int_ty, 0));
                 *out_val = be_state->builder->CreateZExt(is_neg, llvm::Type::getInt32Ty(*be_state->context));
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_fpclassify") {
                 // __builtin_fpclassify(fp_nan, fp_infinite, fp_normal, fp_subnormal, fp_zero, x)
                 llvm::Value* x = args[5];
                 llvm::Value* zero = llvm::ConstantFP::get(x->getType(), 0.0);
                 llvm::Value* is_nan = be_state->builder->CreateFCmpUNO(x, x);
                 llvm::Value* is_zero = be_state->builder->CreateFCmpOEQ(x, zero);
                 llvm::Function* fpcls = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::is_fpclass, {x->getType()});
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::Value* is_inf = be_state->builder->CreateCall(fpcls, {x, llvm::ConstantInt::get(i32_ty, 516u)});
                 llvm::Value* is_norm = be_state->builder->CreateCall(fpcls, {x, llvm::ConstantInt::get(i32_ty, 264u)});
                 llvm::Value* sel = be_state->builder->CreateSelect(is_norm, args[2], args[3]);
                 sel = be_state->builder->CreateSelect(is_zero, args[4], sel);
                 sel = be_state->builder->CreateSelect(is_inf, args[1], sel);
                 *out_val = be_state->builder->CreateSelect(is_nan, args[0], sel);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_parity")) {
                 llvm::Function* popcnt = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctpop, {args[0]->getType()});
                 llvm::Value* pop = be_state->builder->CreateCall(popcnt, {args[0]});
                 *out_val = be_state->builder->CreateAnd(pop, llvm::ConstantInt::get(args[0]->getType(), 1));
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("__builtin_rotateleft") == 0 ||
                        rname.find("__builtin_stdc_rotate_left") == 0) {
                 llvm::Function* rotl = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::fshl, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(rotl, {args[0], args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("__builtin_rotateright") == 0 ||
                        rname.find("__builtin_stdc_rotate_right") == 0) {
                 llvm::Function* rotr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::fshr, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(rotr, {args[0], args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_object_size" || rname == "__builtin_dynamic_object_size") {
                 llvm::Function* objsize = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::objectsize, {llvm::Type::getInt64Ty(*be_state->context), args[0]->getType()});
                 // LLVM objectsize takes 4 args: ptr, min, nullunknown, dynamic
                 llvm::Value* min_val = be_state->builder->CreateAnd(args[1], llvm::ConstantInt::get(args[1]->getType(), 2));
                 llvm::Value* is_min = be_state->builder->CreateICmpNE(min_val, llvm::ConstantInt::get(args[1]->getType(), 0));
                 llvm::Value* dynamic = (rname == "__builtin_dynamic_object_size") ? be_state->builder->getTrue() : be_state->builder->getFalse();
                 *out_val = be_state->builder->CreateCall(objsize, {args[0], is_min, be_state->builder->getFalse(), dynamic});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_fma")) {
                 llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::fma, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(intr, {args[0], args[1], args[2]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin___emutls_get_address") {
                 llvm::Function* emutls_fn = be_state->module->getFunction("__emutls_get_address");
                 if (!emutls_fn) {
                     llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                     llvm::FunctionType* fn_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty}, false);
                     emutls_fn = llvm::Function::Create(fn_ty, llvm::GlobalValue::ExternalLinkage, "__emutls_get_address", be_state->module.get());
                 }
                 *out_val = be_state->builder->CreateCall(emutls_fn, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_memcpy" || rname == "__builtin___memcpy_chk") {
                 be_state->builder->CreateMemCpy(args[0], llvm::MaybeAlign(), args[1], llvm::MaybeAlign(), args[2]);
                 *out_val = args[0]; // Returns dest
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_memmove" || rname == "__builtin___memmove_chk") {
                 be_state->builder->CreateMemMove(args[0], llvm::MaybeAlign(), args[1], llvm::MaybeAlign(), args[2]);
                 *out_val = args[0]; // Returns dest
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_strlen") {
                 llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt64Ty(*be_state->context), {args[0]->getType()}, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strlen", ft);
                 *out_val = be_state->builder->CreateCall(fn, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_strcmp") {
                 llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*be_state->context), {args[0]->getType(), args[1]->getType()}, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strcmp", ft);
                 *out_val = be_state->builder->CreateCall(fn, {args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_strchr") {
                 llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::getUnqual(*be_state->context), {args[0]->getType(), llvm::Type::getInt32Ty(*be_state->context)}, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strchr", ft);
                 llvm::Value* arg1 = args[1];
                 if (arg1->getType()->getIntegerBitWidth() < 32) arg1 = be_state->builder->CreateSExt(arg1, llvm::Type::getInt32Ty(*be_state->context));
                 else if (arg1->getType()->getIntegerBitWidth() > 32) arg1 = be_state->builder->CreateTrunc(arg1, llvm::Type::getInt32Ty(*be_state->context));
                 *out_val = be_state->builder->CreateCall(fn, {args[0], arg1});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_memset" || rname == "__builtin___memset_chk") {
                 llvm::Value* val_i8 = be_state->builder->CreateTrunc(args[1], llvm::Type::getInt8Ty(*be_state->context));
                 be_state->builder->CreateMemSet(args[0], val_i8, args[2], llvm::MaybeAlign());
                 *out_val = args[0]; // Returns dest
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__va_start") {
                 return lower_win64_va_start(args[0], out_val);
             } else if (llvm::StringRef(rname).starts_with("__builtin___") &&
                        llvm::StringRef(rname).ends_with("_chk")) {
                 std::string chk_name = rname.substr(10);
                 llvm::Type* ret_ty = nullptr;
                 LLVM_GEN_BE_ASSERT_OK(get_llvm_type(expr->type, &ret_ty));
                 std::vector<llvm::Type*> param_tys;
                 for (llvm::Value* a : args) param_tys.push_back(a->getType());
                 llvm::FunctionType* chk_fty = llvm::FunctionType::get(ret_ty, param_tys, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(chk_name, chk_fty);
                 *out_val = be_state->builder->CreateCall(fn, args);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__atomic_always_lock_free" || rname == "__atomic_is_lock_free") {
                 llvm::Value* size_val = args[0];
                 if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(size_val)) {
                     uint64_t size = ci->getZExtValue();
                     bool lock_free = (size > 0 && size <= 8 && llvm::isPowerOf2_64(size));
                     if (size == 16) {
                         llvm::Triple triple(be_state->module->getTargetTriple());
                         if (triple.getArch() == llvm::Triple::x86_64 || triple.getArch() == llvm::Triple::aarch64) lock_free = true;
                     }
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*be_state->context), lock_free ? 1 : 0);
                 } else {
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*be_state->context), 0); // conservative fallback
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__atomic_thread_fence" || rname == "__atomic_signal_fence") {
                 llvm::SyncScope::ID scope = (rname == "__atomic_signal_fence") ? llvm::SyncScope::SingleThread : llvm::SyncScope::System;
                 if (llvm::isa<llvm::ConstantInt>(args[0])) {
                     llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                     LLVM_GEN_BE_ASSERT_OK(decode_atomic_ordering(args[0], false, &order));
                     be_state->builder->CreateFence(order, scope);
                 } else {
                     LLVM_GEN_BE_ASSERT_OK(emit_dynamic_atomic_ordering_fence(args[0], scope));
                 }
                 *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0); // void/ignored
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_atomic_wait" || rname == "__builtin_atomic_notify") {
                 std::string fn_name = (rname == "__builtin_atomic_wait") ? "__atomic_wait" : "__atomic_notify";
                 llvm::Function* wait_notify_fn = be_state->module->getFunction(fn_name);
                 if (!wait_notify_fn) {
                     std::vector<llvm::Type*> arg_types;
                     for (auto* arg_val : args) arg_types.push_back(arg_val->getType());
                     llvm::FunctionType* fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), arg_types, false);
                     wait_notify_fn = llvm::Function::Create(fn_ty, llvm::GlobalValue::ExternalLinkage, fn_name, be_state->module.get());
                 }
                 *out_val = be_state->builder->CreateCall(wait_notify_fn, args);
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("atomic_fetch_") != std::string::npos || rname.find("__sync_") == 0) {
                 if (rname == "__sync_synchronize") {
                     be_state->builder->CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
                     return llvm_gen_be_error_t::ok;
                 }
                 if (rname == "__sync_lock_test_and_set") {
                     llvm::AtomicRMWInst* rmw = be_state->builder->CreateAtomicRMW(llvm::AtomicRMWInst::Xchg, args[0], args[1], llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent);
                     *out_val = rmw;
                     return llvm_gen_be_error_t::ok;
                 }
                 if (rname == "__sync_lock_release") {
                     llvm::Type* val_ty = llvm::Type::getInt32Ty(*be_state->context);
                     llvm::StoreInst* store = be_state->builder->CreateStore(llvm::ConstantInt::get(val_ty, 0), args[0]);
                     store->setAtomic(llvm::AtomicOrdering::Release);
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
                     return llvm_gen_be_error_t::ok;
                 }
                 if (rname == "__sync_val_compare_and_swap" || rname == "__sync_bool_compare_and_swap") {
                     llvm::AtomicCmpXchgInst* cmpxchg = be_state->builder->CreateAtomicCmpXchg(args[0], args[1], args[2], llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent, llvm::AtomicOrdering::SequentiallyConsistent);
                     if (rname == "__sync_val_compare_and_swap") {
                         *out_val = be_state->builder->CreateExtractValue(cmpxchg, 0);
                     } else {
                         *out_val = be_state->builder->CreateExtractValue(cmpxchg, 1);
                     }
                     return llvm_gen_be_error_t::ok;
                 }
                 bool is_fp = args[1]->getType()->isFloatingPointTy();
                 llvm::AtomicRMWInst::BinOp op = is_fp ? llvm::AtomicRMWInst::FAdd : llvm::AtomicRMWInst::Add;
                 if (rname.find("sub") != std::string::npos) op = is_fp ? llvm::AtomicRMWInst::FSub : llvm::AtomicRMWInst::Sub;
                 else if (rname.find("nand") != std::string::npos) op = llvm::AtomicRMWInst::Nand;
                 else if (rname.find("xor") != std::string::npos) op = llvm::AtomicRMWInst::Xor;
                 else if (rname.find("or") != std::string::npos) op = llvm::AtomicRMWInst::Or;
                 else if (rname.find("min") != std::string::npos) op = is_fp ? llvm::AtomicRMWInst::FMin : llvm::AtomicRMWInst::Min;
                 else if (rname.find("max") != std::string::npos) op = is_fp ? llvm::AtomicRMWInst::FMax : llvm::AtomicRMWInst::Max;
                 else if (rname.find("add") == std::string::npos) op = llvm::AtomicRMWInst::And;
                 
                 llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                 if (rname.find("__sync_") != 0) {
                     LLVM_GEN_BE_ASSERT_OK(decode_atomic_ordering(args[2], false, &order));
                 }
                 
                 llvm::AtomicRMWInst* rmw = be_state->builder->CreateAtomicRMW(op, args[0], args[1], llvm::MaybeAlign(), order);

                 if (llvm::StringRef(rname).ends_with("_and_fetch")) {
                     if (op == llvm::AtomicRMWInst::Add) *out_val = be_state->builder->CreateAdd(rmw, args[1]);
                     else if (op == llvm::AtomicRMWInst::FAdd) *out_val = be_state->builder->CreateFAdd(rmw, args[1]);
                     else if (op == llvm::AtomicRMWInst::Sub) *out_val = be_state->builder->CreateSub(rmw, args[1]);
                     else if (op == llvm::AtomicRMWInst::FSub) *out_val = be_state->builder->CreateFSub(rmw, args[1]);
                     else if (op == llvm::AtomicRMWInst::And) *out_val = be_state->builder->CreateAnd(rmw, args[1]);
                     else if (op == llvm::AtomicRMWInst::Or) *out_val = be_state->builder->CreateOr(rmw, args[1]);
                     else if (op == llvm::AtomicRMWInst::Xor) *out_val = be_state->builder->CreateXor(rmw, args[1]);
                     else {
                         llvm::Value* and_val = be_state->builder->CreateAnd(rmw, args[1]);
                         *out_val = be_state->builder->CreateNot(and_val);
                     }
                 } else {
                     *out_val = rmw;
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("atomic_load") != std::string::npos) {
                 llvm::Value* ptr_arg = args[0];
                 llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                 LLVM_GEN_BE_ASSERT_OK(decode_atomic_ordering(args.back(), true, &order));

                 llvm::Type* loaded_ty = nullptr;
                 llvm_gen_be_error_t err = get_llvm_type(expr->type, &loaded_ty);
                 if (err != llvm_gen_be_error_t::ok) return err;

                 if (loaded_ty->isVoidTy()) {
                     // __atomic_load(ptr, ret_ptr, order)
                     err = get_llvm_type(op1->next->next->type->variant.pointer.type, &loaded_ty);
                     if (err != llvm_gen_be_error_t::ok) return err;
                     uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(loaded_ty);
                     if (!loaded_ty->isIntOrPtrTy() && !loaded_ty->isFloatingPointTy()) {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_load", lib_ft);
                         *out_val = be_state->builder->CreateCall(
                             lib_fn, {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, args[1],
                                      be_state->builder->CreateIntCast(args.back(), i32_ty, false)});
                     } else {
                         llvm::LoadInst* load = be_state->builder->CreateLoad(loaded_ty, ptr_arg);
                         load->setAtomic(order);
                         be_state->builder->CreateStore(load, args[1]);
                         *out_val = load;
                     }
                 } else {
                     llvm::LoadInst* load = be_state->builder->CreateLoad(loaded_ty, ptr_arg);
                     load->setAtomic(order);
                     *out_val = load;
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("atomic_store") != std::string::npos) {
                 llvm::Value* ptr_arg = args[0];
                 llvm::Value* val_arg = args[1];
                 llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                 LLVM_GEN_BE_ASSERT_OK(decode_atomic_ordering(args.back(), false, &order));

                 if (rname != "__atomic_store") {
                     llvm::StoreInst* store = be_state->builder->CreateStore(val_arg, ptr_arg);
                     store->setAtomic(order);
                 } else {
                     // __atomic_store(ptr, val_ptr, order)
                     llvm::Type* val_ty = nullptr;
                     llvm_gen_be_error_t err_ty = get_llvm_type(op1->next->next->type->variant.pointer.type, &val_ty);
                     if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                     uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(val_ty);
                     if (!val_ty->isIntOrPtrTy() && !val_ty->isFloatingPointTy()) {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_store", lib_ft);
                         be_state->builder->CreateCall(
                             lib_fn, {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, val_arg,
                                      be_state->builder->CreateIntCast(args.back(), i32_ty, false)});
                     } else {
                         llvm::Value* loaded_val = be_state->builder->CreateLoad(val_ty, val_arg);
                         llvm::StoreInst* store = be_state->builder->CreateStore(loaded_val, ptr_arg);
                         store->setAtomic(order);
                     }
                 }
                 *out_val = llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*be_state->context)); // void
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("atomic_exchange") != std::string::npos) {
                 llvm::Value* ptr_arg = args[0];
                 llvm::Value* val_arg = args[1];
                 llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                 LLVM_GEN_BE_ASSERT_OK(decode_atomic_ordering(args.back(), false, &order));

                 if (rname != "__atomic_exchange") {
                     *out_val = be_state->builder->CreateAtomicRMW(llvm::AtomicRMWInst::Xchg, ptr_arg, val_arg, llvm::MaybeAlign(), order);
                 } else {
                     llvm::Type* val_ty = nullptr;
                     llvm_gen_be_error_t err_ty = get_llvm_type(op1->next->next->type->variant.pointer.type, &val_ty);
                     if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                     uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(val_ty);
                     if (!val_ty->isIntOrPtrTy() && !val_ty->isFloatingPointTy()) {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_exchange", lib_ft);
                         *out_val = be_state->builder->CreateCall(
                             lib_fn, {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, val_arg, args[2],
                                      be_state->builder->CreateIntCast(args.back(), i32_ty, false)});
                     } else {
                         llvm::Value* loaded_val = be_state->builder->CreateLoad(val_ty, val_arg);
                         llvm::Value* old_val = be_state->builder->CreateAtomicRMW(llvm::AtomicRMWInst::Xchg, ptr_arg, loaded_val, llvm::MaybeAlign(), order);
                         be_state->builder->CreateStore(old_val, args[2]); // __atomic_exchange(ptr, val_ptr, ret_ptr, order)
                         *out_val = old_val;
                     }
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("atomic_compare_exchange") != std::string::npos) {
                 llvm::AtomicOrdering succ_order = llvm::AtomicOrdering::SequentiallyConsistent;
                 llvm::AtomicOrdering fail_order = llvm::AtomicOrdering::SequentiallyConsistent;
                 LLVM_GEN_BE_ASSERT_OK(decode_atomic_ordering(args[4], false, &succ_order));
                 LLVM_GEN_BE_ASSERT_OK(decode_atomic_ordering(args[5], true, &fail_order));
                 
                 llvm::Type* val_ty = args[2]->getType();
                 llvm::Value* expected_val = be_state->builder->CreateLoad(val_ty, args[1]);
                 llvm::Value* desired_val = args[2];
                 llvm::Type* orig_val_ty = val_ty;

                 if (val_ty->isFloatingPointTy()) {
                     llvm::Type* int_ty = llvm::IntegerType::get(*be_state->context, val_ty->getPrimitiveSizeInBits());
                     expected_val = be_state->builder->CreateBitCast(expected_val, int_ty);
                     desired_val = be_state->builder->CreateBitCast(desired_val, int_ty);
                 }
                 
                 llvm::AtomicCmpXchgInst* cmpxchg = be_state->builder->CreateAtomicCmpXchg(
                     args[0], expected_val, desired_val, llvm::MaybeAlign(), succ_order, fail_order);
                     
                 llvm::Value* success = be_state->builder->CreateExtractValue(cmpxchg, 1);
                 llvm::Value* loaded_val = be_state->builder->CreateExtractValue(cmpxchg, 0);
                 if (orig_val_ty->isFloatingPointTy()) {
                     loaded_val = be_state->builder->CreateBitCast(loaded_val, orig_val_ty);
                 }
                 
                 // If fail, store loaded_val back to expected_ptr
                 be_state->builder->CreateStore(loaded_val, args[1]);
                 
                 // Return boolean success
                 *out_val = be_state->builder->CreateZExt(success, llvm::Type::getInt8Ty(*be_state->context));
                 return llvm_gen_be_error_t::ok;
             }
      }
  }

  if (!callee_ty) {
     if (llvm::Function* f = llvm::dyn_cast<llvm::Function>(v1)) {
       callee_ty = f->getFunctionType();
     } else {
       std::vector<llvm::Type*> inferred_param_tys;
       for (llvm::Value* a : args) {
         inferred_param_tys.push_back(a->getType());
       }
       llvm::Type* inferred_ret_ty = llvm::Type::getVoidTy(*be_state->context);
       if (expr->type) {
         llvm_gen_be_error_t err = get_llvm_type(expr->type, &inferred_ret_ty);
         if (err != llvm_gen_be_error_t::ok) return err;
       }
       callee_ty = llvm::FunctionType::get(inferred_ret_ty, inferred_param_tys, false);
     }
  }

  llvm::CallBase* call_res = nullptr;
  llvm::Function* callee_fn = llvm::dyn_cast<llvm::Function>(v1);
  bool can_throw = !callee_fn || !callee_fn->doesNotThrow();
  llvm::BasicBlock* lpad_bb = nullptr;
  if (can_throw) {
    LLVM_GEN_BE_ASSERT_OK(llvm_get_or_create_current_unwind_dest(&lpad_bb));
  }
  if (lpad_bb) {
    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
    call_res = be_state->builder->CreateInvoke(callee_ty, v1, normal_bb, lpad_bb, args);
    be_state->builder->SetInsertPoint(normal_bb);
  } else {
    call_res = be_state->builder->CreateCall(callee_ty, v1, args);
  }
  if (callee_fn) {
    call_res->setCallingConv(callee_fn->getCallingConv());
  }
  if (!callee_attrs.isEmpty()) {
    call_res->setAttributes(callee_attrs);
  }

  if (sret_alloc) {
    *out_val = be_state->builder->CreateLoad(sret_ty, sret_alloc, "sret.load");
  } else if (expr->type && !call_res->getType()->isVoidTy()) {
    llvm::Type* expected_ret_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(expr->type, &expected_ret_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (expected_ret_ty != call_res->getType()) {
      llvm::AllocaInst* unpack_alloc = be_state->builder->CreateAlloca(call_res->getType(), nullptr, "ret.unpack");
      be_state->builder->CreateStore(call_res, unpack_alloc);
      *out_val = be_state->builder->CreateLoad(expected_ret_ty, unpack_alloc, "ret.unpacked");
    } else {
      *out_val = call_res;
    }
  } else {
    *out_val = call_res;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves a `va_list` expression operand to its underlying pointer or stack slot.
 * @details Uses `be_state->local_vars` directly when `op` is a local `va_list` variable,
 *          or lowers `op` as an expression otherwise.
 * @param[in] op Pointer to the EDG `va_list` expression operand.
 * @param[out] out_ptr Receives the resolved `va_list` pointer value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t resolve_va_list_ptr(
    an_expr_node_ptr op,
    llvm::Value** out_ptr) noexcept {
  if (op->kind == enk_variable) {
    *out_ptr = be_state->local_vars[op->variant.variable.ptr];
    return llvm_gen_be_error_t::ok;
  }
  return llvm_lower_expression(op, out_ptr);
}

/**
 * @brief Extracts a bitfield descriptor from a field-selection expression when applicable.
 * @details Inspects `expr` for `eok_dot_field` or `eok_points_to_field` targeting a bitfield.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_field Receives the `a_field_ptr` if `expr` accesses a bitfield, or `nullptr`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t extract_bitfield_target(
    an_expr_node_ptr expr,
    a_field_ptr* out_field) noexcept {
  *out_field = nullptr;
  if (expr->kind == enk_operation &&
      (expr->variant.operation.kind == eok_dot_field ||
       expr->variant.operation.kind == eok_points_to_field)) {
    an_expr_node_ptr op2 = expr->variant.operation.operands->next;
    if (op2->variant.field.ptr->is_bit_field) {
      *out_field = op2->variant.field.ptr;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves or declares a `void(ptr)` destructor or cleanup function in the current module.
 * @details Looks up `routine->source_corresp.name` in `be_state->module` or creates an external
 *          `void(ptr)` declaration if not yet present.
 * @param[in] routine Pointer to the EDG destructor or cleanup routine.
 * @param[out] out_fn Receives the resolved `llvm::Function*`, or `nullptr` if unnamed/null.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t get_or_create_void_ptr_fn(
    a_routine_ptr routine,
    llvm::Function** out_fn) noexcept {
  *out_fn = nullptr;
  if (routine) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), {ptr_ty}, false);
    llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(
        routine->source_corresp.name, ft);
    *out_fn = llvm::dyn_cast<llvm::Function>(callee.getCallee());
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Evaluates runtime fallback for type-trait `a_builtin_operation_kind` values (`bok_is_*`, `bok_has_*`).
 * @details Inspects the target EDG type operand(s) and produces a boolean/integer `1` or `0` result.
 * @param[in] bok The builtin operation kind.
 * @param[in] op1 Pointer to the first operand (`enk_type_operand` or expression).
 * @param[out] out_result Receives `1` if the trait holds, or `0` otherwise.
 * @param[out] out_handled Receives `true` if @p bok is a recognized type-trait builtin.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t evaluate_type_trait_builtin(
    a_builtin_operation_kind bok,
    an_expr_node_ptr op1,
    int32_t* out_result,
    bool* out_handled) noexcept {
  *out_handled = true;
  *out_result = 0;
  a_type_ptr t1 = (op1->kind == enk_type_operand) ? op1->variant.type_operand.type : op1->type;
  if (t1) t1 = skip_typerefs(t1);
  an_expr_node_ptr op2 = op1->next;
  a_type_ptr t2 = (op2 && op2->kind == enk_type_operand) ? op2->variant.type_operand.type
                                                         : (op2 ? op2->type : nullptr);
  if (t2) t2 = skip_typerefs(t2);

  if (bok == bok_is_class) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct)) ? 1 : 0;
  } else if (bok == bok_is_union) {
    *out_result = (t1 && t1->kind == tk_union) ? 1 : 0;
  } else if (bok == bok_is_enum) {
    *out_result = (t1 && t1->kind == tk_integer && t1->variant.integer.enum_type) ? 1 : 0;
  } else if (bok == bok_is_empty) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   t1->variant.class_struct_union.field_list == nullptr &&
                   !t1->variant.class_struct_union.any_virtual_functions_including_in_base_classes)
                      ? 1
                      : 0;
  } else if (bok == bok_is_polymorphic) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   t1->variant.class_struct_union.any_virtual_functions)
                      ? 1
                      : 0;
  } else if (bok == bok_is_abstract) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   t1->variant.class_struct_union.abstract)
                      ? 1
                      : 0;
  } else if (bok == bok_is_final) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   t1->variant.class_struct_union.final)
                      ? 1
                      : 0;
  } else if (bok == bok_is_same || bok == bok_is_same_as) {
    *out_result = (t1 != nullptr && t1 == t2) ? 1 : 0;
  } else if (bok == bok_is_base_of || bok == bok_builtin_is_virtual_base_of) {
    if (t1 && t2 && (t1->kind == tk_class || t1->kind == tk_struct) &&
        (t2->kind == tk_class || t2->kind == tk_struct)) {
      if (t1 == t2 && bok == bok_is_base_of) {
        *out_result = 1;
      } else if (t2->variant.class_struct_union.extra_info) {
        for (a_base_class_ptr bc = t2->variant.class_struct_union.extra_info->base_classes;
             bc != nullptr; bc = bc->next) {
          if (bc->type && skip_typerefs(bc->type) == t1) {
            if (bok == bok_is_base_of || bc->is_virtual) {
              *out_result = 1;
              break;
            }
          }
        }
      }
    }
  } else if (bok == bok_is_void) {
    *out_result = (t1 && t1->kind == tk_void) ? 1 : 0;
  } else if (bok == bok_is_integral) {
    *out_result = (t1 && t1->kind == tk_integer && !t1->variant.integer.enum_type) ? 1 : 0;
  } else if (bok == bok_is_floating_point) {
    *out_result = (t1 && t1->kind == tk_float) ? 1 : 0;
  } else if (bok == bok_is_arithmetic) {
    *out_result = (t1 && ((t1->kind == tk_integer && !t1->variant.integer.enum_type) || t1->kind == tk_float)) ? 1 : 0;
  } else if (bok == bok_is_fundamental) {
    *out_result = (t1 && (t1->kind == tk_void || t1->kind == tk_nullptr ||
                          (t1->kind == tk_integer && !t1->variant.integer.enum_type) ||
                          t1->kind == tk_float)) ? 1 : 0;
  } else if (bok == bok_is_compound) {
    *out_result = (t1 && !(t1->kind == tk_void || t1->kind == tk_nullptr ||
                           (t1->kind == tk_integer && !t1->variant.integer.enum_type) ||
                           t1->kind == tk_float)) ? 1 : 0;
  } else if (bok == bok_is_array) {
    *out_result = (t1 && t1->kind == tk_array) ? 1 : 0;
  } else if (bok == bok_is_bounded_array) {
    *out_result = (t1 && t1->kind == tk_array && t1->variant.array.variant.number_of_elements > 0) ? 1 : 0;
  } else if (bok == bok_is_unbounded_array) {
    *out_result = (t1 && t1->kind == tk_array && t1->variant.array.variant.number_of_elements == 0) ? 1 : 0;
  } else if (bok == bok_is_pointer) {
    *out_result = (t1 && t1->kind == tk_pointer && !t1->variant.pointer.is_reference) ? 1 : 0;
  } else if (bok == bok_is_lvalue_reference) {
    *out_result = (t1 && t1->kind == tk_pointer && t1->variant.pointer.is_reference &&
                   !t1->variant.pointer.is_rvalue_reference) ? 1 : 0;
  } else if (bok == bok_is_rvalue_reference) {
    *out_result = (t1 && t1->kind == tk_pointer && t1->variant.pointer.is_reference &&
                   t1->variant.pointer.is_rvalue_reference) ? 1 : 0;
  } else if (bok == bok_is_reference) {
    *out_result = (t1 && t1->kind == tk_pointer && t1->variant.pointer.is_reference) ? 1 : 0;
  } else if (bok == bok_is_member_pointer) {
    *out_result = (t1 && t1->kind == tk_ptr_to_member) ? 1 : 0;
  } else if (bok == bok_is_member_function_pointer) {
    *out_result = (t1 && t1->kind == tk_ptr_to_member && t1->variant.ptr_to_member.type &&
                   skip_typerefs(t1->variant.ptr_to_member.type)->kind == tk_routine) ? 1 : 0;
  } else if (bok == bok_is_member_object_pointer) {
    *out_result = (t1 && t1->kind == tk_ptr_to_member && t1->variant.ptr_to_member.type &&
                   skip_typerefs(t1->variant.ptr_to_member.type)->kind != tk_routine) ? 1 : 0;
  } else if (bok == bok_is_function) {
    *out_result = (t1 && t1->kind == tk_routine) ? 1 : 0;
  } else if (bok == bok_is_scalar) {
    *out_result = (t1 && (t1->kind == tk_integer || t1->kind == tk_float || t1->kind == tk_nullptr ||
                          t1->kind == tk_ptr_to_member ||
                          (t1->kind == tk_pointer && !t1->variant.pointer.is_reference))) ? 1 : 0;
  } else if (bok == bok_is_object) {
    *out_result = (t1 && t1->kind != tk_void && t1->kind != tk_routine &&
                   !(t1->kind == tk_pointer && t1->variant.pointer.is_reference)) ? 1 : 0;
  } else if (bok == bok_is_const) {
    a_type_ptr raw_t1 = (op1->kind == enk_type_operand) ? op1->variant.type_operand.type : op1->type;
    *out_result = (raw_t1 && (f_get_type_qualifiers(raw_t1, FALSE) & TQ_CONST) != 0) ? 1 : 0;
  } else if (bok == bok_is_volatile) {
    a_type_ptr raw_t1 = (op1->kind == enk_type_operand) ? op1->variant.type_operand.type : op1->type;
    *out_result = (raw_t1 && (f_get_type_qualifiers(raw_t1, FALSE) & TQ_VOLATILE) != 0) ? 1 : 0;
  } else if (bok == bok_is_signed) {
    *out_result = (t1 && (t1->kind == tk_float ||
                          (t1->kind == tk_integer && !t1->variant.integer.bool_type &&
                           int_kind_is_signed[t1->variant.integer.int_kind]))) ? 1 : 0;
  } else if (bok == bok_is_unsigned) {
    *out_result = (t1 && t1->kind == tk_integer &&
                   (t1->variant.integer.bool_type || !int_kind_is_signed[t1->variant.integer.int_kind])) ? 1 : 0;
  } else if (bok == bok_is_scoped_enum) {
    *out_result = (t1 && t1->kind == tk_integer && t1->variant.integer.enum_type &&
                   t1->variant.integer.is_scoped_enum) ? 1 : 0;
  } else if (bok == bok_has_virtual_destructor) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   t1->variant.class_struct_union.any_virtual_functions) ? 1 : 0;
  } else if (bok == bok_is_aggregate) {
    *out_result = (t1 && (t1->kind == tk_array || t1->kind == tk_union ||
                          ((t1->kind == tk_class || t1->kind == tk_struct) &&
                           !t1->variant.class_struct_union.any_virtual_functions))) ? 1 : 0;
  } else if (bok == bok_is_layout_compatible) {
    *out_result = (t1 && t2 && (t1 == t2 || (t1->kind == t2->kind && t1->size == t2->size))) ? 1 : 0;
  } else if (bok == bok_is_pointer_interconvertible_base_of) {
    *out_result = (t1 && t2 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   (t2->kind == tk_class || t2->kind == tk_struct) &&
                   !t2->variant.class_struct_union.any_virtual_functions) ? 1 : 0;
  } else if (bok == bok_reference_binds_to_temporary || bok == bok_reference_constructs_from_temporary ||
             bok == bok_reference_converts_from_temporary) {
    *out_result = (t1 && t2 && t1->kind == tk_pointer && t1->variant.pointer.is_reference &&
                   skip_typerefs(t1->variant.pointer.type) != t2) ? 1 : 0;
  } else if (bok == bok_array_rank) {
    int32_t rank = 0;
    a_type_ptr cur = t1;
    while (cur && cur->kind == tk_array) {
      rank++;
      cur = skip_typerefs(cur->variant.array.element_type);
    }
    *out_result = rank;
  } else if (bok == bok_array_extent) {
    uint64_t dim_idx = 0;
    if (op2 && op2->kind == enk_constant && op2->variant.constant.ptr) {
      dim_idx = EDG_INT_VAL_LOW64(op2->variant.constant.ptr->variant.integer_value);
    }
    a_type_ptr cur = t1;
    for (uint64_t d = 0; d < dim_idx && cur->kind == tk_array; ++d) {
      cur = skip_typerefs(cur->variant.array.element_type);
    }
    if (cur && cur->kind == tk_array) {
      *out_result = static_cast<int32_t>(cur->variant.array.variant.number_of_elements);
    }
  } else if (bok == bok_is_standard_layout || bok == bok_is_pod || bok == bok_is_literal_type ||
             bok == bok_is_trivially_copyable || bok == bok_is_trivial) {
    bool is_trivial_or_std_layout = false;
    if (t1) {
      if (t1->kind == tk_class || t1->kind == tk_struct) {
        is_trivial_or_std_layout = !t1->variant.class_struct_union.any_virtual_functions &&
                                   !t1->variant.class_struct_union.any_virtual_base_classes &&
                                   !type_has_nontrivial_destructor(t1);
      } else {
        is_trivial_or_std_layout = (t1->kind != tk_void);
      }
    }
    *out_result = is_trivial_or_std_layout ? 1 : 0;
  } else if (bok == bok_has_unique_object_representations) {
    bool has_unique_rep = false;
    if (t1->size > 0 && t1->kind != tk_pointer) {
      std::vector<uint8_t> pad_mask;
      LLVM_GEN_BE_ASSERT_OK(llvm_compute_type_padding_mask(t1, &pad_mask));
      has_unique_rep = true;
      for (uint8_t mb : pad_mask) {
        if (mb != 0xFF) {
          has_unique_rep = false;
          break;
        }
      }
    }
    *out_result = has_unique_rep ? 1 : 0;
  } else if (bok == bok_is_trivially_constructible || bok == bok_is_constructible ||
             bok == bok_is_nothrow_constructible || bok == bok_is_destructible ||
             bok == bok_is_nothrow_destructible || bok == bok_is_trivially_destructible) {
    bool ok_trait = (t1->kind != tk_void && t1->kind != tk_routine &&
                     !(t1->kind == tk_class && t1->variant.class_struct_union.abstract));
    if (ok_trait && (bok == bok_is_trivially_constructible || bok == bok_is_trivially_destructible)) {
      ok_trait = !type_has_nontrivial_destructor(t1);
    }
    *out_result = ok_trait ? 1 : 0;
  } else if (bok < bok_last) {
    bool ok_assign_or_conv = (t1->kind != tk_void && t2->kind != tk_void &&
                              !type_has_nontrivial_destructor(t1));
    *out_result = ok_assign_or_conv ? 1 : 0;
  } else {
    *out_handled = false;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG builtin operation node (`enk_builtin_operation`, `a_builtin_operation_kind`) into LLVM IR.
 * @details Supports `bok_offsetof`, `bok_intaddr`, `bok_builtin_bit_cast`, `bok_builtin_has_attribute`,
 *          `bok_types_compatible`, `bok_builtin_complex`, `bok_builtin_addressof`, `bok_builtin_shuffle`,
 *          `bok_builtin_shufflevector`, `bok_builtin_convertvector`, and runtime fallbacks for compiler
 *          type-trait `bok_*` operations.
 * @param[in] expr Pointer to the EDG `enk_builtin_operation` expression node.
 * @param[out] out_val Receives the lowered `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_builtin_operation(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  a_builtin_operation_kind bok = expr->variant.builtin_operation.kind;
  an_expr_node_ptr op1 = expr->variant.builtin_operation.operands;
  if (!op1) return llvm_gen_be_error_t::invalid_argument;

  if (bok == bok_offsetof) {
    uint64_t total_offset = 0;
    an_expr_node_ptr curr = op1->next;
    while (curr) {
      if (curr->kind == enk_field) {
        total_offset += curr->variant.field.ptr->offset;
      } else {
        an_expr_node_ptr sub_op2 = curr->variant.operation.operands->next;
        total_offset += sub_op2->variant.field.ptr->offset;
      }
      curr = curr->next;
    }
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), total_offset);
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_intaddr) {
    llvm::Value* addr_val = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &addr_val));
    llvm::Type* dst_ty = llvm::Type::getInt64Ty(*be_state->context);
    if (expr->type) {
      LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &dst_ty));
    }
    if (addr_val->getType()->isPointerTy()) {
      *out_val = be_state->builder->CreatePtrToInt(addr_val, dst_ty);
    } else {
      *out_val = be_state->builder->CreateIntCast(addr_val, dst_ty, false);
    }
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_bit_cast) {
    llvm::Value* src_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(op1->next, &src_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    llvm::Type* dest_ty = nullptr;
    err = get_llvm_type(expr->type, &dest_ty);
    if (err != llvm_gen_be_error_t::ok) return err;

    if (src_val->getType()->isAggregateType() || dest_ty->isAggregateType()) {
      llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(src_val->getType());
      be_state->builder->CreateStore(src_val, alloca);
      *out_val = be_state->builder->CreateLoad(dest_ty, alloca);
    } else {
      if (src_val->getType() == dest_ty) *out_val = src_val;
      else *out_val = be_state->builder->CreateBitCast(src_val, dest_ty);
    }
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_has_attribute) {
    an_expr_node_ptr arg2 = op1->next;
    an_attribute_ptr target_attr = arg2 ? arg2->variant.constant.ptr->source_corresp.attributes : nullptr;

    an_expr_node_ptr subj = op1;
    while (subj->kind == enk_operation) {
      an_expr_operator_kind k = subj->variant.operation.kind;
      if (k == eok_parens) {
        subj = subj->variant.operation.operands;
      } else if (k == eok_dot_field) {
        subj = subj->variant.operation.operands->next;
      } else {
        break;
      }
    }

    an_attribute_ptr entity_attrs = nullptr;
    if (subj->kind == enk_variable) {
      entity_attrs = subj->variant.variable.ptr->source_corresp.attributes;
    } else if (subj->kind == enk_routine) {
      entity_attrs = subj->variant.routine.ptr->source_corresp.attributes;
    } else if (subj->kind == enk_field) {
      entity_attrs = subj->variant.field.ptr->source_corresp.attributes;
    } else if (subj->kind == enk_type_operand) {
      entity_attrs = subj->variant.type_operand.type->source_corresp.attributes;
    } else {
      entity_attrs = subj->type->source_corresp.attributes;
    }

    int32_t has_attr = 0;
    for (an_attribute_ptr ap = entity_attrs; ap != nullptr; ap = ap->next) {
      if (!target_attr || ap->kind == target_attr->kind) {
        has_attr = 1;
        break;
      }
    }
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), has_attr);
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_types_compatible) {
    an_expr_node_ptr op2 = op1->next;
    a_type_ptr t1 = (op1->kind == enk_type_operand) ? op1->variant.type_operand.type : op1->type;
    a_type_ptr t2 = (op2 && op2->kind == enk_type_operand) ? op2->variant.type_operand.type
                                                           : (op2 ? op2->type : nullptr);
    if (t1) t1 = skip_typerefs(t1);
    if (t2) t2 = skip_typerefs(t2);
    bool compat = (t1 != nullptr && t2 != nullptr &&
                   (t1 == t2 || (t1->kind == t2->kind && t1->size == t2->size)));
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), compat ? 1 : 0);
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_complex) {
    llvm::Value* real_v = nullptr;
    llvm::Value* imag_v = nullptr;
    LLVM_GEN_BE_CHECK(lower_binary_operands(op1, &real_v, &imag_v));
    llvm::Type* cplx_ty = llvm::StructType::get(*be_state->context, {real_v->getType(), imag_v->getType()});
    llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(cplx_ty), real_v, 0);
    *out_val = be_state->builder->CreateInsertValue(res, imag_v, 1);
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_addressof) {
    return llvm_lower_lvalue_expression(op1, out_val);
  } else if (bok == bok_builtin_shuffle || bok == bok_builtin_shufflevector) {
    llvm::Value* v1 = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &v1));
    an_expr_node_ptr op2 = op1->next;
    llvm::Value* v2 = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op2, &v2));
    auto* v_ty = llvm::cast<llvm::VectorType>(v1->getType());
    unsigned num_elems = v_ty->getElementCount().getKnownMinValue();
    if (bok == bok_builtin_shufflevector && op2->next != nullptr) {
      std::vector<int> mask_indices;
      for (an_expr_node_ptr cur = op2->next; cur != nullptr; cur = cur->next) {
        llvm::Value* idx_v = nullptr;
        LLVM_GEN_BE_CHECK(llvm_lower_expression(cur, &idx_v));
        int m_idx = 0;
        if (auto* ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(idx_v)) {
          m_idx = static_cast<int>(ci->getSExtValue());
        }
        mask_indices.push_back(m_idx);
      }
      *out_val = be_state->builder->CreateShuffleVector(v1, v2, mask_indices);
      return llvm_gen_be_error_t::ok;
    }
    llvm::Value* mask_val = v2;
    llvm::Value* vec2_val = llvm::PoisonValue::get(v1->getType());
    if (op2->next != nullptr) {
      vec2_val = v2;
      LLVM_GEN_BE_CHECK(llvm_lower_expression(op2->next, &mask_val));
    }
    std::vector<int> mask_indices(num_elems, 0);
    if (auto* cv = llvm::dyn_cast<llvm::Constant>(mask_val)) {
      for (unsigned i = 0; i < num_elems; ++i) {
        auto* elem_ci = llvm::cast<llvm::ConstantInt>(cv->getAggregateElement(i));
        mask_indices[i] = static_cast<int>(elem_ci->getZExtValue() % (num_elems * 2));
      }
    }
    *out_val = be_state->builder->CreateShuffleVector(v1, vec2_val, mask_indices);
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_convertvector) {
    llvm::Value* src_vec = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &src_vec));
    llvm::Type* dst_ty = nullptr;
    LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &dst_ty));
    llvm::Type* src_ty = src_vec->getType();
    if (src_ty->isIntOrIntVectorTy() && dst_ty->isIntOrIntVectorTy()) {
      *out_val = be_state->builder->CreateIntCast(src_vec, dst_ty, true);
    } else if (src_ty->isFPOrFPVectorTy() && dst_ty->isFPOrFPVectorTy()) {
      *out_val = be_state->builder->CreateFPCast(src_vec, dst_ty);
    } else if (src_ty->isIntOrIntVectorTy()) {
      *out_val = be_state->builder->CreateSIToFP(src_vec, dst_ty);
    } else {
      *out_val = be_state->builder->CreateFPToSI(src_vec, dst_ty);
    }
    return llvm_gen_be_error_t::ok;
  } else {
    int32_t trait_res = 0;
    bool trait_handled = false;
    LLVM_GEN_BE_ASSERT_OK(evaluate_type_trait_builtin(bok, op1, &trait_res, &trait_handled));
    if (trait_handled) {
      llvm::Type* ret_ty = llvm::Type::getInt32Ty(*be_state->context);
      if (expr->type && skip_typerefs(expr->type)->variant.integer.bool_type) {
        ret_ty = llvm::Type::getInt1Ty(*be_state->context);
      }
      *out_val = llvm::ConstantInt::get(ret_ty, trait_res);
      return llvm_gen_be_error_t::ok;
    }
  }

  return llvm_gen_be_error_t::unsupported_expr;
}

#if !BUILTIN_FUNCTIONS_ENABLED
static constexpr an_expr_node_kind enk_builtin_choose_expr = static_cast<an_expr_node_kind>(250);
#endif

/**
 * @brief Lowers a GNU `__builtin_choose_expr` node (`enk_builtin_choose_expr`) into LLVM IR.
 * @details Evaluates either the second or third operand of `expr->variant.builtin_choose_expr`
 *          depending on `choose_first`.
 * @param[in] expr Pointer to the EDG `enk_builtin_choose_expr` expression node.
 * @param[out] out_val Receives the lowered `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if operands are missing.
 */
llvm_gen_be_error_t llvm_lower_builtin_choose_expr(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
#if BUILTIN_FUNCTIONS_ENABLED
  an_expr_node_ptr op1 = expr->variant.builtin_choose_expr.operands;
  if (!op1 || !op1->next || !op1->next->next) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  an_expr_node_ptr chosen = expr->variant.builtin_choose_expr.choose_first
                                ? op1->next
                                : op1->next->next;
  return llvm_lower_expression(chosen, out_val);
#else
  (void)expr;
  (void)out_val;
  return llvm_gen_be_error_t::invalid_argument;
#endif
}

/**
 * @brief Lowers an arbitrary EDG expression node (`an_expr_node_ptr`) into an `llvm::Value*`.
 * @details Dispatches constants, variables, operations, conditions, temporaries, `new`/`delete`,
 *          `throw`, `typeid`, `dynamic_cast`, coroutines, and statement expressions.
 * @param[in] expr Pointer to the EDG expression node to lower.
 * @param[out] out_val Receives the lowered `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  if (!expr) {
    *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @struct DILocSaver
   * @brief RAII helper that saves and restores the active `IRBuilder` debug location.
   * @details Ensures nested sub-expression debug locations do not leak to outer expressions.
   */
  struct DILocSaver {
      llvm::DebugLoc saved_loc; ///< Saved debug location on entry.
      DILocSaver() : saved_loc(be_state->builder->getCurrentDebugLocation()) {} ///< Captures the current debug location.
      ~DILocSaver() { be_state->builder->SetCurrentDebugLocation(saved_loc); } ///< Restores the saved debug location.
  } loc_saver;

  if (be_state->dbg_state && expr->position.seq != 0) {
      llvm::DIScope* scope = be_state->dbg_state->scope_stack.back();
      llvm::DILocation* loc = nullptr;
      llvm_gen_be_error_t err = get_di_location(be_state->dbg_state, expr->position, scope, &loc);
      if (err == llvm_gen_be_error_t::ok && loc) {
          be_state->builder->SetCurrentDebugLocation(loc);
      }
  }

  // Handle LValue evaluation explicitly
  if (expr->is_lvalue) {
    switch (expr->kind) {
      case enk_variable:
      case enk_operation:
        if (expr->kind == enk_operation) {
           an_expr_operator_kind op_kind = expr->variant.operation.kind;
           if (op_kind != eok_subscript && op_kind != eok_dot_field && 
               op_kind != eok_points_to_field && op_kind != eok_indirect &&
               op_kind != eok_parens && op_kind != eok_lvalue) {
               break; // Not an LValue-producing operation
           }
        }
        return llvm_lower_lvalue_expression(expr, out_val);
      default: break;
    }
  }

#if VLA_DEALLOCATIONS_IN_IL
  if (expr->kind == enk_vla_dealloc) {
    a_variable_ptr var = expr->variant.vla_variable;
    if (be_state->vla_saved_stacks.count(var)) {
      llvm::Function* stackrestore = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::stackrestore,
          {llvm::PointerType::getUnqual(*be_state->context)});
      be_state->builder->CreateCall(stackrestore, be_state->vla_saved_stacks[var]);
    }
    *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }
#endif /* VLA_DEALLOCATIONS_IN_IL */

  switch (expr->kind) {
    case enk_braced_init_list: {
      llvm::Type* agg_ty = nullptr;
      llvm_gen_be_error_t ty_err = get_llvm_type(expr->type, &agg_ty);
      if (ty_err != llvm_gen_be_error_t::ok) return ty_err;
      if (agg_ty->isVoidTy()) {
          *out_val = nullptr;
          return llvm_gen_be_error_t::ok;
      }

      std::vector<llvm::Value*> values;
      for (an_expr_node_ptr item = expr->variant.braced_init_list; item; item = item->next) {
         llvm::Value* val = nullptr;
         llvm_gen_be_error_t err = llvm_lower_expression(item, &val);
         if (err != llvm_gen_be_error_t::ok) return err;
         values.push_back(val);
      }

      llvm::Value* agg = llvm::UndefValue::get(agg_ty);
      if (agg_ty->isAggregateType()) {
          for (size_t i = 0; i < values.size(); ++i) {
             agg = be_state->builder->CreateInsertValue(agg, values[i], i);
          }
      } else if (values.size() == 1) {
          agg = values[0];
      }
      *out_val = agg;
      return llvm_gen_be_error_t::ok;
    }

    case enk_reuse_value: {
      a_dynamic_init_ptr init = expr->variant.reused_value_init;
      if (!init) return llvm_gen_be_error_t::invalid_argument;
      auto it = be_state->reused_values.find(init);
      if (it != be_state->reused_values.end()) {
          *out_val = it->second;
          return llvm_gen_be_error_t::ok;
      }
      if (init->kind == dik_expression || init->kind == dik_class_result_via_ctor) {
          llvm::Value* val = nullptr;
          llvm_gen_be_error_t err = llvm_lower_expression(init->variant.expression, &val);
          if (err != llvm_gen_be_error_t::ok) return err;
          be_state->reused_values[init] = val;
          *out_val = val;
          return llvm_gen_be_error_t::ok;
      }
      llvm::Type* expr_ty = nullptr;
      llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
      llvm::Value* val = nullptr;
      if (init->kind == dik_zero) {
          val = llvm::Constant::getNullValue(expr_ty);
      } else if (init->kind == dik_constant) {
          llvm::Constant* con_val = nullptr;
          err = evaluate_constant(init->variant.constant.ptr, expr_ty, &con_val);
          if (err != llvm_gen_be_error_t::ok) return err;
          val = con_val;
      } else if (init->kind == dik_constructor || init->kind == dik_nonconstant_aggregate ||
                 init->kind == dik_bitwise_copy) {
          llvm::AllocaInst* tmp_alloca = be_state->builder->CreateAlloca(expr_ty, nullptr, "reuse.tmp");
          err = llvm_emit_dynamic_init(init, tmp_alloca, expr_ty);
          if (err != llvm_gen_be_error_t::ok) return err;
          val = be_state->builder->CreateLoad(expr_ty, tmp_alloca, "reuse.val");
      } else {
          return llvm_gen_be_error_t::unsupported_expr;
      }
      be_state->reused_values[init] = val;
      *out_val = val;
      return llvm_gen_be_error_t::ok;
    }

    case enk_c11_generic: {
      if (!expr->variant.c11_generic.result) {
        return llvm_gen_be_error_t::unsupported_expr;
      }
      return llvm_lower_expression(expr->variant.c11_generic.result, out_val);
    }

    case enk_fold:
      return llvm_lower_fold_expression(expr, out_val);

    case enk_pack_index: {
      an_expr_node_ptr pack_expr = expr->variant.pack_index.expr;
      if (!pack_expr) {
        return llvm_gen_be_error_t::unsupported_expr;
      }
      uint64_t target_idx = 0;
      if (expr->variant.pack_index.index_expr) {
        llvm::Value* idx_val = nullptr;
        llvm_gen_be_error_t err = llvm_lower_expression(expr->variant.pack_index.index_expr, &idx_val);
        if (err != llvm_gen_be_error_t::ok) return err;
        if (auto* ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(idx_val)) {
          target_idx = ci->getZExtValue();
        }
      }
      an_expr_node_ptr selected = pack_expr;
      for (uint64_t i = 0; i < target_idx && selected->next != nullptr; ++i) {
        selected = selected->next;
      }
      return llvm_lower_expression(selected, out_val);
    }
    case enk_constant: {
       llvm::Type* expr_llvm_ty = nullptr;
       llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
       if (err != llvm_gen_be_error_t::ok) return err;
       a_constant_ptr con = expr->variant.constant.ptr;
       if (!con) {
           *out_val = llvm::Constant::getNullValue(expr_llvm_ty);
           return llvm_gen_be_error_t::ok;
       }
       llvm::Constant* out_const = nullptr;
       err = evaluate_constant(con, expr_llvm_ty, &out_const);
       if (err != llvm_gen_be_error_t::ok) return err;
       *out_val = out_const;
       return llvm_gen_be_error_t::ok;
    }
    case enk_routine: {
       a_routine_ptr routine = expr->variant.routine.ptr;
       if (!routine) {
           llvm::Type* expr_llvm_ty = nullptr;
           llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_val = llvm::Constant::getNullValue(expr_llvm_ty);
           return llvm_gen_be_error_t::ok;
       }
       llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
       if (!func) {
         llvm::Type* func_ty = nullptr;
         llvm_gen_be_error_t err = get_llvm_type(routine->type, &func_ty);
         if (err != llvm_gen_be_error_t::ok) return err;
         func = llvm::Function::Create(
            llvm::isa<llvm::FunctionType>(func_ty) ? llvm::cast<llvm::FunctionType>(func_ty) : llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false),
            llvm::GlobalValue::ExternalLinkage,
            routine->source_corresp.name,
            be_state->module.get()
         );
       }
       *out_val = func;
       return llvm_gen_be_error_t::ok;
    }
    case enk_variable: {
       llvm::Value* ptr = nullptr;
       llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
       if (err != llvm_gen_be_error_t::ok) return err;
       return load_from_lvalue_ptr(ptr, expr->type, out_val);
    }
    case enk_operation: {
       if (expr->variant.operation.kind == eok_comma) {
           llvm::Value* v1 = nullptr;
           LLVM_GEN_BE_CHECK(llvm_lower_expression(expr->variant.operation.operands, &v1));
           return llvm_lower_expression(expr->variant.operation.operands->next, out_val);
       }
       
       switch (expr->variant.operation.kind) {
         case eok_negate:
         case eok_unary_plus:
         case eok_add:
         case eok_subtract:
         case eok_multiply:
         case eok_divide:
         case eok_remainder:
         case eok_padd:
         case eok_psubtract:
         case eok_pdiff:
         case eok_fjadd:
         case eok_jfadd:
         case eok_fjsubtract:
         case eok_jfsubtract:
         case eok_jmultiply:
         case eok_jdivide:
         case eok_xconj:
         case eok_complement:
         case eok_and:
         case eok_or:
         case eok_xor:
         case eok_shiftl:
         case eok_shiftr:
         case eok_gnu_min:
         case eok_gnu_max:
           return llvm_lower_arithmetic_expression(expr, out_val);
           
         case eok_eq:
         case eok_ne:
         case eok_lt:
         case eok_le:
         case eok_gt:
         case eok_ge:
         case eok_spaceship:
         case eok_not:
         case eok_land:
         case eok_lor:
           return llvm_lower_logical_expression(expr, out_val);
           
         case eok_cast:
         case eok_lvalue_cast:
           return llvm_lower_cast_expression(expr, out_val);

         case eok_pm_base_class_cast:
         case eok_pm_derived_class_cast:
           return llvm_lower_pm_cast(expr, out_val);

         case eok_dynamic_cast:
         case eok_ref_dynamic_cast:
           return llvm_lower_dynamic_cast(expr, out_val);

         case eok_pre_incr:
         case eok_pre_decr:
         case eok_post_incr:
         case eok_post_decr:
         case eok_add_assign:
         case eok_subtract_assign:
         case eok_multiply_assign:
         case eok_divide_assign:
         case eok_remainder_assign:
         case eok_and_assign:
         case eok_or_assign:
         case eok_xor_assign:
         case eok_shiftl_assign:
         case eok_shiftr_assign:
         case eok_padd_assign:
         case eok_psubtract_assign:
         case eok_bassign:
           return llvm_lower_compound_assignment(expr, out_val);
           
         case eok_dot_member_call:
         case eok_points_to_member_call:
         case eok_call:
           return llvm_lower_call_expression(expr, out_val);

         case eok_dot_pm_call:
         case eok_points_to_pm_call:
           return llvm_lower_pm_call(expr, out_val);

         case eok_dot_pm_func_ptr:
         case eok_points_to_pm_func_ptr: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* obj_val = nullptr;
           llvm::Value* pm_val = nullptr;
           LLVM_GEN_BE_CHECK(lower_binary_operands(op1, &obj_val, &pm_val));
           if (pm_val->getType()->isStructTy()) {
             llvm::Value* fn_or_off = be_state->builder->CreateExtractValue(pm_val, 0);
             *out_val = be_state->builder->CreateIntToPtr(
                 fn_or_off, llvm::PointerType::getUnqual(*be_state->context));
           } else {
             *out_val = pm_val;
           }
           return llvm_gen_be_error_t::ok;
         }

         case eok_parens:
         case eok_lvalue:
         case eok_lvalue_adjust: {
           return llvm_lower_expression(expr->variant.operation.operands, out_val);
         }

         case eok_pm_field:
         case eok_pm_points_to_field:
           return llvm_lower_pm_field_access(expr, out_val);
           
         case eok_subscript:
         case eok_vector_subscript: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Type* op1_ll_ty = nullptr;
           LLVM_GEN_BE_CHECK(get_llvm_type(op1->type, &op1_ll_ty));
           if (op1_ll_ty->isVectorTy()) {
             llvm::Value* v1 = nullptr;
             llvm::Value* v2 = nullptr;
             LLVM_GEN_BE_CHECK(lower_binary_operands(op1, &v1, &v2));
             *out_val = be_state->builder->CreateExtractElement(v1, v2);
             return llvm_gen_be_error_t::ok;
           }

           llvm::Value* ptr = nullptr;
           llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
           if (err != llvm_gen_be_error_t::ok) return err;
           return load_from_lvalue_ptr(ptr, expr->type, out_val);
         }
         case eok_dot_field:
         case eok_points_to_field:
         case eok_indirect: {
           llvm::Value* ptr = nullptr;
           llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (skip_typerefs(expr->type)->kind == tk_routine) {
             *out_val = ptr;
             return llvm_gen_be_error_t::ok;
           }

           a_field_ptr bf_field = nullptr;
           LLVM_GEN_BE_ASSERT_OK(extract_bitfield_target(expr, &bf_field));
           if (bf_field) {
             return llvm_emit_bitfield_load(ptr, bf_field, expr->type, out_val);
           }
           return load_from_lvalue_ptr(ptr, expr->type, out_val);
         }

         case eok_dot_static:
         case eok_points_to_static: {
           llvm::Value* v1 = nullptr;
           return lower_binary_operands(expr->variant.operation.operands, &v1, out_val);
         }

         case eok_dot_vacuous_destructor_call:
         case eok_points_to_vacuous_destructor_call: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_val = nullptr; // Vacuous destructor evaluates to nothing
           return llvm_gen_be_error_t::ok;
         }

         case eok_handle_to:
         case eok_handle_to_box:
         case eok_box:
         case eok_unbox:
         case eok_unbox_lvalue:
         case eok_cli_subscript: {
           // CLI operations are not supported by the LLVM backend.
           return llvm_gen_be_error_t::unsupported_expr;
         }
         case eok_va_start:
         case eok_va_start_single_operand:
         case eok_va_arg:
         case eok_va_end:
         case eok_va_copy: {
           an_expr_operator_kind vk = expr->variant.operation.kind;
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* va_ptr = nullptr;
           LLVM_GEN_BE_CHECK(resolve_va_list_ptr(op1, &va_ptr));
           llvm::Value* src_ptr = nullptr;
           if (vk == eok_va_copy) {
             LLVM_GEN_BE_CHECK(resolve_va_list_ptr(op1->next, &src_ptr));
           }
           llvm::Triple triple(be_state->module->getTargetTriple());
           if (triple.isOSWindows()) {
             if (vk == eok_va_arg) return lower_win64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_win64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_win64_va_copy(va_ptr, src_ptr, out_val);
             return lower_win64_va_start(va_ptr, out_val);
           } else if (triple.isAArch64()) {
             if (vk == eok_va_arg) return lower_aapcs64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_aapcs64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_aapcs64_va_copy(va_ptr, src_ptr, out_val);
             return lower_aapcs64_va_start(va_ptr, out_val);
           } else if (triple.getArch() == llvm::Triple::x86) {
             if (vk == eok_va_arg) return lower_x86_32_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_x86_32_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_x86_32_va_copy(va_ptr, src_ptr, out_val);
             return lower_x86_32_va_start(va_ptr, out_val);
           } else if (triple.isARM() || triple.isThumb()) {
             if (vk == eok_va_arg) return lower_arm32_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_arm32_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_arm32_va_copy(va_ptr, src_ptr, out_val);
             return lower_arm32_va_start(va_ptr, out_val);
           } else if (triple.isRISCV()) {
             if (vk == eok_va_arg) return lower_riscv64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_riscv64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_riscv64_va_copy(va_ptr, src_ptr, out_val);
             return lower_riscv64_va_start(va_ptr, out_val);
           } else if (triple.isLoongArch()) {
             if (vk == eok_va_arg) return lower_loongarch64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_loongarch64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_loongarch64_va_copy(va_ptr, src_ptr, out_val);
             return lower_loongarch64_va_start(va_ptr, out_val);
           } else if (triple.isMIPS()) {
             if (vk == eok_va_arg) return lower_mips_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_mips_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_mips_va_copy(va_ptr, src_ptr, out_val);
             return lower_mips_va_start(va_ptr, out_val);
           } else if (triple.isPPC()) {
             if (vk == eok_va_arg) return lower_ppc64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_ppc64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_ppc64_va_copy(va_ptr, src_ptr, out_val);
             return lower_ppc64_va_start(va_ptr, out_val);
           } else if (triple.isSystemZ()) {
             if (vk == eok_va_arg) return lower_s390x_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_s390x_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_s390x_va_copy(va_ptr, src_ptr, out_val);
             return lower_s390x_va_start(va_ptr, out_val);
           } else if (triple.isSPARC()) {
             if (vk == eok_va_arg) return lower_sparc_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_sparc_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_sparc_va_copy(va_ptr, src_ptr, out_val);
             return lower_sparc_va_start(va_ptr, out_val);
           } else if (triple.isWasm()) {
             if (vk == eok_va_arg) return lower_wasm_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_wasm_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_wasm_va_copy(va_ptr, src_ptr, out_val);
             return lower_wasm_va_start(va_ptr, out_val);
           }
           if (vk == eok_va_arg) return lower_sysv_va_arg(va_ptr, expr->type, out_val);
           if (vk == eok_va_end) return lower_sysv_va_end(va_ptr, out_val);
           if (vk == eok_va_copy) return lower_sysv_va_copy(va_ptr, src_ptr, out_val);
           return lower_sysv_va_start(va_ptr, out_val);
         }
         case eok_real_part:
         case eok_imag_part: {
           bool is_imag = (expr->variant.operation.kind == eok_imag_part);
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (v1->getType()->isStructTy()) {
               *out_val = be_state->builder->CreateExtractValue(v1, is_imag ? 1 : 0);
           } else {
               *out_val = is_imag ? llvm::Constant::getNullValue(v1->getType()) : v1;
           }
           return llvm_gen_be_error_t::ok;
         }
         case eok_array_to_pointer: {
           return llvm_lower_lvalue_expression(expr->variant.operation.operands, out_val);
         }
         case eok_address_of: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           if (op1->is_lvalue ||
               (op1->kind == enk_operation &&
                (op1->variant.operation.kind == eok_lvalue_adjust ||
                 op1->variant.operation.kind == eok_question))) {
             return llvm_lower_lvalue_expression(op1, out_val);
           }
           return llvm_lower_expression(op1, out_val);
         }
         case eok_question:
         case eok_vector_question: {
           an_expr_node_ptr cond_expr = expr->variant.operation.operands;
           an_expr_node_ptr true_expr = cond_expr->next;
           an_expr_node_ptr false_expr = true_expr->next;
           
           llvm::Value* cond = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(cond_expr, &cond);
           if (err != llvm_gen_be_error_t::ok) return err;

           if (cond->getType()->isVectorTy()) {
               llvm::Value* tv = nullptr;
               llvm::Value* fv = nullptr;
               LLVM_GEN_BE_CHECK(lower_binary_operands(true_expr, &tv, &fv));
               llvm::Value* vcond = be_state->builder->CreateICmpNE(
                   cond, llvm::Constant::getNullValue(cond->getType()));
               *out_val = be_state->builder->CreateSelect(vcond, tv, fv);
               return llvm_gen_be_error_t::ok;
           }
           
           if (!cond->getType()->isIntegerTy(1)) {
               cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
           }
           
           llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
           llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(*be_state->context, "cond.true", func);
           llvm::BasicBlock* false_bb = llvm::BasicBlock::Create(*be_state->context, "cond.false");
           llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "cond.end");
           
           be_state->builder->CreateCondBr(cond, true_bb, false_bb);
           
           be_state->builder->SetInsertPoint(true_bb);
           llvm::Value* true_val = nullptr;
           err = llvm_lower_expression(true_expr, &true_val);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::BasicBlock* true_end_bb = be_state->builder->GetInsertBlock();
           bool true_terminated = (EDG_LLVM_GET_TERMINATOR(true_end_bb) != nullptr);
           if (!true_terminated) {
             be_state->builder->CreateBr(end_bb);
           }
           
           func->insert(func->end(), false_bb);
           be_state->builder->SetInsertPoint(false_bb);
           llvm::Value* false_val = nullptr;
           err = llvm_lower_expression(false_expr, &false_val);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::BasicBlock* false_end_bb = be_state->builder->GetInsertBlock();
           bool false_terminated = (EDG_LLVM_GET_TERMINATOR(false_end_bb) != nullptr);
           if (!false_terminated) {
             be_state->builder->CreateBr(end_bb);
           }
           
           func->insert(func->end(), end_bb);
           be_state->builder->SetInsertPoint(end_bb);
           
           if (expr->type->kind != tk_void) {
             llvm::Type* expr_llvm_ty = nullptr;
             err = get_llvm_type(expr->type, &expr_llvm_ty);
             if (err != llvm_gen_be_error_t::ok) return err;
             llvm::Type* phi_ty = !true_terminated ? true_val->getType() : expr_llvm_ty;
             llvm::PHINode* phi = be_state->builder->CreatePHI(phi_ty, 2, "cond.phi");
             if (!true_terminated) {
               phi->addIncoming(true_val, true_end_bb);
             }
             if (!false_terminated) {
               phi->addIncoming(false_val, false_end_bb);
             }
             *out_val = phi;
           } else {
             *out_val = nullptr;
           }
           return llvm_gen_be_error_t::ok;
         }
         case eok_assign: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           if (op1->kind == enk_operation &&
               op1->variant.operation.kind == eok_vector_subscript) {
             an_expr_node_ptr sub_base = op1->variant.operation.operands;
             llvm::Value* vec_ptr = nullptr;
             LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(sub_base, &vec_ptr));
             llvm::Value* idx_val = nullptr;
             LLVM_GEN_BE_CHECK(llvm_lower_expression(sub_base->next, &idx_val));
             llvm::Value* rhs_val = nullptr;
             LLVM_GEN_BE_CHECK(llvm_lower_expression(op1->next, &rhs_val));
             llvm::Type* vec_ty = nullptr;
             LLVM_GEN_BE_CHECK(get_llvm_type(sub_base->type, &vec_ty));
             llvm::Value* old_vec = be_state->builder->CreateLoad(vec_ty, vec_ptr);
             llvm::Value* new_vec = be_state->builder->CreateInsertElement(old_vec, rhs_val, idx_val);
             be_state->builder->CreateStore(new_vec, vec_ptr);
             *out_val = rhs_val;
             return llvm_gen_be_error_t::ok;
           }
           llvm::Value* v1 = nullptr;
           llvm::Value* v2 = nullptr;
           llvm_gen_be_error_t err = lower_binary_operands(op1, &v1, &v2);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           if (v2) {
             a_field_ptr bf_field = nullptr;
             LLVM_GEN_BE_ASSERT_OK(extract_bitfield_target(op1, &bf_field));
             if (bf_field) {
                 LLVM_GEN_BE_ASSERT_OK(llvm_emit_bitfield_store(v1, v2, bf_field, op1->type));
             } else {
                 llvm::Type* ty = nullptr;
                 err = get_llvm_type(expr->type, &ty);
                 if (err != llvm_gen_be_error_t::ok) return err;
                 a_type_qualifier_set lhs_quals = f_get_type_qualifiers(op1->type, TRUE);
                 if ((lhs_quals & TQ_C11_ATOMIC) != 0) {
                     uint64_t size = be_state->module->getDataLayout().getTypeStoreSize(ty);
                     if ((ty->isIntegerTy() || ty->isPointerTy()) &&
                         (size == 1 || size == 2 || size == 4 || size == 8 || size == 16)) {
                         llvm::StoreInst* st = be_state->builder->CreateStore(v2, v1);
                         st->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);
                         st->setAlignment(llvm::Align(size));
                         if ((lhs_quals & TQ_VOLATILE) != 0) st->setVolatile(true);
                     } else {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::AllocaInst* tmp = be_state->builder->CreateAlloca(ty, nullptr, "atomic.store.tmp");
                         be_state->builder->CreateStore(v2, tmp);
                         llvm::FunctionType* fty = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("__atomic_store", fty);
                         be_state->builder->CreateCall(
                             fn, {llvm::ConstantInt::get(i64_ty, size), v1, tmp, llvm::ConstantInt::get(i32_ty, 5)});
                     }
                 } else if (be_state->module->getDataLayout().getTypeStoreSize(ty) > 16) {
                     if (llvm::LoadInst* li = llvm::dyn_cast<llvm::LoadInst>(v2)) {
                         llvm::Value* src_ptr = li->getPointerOperand();
                         uint64_t size = be_state->module->getDataLayout().getTypeStoreSize(ty);
                         llvm::Align align = be_state->module->getDataLayout().getABITypeAlign(ty);
                         llvm::CallInst* mc = be_state->builder->CreateMemCpy(v1, align, src_ptr, align, size);
                         if (be_state->opt_level != llvm_opt_level_t::O0) {
                             llvm::MDNode* tbaa_st = nullptr;
                             LLVM_GEN_BE_ASSERT_OK(llvm_get_tbaa_struct_node(expr->type, &tbaa_st));
                             mc->setMetadata(llvm::LLVMContext::MD_tbaa_struct, tbaa_st);
                         }
                     } else {
                         be_state->builder->CreateStore(v2, v1);
                     }
                 } else {
                     llvm::StoreInst* st = be_state->builder->CreateStore(v2, v1);
                     if (be_state->opt_level != llvm_opt_level_t::O0 &&
                         (ty->isIntegerTy() || ty->isFloatingPointTy() || ty->isPointerTy())) {
                         llvm::MDNode* tbaa_tag = nullptr;
                         LLVM_GEN_BE_ASSERT_OK(llvm_get_tbaa_access_tag(expr->type, expr->type, 0, &tbaa_tag));
                         st->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_tag);
                     }
                 }
             }

             if (be_state->dbg_state && op1->kind == enk_variable) {
                 a_variable_ptr var = op1->variant.variable.ptr;
                 err = emit_dbg_value_for_variable(be_state->dbg_state, var, v2, nullptr, be_state->builder->GetInsertBlock());
                 if (err != llvm_gen_be_error_t::ok) return err;
             }

             *out_val = v2;
             return llvm_gen_be_error_t::ok;
           }
           return llvm_gen_be_error_t::unsupported_expr;
         }
         default:
           return llvm_gen_be_error_t::unsupported_expr;
       }
    }
    
    case enk_await:
    case enk_yield:
      return llvm_lower_coro_suspend(expr, out_val);

    case enk_builtin_operation:
      return llvm_lower_builtin_operation(expr, out_val);

    case enk_builtin_choose_expr:
      return llvm_lower_builtin_choose_expr(expr, out_val);

    case enk_condition: {
      a_condition_supplement_ptr cond_supp = expr->variant.condition;
      if (!cond_supp) return llvm_gen_be_error_t::invalid_argument;
      if (cond_supp->initialization) {
        LLVM_GEN_BE_CHECK(llvm_lower_statement(cond_supp->initialization));
      }
      if (cond_supp->dynamic_init) {
        a_variable_ptr cvar = cond_supp->dynamic_init->variable;
        llvm::Type* cvar_ty = nullptr;
        LLVM_GEN_BE_CHECK(get_llvm_type(cvar->type, &cvar_ty));
        llvm::Value* cvar_ptr = nullptr;
        auto it = be_state->local_vars.find(cvar);
        if (it != be_state->local_vars.end()) {
          cvar_ptr = it->second;
        } else {
          llvm::Function* parent_func = be_state->builder->GetInsertBlock()->getParent();
          llvm::IRBuilder<> entry_b(&parent_func->getEntryBlock(), parent_func->getEntryBlock().begin());
          cvar_ptr = entry_b.CreateAlloca(cvar_ty, nullptr, "cond.var");
          be_state->local_vars[cvar] = cvar_ptr;
        }
        LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(cond_supp->dynamic_init, cvar_ptr, cvar_ty));
      }
      return llvm_lower_expression(cond_supp->expr, out_val);
    }

    case enk_field: {
      if (!expr->variant.field.ptr) return llvm_gen_be_error_t::invalid_argument;
      *out_val = llvm::ConstantInt::get(
          llvm::Type::getInt64Ty(*be_state->context), expr->variant.field.ptr->offset);
      return llvm_gen_be_error_t::ok;
    }

    case enk_type_operand: {
      *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
      return llvm_gen_be_error_t::ok;
    }

    case enk_sizeof:
    case enk_alignof:
    case enk_datasizeof: {
      llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
      a_type_ptr target_ty = expr->variant.sizeof_info.is_type
                                 ? expr->variant.sizeof_info.variant.type
                                 : (expr->variant.sizeof_info.variant.expr
                                        ? expr->variant.sizeof_info.variant.expr->type
                                        : nullptr);
      if (expr->kind == enk_alignof) {
        uint64_t al = target_ty ? EDG_ALIGN_OF_TYPE(target_ty) : 1;
        *out_val = llvm::ConstantInt::get(i64_ty, al);
        return llvm_gen_be_error_t::ok;
      }
      if (target_ty) {
        a_type_ptr ut = skip_typerefs(target_ty);
        if (be_state->array_to_vla_dim.count(ut)) {
          llvm::Value* total_sz = llvm::ConstantInt::get(i64_ty, 1);
          a_type_ptr cur = ut;
          while (cur->kind == tk_array) {
            if (be_state->array_to_vla_dim.count(cur)) {
              a_vla_dimension_ptr dim = be_state->array_to_vla_dim[cur];
              if (be_state->vla_dimension_values.count(dim)) {
                llvm::Value* dval = be_state->builder->CreateZExtOrTrunc(
                    be_state->vla_dimension_values[dim], i64_ty);
                total_sz = be_state->builder->CreateMul(total_sz, dval);
              }
            } else if (cur->variant.array.variant.number_of_elements > 0) {
              total_sz = be_state->builder->CreateMul(
                  total_sz, llvm::ConstantInt::get(i64_ty, cur->variant.array.variant.number_of_elements));
            }
            cur = skip_typerefs(cur->variant.array.element_type);
          }
          if (cur->size > 1) {
            total_sz = be_state->builder->CreateMul(total_sz, llvm::ConstantInt::get(i64_ty, cur->size));
          }
          *out_val = total_sz;
          return llvm_gen_be_error_t::ok;
        }
        *out_val = llvm::ConstantInt::get(i64_ty, ut->size);
        return llvm_gen_be_error_t::ok;
      }
      *out_val = llvm::ConstantInt::get(i64_ty, 0);
      return llvm_gen_be_error_t::ok;
    }

    case enk_sizeof_pack:
      return llvm_lower_sizeof_pack(expr, out_val);

    case enk_initializer: {
      a_dynamic_init_ptr dip = expr->variant.initializer.dyn_init;
      if (!dip) return llvm_gen_be_error_t::invalid_argument;
      llvm::Type* expr_ty = nullptr;
      LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &expr_ty));
      llvm::AllocaInst* tmp_alloca = be_state->builder->CreateAlloca(expr_ty, nullptr, "init.tmp");
      LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(dip, tmp_alloca, expr_ty));
      *out_val = be_state->builder->CreateLoad(expr_ty, tmp_alloca);
      return llvm_gen_be_error_t::ok;
    }

    case enk_statement: {
      llvm::Type* res_ty = nullptr;
      llvm_gen_be_error_t err_ty = get_llvm_type(expr->type, &res_ty);
      if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
      
      llvm::Value* prev_ptr = be_state->current_stmt_expr_result_ptr;
      llvm::Value* result_ptr = nullptr;
      
      if (!res_ty->isVoidTy()) {
          result_ptr = be_state->builder->CreateAlloca(res_ty, nullptr, "stmt_expr_res");
      }
      
      be_state->current_stmt_expr_result_ptr = result_ptr;
      be_state->stmt_expr_cleanup_depths.push_back(be_state->cleanup_stack.cleanups.size());
      llvm_gen_be_error_t err = llvm_lower_statement(expr->variant.statement);
      be_state->stmt_expr_cleanup_depths.pop_back();
      be_state->current_stmt_expr_result_ptr = prev_ptr;
      
      if (err != llvm_gen_be_error_t::ok) return err;
      
      if (result_ptr) {
          *out_val = be_state->builder->CreateLoad(res_ty, result_ptr);
      } else {
          *out_val = nullptr;
      }
      return llvm_gen_be_error_t::ok;
    }

    case enk_throw: {
       a_throw_supplement_ptr throw_info = expr->variant.throw_info;
       llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
       llvm::Triple triple(be_state->module->getTargetTriple());
       bool is_msvc = triple.isWindowsMSVCEnvironment();

       if (!throw_info) {
         // Rethrow
         if (is_msvc) {
             llvm::FunctionCallee rethrow_fn = be_state->module->getOrInsertFunction("_CxxThrowException",
                 llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty}, false));
             llvm::Constant* null_p = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
             be_state->builder->CreateCall(rethrow_fn, {null_p, null_p});
         } else {
             llvm::FunctionCallee rethrow_fn = be_state->module->getOrInsertFunction("__cxa_rethrow",
                 llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
             if (!be_state->current_landing_pads.empty()) {
                 llvm::BasicBlock* lpad_bb = be_state->current_landing_pads.back();
                 llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
                 llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
                 be_state->builder->CreateInvoke(rethrow_fn, normal_bb, lpad_bb);
                 be_state->builder->SetInsertPoint(normal_bb);
             } else {
                 be_state->builder->CreateCall(rethrow_fn);
             }
         }
         be_state->builder->CreateUnreachable();
         *out_val = nullptr;
         return llvm_gen_be_error_t::ok;
       }

       llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
       llvm::FunctionCallee alloc_fn = be_state->module->getOrInsertFunction(
           is_msvc ? "malloc" : "__cxa_allocate_exception",
           llvm::FunctionType::get(int8_ptr_ty, {size_t_ty}, false));
       uint32_t type_size = throw_info->type->size;
       if (type_size == 0) type_size = 1;
       llvm::Value* size_val = llvm::ConstantInt::get(size_t_ty, type_size);
       llvm::Value* exc_mem = be_state->builder->CreateCall(alloc_fn, {size_val});

       llvm::Constant* dtor_const = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
       if (throw_info->dynamic_init) {
           llvm::Type* exc_ty = nullptr;
           llvm_gen_be_error_t err_ty = get_llvm_type(throw_info->type, &exc_ty);
           if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
           llvm_gen_be_error_t err = llvm_emit_dynamic_init(throw_info->dynamic_init, exc_mem, exc_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::Function* dtor_fn = nullptr;
           LLVM_GEN_BE_ASSERT_OK(get_or_create_void_ptr_fn(throw_info->dynamic_init->destructor, &dtor_fn));
           if (dtor_fn) dtor_const = dtor_fn;
       }

       llvm::Constant* type_desc_const = nullptr;
       LLVM_GEN_BE_ASSERT_OK(get_typeinfo_global(throw_info->type, &type_desc_const));

       llvm::FunctionCallee throw_fn;
       std::vector<llvm::Value*> throw_args;
       if (is_msvc) {
           throw_fn = be_state->module->getOrInsertFunction("_CxxThrowException",
               llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty}, false));
           llvm::Constant* ti_gv = nullptr;
           LLVM_GEN_BE_ASSERT_OK(llvm_get_or_create_msvc_throw_info(throw_info->type, dtor_const, &ti_gv));
           throw_args = {exc_mem, ti_gv};
       } else {
           throw_fn = be_state->module->getOrInsertFunction("__cxa_throw",
               llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty, int8_ptr_ty}, false));
           throw_args = {exc_mem, type_desc_const, dtor_const};
       }

       llvm::BasicBlock* lpad_bb = nullptr;
       LLVM_GEN_BE_ASSERT_OK(llvm_get_or_create_current_unwind_dest(&lpad_bb));
       if (lpad_bb) {
           llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
           llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
           be_state->builder->CreateInvoke(throw_fn, normal_bb, lpad_bb, throw_args);
           be_state->builder->SetInsertPoint(normal_bb);
       } else {
           be_state->builder->CreateCall(throw_fn, throw_args);
       }
       be_state->builder->CreateUnreachable();
       *out_val = nullptr;
       return llvm_gen_be_error_t::ok;
    }
    case enk_temp_init:
    case enk_lambda: {
       llvm::Value* temp_storage = nullptr;
       LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(expr, &temp_storage));
       if (expr->is_lvalue) {
           *out_val = temp_storage;
       } else {
           llvm::Type* expr_llvm_ty = nullptr;
           LLVM_GEN_BE_ASSERT_OK(get_llvm_type(expr->type, &expr_llvm_ty));
           *out_val = be_state->builder->CreateLoad(expr_llvm_ty, temp_storage);
       }
       return llvm_gen_be_error_t::ok;
    }
    case enk_object_lifetime: {
       llvm::Value* result_val = nullptr;
       an_expr_node_ptr inner_expr = expr->variant.object_lifetime.expr;
       
       size_t initial_cleanups = be_state->cleanup_stack.cleanups.size();
       
       if (inner_expr) {
           llvm_gen_be_error_t err = llvm_lower_expression(inner_expr, &result_val);
           if (err != llvm_gen_be_error_t::ok) return err;
       }
       
       an_object_lifetime_ptr lifetime = expr->variant.object_lifetime.ptr;
       if (lifetime) {
           for (a_dynamic_init_ptr dtor_init = lifetime->destructions; dtor_init; dtor_init = dtor_init->next_in_destruction_list) {
               llvm::Function* dtor_func = nullptr;
               LLVM_GEN_BE_ASSERT_OK(get_or_create_void_ptr_fn(dtor_init->destructor, &dtor_func));
               if (dtor_func) {
                   auto it = be_state->reused_values.find(dtor_init);
                   llvm::Value* obj_ptr = (it != be_state->reused_values.end())
                                              ? it->second
                                              : be_state->local_vars[dtor_init->variable];
                   be_state->builder->CreateCall(dtor_func, {obj_ptr});
               }
           }
       }
       
       while (be_state->cleanup_stack.cleanups.size() > initial_cleanups) {
           ScopeCleanup cleanup = be_state->cleanup_stack.cleanups.back();
           be_state->cleanup_stack.cleanups.pop_back();
           llvm::Function* dtor_func = nullptr;
           LLVM_GEN_BE_ASSERT_OK(get_or_create_void_ptr_fn(cleanup.dtor_routine, &dtor_func));
           be_state->builder->CreateCall(dtor_func, {cleanup.object_ptr});
       }
       
       *out_val = result_val;
       return llvm_gen_be_error_t::ok;
    }
    case enk_typeid:
      return llvm_lower_typeid(expr, out_val);
    case enk_new_delete: {
       a_new_delete_supplement_ptr nd = expr->variant.new_delete;
       if (!nd) return llvm_gen_be_error_t::invalid_argument;
       if (nd->is_new) {
         return llvm_lower_new_expression(expr, out_val);
       }
       return llvm_lower_delete_expression(expr, out_val);
    }
    default:
       return llvm_gen_be_error_t::unsupported_expr;
  }
}

/**
 * @brief Lowers a compound assignment (`+=`, `-=`, `*=`, etc.) or increment/decrement (`++`, `--`) expression.
 * @details Evaluates the target lvalue address once, loads the current value (supporting volatile and bitfields),
 *          computes the updated value, stores it back, and returns either the pre- or post-update value.
 * @param[in] expr Pointer to the EDG compound assignment or increment/decrement expression node.
 * @param[out] out_val Receives the resulting `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_compound_assignment(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  *out_val = nullptr;

  if (expr->kind != enk_operation) return llvm_gen_be_error_t::invalid_argument;

  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1->next;

  llvm::Value* lhs_ptr = nullptr;
  llvm::Value* vec_elem_idx = nullptr;
  llvm::Type* vec_container_ty = nullptr;
  llvm::Value* loaded_val = nullptr;
  a_field_ptr bitfield_ptr = nullptr;
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

  if (op1->kind == enk_operation &&
      op1->variant.operation.kind == eok_vector_subscript) {
      an_expr_node_ptr vbase = op1->variant.operation.operands;
      LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(vbase, &lhs_ptr));
      LLVM_GEN_BE_CHECK(llvm_lower_expression(vbase->next, &vec_elem_idx));
      LLVM_GEN_BE_CHECK(get_llvm_type(vbase->type, &vec_container_ty));
      llvm::Value* full_vec = be_state->builder->CreateLoad(vec_container_ty, lhs_ptr);
      loaded_val = be_state->builder->CreateExtractElement(full_vec, vec_elem_idx);
  } else {
      err = llvm_lower_lvalue_expression(op1, &lhs_ptr);
      if (err != llvm_gen_be_error_t::ok) return err;

      LLVM_GEN_BE_ASSERT_OK(extract_bitfield_target(op1, &bitfield_ptr));

      if (bitfield_ptr) {
          err = llvm_emit_bitfield_load(lhs_ptr, bitfield_ptr, op1->type, &loaded_val);
      } else {
          err = load_from_lvalue_ptr(lhs_ptr, op1->type, &loaded_val);
      }
      if (err != llvm_gen_be_error_t::ok) return err;
  }

  llvm::Value* rhs_val = nullptr;
  if (op2) {
      err = llvm_lower_expression(op2, &rhs_val);
      if (err != llvm_gen_be_error_t::ok) return err;
  } else if (loaded_val->getType()->isPointerTy()) {
      rhs_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 1);
  } else {
      // pre/post incr/decr implicitly 1
      rhs_val = llvm::ConstantInt::get(loaded_val->getType(), 1);
  }

  a_type_qualifier_set lhs_quals = f_get_type_qualifiers(op1->type, TRUE);
  if ((lhs_quals & TQ_C11_ATOMIC) != 0) {
    an_expr_operator_kind k = expr->variant.operation.kind;
    bool is_fp = loaded_val->getType()->isFloatingPointTy();
    bool has_atomic_rmw = false;
    llvm::AtomicRMWInst::BinOp rmw_op = llvm::AtomicRMWInst::Add;
    if (k == eok_pre_incr || k == eok_post_incr || k == eok_add_assign) {
      rmw_op = is_fp ? llvm::AtomicRMWInst::FAdd : llvm::AtomicRMWInst::Add;
      has_atomic_rmw = true;
    } else if (k == eok_pre_decr || k == eok_post_decr || k == eok_subtract_assign) {
      rmw_op = is_fp ? llvm::AtomicRMWInst::FSub : llvm::AtomicRMWInst::Sub;
      has_atomic_rmw = true;
    } else if (k == eok_and_assign) {
      rmw_op = llvm::AtomicRMWInst::And;
      has_atomic_rmw = true;
    } else if (k == eok_or_assign) {
      rmw_op = llvm::AtomicRMWInst::Or;
      has_atomic_rmw = true;
    } else if (k == eok_xor_assign) {
      rmw_op = llvm::AtomicRMWInst::Xor;
      has_atomic_rmw = true;
    }
    if (has_atomic_rmw) {
      llvm::Value* rmw_rhs = (is_fp && !op2) ? llvm::ConstantFP::get(loaded_val->getType(), 1.0) : rhs_val;
      llvm::AtomicRMWInst* old_v = be_state->builder->CreateAtomicRMW(
          rmw_op, lhs_ptr, rmw_rhs, llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent);
      if (k == eok_post_incr || k == eok_post_decr) {
        *out_val = old_v;
      } else if (rmw_op == llvm::AtomicRMWInst::Add) {
        *out_val = be_state->builder->CreateAdd(old_v, rmw_rhs);
      } else if (rmw_op == llvm::AtomicRMWInst::FAdd) {
        *out_val = be_state->builder->CreateFAdd(old_v, rmw_rhs);
      } else if (rmw_op == llvm::AtomicRMWInst::Sub) {
        *out_val = be_state->builder->CreateSub(old_v, rmw_rhs);
      } else if (rmw_op == llvm::AtomicRMWInst::FSub) {
        *out_val = be_state->builder->CreateFSub(old_v, rmw_rhs);
      } else if (rmw_op == llvm::AtomicRMWInst::And) {
        *out_val = be_state->builder->CreateAnd(old_v, rmw_rhs);
      } else if (rmw_op == llvm::AtomicRMWInst::Or) {
        *out_val = be_state->builder->CreateOr(old_v, rmw_rhs);
      } else {
        *out_val = be_state->builder->CreateXor(old_v, rmw_rhs);
      }
      return llvm_gen_be_error_t::ok;
    }
  }

  llvm::Value* result_val = nullptr;
  bool is_complex = loaded_val->getType()->isStructTy();
  bool is_float = loaded_val->getType()->isFPOrFPVectorTy();
  bool is_ptr = loaded_val->getType()->isPointerTy();
  a_type_ptr raw_op1_ty = skip_typerefs(op1->type);
  bool is_signed = raw_op1_ty->kind == tk_integer ? int_kind_is_signed[raw_op1_ty->variant.integer.int_kind] : false;
  if (is_complex && !rhs_val->getType()->isStructTy()) {
      llvm::Type* c_elem_ty = loaded_val->getType()->getStructElementType(0);
      llvm::Value* r_cast = rhs_val->getType()->isFloatingPointTy()
                                ? be_state->builder->CreateFPCast(rhs_val, c_elem_ty)
                                : be_state->builder->CreateSIToFP(rhs_val, c_elem_ty);
      llvm::Value* c_tmp = be_state->builder->CreateInsertValue(
          llvm::UndefValue::get(loaded_val->getType()), r_cast, 0);
      rhs_val = be_state->builder->CreateInsertValue(
          c_tmp, llvm::Constant::getNullValue(c_elem_ty), 1);
  } else if (loaded_val->getType()->isIntOrIntVectorTy() &&
             rhs_val->getType() != loaded_val->getType()) {
      bool is_shift = (expr->variant.operation.kind == eok_shiftl_assign ||
                       expr->variant.operation.kind == eok_shiftr_assign);
      rhs_val = be_state->builder->CreateIntCast(
          rhs_val, loaded_val->getType(), !is_shift && is_signed);
  }
  
  switch (expr->variant.operation.kind) {
      case eok_pre_incr:
      case eok_post_incr:
      case eok_add_assign:
      case eok_padd_assign:
          if (is_complex) {
              llvm::Value* r = be_state->builder->CreateFAdd(
                  be_state->builder->CreateExtractValue(loaded_val, 0),
                  be_state->builder->CreateExtractValue(rhs_val, 0));
              llvm::Value* i = be_state->builder->CreateFAdd(
                  be_state->builder->CreateExtractValue(loaded_val, 1),
                  be_state->builder->CreateExtractValue(rhs_val, 1));
              result_val = be_state->builder->CreateInsertValue(
                  be_state->builder->CreateInsertValue(llvm::UndefValue::get(loaded_val->getType()), r, 0),
                  i, 1);
          } else if (is_float) result_val = be_state->builder->CreateFAdd(loaded_val, rhs_val);
          else if (is_ptr) {
              llvm::Type* elem_ty = llvm::Type::getInt8Ty(*be_state->context);
              if (raw_op1_ty->kind == tk_pointer) {
                LLVM_GEN_BE_ASSERT_OK(get_llvm_type(raw_op1_ty->variant.pointer.type, &elem_ty));
              }
              result_val = be_state->builder->CreateInBoundsGEP(elem_ty, loaded_val, rhs_val);
          } else {
              result_val = be_state->builder->CreateAdd(loaded_val, rhs_val);
          }
          break;
      case eok_pre_decr:
      case eok_post_decr:
      case eok_subtract_assign:
      case eok_psubtract_assign:
          if (is_complex) {
              llvm::Value* r = be_state->builder->CreateFSub(
                  be_state->builder->CreateExtractValue(loaded_val, 0),
                  be_state->builder->CreateExtractValue(rhs_val, 0));
              llvm::Value* i = be_state->builder->CreateFSub(
                  be_state->builder->CreateExtractValue(loaded_val, 1),
                  be_state->builder->CreateExtractValue(rhs_val, 1));
              result_val = be_state->builder->CreateInsertValue(
                  be_state->builder->CreateInsertValue(llvm::UndefValue::get(loaded_val->getType()), r, 0),
                  i, 1);
          } else if (is_float) result_val = be_state->builder->CreateFSub(loaded_val, rhs_val);
          else if (is_ptr) {
              llvm::Type* elem_ty = llvm::Type::getInt8Ty(*be_state->context);
              if (raw_op1_ty->kind == tk_pointer) {
                LLVM_GEN_BE_ASSERT_OK(get_llvm_type(raw_op1_ty->variant.pointer.type, &elem_ty));
              }
              result_val = be_state->builder->CreateInBoundsGEP(elem_ty, loaded_val, be_state->builder->CreateNeg(rhs_val));
          } else {
              result_val = be_state->builder->CreateSub(loaded_val, rhs_val);
          }
          break;
      case eok_multiply_assign:
          if (is_complex) {
              LLVM_GEN_BE_ASSERT_OK(emit_complex_libcall("__mulsc3", "__muldc3", loaded_val, rhs_val, &result_val));
          } else if (is_float) result_val = be_state->builder->CreateFMul(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateMul(loaded_val, rhs_val);
          break;
      case eok_divide_assign:
          if (is_complex) {
              LLVM_GEN_BE_ASSERT_OK(emit_complex_libcall("__divsc3", "__divdc3", loaded_val, rhs_val, &result_val));
          } else if (is_float) result_val = be_state->builder->CreateFDiv(loaded_val, rhs_val);
          else if (is_signed) result_val = be_state->builder->CreateSDiv(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateUDiv(loaded_val, rhs_val);
          break;
      case eok_remainder_assign:
          if (is_float) result_val = be_state->builder->CreateFRem(loaded_val, rhs_val);
          else if (is_signed) result_val = be_state->builder->CreateSRem(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateURem(loaded_val, rhs_val);
          break;
      case eok_and_assign:
          result_val = be_state->builder->CreateAnd(loaded_val, rhs_val);
          break;
      case eok_or_assign:
          result_val = be_state->builder->CreateOr(loaded_val, rhs_val);
          break;
      case eok_xor_assign:
          result_val = be_state->builder->CreateXor(loaded_val, rhs_val);
          break;
      case eok_shiftl_assign:
          result_val = be_state->builder->CreateShl(loaded_val, rhs_val);
          break;
      case eok_shiftr_assign:
          if (is_signed) result_val = be_state->builder->CreateAShr(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateLShr(loaded_val, rhs_val);
          break;
      case eok_bassign: {
          // Block assignment
          uint64_t size = raw_op1_ty->size;
          llvm::Align align = llvm::Align(EDG_ALIGN_OF_TYPE(op1->type));
          be_state->builder->CreateMemCpy(lhs_ptr, align, rhs_val, align, size);
          *out_val = lhs_ptr; // Yield destination pointer for bassign
          return llvm_gen_be_error_t::ok;
      }
      default:
          return llvm_gen_be_error_t::unsupported_expr;
  }

  if (vec_container_ty) {
      llvm::Value* full_vec = be_state->builder->CreateLoad(vec_container_ty, lhs_ptr);
      llvm::Value* updated_vec = be_state->builder->CreateInsertElement(full_vec, result_val, vec_elem_idx);
      be_state->builder->CreateStore(updated_vec, lhs_ptr);
  } else if (bitfield_ptr) {
      LLVM_GEN_BE_ASSERT_OK(llvm_emit_bitfield_store(lhs_ptr, result_val, bitfield_ptr, op1->type));
  } else {
      llvm::StoreInst* store = be_state->builder->CreateStore(result_val, lhs_ptr);
      bool is_vol = false;
      LLVM_GEN_BE_ASSERT_OK(check_type_is_volatile(op1->type, &is_vol));
      if (is_vol) store->setVolatile(true);
  }

  if (expr->variant.operation.kind == eok_post_incr || expr->variant.operation.kind == eok_post_decr) {
      *out_val = loaded_val;
  } else {
      *out_val = result_val;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an ISO Embedded C fixed-point binary arithmetic operation using LLVM intrinsics.
 * @details Supports saturating (`llvm.sadd.sat`, `llvm.uadd.sat`, `llvm.ssub.sat`, `llvm.usub.sat`,
 *          `llvm.smul.fix.sat`, `llvm.umul.fix.sat`) and non-saturating (`llvm.smul.fix`, `llvm.umul.fix`,
 *          `llvm.sdiv.fix`, `llvm.udiv.fix`, `add`, `sub`) fixed-point arithmetic.
 * @param[in] op_kind The EDG binary expression operator kind (`eok_add`, `eok_subtract`, `eok_multiply`, `eok_divide`).
 * @param[in] is_signed Whether the fixed-point type is signed.
 * @param[in] is_saturating Whether the fixed-point type is saturating (`_Sat`).
 * @param[in] scale Fractional bit scale factor for fixed-point multiplication/division.
 * @param[in] lhs Lowered left-hand integer representation operand.
 * @param[in] rhs Lowered right-hand integer representation operand.
 * @param[out] out_val Receives the computed fixed-point result value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_fixed_point_binop(
    an_expr_operator_kind op_kind,
    bool is_signed,
    bool is_saturating,
    unsigned scale,
    llvm::Value* lhs,
    llvm::Value* rhs,
    llvm::Value** out_val) noexcept {
  if (!lhs || !rhs || !out_val) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::Type* int_ty = lhs->getType();
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Value* scale_val = llvm::ConstantInt::get(i32_ty, scale);

  if (op_kind == eok_add) {
    if (is_saturating) {
      llvm::Intrinsic::ID id = is_signed ? llvm::Intrinsic::sadd_sat : llvm::Intrinsic::uadd_sat;
      llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), id, {int_ty});
      *out_val = be_state->builder->CreateCall(fn, {lhs, rhs});
    } else {
      *out_val = be_state->builder->CreateAdd(lhs, rhs);
    }
    return llvm_gen_be_error_t::ok;
  }
  if (op_kind == eok_subtract) {
    if (is_saturating) {
      llvm::Intrinsic::ID id = is_signed ? llvm::Intrinsic::ssub_sat : llvm::Intrinsic::usub_sat;
      llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), id, {int_ty});
      *out_val = be_state->builder->CreateCall(fn, {lhs, rhs});
    } else {
      *out_val = be_state->builder->CreateSub(lhs, rhs);
    }
    return llvm_gen_be_error_t::ok;
  }
  if (op_kind == eok_multiply) {
    llvm::Intrinsic::ID id = is_saturating
                                 ? (is_signed ? llvm::Intrinsic::smul_fix_sat : llvm::Intrinsic::umul_fix_sat)
                                 : (is_signed ? llvm::Intrinsic::smul_fix : llvm::Intrinsic::umul_fix);
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), id, {int_ty});
    *out_val = be_state->builder->CreateCall(fn, {lhs, rhs, scale_val});
    return llvm_gen_be_error_t::ok;
  }
  if (op_kind == eok_divide) {
    llvm::Intrinsic::ID id = is_signed ? llvm::Intrinsic::sdiv_fix : llvm::Intrinsic::udiv_fix;
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), id, {int_ty});
    *out_val = be_state->builder->CreateCall(fn, {lhs, rhs, scale_val});
    return llvm_gen_be_error_t::ok;
  }
  return llvm_gen_be_error_t::unsupported_expr;
}

/**
 * @brief Computes a per-byte padding mask (`0x00` = padding byte, `0xFF` = value byte, or bitmask for bitfields) for an EDG type.
 * @details Marks all non-padding field, bitfield, and base-class bytes in @p out_mask so that
 *          `__builtin_clear_padding` zeroes only padding bytes and bits.
 * @param[in] type Pointer to the EDG type to inspect.
 * @param[out] out_mask Receives a byte vector of length `type->size` where `1` bits represent active value bits and `0` bits represent padding.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_mask` is null.
 */
llvm_gen_be_error_t llvm_compute_type_padding_mask(
    a_type_ptr type,
    std::vector<uint8_t>* out_mask) noexcept {
  if (!out_mask) return llvm_gen_be_error_t::invalid_argument;
  out_mask->clear();
  if (!type) return llvm_gen_be_error_t::ok;
  a_type_ptr ut = skip_typerefs(type);
  uint64_t sz = ut->size;
  if (sz == 0) return llvm_gen_be_error_t::ok;

  if (ut->kind != tk_struct && ut->kind != tk_class) {
    out_mask->assign(sz, 0xFFu);
    return llvm_gen_be_error_t::ok;
  }

  out_mask->assign(sz, 0x00u);
  if (ut->variant.class_struct_union.extra_info) {
    for (a_base_class_ptr bc = ut->variant.class_struct_union.extra_info->base_classes;
         bc != nullptr; bc = bc->next) {
      if (bc->type) {
        std::vector<uint8_t> base_mask;
        LLVM_GEN_BE_ASSERT_OK(llvm_compute_type_padding_mask(bc->type, &base_mask));
        uint64_t b_off = static_cast<uint64_t>(bc->offset);
        for (size_t i = 0; i < base_mask.size() && (b_off + i) < sz; ++i) {
          (*out_mask)[b_off + i] |= base_mask[i];
        }
      }
    }
  }
  for (a_field_ptr f = ut->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
    uint64_t f_off = static_cast<uint64_t>(f->offset);
    if (f->is_bit_field) {
      unsigned bit_rem = static_cast<unsigned>(f->offset_bit_remainder);
      unsigned bit_sz = static_cast<unsigned>(f->bit_size);
      for (unsigned b = 0; b < bit_sz; ++b) {
        unsigned total_bit = bit_rem + b;
        uint64_t byte_idx = f_off + (total_bit / 8);
        unsigned bit_in_byte = total_bit % 8;
        if (byte_idx < sz) {
          (*out_mask)[byte_idx] |= static_cast<uint8_t>(1u << bit_in_byte);
        }
      }
    } else if (f->type) {
      std::vector<uint8_t> fld_mask;
      LLVM_GEN_BE_ASSERT_OK(llvm_compute_type_padding_mask(f->type, &fld_mask));
      for (size_t i = 0; i < fld_mask.size() && (f_off + i) < sz; ++i) {
        (*out_mask)[f_off + i] |= fld_mask[i];
      }
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++11 `sizeof...(pack)` expression (`enk_sizeof_pack`).
 * @details Extracts the resolved pack element count from the EDG expression node and returns an integer constant.
 * @param[in] expr Pointer to the EDG `enk_sizeof_pack` expression node.
 * @param[out] out_val Receives the `llvm::ConstantInt*` pack count.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_sizeof_pack(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  uint64_t pack_count = 0;
  for (an_expr_node_ptr cur = expr->next; cur != nullptr; cur = cur->next) {
    pack_count++;
  }
  *out_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), pack_count);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++17 fold expression (`enk_fold`).
 * @details Evaluates unary and binary left/right folds across arithmetic, bitwise, shift, relational,
 *          short-circuit logical (`&&`, `||`), assignment, pointer-to-member, and comma operators.
 * @param[in] expr Pointer to the EDG `enk_fold` expression node.
 * @param[out] out_val Receives the folded `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_fold_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr f_op1 = expr->variant.fold.operands;
  if (!f_op1) {
    return llvm_gen_be_error_t::unsupported_expr;
  }
  a_token_kind f_tok = expr->variant.fold.operator_token;
  llvm::Value* acc = nullptr;
  LLVM_GEN_BE_CHECK(llvm_lower_expression(f_op1, &acc));

  for (an_expr_node_ptr curr = f_op1->next; curr != nullptr; curr = curr->next) {
    if (f_tok == tok_and_and || f_tok == tok_or_or) {
      bool is_and = (f_tok == tok_and_and);
      llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(
          *be_state->context, is_and ? "fold.land.rhs" : "fold.lor.rhs", func);
      llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(
          *be_state->context, is_and ? "fold.land.end" : "fold.lor.end");
      llvm::Value* lhs_cond = be_state->builder->CreateICmpNE(
          acc, llvm::Constant::getNullValue(acc->getType()));
      llvm::BasicBlock* lhs_end_bb = be_state->builder->GetInsertBlock();
      if (is_and) {
        be_state->builder->CreateCondBr(lhs_cond, rhs_bb, end_bb);
      } else {
        be_state->builder->CreateCondBr(lhs_cond, end_bb, rhs_bb);
      }

      be_state->builder->SetInsertPoint(rhs_bb);
      llvm::Value* rhs = nullptr;
      LLVM_GEN_BE_CHECK(llvm_lower_expression(curr, &rhs));
      llvm::Value* rhs_cond = be_state->builder->CreateICmpNE(
          rhs, llvm::Constant::getNullValue(rhs->getType()));
      llvm::BasicBlock* rhs_end_bb = be_state->builder->GetInsertBlock();
      be_state->builder->CreateBr(end_bb);

      func->insert(func->end(), end_bb);
      be_state->builder->SetInsertPoint(end_bb);
      llvm::PHINode* phi = be_state->builder->CreatePHI(
          be_state->builder->getInt1Ty(), 2, is_and ? "fold.land" : "fold.lor");
      phi->addIncoming(
          is_and ? llvm::ConstantInt::getFalse(*be_state->context)
                 : llvm::ConstantInt::getTrue(*be_state->context),
          lhs_end_bb);
      phi->addIncoming(rhs_cond, rhs_end_bb);
      acc = acc->getType()->isIntegerTy()
                ? be_state->builder->CreateZExtOrTrunc(phi, acc->getType())
                : static_cast<llvm::Value*>(phi);
      continue;
    }

    llvm::Value* rhs = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(curr, &rhs));
    bool is_fp = acc->getType()->isFloatingPointTy();
    bool is_signed = (f_op1->type->kind == tk_integer)
                         ? static_cast<bool>(int_kind_is_signed[f_op1->type->variant.integer.int_kind])
                         : true;

    switch (f_tok) {
      case tok_plus:
      case tok_plus_assign:
        acc = is_fp ? be_state->builder->CreateFAdd(acc, rhs)
                    : be_state->builder->CreateAdd(acc, rhs);
        break;
      case tok_minus:
      case tok_minus_assign:
        acc = is_fp ? be_state->builder->CreateFSub(acc, rhs)
                    : be_state->builder->CreateSub(acc, rhs);
        break;
      case tok_star:
      case tok_times_assign:
        acc = is_fp ? be_state->builder->CreateFMul(acc, rhs)
                    : be_state->builder->CreateMul(acc, rhs);
        break;
      case tok_divide:
      case tok_divide_assign:
        acc = is_fp ? be_state->builder->CreateFDiv(acc, rhs)
                    : (is_signed ? be_state->builder->CreateSDiv(acc, rhs)
                                 : be_state->builder->CreateUDiv(acc, rhs));
        break;
      case tok_remainder:
      case tok_remainder_assign:
        acc = is_fp ? be_state->builder->CreateFRem(acc, rhs)
                    : (is_signed ? be_state->builder->CreateSRem(acc, rhs)
                                 : be_state->builder->CreateURem(acc, rhs));
        break;
      case tok_ampersand:
      case tok_and_assign:
        acc = be_state->builder->CreateAnd(acc, rhs);
        break;
      case tok_or:
      case tok_or_assign:
        acc = be_state->builder->CreateOr(acc, rhs);
        break;
      case tok_excl_or:
      case tok_excl_or_assign:
        acc = be_state->builder->CreateXor(acc, rhs);
        break;
      case tok_shift_left:
      case tok_shift_left_assign:
        acc = be_state->builder->CreateShl(acc, rhs);
        break;
      case tok_shift_right:
      case tok_shift_right_assign:
        acc = is_signed ? be_state->builder->CreateAShr(acc, rhs)
                        : be_state->builder->CreateLShr(acc, rhs);
        break;
      case tok_eq:
      case tok_ne:
      case tok_lt:
      case tok_le:
      case tok_gt:
      case tok_ge: {
        llvm::Value* cmp = nullptr;
        if (f_tok == tok_eq) cmp = is_fp ? be_state->builder->CreateFCmpOEQ(acc, rhs) : be_state->builder->CreateICmpEQ(acc, rhs);
        else if (f_tok == tok_ne) cmp = is_fp ? be_state->builder->CreateFCmpONE(acc, rhs) : be_state->builder->CreateICmpNE(acc, rhs);
        else if (f_tok == tok_lt) cmp = is_fp ? be_state->builder->CreateFCmpOLT(acc, rhs) : (is_signed ? be_state->builder->CreateICmpSLT(acc, rhs) : be_state->builder->CreateICmpULT(acc, rhs));
        else if (f_tok == tok_le) cmp = is_fp ? be_state->builder->CreateFCmpOLE(acc, rhs) : (is_signed ? be_state->builder->CreateICmpSLE(acc, rhs) : be_state->builder->CreateICmpULE(acc, rhs));
        else if (f_tok == tok_gt) cmp = is_fp ? be_state->builder->CreateFCmpOGT(acc, rhs) : (is_signed ? be_state->builder->CreateICmpSGT(acc, rhs) : be_state->builder->CreateICmpUGT(acc, rhs));
        else cmp = is_fp ? be_state->builder->CreateFCmpOGE(acc, rhs) : (is_signed ? be_state->builder->CreateICmpSGE(acc, rhs) : be_state->builder->CreateICmpUGE(acc, rhs));
        acc = acc->getType()->isIntegerTy()
                  ? be_state->builder->CreateZExtOrTrunc(cmp, acc->getType())
                  : cmp;
        break;
      }
      case tok_comma:
      case tok_assign:
      case tok_period_star:
      case tok_arrow_star:
        acc = rhs;
        break;
      default:
        acc = be_state->builder->CreateAdd(acc, rhs);
        break;
    }
  }
  *out_val = acc;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++ `new` or `new[]` expression (`enk_new_delete` or unlowered `eok_new`).
 * @details Computes allocation size (including array cookies and overflow checks), invokes `operator new`,
 *          initializes the storage via `llvm_emit_dynamic_init`, and registers exception cleanup.
 * @param[in] expr Pointer to the EDG `new` expression node.
 * @param[out] out_val Receives the allocated and initialized pointer `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_new_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  if (expr->kind != enk_new_delete) {
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
    llvm::Value* size_val = nullptr;
    if (op1) {
      LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &size_val));
      size_val = be_state->builder->CreateIntCast(size_val, size_t_ty, false);
    } else {
      uint64_t alloc_sz = 1;
      uint64_t alloc_align = 8;
      if (expr->type && expr->type->kind == tk_pointer && expr->type->variant.pointer.type) {
        uint64_t ptee_sz = expr->type->variant.pointer.type->size;
        if (ptee_sz > 0) alloc_sz = ptee_sz;
        if (expr->type->variant.pointer.type->alignment > 16) {
          alloc_align = static_cast<uint64_t>(expr->type->variant.pointer.type->alignment);
        }
      }
      size_val = llvm::ConstantInt::get(size_t_ty, alloc_sz);
      if (alloc_align > 16) {
        llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
        llvm::FunctionType* aligned_new_fty = llvm::FunctionType::get(
            ptr_ty, {size_t_ty, size_t_ty}, false);
        llvm::FunctionCallee aligned_new_fn = be_state->module->getOrInsertFunction(
            "_ZnwmSt11align_val_t", aligned_new_fty);
        *out_val = be_state->builder->CreateCall(
            aligned_new_fn, {size_val, llvm::ConstantInt::get(size_t_ty, alloc_align)});
        return llvm_gen_be_error_t::ok;
      }
    }
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::FunctionType* new_fty = llvm::FunctionType::get(ptr_ty, {size_t_ty}, false);
    llvm::FunctionCallee new_fn = be_state->module->getOrInsertFunction("_Znwm", new_fty);
    *out_val = be_state->builder->CreateCall(new_fn, {size_val});
    return llvm_gen_be_error_t::ok;
  }

  a_new_delete_supplement_ptr nd = expr->variant.new_delete;
  if (!nd) return llvm_gen_be_error_t::invalid_argument;
  a_routine_ptr routine = nd->routine;
  if (!routine || !routine->source_corresp.name) return llvm_gen_be_error_t::unsupported_expr;

  llvm::Type* alloc_type = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(nd->type, &alloc_type);
  if (err != llvm_gen_be_error_t::ok) return err;

  std::vector<llvm::Value*> alloc_args;
  llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Value* size_val = nullptr;
  llvm::Value* num_elems = nullptr;

  if (!nd->placement_new || llvm::StringRef(routine->source_corresp.name).starts_with("operator new")) {
    if (nd->number_of_elements) {
      err = llvm_lower_expression(nd->number_of_elements, &num_elems);
      if (err != llvm_gen_be_error_t::ok) return err;

      uint64_t type_size = be_state->module->getDataLayout().getTypeAllocSize(alloc_type);
      llvm::Value* type_size_val = llvm::ConstantInt::get(size_t_ty, type_size);

      num_elems = be_state->builder->CreateIntCast(num_elems, size_t_ty, false);
      size_val = be_state->builder->CreateMul(num_elems, type_size_val);

      if (type_has_nontrivial_destructor(nd->type)) {
        uint64_t cookie_sz = (nd->type->alignment > 8) ? static_cast<uint64_t>(nd->type->alignment) : 8ULL;
        llvm::Value* cookie_size_val = llvm::ConstantInt::get(size_t_ty, cookie_sz);
        size_val = be_state->builder->CreateAdd(size_val, cookie_size_val);
      }
    } else {
      uint64_t type_size = be_state->module->getDataLayout().getTypeAllocSize(alloc_type);
      size_val = llvm::ConstantInt::get(size_t_ty, type_size);
    }

    alloc_args.push_back(size_val);
    if (nd->type->alignment > 16 &&
        llvm::StringRef(routine->source_corresp.name).contains("St11align_val_t")) {
      alloc_args.push_back(
          llvm::ConstantInt::get(size_t_ty, static_cast<uint64_t>(nd->type->alignment)));
    }
  }

  for (an_expr_node_ptr arg = nd->arg; arg; arg = arg->next) {
    llvm::Value* arg_val = nullptr;
    err = llvm_lower_expression(arg, &arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    alloc_args.push_back(arg_val);
  }

  llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
  if (!func) {
    std::vector<llvm::Type*> alloc_arg_tys;
    for (auto* a : alloc_args) alloc_arg_tys.push_back(a->getType());
    llvm::FunctionType* expected_func_ty = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(*be_state->context), alloc_arg_tys, false);
    func = llvm::Function::Create(
        expected_func_ty,
        llvm::GlobalValue::ExternalLinkage,
        routine->source_corresp.name,
        be_state->module.get());
  }

  llvm::Value* allocated_mem = nullptr;
  llvm::BasicBlock* alloc_unwind_bb = nullptr;
  if (!func->doesNotThrow()) {
    LLVM_GEN_BE_ASSERT_OK(llvm_get_or_create_current_unwind_dest(&alloc_unwind_bb));
  }
  if (alloc_unwind_bb) {
    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* alloc_cont_bb = llvm::BasicBlock::Create(*be_state->context, "new.alloc.cont", parent_fn);
    allocated_mem = be_state->builder->CreateInvoke(func, alloc_cont_bb, alloc_unwind_bb, alloc_args);
    be_state->builder->SetInsertPoint(alloc_cont_bb);
  } else {
    allocated_mem = be_state->builder->CreateCall(func, alloc_args);
  }
  llvm::Value* user_mem = allocated_mem;

  if (nd->number_of_elements && type_has_nontrivial_destructor(nd->type)) {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    uint64_t cookie_sz = (nd->type->alignment > 8) ? static_cast<uint64_t>(nd->type->alignment) : 8ULL;
    llvm::Value* cookie_size_val = llvm::ConstantInt::get(size_t_ty, cookie_sz);
    llvm::Value* num_elems_cast = be_state->builder->CreateIntCast(num_elems, size_t_ty, false);
    user_mem = be_state->builder->CreateInBoundsGEP(i8_ty, allocated_mem, cookie_size_val);
    llvm::Value* count_slot = be_state->builder->CreateInBoundsGEP(
        i8_ty, user_mem, llvm::ConstantInt::get(size_t_ty, -8, true), "cookie.count.slot");
    be_state->builder->CreateStore(num_elems_cast, count_slot);
  }

  llvm::Type* expr_llvm_ty = nullptr;
  err = get_llvm_type(expr->type, &expr_llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Value* casted_mem = user_mem;

  if (nd->dynamic_init) {
    if (nd->freeing_of_storage_on_exception) {
      llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* normal_dest = llvm::BasicBlock::Create(*be_state->context, "new.init.cont", parent_fn);
      llvm::BasicBlock* unwind_dest = llvm::BasicBlock::Create(*be_state->context, "new.init.unwind", parent_fn);
      be_state->current_landing_pads.push_back(unwind_dest);
      err = llvm_emit_dynamic_init(nd->dynamic_init, casted_mem, alloc_type);
      be_state->current_landing_pads.pop_back();
      if (err != llvm_gen_be_error_t::ok) return err;
      be_state->builder->CreateBr(normal_dest);
      be_state->builder->SetInsertPoint(unwind_dest);
      llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
      llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
      if (!parent_fn->hasPersonalityFn()) {
        llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction(
            "__gxx_personality_v0", llvm::FunctionType::get(i32_ty, true));
        parent_fn->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));
      }
      llvm::StructType* lpad_ty = llvm::StructType::get(*be_state->context, {ptr_ty, i32_ty});
      llvm::LandingPadInst* lpad = be_state->builder->CreateLandingPad(lpad_ty, 0);
      lpad->setCleanup(true);
      llvm::Function* cleanup_func = nullptr;
      LLVM_GEN_BE_ASSERT_OK(get_or_create_void_ptr_fn(
          nd->freeing_of_storage_on_exception->destructor, &cleanup_func));
      be_state->builder->CreateCall(cleanup_func, {allocated_mem});
      be_state->builder->CreateResume(lpad);
      be_state->builder->SetInsertPoint(normal_dest);
    } else {
      err = llvm_emit_dynamic_init(nd->dynamic_init, casted_mem, alloc_type);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }

  *out_val = casted_mem;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++ `delete` or `delete[]` expression (`enk_new_delete` or unlowered `eok_delete`).
 * @details Emits a null check, runs scalar or reverse-order array element destructors (adjusting for
 *          array cookies), and invokes `operator delete`.
 * @param[in] expr Pointer to the EDG `delete` expression node.
 * @param[out] out_val Receives a null pointer `llvm::Value*` representing the `void` result.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_delete_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  if (expr->kind != enk_new_delete) {
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    if (!op1) return llvm_gen_be_error_t::invalid_argument;
    llvm::Value* ptr_to_del = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &ptr_to_del));
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::FunctionType* del_fty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), {ptr_ty}, false);
    llvm::FunctionCallee del_fn = be_state->module->getOrInsertFunction("_ZdlPv", del_fty);
    be_state->builder->CreateCall(del_fn, {ptr_to_del});
    *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }

  a_new_delete_supplement_ptr nd = expr->variant.new_delete;
  if (!nd) return llvm_gen_be_error_t::invalid_argument;

  llvm::Value* ptr_to_delete = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(nd->arg, &ptr_to_delete);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Value* base_ptr = ptr_to_delete;
  llvm::Value* array_total_bytes = nullptr;

  if (nd->array_delete && type_has_nontrivial_destructor(nd->type)) {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
    int64_t cookie_sz = (nd->type->alignment > 8) ? static_cast<int64_t>(nd->type->alignment) : 8LL;
    llvm::Value* cookie_size_val = llvm::ConstantInt::get(size_t_ty, -cookie_sz, true);
    base_ptr = be_state->builder->CreateInBoundsGEP(i8_ty, ptr_to_delete, cookie_size_val);
    llvm::Value* count_ptr = be_state->builder->CreateInBoundsGEP(
        i8_ty, ptr_to_delete, llvm::ConstantInt::get(size_t_ty, -8, true), "cookie.count.ptr");
    llvm::Value* count_val = be_state->builder->CreateLoad(size_t_ty, count_ptr, "cookie.count");
    llvm::Type* elem_llvm_ty = nullptr;
    LLVM_GEN_BE_CHECK(get_llvm_type(nd->type, &elem_llvm_ty));
    uint64_t elem_alloc_sz = be_state->module->getDataLayout().getTypeAllocSize(elem_llvm_ty);
    array_total_bytes = be_state->builder->CreateAdd(
        be_state->builder->CreateMul(count_val, llvm::ConstantInt::get(size_t_ty, elem_alloc_sz)),
        llvm::ConstantInt::get(size_t_ty, static_cast<uint64_t>(cookie_sz)));
    if (nd->dynamic_init && nd->dynamic_init->destructor) {
      llvm::Function* dtor_fn = nullptr;
      LLVM_GEN_BE_ASSERT_OK(get_or_create_void_ptr_fn(nd->dynamic_init->destructor, &dtor_fn));
      llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* pre_bb = be_state->builder->GetInsertBlock();
      llvm::BasicBlock* loop_cond_bb = llvm::BasicBlock::Create(*be_state->context, "del.arr.cond", parent_fn);
      llvm::BasicBlock* loop_body_bb = llvm::BasicBlock::Create(*be_state->context, "del.arr.body", parent_fn);
      llvm::BasicBlock* loop_end_bb = llvm::BasicBlock::Create(*be_state->context, "del.arr.end", parent_fn);
      be_state->builder->CreateBr(loop_cond_bb);

      be_state->builder->SetInsertPoint(loop_cond_bb);
      llvm::PHINode* idx_phi = be_state->builder->CreatePHI(size_t_ty, 2, "del.idx");
      idx_phi->addIncoming(count_val, pre_bb);
      llvm::Value* has_more = be_state->builder->CreateICmpUGT(
          idx_phi, llvm::ConstantInt::get(size_t_ty, 0));
      be_state->builder->CreateCondBr(has_more, loop_body_bb, loop_end_bb);

      be_state->builder->SetInsertPoint(loop_body_bb);
      llvm::Value* prev_idx = be_state->builder->CreateSub(
          idx_phi, llvm::ConstantInt::get(size_t_ty, 1), "del.prev.idx");
      idx_phi->addIncoming(prev_idx, loop_body_bb);
      llvm::Value* elem_ptr = be_state->builder->CreateInBoundsGEP(
          elem_llvm_ty, ptr_to_delete, prev_idx);
      be_state->builder->CreateCall(dtor_fn, {elem_ptr});
      be_state->builder->CreateBr(loop_cond_bb);

      be_state->builder->SetInsertPoint(loop_end_bb);
    }
  } else if (nd->dynamic_init && nd->dynamic_init->destructor) {
    llvm::Function* dtor_fn = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_void_ptr_fn(nd->dynamic_init->destructor, &dtor_fn));
    be_state->builder->CreateCall(dtor_fn, {ptr_to_delete});
  }

  a_routine_ptr routine = nd->routine;
  std::vector<llvm::Value*> dealloc_args;
  dealloc_args.push_back(base_ptr);

  for (an_expr_node_ptr extra_arg = nd->arg->next; extra_arg; extra_arg = extra_arg->next) {
    llvm::Value* extra_arg_val = nullptr;
    err = llvm_lower_expression(extra_arg, &extra_arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    dealloc_args.push_back(extra_arg_val);
  }
  llvm::StringRef del_rname(routine->source_corresp.name);
  if (dealloc_args.size() == 1) {
    llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
    if (array_total_bytes && (del_rname == "_ZdaPvm" || del_rname == "_ZdaPvmSt11align_val_t")) {
      dealloc_args.push_back(array_total_bytes);
    } else if (!nd->array_delete &&
               (del_rname == "_ZdlPvm" || del_rname == "_ZdlPvmSt11align_val_t")) {
      dealloc_args.push_back(
          llvm::ConstantInt::get(size_t_ty, static_cast<uint64_t>(nd->type->size)));
    }
    if (nd->type->alignment > 16 && del_rname.contains("St11align_val_t")) {
      dealloc_args.push_back(
          llvm::ConstantInt::get(size_t_ty, static_cast<uint64_t>(nd->type->alignment)));
    }
  }

  llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
  if (!func) {
    std::vector<llvm::Type*> dealloc_arg_tys;
    for (auto* a : dealloc_args) dealloc_arg_tys.push_back(a->getType());
    llvm::FunctionType* expected_func_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), dealloc_arg_tys, false);
    func = llvm::Function::Create(
        expected_func_ty,
        llvm::GlobalValue::ExternalLinkage,
        routine->source_corresp.name,
        be_state->module.get());
  }
  be_state->builder->CreateCall(func, dealloc_args);

  *out_val = nullptr;
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
