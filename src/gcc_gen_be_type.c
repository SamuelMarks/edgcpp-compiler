/**
 * @file gcc_gen_be_type.c
 * @brief Implementation of type lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_type.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include "error.h"
#include "expr.h" /* for value_of_integer_constant if needed, though vectors might use variant.integer_value */
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




/**
 * @brief Lowers an EDG frontend type into a libgccjit type.
 *
 * @param tp The frontend type to lower.
 * @param out_type A pointer to a gcc_jit_type pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_type` populated.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
  if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_type = NULL;
  if (!tp) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  void *cached = NULL;
  gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_TYPE, tp, &cached);
  if (err != GCC_GEN_BE_SUCCESS) return err;
  if (cached) {
      *out_type = (gcc_jit_type *)cached;
      return GCC_GEN_BE_SUCCESS;
  }
  
  if (tp->kind == tk_typeref) {
    gcc_jit_type *base = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.typeref.type, &base));

    if (tp->variant.typeref.qualifiers & TQ_CONST) {
      base = gcc_jit_type_get_const(base);
    }
    if (tp->variant.typeref.qualifiers & TQ_VOLATILE) {
      base = gcc_jit_type_get_volatile(base);
    }
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, base));
    *out_type = base;
    return GCC_GEN_BE_SUCCESS;
  }

  gcc_jit_type *res = NULL;
  switch (tp->kind) {
    case tk_void:
      res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID); break;
    
    case tk_integer:
      switch (tp->variant.integer.int_kind) {
        case ik_char: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR); break;
        case ik_signed_char: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIGNED_CHAR); break;
        case ik_unsigned_char: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR); break;
        case ik_short: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SHORT); break;
        case ik_unsigned_short: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_SHORT); break;
        case ik_int: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT); break;
        case ik_unsigned_int: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT); break;
        case ik_long: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG); break;
        case ik_unsigned_long: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG); break;
#if LONG_LONG_ALLOWED
        case ik_long_long: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG); break;
        case ik_unsigned_long_long: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG); break;
#endif
#if INT128_EXTENSIONS_ALLOWED
        case ik_int128:
        case ik_unsigned_int128:
            {
#if defined(GCC_JIT_TYPE_INT128_T)
                res = gcc_jit_context_get_type(ctx, tp->variant.integer.int_kind == ik_int128 ? GCC_JIT_TYPE_INT128_T : GCC_JIT_TYPE_UINT128_T);
#else
                gcc_jit_type *u64 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
                res = gcc_jit_context_new_array_type(ctx, NULL, u64, 2);
#endif
            }
            break;
#endif
        default: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT); break;
      }
      break;

    case tk_float:
      switch (tp->variant.float_kind) {
        case fk_float: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT); break;
        case fk_double: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
        case fk_long_double: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE); break;
#if defined(GCC_JIT_TYPE_FLOAT16)
        case fk_float16: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT16); break;
#endif
#if defined(GCC_JIT_TYPE_FLOAT32)
        case fk_float32x: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT32); break;
#endif
#if defined(GCC_JIT_TYPE_FLOAT64)
        case fk_float64x: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT64); break;
#endif
#if defined(GCC_JIT_TYPE_FLOAT128)
        case fk_float128: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT128); break;
        case fk_float80:  res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT128); break; /* Fallback for 80-bit */
#endif
        default: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
      }
      break;

#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
    case tk_imaginary:
      {
          gcc_jit_type *elem = NULL;
          switch (tp->variant.float_kind) {
              case fk_float: elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT); break;
              case fk_double: elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
              case fk_long_double: elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE); break;
              default: elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
          }
          if (tp->kind == tk_imaginary) {
              res = elem;
          } else {
#if defined(GCC_JIT_TYPE_COMPLEX_FLOAT)
              switch (tp->variant.float_kind) {
                  case fk_float: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_FLOAT); break;
                  case fk_double: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_DOUBLE); break;
                  case fk_long_double: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_LONG_DOUBLE); break;
                  default: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_DOUBLE); break;
              }
#else
              gcc_jit_field *real_f = gcc_jit_context_new_field(ctx, NULL, elem, "real");
              gcc_jit_field *imag_f = gcc_jit_context_new_field(ctx, NULL, elem, "imag");
              gcc_jit_field *fields[] = {real_f, imag_f};
              gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "complex", 2, fields);
              res = gcc_jit_struct_as_type(s);
#endif
          }
      }
      break;
#endif

    case tk_pointer:
      {
          gcc_jit_type *elem = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.pointer.type, &elem));
          if (tp->variant.pointer.type->kind == tk_routine) {
              res = elem;
          } else {
              res = gcc_jit_type_get_pointer(elem);
          }
      }
      break;
      
    case tk_array:
      if (!tp->variant.array.is_variable_size_array && !tp->variant.array.is_template_dependent_size_array && tp->variant.array.variant.number_of_elements != 0) {
         int size = (int)tp->variant.array.variant.number_of_elements;
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.array.element_type, &elem));
         res = gcc_jit_context_new_array_type(ctx, NULL, elem, size);
      } else {
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.array.element_type, &elem));
         res = gcc_jit_type_get_pointer(elem);
      }
      break;

    case tk_class:
    case tk_struct:
    case tk_union:
      {
         a_const_char *name = tp->source_corresp.name;
         if (name) {
             void *cached_named = NULL;
             err = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)name, &cached_named);
             if (err == GCC_GEN_BE_SUCCESS && cached_named) {
                 res = (gcc_jit_type *)cached_named;
                 GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, res));
                 *out_type = res;
                 return GCC_GEN_BE_SUCCESS;
             }
         }
         if (!name) name = "unnamed_struct";
         
         gcc_jit_struct *s = gcc_jit_context_new_opaque_struct(ctx, NULL, name);
         res = gcc_jit_struct_as_type(s);
         GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, res));
         if (tp->source_corresp.name) {
             GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)tp->source_corresp.name, res));
         }
         
         typedef struct {
             int is_base;
             void *ptr; /* a_field_ptr or a_base_class_ptr */
             unsigned long offset;
             unsigned int bit_rem;
         } layout_entry_t;
         
         int num_entries = 0;
         for (a_field_ptr f = tp->variant.class_struct_union.field_list; f != NULL; f = f->next) num_entries++;
         if (tp->kind == tk_class || tp->kind == tk_struct) {
             a_class_type_supplement_ptr extra = tp->variant.class_struct_union.extra_info;
             if (extra) {
                 for (a_base_class_ptr b = extra->direct_base_classes; b != NULL; b = b->next_direct) num_entries++;
                 for (a_base_class_ptr b = extra->base_classes; b != NULL; b = b->next) {
                     if (b->is_virtual && !b->direct) num_entries++;
                 }
             }
         }
         
         layout_entry_t *entries = NULL;
         if (num_entries > 0) {
             entries = (layout_entry_t *)malloc((size_t)num_entries * sizeof(layout_entry_t));
             if (!entries) return GCC_GEN_BE_ERROR_OOM;
             
             int i = 0;
             for (a_field_ptr f = tp->variant.class_struct_union.field_list; f != NULL; f = f->next) {
                 entries[i].is_base = 0;
                 entries[i].ptr = f;
                 entries[i].offset = f->offset;
                 entries[i].bit_rem = f->offset_bit_remainder;
                 i++;
             }
             if (tp->kind == tk_class || tp->kind == tk_struct) {
                 a_class_type_supplement_ptr extra = tp->variant.class_struct_union.extra_info;
                 if (extra) {
                     for (a_base_class_ptr b = extra->direct_base_classes; b != NULL; b = b->next_direct) {
                         entries[i].is_base = 1;
                         entries[i].ptr = b;
                         entries[i].offset = b->offset;
                         entries[i].bit_rem = 0;
                         i++;
                     }
                     for (a_base_class_ptr b = extra->base_classes; b != NULL; b = b->next) {
                         if (b->is_virtual && !b->direct) {
                             entries[i].is_base = 1;
                             entries[i].ptr = b;
                             entries[i].offset = b->offset;
                             entries[i].bit_rem = 0;
                             i++;
                         }
                     }
                 }
             }
             
             /* Sort entries by offset and bit_rem using bubble sort for simplicity */
             for (int pass = 0; pass < num_entries - 1; pass++) {
                 for (int j = 0; j < num_entries - pass - 1; j++) {
                     if (entries[j].offset > entries[j+1].offset ||
                         (entries[j].offset == entries[j+1].offset && entries[j].bit_rem > entries[j+1].bit_rem)) {
                         layout_entry_t tmp = entries[j];
                         entries[j] = entries[j+1];
                         entries[j+1] = tmp;
                     }
                 }
             }
         }
         
         /* We will build the fields array including padding */
         int max_fields = num_entries * 2 + 1; /* worst case: pad before every field + tail pad */
         gcc_jit_field **fields = NULL;
         if (max_fields > 0) {
             fields = (gcc_jit_field **)malloc((size_t)max_fields * sizeof(gcc_jit_field*));
             if (!fields) {
                 if (entries) free(entries);
                 return GCC_GEN_BE_ERROR_OOM;
             }
             
             int num_jit_fields = 0;
             unsigned long current_offset = 0;
             
             for (int k = 0; k < num_entries; k++) {
                 layout_entry_t *entry = &entries[k];
                 
                 /* Check for padding */
                 if (entry->offset > current_offset && tp->kind != tk_union) {
                     unsigned long pad_size = entry->offset - current_offset;
                     gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                     gcc_jit_type *pad_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, (int)pad_size);
                     char pad_name[32];
                     snprintf(pad_name, sizeof(pad_name), "__pad_%lu", current_offset);
                     fields[num_jit_fields++] = gcc_jit_context_new_field(ctx, NULL, pad_type, pad_name);
                     current_offset = entry->offset;
                 }
                 
                 if (entry->is_base) {
                     a_base_class_ptr b = (a_base_class_ptr)entry->ptr;
                     gcc_jit_type *btype = NULL;
                     err = gcc_gen_be_lower_type(b->type, &btype);
                     if (err != GCC_GEN_BE_SUCCESS) { free(fields); free(entries); return err; }
                     
                     if (b->is_optimized_empty_base) {
                         /* Zero-sized array for empty base */
                         gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                         gcc_jit_type *empty_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, 0);
                         char bname[64];
                         a_const_char *tname = b->type->source_corresp.name;
                         snprintf(bname, sizeof(bname), "__base_%s", tname ? tname : "unnamed");
                         fields[num_jit_fields++] = gcc_jit_context_new_field(ctx, NULL, empty_type, bname);
                     } else {
                         char bname[64];
                         a_const_char *tname = b->type->source_corresp.name;
                         snprintf(bname, sizeof(bname), "__base_%s", tname ? tname : "unnamed");
                         fields[num_jit_fields++] = gcc_jit_context_new_field(ctx, NULL, btype, bname);
                         
                         /* In a true ABI layout, base classes can overlap if they have tail padding.
                            For now, assume size is b->type->size. If overlapping happens, we might have issues. */
                         unsigned long bsize = b->type->size;
                         if (current_offset + bsize > current_offset) {
                             current_offset += bsize;
                         }
                     }
                 } else {
                     a_field_ptr f = (a_field_ptr)entry->ptr;
                     a_const_char *fname = f->source_corresp.name;
                     if (!fname) fname = "unnamed_field";
                     
                     gcc_jit_type *ftype = NULL;
                     err = gcc_gen_be_lower_type(f->type, &ftype);
                     if (err != GCC_GEN_BE_SUCCESS) { free(fields); free(entries); return err; }
                     
                     if (f->is_optimized_empty_class) {
                         gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                         gcc_jit_type *empty_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, 0);
                         fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, empty_type, fname);
                         GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields]));
                         num_jit_fields++;
                     } else if (f->bit_size > 0 || f->is_zero_width_bitfield) {
                         /* Phase 6: Bitfield Allocation Engine. 
                            Synthesize an explicit container field matching EDG's allocation unit 
                            instead of using gcc_jit_context_new_bitfield. */
                         
                         a_boolean container_already_emitted = FALSE;
                         /* If current_offset is ahead of f->offset, we already emitted a container 
                            that covers this bitfield. */
                         if (current_offset > f->offset) {
                             container_already_emitted = TRUE;
                         }
                         
                         if (!container_already_emitted && !f->is_zero_width_bitfield) {
                             char cname[64];
                             snprintf(cname, sizeof(cname), "__bf_container_%lu", current_offset);
                             fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, ftype, cname);
                             GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields]));
                             num_jit_fields++;
                             current_offset += gcc_jit_type_get_size(ftype);
                         } else {
                             if (num_jit_fields > 0) {
                                 GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields - 1]));
                             }
                             if (f->is_zero_width_bitfield) {
                                 /* Zero-width bitfields force alignment to next boundary.
                                    EDG handles offset calculation, so we just let next fields pad. */
                             }
                         }
                     } else {
                         fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, ftype, fname);
                         GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields]));
                         num_jit_fields++;
                         unsigned long fsize = f->type->size;
                         if (current_offset + fsize > current_offset) {
                             current_offset += fsize;
                         }
                     }
                 }
             }
             
             /* Tail padding */
             if (current_offset < tp->size && tp->kind != tk_union) {
                 unsigned long pad_size = tp->size - current_offset;
                 gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                 gcc_jit_type *pad_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, (int)pad_size);
                 fields[num_jit_fields++] = gcc_jit_context_new_field(ctx, NULL, pad_type, "__tail_pad");
             }
             
             if (entries) free(entries);
             
            if (tp->kind == tk_union) {
                res = gcc_jit_context_new_union_type(ctx, NULL, name, num_jit_fields, fields);
                /* Overwrite cached opaque struct with the actual union */
                err = cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, res);
                if (err != GCC_GEN_BE_SUCCESS) { free(fields); return err; }
            } else {
                gcc_jit_struct_set_fields(s, NULL, num_jit_fields, fields);
            }
            free(fields);
         }
         *out_type = res;
         return GCC_GEN_BE_SUCCESS;
      }

    case tk_routine:
      {
         a_routine_type_supplement_ptr supp = tp->variant.routine.extra_info;
         gcc_jit_type *ret_type = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.routine.return_type, &ret_type));
         
         int num_params = 0;
         a_param_type_ptr ptp;
         for (ptp = supp->param_type_list; ptp != NULL; ptp = ptp->next) {
             num_params++;
         }
         
         gcc_jit_type **param_types = NULL;
         if (num_params > 0) {
             param_types = (gcc_jit_type **)malloc((size_t)num_params * sizeof(gcc_jit_type*));
             if (!param_types) return GCC_GEN_BE_ERROR_OOM;
             
             int i = 0;
             for (ptp = supp->param_type_list; ptp != NULL; ptp = ptp->next) {
                 err = gcc_gen_be_lower_type(ptp->type, &param_types[i]);
                 if (err != GCC_GEN_BE_SUCCESS) { free(param_types); return err; }
                 i++;
             }
         }
         
         int is_variadic = supp->has_ellipsis ? 1 : 0;
         res = gcc_jit_context_new_function_ptr_type(ctx, NULL, ret_type, num_params, param_types, is_variadic);
         if (param_types) free(param_types);
      }
      break;
      
    case tk_ptr_to_member:
      {
         a_type_ptr member_type = tp->variant.ptr_to_member.type;
         if (member_type->kind == tk_routine) {
             gcc_jit_field *f1 = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "func_ptr");
             gcc_jit_field *f2 = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), "this_adj");
             gcc_jit_field *fields[] = {f1, f2};
             gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "ptr_to_member", 2, fields);
             res = gcc_jit_struct_as_type(s);
         } else {
             res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
         }
      }
      break;
      
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      {
         int num_elements = 1;
         if (tp->variant.vector.number_of_elements) {
            num_elements = (int)tp->variant.vector.number_of_elements->variant.integer_value;
         }
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.vector.element_type, &elem));
#if defined(LIBGCCJIT_HAVE_gcc_jit_type_get_vector)
         res = gcc_jit_type_get_vector(elem, num_elements);
#else
         /* Fallback for older libgccjit that does not support vector types */
         res = gcc_jit_context_new_array_type(ctx, NULL, elem, num_elements);
#endif
      }
      break;
#endif

    case tk_error:
      /* Ignore / Fallback to void */
      res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
      break;
      
    default:
      res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
      break;
  }
  
  if (res) {
      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, res));
  }
  
  *out_type = res;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Creates a zero-initialized rvalue for the specified type.
 *
 * @details Creates a null pointer for pointers, a zero-initialized constructor
 * for aggregates, or a scalar zero value for numeric types.
 *
 * @param[in] tp The frontend type.
 * @param[in] jit_type The lowered libgccjit type.
 * @param[out] out_rval Pointer to receive the zero rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_zero_rvalue(a_type_ptr tp, struct gcc_jit_type *jit_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
  if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_rval = NULL;

  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  if (!jit_type && tp) {
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp, &jit_type));
  }
  if (!jit_type) {
    jit_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
  }

  a_type_ptr stripped = tp;
  while (stripped && stripped->kind == tk_typeref) {
    stripped = stripped->variant.typeref.type;
  }
  if (stripped && (stripped->kind == tk_pointer || stripped->kind == tk_ptr_to_member)) {
    gcc_jit_type *ptr_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &ptr_type));
    *out_rval = gcc_jit_context_null(ctx, ptr_type ? ptr_type : jit_type);
  } else if (stripped && (stripped->kind == tk_class || stripped->kind == tk_struct)) {
    gcc_jit_type *struct_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &struct_type));
    *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, 0, NULL, NULL);
  } else if (stripped && stripped->kind == tk_array) {
    gcc_jit_type *arr_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &arr_type));
    *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, arr_type, 0, NULL);
  } else if (stripped && stripped->kind == tk_void) {
    *out_rval = NULL;
  } else {
    *out_rval = gcc_jit_context_zero(ctx, jit_type);
  }

  return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */