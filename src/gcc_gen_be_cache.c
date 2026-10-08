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

static be_cache_entry ***get_cache_array(gcc_gen_be_cache_type_t cache_type) {
    switch (cache_type) {
        case GCC_GEN_BE_CACHE_TYPE: return (be_cache_entry ***)&type_cache;
        case GCC_GEN_BE_CACHE_VAR: return (be_cache_entry ***)&var_cache;
        case GCC_GEN_BE_CACHE_FUNC: return (be_cache_entry ***)&func_cache;
        case GCC_GEN_BE_CACHE_LABEL: return (be_cache_entry ***)&label_cache;
        default: return NULL;
    }
}

gcc_gen_be_error_t cache_lookup(gcc_gen_be_cache_type_t cache_type, void *key, void **out_value) {
    if (!out_value) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    be_cache_entry ***cache_ptr = get_cache_array(cache_type);
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

gcc_gen_be_error_t cache_insert(gcc_gen_be_cache_type_t cache_type, void *key, void *value) {
    be_cache_entry ***cache_ptr = get_cache_array(cache_type);
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

gcc_gen_be_error_t cache_clear(gcc_gen_be_cache_type_t cache_type) {
    be_cache_entry ***cache_ptr = get_cache_array(cache_type);
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

gcc_gen_be_error_t cache_clear_all(void) {
    gcc_gen_be_error_t err;
    
    err = cache_clear(GCC_GEN_BE_CACHE_TYPE);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    err = cache_clear(GCC_GEN_BE_CACHE_VAR);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    err = cache_clear(GCC_GEN_BE_CACHE_FUNC);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    err = cache_clear(GCC_GEN_BE_CACHE_LABEL);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
