/**
 * @file linux_libc_compat.c
 * @brief Portable Linux libc compatibility shims for glibc 2.38+ C23 symbols and musl.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#if defined(__linux__)

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern long edg_posix_strtol(const char *nptr, char **endptr, int base) __asm__("strtol");
extern long long edg_posix_strtoll(const char *nptr, char **endptr, int base) __asm__("strtoll");
extern unsigned long edg_posix_strtoul(const char *nptr, char **endptr, int base) __asm__("strtoul");
extern unsigned long long edg_posix_strtoull(const char *nptr, char **endptr, int base) __asm__("strtoull");
extern intmax_t edg_posix_strtoimax(const char *nptr, char **endptr, int base) __asm__("strtoimax");
extern uintmax_t edg_posix_strtoumax(const char *nptr, char **endptr, int base) __asm__("strtoumax");
extern int edg_posix_vfscanf(FILE *stream, const char *format, va_list ap) __asm__("vfscanf");
extern int edg_posix_vsscanf(const char *str, const char *format, va_list ap) __asm__("vsscanf");

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtol`.
 */
__attribute__((weak, visibility("default")))
long __isoc23_strtol(const char *nptr, char **endptr, int base) {
  return edg_posix_strtol(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoll`.
 */
__attribute__((weak, visibility("default")))
long long __isoc23_strtoll(const char *nptr, char **endptr, int base) {
  return edg_posix_strtoll(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoul`.
 */
__attribute__((weak, visibility("default")))
unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base) {
  return edg_posix_strtoul(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoull`.
 */
__attribute__((weak, visibility("default")))
unsigned long long __isoc23_strtoull(const char *nptr, char **endptr, int base) {
  return edg_posix_strtoull(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoimax`.
 */
__attribute__((weak, visibility("default")))
intmax_t __isoc23_strtoimax(const char *nptr, char **endptr, int base) {
  return edg_posix_strtoimax(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoumax`.
 */
__attribute__((weak, visibility("default")))
uintmax_t __isoc23_strtoumax(const char *nptr, char **endptr, int base) {
  return edg_posix_strtoumax(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_vfscanf`.
 */
__attribute__((weak, visibility("default")))
int __isoc23_vfscanf(FILE *stream, const char *format, va_list ap) {
  return edg_posix_vfscanf(stream, format, ap);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_vsscanf`.
 */
__attribute__((weak, visibility("default")))
int __isoc23_vsscanf(const char *str, const char *format, va_list ap) {
  return edg_posix_vsscanf(str, format, ap);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_fscanf`.
 */
__attribute__((weak, visibility("default")))
int __isoc23_fscanf(FILE *stream, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int rc = edg_posix_vfscanf(stream, format, ap);
  va_end(ap);
  return rc;
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_sscanf`.
 */
__attribute__((weak, visibility("default")))
int __isoc23_sscanf(const char *str, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int rc = edg_posix_vsscanf(str, format, ap);
  va_end(ap);
  return rc;
}

/**
 * @brief Per-thread unique address token for ABI-portable recursive mutex ownership on Linux (glibc + musl).
 */
static __thread char edg_linux_thread_token;

/**
 * @brief ABI-portable `pthread_mutex_init` compatible with both glibc and musl `pthread_mutex_t` layouts.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_init(void *mutex, const void *attr) {
  (void)attr;
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  __atomic_store_n(&words[0], (uintptr_t)0, __ATOMIC_RELEASE);
  __atomic_store_n(&words[1], (uintptr_t)0, __ATOMIC_RELEASE);
  return 0;
}

/**
 * @brief ABI-portable `pthread_mutex_destroy` compatible with both glibc and musl `pthread_mutex_t` layouts.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_destroy(void *mutex) {
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  __atomic_store_n(&words[0], (uintptr_t)0, __ATOMIC_RELEASE);
  __atomic_store_n(&words[1], (uintptr_t)0, __ATOMIC_RELEASE);
  return 0;
}

/**
 * @brief ABI-portable recursive-safe `pthread_mutex_lock` preventing glibc/musl `pthread_mutex_t` offset-16 deadlocks.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_lock(void *mutex) {
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  uintptr_t self = (uintptr_t)&edg_linux_thread_token;
  if (__atomic_load_n(&words[0], __ATOMIC_ACQUIRE) == self) {
    words[1] += 1;
    return 0;
  }
  for (;;) {
    uintptr_t expected = 0;
    if (__atomic_compare_exchange_n(&words[0], &expected, self, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
      words[1] = 1;
      return 0;
    }
  }
}

/**
 * @brief ABI-portable recursive-safe `pthread_mutex_trylock` compatible with both glibc and musl.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_trylock(void *mutex) {
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  uintptr_t self = (uintptr_t)&edg_linux_thread_token;
  if (__atomic_load_n(&words[0], __ATOMIC_ACQUIRE) == self) {
    words[1] += 1;
    return 0;
  }
  uintptr_t expected = 0;
  if (__atomic_compare_exchange_n(&words[0], &expected, self, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
    words[1] = 1;
    return 0;
  }
  return 16; /* EBUSY */
}

/**
 * @brief ABI-portable recursive-safe `pthread_mutex_unlock` compatible with both glibc and musl.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_unlock(void *mutex) {
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  uintptr_t self = (uintptr_t)&edg_linux_thread_token;
  if (__atomic_load_n(&words[0], __ATOMIC_RELAXED) == self) {
    if (words[1] > 1) {
      words[1] -= 1;
      return 0;
    }
    words[1] = 0;
    __atomic_store_n(&words[0], (uintptr_t)0, __ATOMIC_RELEASE);
  }
  return 0;
}

#ifdef __cplusplus
}
#endif

#else
typedef int edg_linux_libc_compat_unused_t;
#endif /* defined(__linux__) */
