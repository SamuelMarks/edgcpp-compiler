/**
 * @file gcc_gen_be_cache.h
 * @brief Cache management for the GCC backend.
 *
 * This file declares the cache structure and functions used to map frontend
 * AST nodes to their backend libgccjit equivalents (e.g., types, variables, functions).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_CACHE_H
#define GCC_GEN_BE_CACHE_H

#include "gcc_gen_be_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct be_cache_entry
 * @brief Represents a single entry in a hash map cache.
 */
typedef struct be_cache_entry {
    void *key;                      /**< The key, typically an AST node pointer. */
    void *value;                    /**< The mapped value, typically a libgccjit object. */
    struct be_cache_entry *next;    /**< Pointer to the next entry in case of a hash collision. */
} be_cache_entry;

/** @brief Size of the hash map arrays. */
#define GCC_GEN_BE_MAP_SIZE 1024

/**
 * @brief Enum for identifying specific global caches.
 */
typedef enum {
    GCC_GEN_BE_CACHE_TYPE,
    GCC_GEN_BE_CACHE_VAR,
    GCC_GEN_BE_CACHE_FUNC,
    GCC_GEN_BE_CACHE_LABEL,
    GCC_GEN_BE_CACHE_FIELD
} gcc_gen_be_cache_type_t;

/**
 * @brief Looks up a value in a specified cache.
 *
 * @param cache_type The type of cache to search.
 * @param key The key to look up.
 * @param out_value A pointer to a void pointer that will receive the value.
 * @return GCC_GEN_BE_SUCCESS if found, GCC_GEN_BE_ERROR_INVALID_ARGUMENT if not found, or another error.
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t cache_lookup(gcc_gen_be_cache_type_t cache_type, void *key, void **out_value);

/**
 * @brief Inserts a key-value pair into a specified cache.
 *
 * @param cache_type The type of cache to modify.
 * @param key The key to insert.
 * @param value The value to associate with the key.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code (e.g., GCC_GEN_BE_ERROR_OOM).
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t cache_insert(gcc_gen_be_cache_type_t cache_type, void *key, void *value);

/**
 * @brief Clears a specified cache, freeing all entries.
 *
 * @param cache_type The type of cache to clear.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t cache_clear(gcc_gen_be_cache_type_t cache_type);

/**
 * @brief Clears all global caches.
 *
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t cache_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif /* GCC_GEN_BE_CACHE_H */
