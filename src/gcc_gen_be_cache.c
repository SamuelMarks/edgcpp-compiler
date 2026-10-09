/**
 * @file gcc_gen_be_cache.c
 * @brief Implementation of cache management for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "fe_common.h"
#include "gcc_gen_be_cache.h"
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

static be_cache_entry *type_cache[GCC_GEN_BE_MAP_SIZE];
static be_cache_entry *var_cache[GCC_GEN_BE_MAP_SIZE];
static be_cache_entry *func_cache[GCC_GEN_BE_MAP_SIZE];
static be_cache_entry *label_cache[GCC_GEN_BE_MAP_SIZE];
static be_cache_entry *field_cache[GCC_GEN_BE_MAP_SIZE];

static gcc_gen_be_error_t get_cache_array(gcc_gen_be_cache_type_t cache_type, be_cache_entry ****out_cache) GCC_GEN_BE_NOEXCEPT {
    if (!out_cache) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    switch (cache_type) {
        case GCC_GEN_BE_CACHE_TYPE: *out_cache = (be_cache_entry ***)&type_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_VAR: *out_cache = (be_cache_entry ***)&var_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_FUNC: *out_cache = (be_cache_entry ***)&func_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_LABEL: *out_cache = (be_cache_entry ***)&label_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_FIELD: *out_cache = (be_cache_entry ***)&field_cache; return GCC_GEN_BE_SUCCESS;
        default: return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
}

gcc_gen_be_error_t cache_lookup(gcc_gen_be_cache_type_t cache_type, void *key, void **out_value) GCC_GEN_BE_NOEXCEPT {
    if (!out_value) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    be_cache_entry ***cache_ptr = NULL;
    GCC_GEN_BE_CHECK(get_cache_array(cache_type, &cache_ptr));
    if (!cache_ptr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    be_cache_entry **cache = *cache_ptr;

    size_t hash = (((size_t)key) >> 3) % GCC_GEN_BE_MAP_SIZE;
    be_cache_entry *e = cache[hash];
    while (e) {
        if (e->key == key) {
            *out_value = e->value;
            return GCC_GEN_BE_SUCCESS;
        }
        e = e->next;
    }

    *out_value = NULL;
    /* We return success but out_value is NULL if not found.
       This is typical for maps, avoiding error propagation for simple misses. */
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t cache_insert(gcc_gen_be_cache_type_t cache_type, void *key, void *value) GCC_GEN_BE_NOEXCEPT {
    be_cache_entry ***cache_ptr = NULL;
    GCC_GEN_BE_CHECK(get_cache_array(cache_type, &cache_ptr));
    if (!cache_ptr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    be_cache_entry **cache = *cache_ptr;

    size_t hash = (((size_t)key) >> 3) % GCC_GEN_BE_MAP_SIZE;
    be_cache_entry *e = (be_cache_entry *)malloc(sizeof(be_cache_entry));
    if (!e) {
        return GCC_GEN_BE_ERROR_OOM;
    }
    e->key = key;
    e->value = value;
    e->next = cache[hash];
    cache[hash] = e;
    
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t cache_clear(gcc_gen_be_cache_type_t cache_type) GCC_GEN_BE_NOEXCEPT {
    be_cache_entry ***cache_ptr = NULL;
    GCC_GEN_BE_CHECK(get_cache_array(cache_type, &cache_ptr));
    if (!cache_ptr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    be_cache_entry **cache = *cache_ptr;

    for (int i = 0; i < GCC_GEN_BE_MAP_SIZE; i++) {
        be_cache_entry *e = cache[i];
        while (e) {
            be_cache_entry *next = e->next;
            free(e);
            e = next;
        }
        cache[i] = NULL;
    }
    
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t cache_clear_all(void) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_VAR));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FUNC));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_LABEL));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FIELD));
    
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
