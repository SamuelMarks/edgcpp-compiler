/**
 * @file llvm_gen_be_type.h
 * @brief Type generation and lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end type structures (a_type_ptr) into LLVM IR
 * types (llvm::Type*). All functions return llvm_gen_be_error_t and output
 * constructed types via output pointers.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_TYPE_H
#define LLVM_GEN_BE_TYPE_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "error.h"
#include "il.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Type.h>

BEGIN_EDG_NAMESPACE

/**
 * @brief Translates an EDG type representation into a corresponding LLVM type.
 * @details Traverses the EDG type structure, looks up or populates the type cache,
 * and creates LLVM primitive, pointer, array, aggregate, or routine types.
 * @param[in] edg_type Pointer to the EDG type description structure. If null, produces void.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, llvm_gen_be_error_t::invalid_argument if
 *         out_type is null, or an error code indicating the failure reason.
 */
llvm_gen_be_error_t llvm_type_from_edg_type(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG integer type into a matching LLVM IntegerType.
 * @details Maps integer kinds (char, short, int, long, long long, __int128, bool)
 * into appropriate bit-width LLVM integer types.
 * @param[in] edg_type Pointer to the EDG integer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_integer(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Computes the fractional bit scale, signedness, and saturation flags for an EDG fixed-point type.
 * @details Maps ISO/IEC TR 18037 `_Fract` and `_Accum` (`fpp_short`, `fpp_default`, `fpp_long`,
 *          signed and unsigned, saturating and non-saturating) to their exact fractional bit count.
 * @param[in] tp Pointer to the EDG `tk_fixed_point` type structure.
 * @param[out] out_frac_bits Receives the number of fractional bits (`F`).
 * @param[out] out_is_signed Receives `true` if the fixed-point type is signed.
 * @param[out] out_is_saturating Receives `true` if the fixed-point type is saturating (`_Sat`).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null or not `tk_fixed_point`.
 */
llvm_gen_be_error_t llvm_get_fixed_point_scale(
    a_type_ptr tp,
    unsigned* out_frac_bits,
    bool* out_is_signed,
    bool* out_is_saturating) noexcept;

/**
 * @brief Lowers an EDG floating-point type into a matching LLVM floating-point Type.
 * @details Handles half/fp16, bfloat16, float, double, x86_fp80, and fp128.
 * @param[in] edg_type Pointer to the EDG floating-point type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_float(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG pointer or reference type into an LLVM opaque PointerType.
 * @details Constructs an opaque pointer using LLVM's unadorned pointer representation.
 * Supports non-default address spaces (e.g. __attribute__((address_space(N)))).
 * @param[in] edg_type Pointer to the EDG pointer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_pointer(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG array type into an LLVM ArrayType or PointerType.
 * @details Maps fixed-size arrays to llvm::ArrayType, zero-bound arrays to 0-length arrays,
 * and variable-length or incomplete arrays to opaque pointers.
 * @param[in] edg_type Pointer to the EDG array type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_array(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG vector type into an LLVM VectorType.
 * @details Translates GNU/Clang vector types into llvm::FixedVectorType.
 * @param[in] edg_type Pointer to the EDG vector type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_vector(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG struct, union, or class type into an LLVM StructType.
 * @details Generates packed structures matching EDG field byte offsets and tail padding.
 * Cyclic struct references are resolved using forward-declared opaque StructTypes.
 * @param[in] edg_type Pointer to the EDG struct, union, or class type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_struct(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG routine (function) type into an LLVM FunctionType.
 * @details Gathers return type, parameter types, and variadic ellipsis flag.
 * Dispatches to target-specific ABI handlers to perform sret and pass-by-value adjustments.
 * @param[in] edg_type Pointer to the EDG routine type structure.
 * @param[out] out_fn_type Pointer to the variable where the resulting FunctionType* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_routine(
    a_type_ptr edg_type,
    llvm::FunctionType** out_fn_type) noexcept;

/**
 * @brief Maps an EDG routine type's calling convention to an LLVM CallingConv::ID.
 * @details Examines the routine type supplement and maps conventions like cdecl, fastcall, stdcall, etc.
 * @param[in] edg_type Pointer to the EDG routine type structure.
 * @param[out] out_cc Pointer to the variable where the resulting LLVM calling convention ID is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_calling_conv_from_routine(
    a_type_ptr edg_type,
    unsigned* out_cc) noexcept;

/**
 * @brief Legacy compatibility helper redirecting to llvm_type_from_edg_type.
 * @details Convenience wrapper returning llvm::Type* while inspecting error codes internally.
 * @param[in] edg_type Pointer to the EDG type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t get_llvm_type(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Constructs or retrieves the LLVM TBAA type descriptor `MDNode` for an EDG type.
 * @details Creates the `"Simple C/C++ TBAA"` root, `"omnipotent char"` node, scalar type nodes,
 *          and struct path descriptor nodes using `llvm::MDBuilder`.
 * @param[in] type Pointer to the EDG type structure.
 * @param[out] out_node Receives the constructed `llvm::MDNode*` type descriptor.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_node` is null.
 */
llvm_gen_be_error_t llvm_get_tbaa_type_node(
    a_type_ptr type,
    llvm::MDNode** out_node) noexcept;

/**
 * @brief Constructs an LLVM `!tbaa` access tag `MDNode` for a memory load or store.
 * @details Combines the base type node, scalar access type node, and byte offset via `MDBuilder::createTBAAAccessTag`.
 * @param[in] base_type Pointer to the base EDG type (struct or scalar).
 * @param[in] access_type Pointer to the accessed scalar/pointer EDG type.
 * @param[in] offset Byte offset of the access within `base_type`.
 * @param[out] out_tag Receives the constructed `!tbaa` access tag `llvm::MDNode*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_tag` is null.
 */
llvm_gen_be_error_t llvm_get_tbaa_access_tag(
    a_type_ptr base_type,
    a_type_ptr access_type,
    uint64_t offset,
    llvm::MDNode** out_tag) noexcept;

/**
 * @brief Constructs an LLVM `!tbaa.struct` metadata node for aggregate memory copy operations.
 * @details Builds field offset/size/tag triples for all non-bitfield fields of `struct_type`.
 * @param[in] struct_type Pointer to the EDG struct/class type.
 * @param[out] out_node Receives the constructed `!tbaa.struct` `llvm::MDNode*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_node` is null.
 */
llvm_gen_be_error_t llvm_get_tbaa_struct_node(
    a_type_ptr struct_type,
    llvm::MDNode** out_node) noexcept;

/**
 * @brief Resolves a symbolic GPU/target address-space name (`shared`, `constant`, `global`, `local`, `generic`) to an LLVM address space ID.
 * @details Maps address-space names for `nvptx64`, `amdgcn`, and `spirv64` target triples (or generic targets).
 * @param[in] space_name Null-terminated address-space name (`"shared"`, `"constant"`, `"global"`, `"local"`, `"generic"`).
 * @param[out] out_addr_space Receives the target-specific LLVM address space integer.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_addr_space` is null.
 */
llvm_gen_be_error_t llvm_resolve_target_address_space(
    const char* space_name,
    unsigned* out_addr_space) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_TYPE_H */
