/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

target.c -- Target configuration support

*/

#include "basic_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Normally, the target macros (e.g., TARG_SIZEOF_INT, etc.) are undefined
   by target.h so that they cannot be used inadvertently in other files where
   the associated variables (e.g., targ_sizeof_int, etc.) should be used
   instead.  The macros are needed in this file, however, because the
   associated variables are initialized here. */
#define DO_NOT_UNDEF_TARGET_MACROS

/* Header files common to all files. */
#include "fe_common.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
In addition to the "legacy" configuration (i.e., the configuration specified by
target-specific configuration macros without suffixes), a number of target
configurations may also be specified.  Just as is the case for the legacy
configuration, a target configuration is defined by the values given for a set
of target-specific configuration macros.  Those macro names are the same as
their legacy conterparts with an underscore and configuration name appended
(e.g., TARG_SIZEOF_INT_my_config for the "my_config" target).  These target
configurations are conditionally compiled into the front end when one or more
TARGET_CONFIGURATION_* macros are defined with the value of the unique name of
the configuration (e.g., "#define TARGET_CONFIGURATION_1 my_config").  The
number of configurations listed here is arbitrary and may be added to as
necessary (also add entries to target_configurations below).
*/
#ifdef TARGET_CONFIGURATION_1
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_1
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451*/
#include "target_cfg.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* TARGET_CONFIGURATION_1 */

#ifdef TARGET_CONFIGURATION_2
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_2
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451*/
#include "target_cfg.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* TARGET_CONFIGURATION_2 */

#ifdef TARGET_CONFIGURATION_3
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_3
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451*/
#include "target_cfg.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* TARGET_CONFIGURATION_3 */

#ifdef TARGET_CONFIGURATION_4
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_4
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451*/
#include "target_cfg.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* TARGET_CONFIGURATION_4 */

#ifdef TARGET_CONFIGURATION_5
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_5
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451*/
#include "target_cfg.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* TARGET_CONFIGURATION_5 */

#ifdef TARGET_CONFIGURATION_6
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_6
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451*/
#include "target_cfg.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* TARGET_CONFIGURATION_6 */

#ifdef TARGET_CONFIGURATION_7
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_7
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451*/
#include "target_cfg.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* TARGET_CONFIGURATION_7 */

#ifdef TARGET_CONFIGURATION_8
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_8
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451*/
#include "target_cfg.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* TARGET_CONFIGURATION_8 */

#ifdef TARGET_CONFIGURATION_9
 #error Need to add additional TARGET_CONFIGURATION_X entries
#endif /* TARGET_CONFIGURATION_9 */

/*
This structure is used to associate a target configuration name with routines
to set the target configuration and dump the target configuration.
*/
typedef struct a_target_configuration {
  a_const_char  *name;  /* The name of this target configuration. */
  void          (*set_target_config)(void);
                        /* The address of a routine that will set the
                           target-specific global variables to the values
                           for this target configuration. */
#if DUMP_CONFIG_ENABLED
  void          (*dump_target_config)(void);
                        /* The address of a routine that will dump the
                           target-specific configuration macros associated
                           with this target configuration.  Used for
                           --dump_configuration. */
#endif /* DUMP_CONFIG_ENABLED */
} a_target_configuration;

/*
Define a set_legacy_target_config function to initialize target-specific
global variables to the values given by the legacy configuration macros
(i.e., those without target-specific suffixes).
*/
/* Routine name: set_legacy_target_config. */
#define TARGET_MAP_ROUTINE_NAME(config) \
  set_legacy_target_config(void)
/* Assign the legacy macro value to the associated global variable. */
/*lint -esym(823,TARGET_MAP_MACRO)*/
#define TARGET_MAP_MACRO(config_macro, global_var, config) \
  (global_var) = (config_macro);
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451 header file included more than once. */
#include "target_map.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */


/*
Define a macro to initialize an a_target_configuration entry
*/
#if DUMP_CONFIG_ENABLED
/*lint -esym(750,DEFINE_TARGET_CONFIGURATION)*/
#define DEFINE_TARGET_CONFIGURATION(name) \
  { stringize(name), \
    EDG_CONCAT(set_target_config_, name), \
    EDG_CONCAT(dump_target_config_, name) \
  }
#else /* !DUMP_CONFIG_ENABLED */
#define DEFINE_TARGET_CONFIGURATION(name) \
  { stringize(name), \
    EDG_CONCAT(set_target_config_, name) \
  }
#endif /* DUMP_CONFIG_ENABLED */

/*
This array contains an entry for each target configuration defined when the
front end is built.  The legacy configuration is always the first entry.
*/
STATIC_THREAD a_target_configuration target_configurations[] = {
  { LEGACY_TARGET_CONFIGURATION_NAME, /* A name for the legacy config. */
    set_legacy_target_config,
#if DUMP_CONFIG_ENABLED
    (void(*)(void))NULL               /* No dump routine needed. */
#endif /* DUMP_CONFIG_ENABLED */
  },
#ifdef TARGET_CONFIGURATION_1
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_1),
#endif /* defined(TARGET_CONFIGURATION_1) */
#ifdef TARGET_CONFIGURATION_2
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_2),
#endif /* defined(TARGET_CONFIGURATION_2) */
#ifdef TARGET_CONFIGURATION_3
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_3),
#endif /* defined(TARGET_CONFIGURATION_3) */
#ifdef TARGET_CONFIGURATION_4
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_4),
#endif /* defined(TARGET_CONFIGURATION_4) */
#ifdef TARGET_CONFIGURATION_5
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_5),
#endif /* defined(TARGET_CONFIGURATION_5) */
#ifdef TARGET_CONFIGURATION_6
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_6),
#endif /* defined(TARGET_CONFIGURATION_6) */
#ifdef TARGET_CONFIGURATION_7
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_7),
#endif /* defined(TARGET_CONFIGURATION_7) */
#ifdef TARGET_CONFIGURATION_8
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_8),
#endif /* defined(TARGET_CONFIGURATION_8) */
  /* More can be added if needed (ensure target_cfg.h is included above). */
};

/* The number of target configurations defined when the front end is built. */
#define NUM_TARGET_CONFIGURATIONS \
  ((int32_t)(sizeof(target_configurations)/sizeof(target_configurations[0])))

#if DUMP_CONFIG_ENABLED

/*
Define the dump_as_target_config function that takes a string argument and
produces a list of #defines suitable for using the "legacy" configuration
as the named target configuration.  E.g., when called with "my_config",
would emit "#define TARG_SIZEOF_INT_my_config 4" if TARG_SIZEOF_INT has
the value 4 in the legacy configuration (for each target-specific
configuration macro).
*/
/* Routine name: dump_as_target_config. */
#define TARGET_MAP_ROUTINE_NAME(config) \
  dump_as_target_config(a_const_char *suffix)
/* Write "#define MACRO_config MACRO-legacy-value" to stderr. */
#define TARGET_MAP_MACRO(config_macro, global_var, config) \
  fprintf(f_error, "#define %s_%s %s\n", #config_macro, \
          suffix, stringize(config_macro));
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint -e451 header file included more than once. */
#include "target_map.h"
/*lint +e451*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */


void dump_legacy_config_as_target_config(a_const_char *config)
/*
Called when processing the --dump_legacy_as_target command-line option.
This option is used as an aid in creating new target configurations; when used,
it emits a set of #defines for a new configuration (as named by the argument)
whose values are the same as those for the "legacy" configuration of the
front end.  The output (after modifying the TARGET_CONFIGURATION_X entry)
can then be included by defines.h and once the front end is recompiled, that
target configuration becomes available.  Note that other command-line options
have no effect on the output.
*/
{
  fprintf(f_error, "/* Target configuration: %s */\n", config);
  fprintf(f_error, "/* NOTE: For multiple target configurations, "
                   "change _1 below as necessary. */\n");
  fprintf(f_error, "#define TARGET_CONFIGURATION_1 %s\n", config);
  dump_as_target_config(config);
}  /* dump_legacy_config_as_target_config */

#endif /* DUMP_CONFIG_ENABLED */

int32_t find_target_configuration(a_const_char *config)
/*
Utility to return the target configuration index for the specified target
configuration string or NO_TARGET_CONFIG if no such target configuration
has been specified.
*/
{
  int32_t  i, result = NO_TARGET_CONFIG;

  for (i = 0; i < NUM_TARGET_CONFIGURATIONS; i++) { /*lint !e681*/
    if (target_configurations[i].name != NULL &&
        strcmp(target_configurations[i].name, config) == 0) {
      result = i;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* find_target_configuration */


void set_target_configuration(int32_t target_index)
/*
Called to set target-specific global variables to the values indicated by
target_configurations[target_index].  If target_index is NO_TARGET_CONFIG,
the global variables are set to their legacy values.  Invoked during
early initialization to set values to the default values (which may or may
not be the legacy configuration), and is re-invoked each time a --target
command-line option is specified.  Note that there is a race condition if any
global variable is set here and also as a side-effect of other command-line
processing (effectively, "last setting wins").
*/
{
  a_boolean unnamed = FALSE;
  a_target_configuration *target;

  if (target_index == NO_TARGET_CONFIG) {
    /* If no target configuration was specified (i.e., no default was given
       and no --target option), use the legacy configuration and the
       unadorned value of EDG_AUXILIARY_INFO_DIR_NAME. */
    target_index = 0;
    unnamed = TRUE;
  }  /* if */
  check_assertion(target_index >= 0 &&
                  target_index < NUM_TARGET_CONFIGURATIONS);
  target = &target_configurations[target_index];
  check_assertion(target->set_target_config != (void(*)(void))NULL);
  target->set_target_config();
  if (unnamed || target->name == NULL) {
    /* If no target is specified, or the target configuration is unnamed,
       use the legacy version of the directory. */
    auxiliary_info_dir_name = (char *)EDG_AUXILIARY_INFO_DIR_NAME;
  } else {
    /* Create a target-specific version of this name so that predefined
       macros can be different for each target configuration. */
    check_assertion(EDG_AUXILIARY_INFO_DIR_NAME != NULL); /*lint !e779*/
    auxiliary_info_dir_name = alloc_general(
                                        strlen(EDG_AUXILIARY_INFO_DIR_NAME) +
                                        strlen(target->name) + 2);
    (void)strcpy(auxiliary_info_dir_name, EDG_AUXILIARY_INFO_DIR_NAME);
    (void)strcat(auxiliary_info_dir_name, "_");
    (void)strcat(auxiliary_info_dir_name, target->name);
  }  /* if */
}  /* set_target_configuration */

#if DUMP_CONFIG_ENABLED

void dump_target_configurations(void)
/*
Dumps any target-configuration specific macros in such a format that they
can be re-read as a defines.h (as part of the processing for
--dump_configuration).
*/
{
  a_target_configuration *legacy = &target_configurations[0];
  int                    i;

  /* The values in the legacy configuration aren't explicitly dumped
     (they're part of the normal --dump_configuration output), so just emit
     the "header". */
  if (legacy->name == NULL) {
    fprintf(f_error, "\n/* Legacy configuration: <unnamed> */\n");
    fprintf(f_error, "#define LEGACY_TARGET_CONFIGURATION_NAME NULL\n");
  } else {
    fprintf(f_error, "\n/* Legacy configuration: %s */\n", legacy->name);
    fprintf(f_error, "#define LEGACY_TARGET_CONFIGURATION_NAME \"%s\"\n",
            legacy->name);
  }  /* if */
  /* Dump the remaining configurations. */
  for (i = 1; i < NUM_TARGET_CONFIGURATIONS; i++) { /*lint !e681*/
    fprintf(f_error, "\n/* Target configuration: %s */\n",
            target_configurations[i].name);
    fprintf(f_error, "#define TARGET_CONFIGURATION_%d %s\n", i,
            target_configurations[i].name);
    check_assertion(target_configurations[i].dump_target_config !=
                    (void(*)(void))NULL);
    target_configurations[i].dump_target_config();
  }  /* for */
}  /* dump_target_configurations */

#endif /* DUMP_CONFIG_ENABLED */

static void set_plain_char_int_kind(a_boolean plain_chars_are_signed)
/*
Set plain_char_int_kind, which indicates the integer kind for "plain"
(neither signed or unsigned) char.  plain_chars_are_signed indicates
whether it should be signed.
*/
{
  if (C_dialect == C_dialect_pcc ||
      (microsoft_mode && C_mode())) {
    /* In pcc mode, a "plain" char is the same as either "signed char"
       or "unsigned char".  Likewise in Microsoft C mode. */
    plain_char_int_kind = plain_chars_are_signed ?
                               (an_integer_kind)ik_signed_char :
                               (an_integer_kind)ik_unsigned_char;
  } else {
    /* In standard mode, a "plain" char is different than "signed char" and
       "unsigned char". */
    plain_char_int_kind = (an_integer_kind)ik_char;
  }  /* if */
}  /* set_plain_char_int_kind */

#if MICROSOFT_EXTENSIONS_ALLOWED

void init_microsoft_sized_int_types(void)
/*
Map __int8, __int16, __int32, and __int64 to the appropriate integer
kinds.  Leave the variables set to ik_none if a match can't be found;
only if a corresponding integer kind is found will the corresponding
keyword be entered into the symbol table.
*/
{
#if STANDALONE_UTILITY_PROGRAM
  /* The signedness of characters can be set on the command line, but for
     standalone utilities, we determine this from the IL header. */
  set_plain_char_int_kind(il_header.plain_chars_are_signed);
#endif /* STANDALONE_UTILITY_PROGRAM */
  /* Map __int8 to plain char if and only if 8-bit chars are being used. */
  if (targ_char_bit == 8) {
    targ_int8_int_kind = plain_char_int_kind;
    targ_unsigned_int8_int_kind = (an_integer_kind)ik_unsigned_char;
  }  /* if */
  /* For the other cases, find the first integer kinds, signed and unsigned,
     that hold exactly 16, 32 and 64 bits, respectively. */  
  targ_int16_int_kind = int_kind_for_bit_size(16, /*signed=*/TRUE);
  if (targ_int16_int_kind != (an_integer_kind)ik_none) {
    targ_unsigned_int16_int_kind = int_kind_for_bit_size(16, /*signed=*/FALSE);
    check_assertion_str(targ_unsigned_int16_int_kind !=
                                              (an_integer_kind)ik_none,
                       "target_init: can't set int kind for unsigned __int16");
  }  /* if */
  targ_int32_int_kind = int_kind_for_bit_size(32, /*signed=*/TRUE);
  if (targ_int32_int_kind != (an_integer_kind)ik_none) {
    targ_unsigned_int32_int_kind = int_kind_for_bit_size(32, /*signed=*/FALSE);
    check_assertion_str(targ_unsigned_int32_int_kind !=
                                              (an_integer_kind)ik_none,
                       "target_init: can't set int kind for unsigned __int32");
  }  /* if */
  targ_int64_int_kind = int_kind_for_bit_size(64, /*signed=*/TRUE);
  if (targ_int64_int_kind != (an_integer_kind)ik_none) {
    targ_unsigned_int64_int_kind = int_kind_for_bit_size(64, /*signed=*/FALSE);
    check_assertion_str(targ_unsigned_int64_int_kind !=
                                              (an_integer_kind)ik_none,
                       "target_init: can't set int kind for unsigned __int64");
  }  /* if */
}  /* init_microsoft_sized_int_types */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_targ_size_t size_of_pointer_to(ARG_UNUSED a_type_ptr        tp,
                                 a_targ_alignment             *alignment)
/*
Return the size and alignment for a pointer type that points to the indicated
type.  This routine should be rewritten for implementations in which
TARG_ALL_POINTERS_SAME_SIZE may not always be TRUE.
*/
{
  a_targ_size_t size;

#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) {
    /* Pointers come in "near" and "far" sizes (e.g., Microsoft 16-bit
       mode). */
    if (is_far_type(tp)) {
      size = targ_sizeof_far_pointer;
      *alignment = targ_alignof_far_pointer;
    } else {
      size = targ_sizeof_near_pointer;
      *alignment = targ_alignof_near_pointer;
    }  /* if */
  } else
#endif /* NEAR_AND_FAR_ALLOWED */
  /* Do not add code here. */
  {
    if (targ_all_pointers_same_size) {
      /* All pointers have the same size and alignment. */
      size = targ_sizeof_pointer;
      *alignment = targ_alignof_pointer;
    } else {
#if CHECKING
      /* If you set TARG_ALL_POINTERS_SAME_SIZE FALSE only because you want 
         to support near/far, see the comments on TARG_ALL_POINTERS_SAME_SIZE
         in targ_def.h and the internal documentation; it's probably not what
         you want. */
      internal_error("size_of_pointer_to: unknown pointer size");
#else /* !CHECKING */
      size = 0;
#endif /* CHECKING */
    }  /* if */
  }  /* if */
  return size;
}  /* size_of_pointer_to */

#if CHECKING

void check_target_configuration(void)
/*
Perform consistency check on target configuration variables.  Note that
some checks that are performed at compilation time are re-checked here
to ensure that incorrect values for configurations specified with --target
are diagnosed.
*/
{
  a_targ_size_t    size, size_max_value;
  a_targ_alignment alignment;
  a_boolean        err;

#if defined(DEFAULT_TARGET_CONFIGURATION_NAME)
  /* Verify that if a default target configuration was specified, it was
     found. */
  if (target_configuration_index == NO_TARGET_CONFIG) {
    internal_error(
                "check_target_config: default target configuration not found");
  }  /* if */
#endif /* defined(DEFAULT_TARGET_CONFIGURATION_NAME) */
  /* The target char may be no bigger than the host long. */
  get_integer_size_and_alignment((an_integer_kind)ik_char,
                                 &size, &alignment);
  if (size > sizeof(long)) {
    internal_error("check_target_config: target char is too large");
  }  /* if */
  /* The target wchar_t may be no bigger than the host long. */
  if (targ_sizeof_wchar_t > sizeof(long)) {
    internal_error("check_target_config: target wchar_t is too large");
  }  /* if */
  /* char16_t and char32 require a check similar to wchar_t, and in addition
     they have minimum size requirements.  They must also be unsigned. */
  if (targ_sizeof_char16_t > sizeof(long)) {
    internal_error("check_target_config: target char16_t is too large");
  } else if (targ_sizeof_char16_t*targ_char_bit < 16) {
    internal_error("check_target_config: target char16_t is too small");
  }  /* if */
  check_assertion_str(!int_kind_is_signed[(int)targ_char16_t_int_kind],
                      "check_target_config: target char16_t must be unsigned");
  if (targ_sizeof_char32_t > sizeof(long)) {
    internal_error("check_target_config: target char32_t is too large");
  } else if (targ_sizeof_char32_t*targ_char_bit < 32) {
    internal_error("check_target_config: target char32_t is too small");
  }  /* if */
  check_assertion_str(!int_kind_is_signed[(int)targ_char32_t_int_kind],
                      "check_target_config: target char32_t must be unsigned");
  /* targ_size_t_max must fit in the target integer type targ_size_t_int_kind
     (but it need not fit exactly). */
  get_integer_size_and_alignment((an_integer_kind)targ_size_t_int_kind,
                                 &size, &alignment);
  size *= targ_char_bit;
  if (size > sizeof(a_targ_size_t)*CHAR_BIT) {
    size = sizeof(a_targ_size_t)*CHAR_BIT;
  }  /* if */
  /* Make a mask of "size" 1 bits for the maximum value. */
  size_max_value = ((((a_targ_size_t)1 << (size-1))-1) << 1);
  /* Final "or" done separately to avoid a bug in Borland C++ 3.0 with -O. */
  size_max_value |= 1;
  if (size_max_value < targ_size_t_max) {
    internal_error("check_target_config: targ_size_t_max is too large");
  }  /* if */
  if (targ_sizeof_largest_integer > MAX_SIZEOF_LARGEST_INTEGER) {
    /* The target-specific value for largest integer must fit in the
       representation the front end was configured for. */
    internal_error(
              "check_target_config: targ_sizeof_largest_integer is too large");
  }  /* if */
#if FIXED_POINT_ALLOWED
  if (targ_sizeof_largest_fixed_point > MAX_SIZEOF_LARGEST_FIXED_POINT) {
    /* The target-specific value for largest fixed point must fit in the
       representation the front end was configured for. */
    internal_error(
          "check_target_config: targ_sizeof_largest_fixed_point is too large");
  }  /* if */
#endif /* FIXED_POINT_ALLOWED */
#if LONG_LONG_ALLOWED
  if (targ_sizeof_largest_integer < targ_sizeof_long_long) {
    internal_error("check_target_config: invalid targ_sizeof_largest_integer");
  }  /* if */
#else /* !LONG_LONG_ALLOWED */
  if (targ_sizeof_largest_integer < targ_sizeof_long) {
    internal_error("check_target_config: invalid targ_sizeof_largest_integer");
  }  /* if */
#endif /* LONG_LONG_ALLOWED */
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  /* When using host integers to represent target integers, make sure the
     host integer selected is large enough. */
  /* Use variable err instead of testing directly to avoid warnings about
     testing invariant values on some compilers. */
  err = (MAX_SIZEOF_LARGEST_INTEGER*targ_char_bit >
         sizeof(an_integer_value)*CHAR_BIT);
  if (err) {
    internal_error("check_target_config: an_integer_value is too small");
  }  /* if */
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  /* When using the simulated large integer approach to represent target
     integers, the sizes must be right. */

  /* Use variable err instead of testing directly to avoid warnings about
     testing invariant values on some compilers. */
  err = (BITS_IN_HOST_LARGE_INTEGER != sizeof(a_host_large_integer)*CHAR_BIT);
  if (err) { /*lint !e774*/
    internal_error("check_target_config: invalid BITS_IN_HOST_LARGE_INTEGER");
  }  /* if */
  err = (SIZEOF_INT_VALUE_PART > sizeof(an_int_value_part));
  if (err) { /*lint !e774*/
    internal_error("check_target_config: invalid SIZEOF_INT_VALUE_PART");
  }  /* if */
  err = (BITS_IN_INT_VALUE_PART != SIZEOF_INT_VALUE_PART*CHAR_BIT ||
         2*BITS_IN_INT_VALUE_PART > BITS_IN_HOST_LARGE_INTEGER); /*lint !e506*/
  if (err) { /*lint !e774*/
    internal_error("check_target_config: invalid BITS_IN_INT_VALUE_PART");
  }  /* if */
  err = (BITS_IN_INT_VALUE_PART*INT_VALUE_PARTS_PER_INTEGER_VALUE !=
         MAX_SIZEOF_LARGEST_INTEGER*targ_char_bit);
  if (err) {
    internal_error(
             "check_target_config: invalid INT_VALUE_PARTS_PER_INTEGER_VALUE");
  }  /* if */
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  if (targ_host_string_char_bit > CHAR_BIT) {
    internal_error("check_target_config: targ_host_string_char_bit too large");
  }  /* if */
  /* Be sure the maximum and minimum values for "pack alignment" are
     appropriate and may be stored within a_targ_alignment, which is a_byte
     (= unsigned char). */
  { a_targ_alignment temp = UCHAR_MAX;  /* Use variable to avoid
                                           lint/gcc complaints. */
    if (targ_minimum_pack_alignment < 1 ||
        targ_minimum_pack_alignment > temp /*lint --e(685)*/) {
      internal_error(
                   "check_target_config: invalid targ_minimum_pack_alignment");
    }  /* if */
  }
  if (targ_maximum_pack_alignment < targ_minimum_pack_alignment ||
      (a_targ_alignment)targ_maximum_pack_alignment !=
                                                targ_maximum_pack_alignment) {
    internal_error("check_target_config: invalid targ_maximum_pack_alignment");
  }  /* if */
#if BUILTIN_FUNCTIONS_ENABLED && GNU_VECTOR_TYPES_ALLOWED
  /* The GNU built-in functions that map on IA-32 vector instructions require
     that integers of specific sizes exist.  To keep things simple, we make
     the slightly stronger requirement that sizeof(short) == 2,
     sizeof(int) == 4, and sizeof(long long) == 8.  (LONG_LONG_ALLOWED is
     always TRUE when BUILTIN_FUNCTIONS_ENABLED is TRUE.) */
  check_assertion_str2(targ_sizeof_short == 2 && targ_sizeof_int == 4 &&
                         targ_sizeof_long_long == 8,
                       "check_target_config: invalid integer sizes for",
                       " GNU IA-32 vector functions");
#endif /* BUILTIN_FUNCTIONS_ENABLED && GNU_VECTOR_TYPES_ALLOWED */
#if IA64_ABI && DO_IL_LOWERING
  { 
    /* Verify that the integer kind used for a vtable entry is signed and
       large enough to accommodate offsets.  Must also be the same size as
       a pointer (for type_info and virtual function pointers). */
    a_targ_size_t    vtbl_entry_size, delta_int_size;
    a_targ_alignment dummy_alignment;

    get_integer_size_and_alignment(targ_ia64_vtable_entry_int_kind,
                                   &vtbl_entry_size, &dummy_alignment);
    get_integer_size_and_alignment(targ_delta_int_kind,
                                   &delta_int_size, &dummy_alignment);
    if (targ_sizeof_pointer != vtbl_entry_size && targ_all_pointers_same_size){
      internal_error(
	    "check_target_config: targ_ia64_vtable_entry_int_kind wrong size");
    }  /* if */
    if (delta_int_size > vtbl_entry_size) {
      internal_error(
          "check_target_config: targ_ia64_vtable_entry_int_kind is too small");
    }  /* if */
    if (!int_kind_is_signed[(int)targ_ia64_vtable_entry_int_kind]) {
      internal_error(
        "check_target_config: targ_ia64_vtable_entry_int_kind must be signed");
    }  /* if */
  }
#endif /* IA64_ABI && DO_IL_LOWERING */
  if (targ_microsoft_bit_field_allocation &&
      targ_bit_field_container_size != -1) {
    internal_error("check_target_config: targ_microsoft_bit_field_allocation "
                      "must be -1 when targ_bit_field_container_size is TRUE");
  }  /* if */
#if ABI_COMPATIBILITY_VERSION <= 241
  check_assertion(!targ_optimize_empty_base_class_layout);
#endif /* ABI_COMPATIBILITY_VERSION <= 241 */
#if IA64_ABI
  check_assertion(targ_optimize_empty_base_class_layout);
#endif /* IA64_ABI */
#if !LONG_LONG_ALLOWED
  if (!target_is_32_bit_x86_based()) {
    internal_error("check_target_config: long long required");
  }  /* if */
#endif /* !LONG_LONG_ALLOWED */
#if IA64_ABI
  if (!targ_field_alloc_sequence_equals_decl_sequence) {
    internal_error("check_target_config: "
                "targ_field_alloc_sequence_equals_decl_sequence must be TRUE");
  }  /* if */
#else /* !IA64_ABI */
#if DO_IL_LOWERING
  if (!ctors_return_this || dtors_return_this) {
    internal_error("check_target_config: "
                     "ctors_return_this or dtors_return_this set incorrectly");
  }  /* if */
#endif /* DO_IL_LOWERING */
#endif /* IA64_ABI */
#if !HOST_TARGET_ENDIAN_MISMATCH_OKAY
  if (targ_little_endian != host_little_endian) {
    /* In cross-compilation configurations, a host/target endian mismatch is
       okay (HOST_TARGET_ENDIAN_MISMATCH_OKAY must be set to TRUE). */
    internal_error("check_target_config: "
                   "unexpected host/target endian mismatch");
  }  /* if */
#endif /* !HOST_TARGET_ENDIAN_MISMATCH_OKAY */
#if !USE_SOFTFLOAT
  /*lint -e{506}*/
  if (sizeof(long double) == sizeof(double) &&
      targ_ldbl_mant_dig == 64) {
    /* Trying to emulate 80-bit long double on a host system that doesn't
       support that type only works if using SoftFloat. */
    internal_error("check_target_config: "
                   "must use SoftFloat library for this configuration");
  }  /* if */
#endif /* !USE_SOFTFLOAT */
#if MICROSOFT_EXTENSIONS_ALLOWED
  check_assertion_str2(!targ_microsoft_ptr_to_member_sizing ||
                       targ_all_pointers_same_size,
                       "Microsoft pointer-to-member layout assumes all",
                       " pointers have the same size");
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* check_target_configuration */

#endif /* CHECKING */

static void init_character_sizes(void)
/*
Initialize the global variables describing the sizes of various character
kinds.
*/
{
  a_targ_alignment  alignment;

  get_integer_size_and_alignment((an_integer_kind)targ_wchar_t_int_kind,
                                 &targ_sizeof_wchar_t, &alignment);
  get_integer_size_and_alignment((an_integer_kind)targ_char16_t_int_kind,
                                 &targ_sizeof_char16_t, &alignment);
  get_integer_size_and_alignment((an_integer_kind)targ_char32_t_int_kind,
                                 &targ_sizeof_char32_t, &alignment);
}  /* init_character_sizes */

#if BACK_END_IS_CP_GEN_BE

void select_cp_gen_be_target_dialect(void)
/*
If CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT is TRUE, select the target dialect
to match the source dialect (including the version of the dialect).
*/
{
#if CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT
#if CHECKING
  {
    /* Note that clang mode is a dialect of GNU mode. */
    int n_dialects =
       (gnu_mode != 0) +
       (microsoft_mode != 0) + /*lint !e514*/
       (sun_mode != 0); /*lint !e514*/
    check_assertion(n_dialects < 2);
  }
#endif /* CHECKING */
  if (gnu_mode) {
    gcc_or_clang_is_generated_code_target = TRUE;
#if GCC_BUILTIN_VARARGS
    gcc_builtin_varargs_in_generated_code = TRUE;
#else /* !GCC_BUILTIN_VARARGS */
    gcc_builtin_varargs_in_generated_code = FALSE;
#endif /* GCC_BUILTIN_VARARGS */
    if (clang_mode) {
      clang_is_generated_code_target = TRUE;
      clang_target_version_number = clang_version;
    } else {
      gcc_is_generated_code_target = TRUE;
      gnu_target_version_number = gnu_version;
    }  /* if */
  }  /* if */
  if (microsoft_mode) {
    microsoft_dialect_is_generated_code_target = TRUE;
    msvc_target_version_number = microsoft_version;
    msvc_is_generated_code_target = MSVC_IS_GENERATED_CODE_TARGET;
  }  /* if */
  if (sun_mode) {
    sun_is_generated_code_target = TRUE;
#ifdef SUN_TARGET_VERSION_NUMBER
    sun_target_version_number = SUN_TARGET_VERSION_NUMBER;
#endif /* ifdef SUN_TARGET_VERSION_NUMBER */
  }  /* if */
  check_assertion_str(gcc_is_generated_code_target +
                      microsoft_dialect_is_generated_code_target +
                      sun_is_generated_code_target +
                      clang_is_generated_code_target <= 1,
                      "Conflicting target dialects set.");
#endif /* CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT */
}  /* select_cp_gen_be_target_dialect */

#endif /* BACK_END_IS_CP_GEN_BE */

/*
GNU C and C++ distinguishes between two alignments for fundamental types:
The intrinsic alignment (returned by __alignof__ in most cases, and imbued on
complete objects of that type), and the field alignment (used to align fields
of that type).  For example, on some Intel-based configurations, long long is
normally 8-byte aligned, but a struct containing a long long need only be
4-byte aligned.  The a_type entry contains the intrinsic alignment.  The
field alignment is accessed through the following arrays.

(Note: one would ordinarily expect this facility to be found in layout.c.
However, it is needed by the C-generating back end, and layout.c is not
present in standalone C-generating back end executables.)
*/
STATIC_THREAD a_targ_alignment
                int_field_alignments[(int)ik_last];
STATIC_THREAD a_targ_alignment
                float_field_alignments[(int)fk_last];


static void init_field_alignment_tables(void)
/*
Set up tables containing the field alignments for the various integer and
floating point types.
*/
{
#if CHECKING
  int k;
#endif /* CHECKING */

  check_assertion(targ_dual_alignments_for_builtin_types);
  int_field_alignments[(int)ik_char] = 1;
  int_field_alignments[(int)ik_signed_char] = 1;
  int_field_alignments[(int)ik_unsigned_char] = 1;
  int_field_alignments[(int)ik_short] = targ_short_field_alignment;
  int_field_alignments[(int)ik_unsigned_short] = targ_short_field_alignment;
  int_field_alignments[(int)ik_int] = targ_int_field_alignment;
  int_field_alignments[(int)ik_unsigned_int] = targ_int_field_alignment;
  int_field_alignments[(int)ik_long] = targ_long_field_alignment;
  int_field_alignments[(int)ik_unsigned_long] = targ_long_field_alignment;
#if LONG_LONG_ALLOWED
  int_field_alignments[(int)ik_long_long] = targ_long_long_field_alignment;
  int_field_alignments[(int)ik_unsigned_long_long] =
                                               targ_long_long_field_alignment;
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
  int_field_alignments[(int)ik_int128] = targ_int128_field_alignment;
  int_field_alignments[(int)ik_unsigned_int128] = targ_int128_field_alignment;
#endif /* INT128_EXTENSIONS_ALLOWED */
  int_field_alignments[ik_bit_precise] = targ_int_field_alignment;
  int_field_alignments[ik_unsigned_bit_precise] = targ_int_field_alignment;
#if CHECKING
  for (k = 0; k<(int)ik_last; ++k) {
    if (int_field_alignments[k] == 0) {
      unexpected_condition();
    }  /* if */
  }  /* for */
#endif /* CHECKING */
  float_field_alignments[(int)fk_float16] = 2;
  float_field_alignments[(int)fk_fp16] = 2;
  float_field_alignments[(int)fk_float] = targ_float_field_alignment;
  float_field_alignments[(int)fk_float32x] = targ_double_field_alignment;
  float_field_alignments[(int)fk_double] = targ_double_field_alignment;
  float_field_alignments[(int)fk_float64x] = targ_long_double_field_alignment;
  float_field_alignments[(int)fk_long_double] =
                                              targ_long_double_field_alignment;
  float_field_alignments[(int)fk_float80] = targ_float80_field_alignment;
  float_field_alignments[(int)fk_float128] = targ_float128_field_alignment;
  float_field_alignments[(int)fk_std_bfloat16] = 2;
  float_field_alignments[(int)fk_std_float16] = 2;
  float_field_alignments[(int)fk_std_float32] = targ_float_field_alignment;
  float_field_alignments[(int)fk_std_float64] = targ_double_field_alignment;
  float_field_alignments[(int)fk_std_float128] = targ_float128_field_alignment;
#if CHECKING
  for (k = 0; k<(int)fk_last; ++k) {
    if (float_field_alignments[k] == 0) {
      unexpected_condition();
    }  /* if */
  }  /* for */
#endif /* CHECKING */
}  /* init_field_alignment_tables */


a_targ_alignment field_alignment_for(a_type_ptr  type)
/*
Return the field alignment for the given type.
*/
{
  a_targ_alignment  result;

  if (targ_dual_alignments_for_builtin_types) {
    switch (type->kind) {
      case tk_integer:
        result = int_field_alignments[type->variant.integer.int_kind];
        break;
      case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_imaginary:
      case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        result = float_field_alignments[type->variant.float_kind];
        break;
      case tk_typeref:
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
        if (type->alignment_set_explicitly) {
          result = type->alignment;
        } else if (clang_mode &&
                   (type->variant.typeref.qualifiers & TQ_C11_ATOMIC) != 0) {
          /* In Clang mode, atomic-qualified types get additional alignment. */
          result = alignment_of_type(type);
        } else if (!gnu_mode || gnu_version/100 != 303) {
          /* In a chain of typedefs, the last one with attribute "aligned"
             normally determines the alignment.  However, gcc/g++ 3.3.x appears
             to ignore any "intermediate" typedefs. */
          result = field_alignment_for(type->variant.typeref.type);
        } else
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          result = field_alignment_for(skip_typerefs(type));
        }  /* if */
        break;
      case tk_array:
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
        if (type->alignment_set_explicitly) {
          /* The alignment cannot be set directly for an array type, but when
             applying cv-qualifiers to a typedef for an array, an alignment
             attribute on the typedef may need to be copied to the array
             type. */
          result = type->alignment;
        } else
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          result = field_alignment_for(underlying_array_element_type(type));
        }  /* if */
        break;
      default:
        result = type->alignment;
    }  /* switch */
  } else {
    /* The field alignment is equal to the intrinsic alignment of the type. */
    result = alignment_of_type(type);
  }  /* if */
  return result;
}  /* field_alignment_for */


void target_early_init(void)
/*
One time initialization that must take place early on in the front end.
Sets global variables to their "default" configuration; a subset of these
variables may be re-set to target-specific values if the --target command-line
option is specified (so these variables should not be used until after
command-line processing, or for the STANDALONE_UTILITY case, after the IL
header has been read and the target has been determined).
*/
{
  /* If a default target configuration is given, use those values as the
     defaults; otherwise use the legacy target configuration. */
#if defined(DEFAULT_TARGET_CONFIGURATION_NAME)
  target_configuration_index =
                  find_target_configuration(DEFAULT_TARGET_CONFIGURATION_NAME);
#else /* !defined(DEFAULT_TARGET_CONFIGURATION_NAME) */
  target_configuration_index = NO_TARGET_CONFIG;
#endif /* defined(DEFAULT_TARGET_CONFIGURATION_NAME) */
  set_target_configuration(target_configuration_index);
#if MICROSOFT_EXTENSIONS_ALLOWED
  targ_int8_int_kind = ((an_integer_kind)ik_none);
  targ_unsigned_int8_int_kind = ((an_integer_kind)ik_none);
  targ_int16_int_kind = ((an_integer_kind)ik_none);
  targ_unsigned_int16_int_kind = ((an_integer_kind)ik_none);
  targ_int32_int_kind = ((an_integer_kind)ik_none);
  targ_unsigned_int32_int_kind = ((an_integer_kind)ik_none);
  targ_int64_int_kind = ((an_integer_kind)ik_none);
  targ_unsigned_int64_int_kind = ((an_integer_kind)ik_none);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  distinct_template_signatures = DEFAULT_DISTINCT_TEMPLATE_SIGNATURES;
  assume_references_cannot_be_null = ASSUME_REFERENCES_CANNOT_BE_NULL;
#if DO_IL_LOWERING
  force_variable_definition_via_zeroing =
                                         FORCE_VARIABLE_DEFINITION_VIA_ZEROING;
  make_all_functions_unprototyped = MAKE_ALL_FUNCTIONS_UNPROTOTYPED;
  assume_this_cannot_be_null_in_conditional_operators =
                           ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS;
#if IA64_ABI
  ctors_return_this = targ_ia64_abi_variant_ctors_and_dtors_return_this;
  dtors_return_this = targ_ia64_abi_variant_ctors_and_dtors_return_this;
#else /* !IA64_ABI */
  ctors_return_this = TRUE;
  dtors_return_this = FALSE;
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING */
  remove_qualifiers_from_param_types =
                                    DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES;
  c_and_cpp_function_types_are_distinct =
                                 DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT;
#if BACK_END_IS_CP_GEN_BE
  old_specializations_for_generated_instances =
                           DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES;
#endif /* BACK_END_IS_CP_GEN_BE */
  type_info_in_namespace_std = DEFAULT_TYPE_INFO_IN_NAMESPACE_STD;
  pass_stdarg_references_to_generated_code =
                              DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE;
  va_list_in_std_namespace = DEFAULT_VA_LIST_IN_STD_NAMESPACE;
  va_list_using_using_decl_in_std_namespace = FALSE;
  instantiate_extern_inline = INSTANTIATE_EXTERN_INLINE;
  instantiate_inline_variables = INSTANTIATE_INLINE_VARIABLES;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  sun_is_generated_code_target = SUN_IS_GENERATED_CODE_TARGET;
  clang_is_generated_code_target = CLANG_IS_GENERATED_CODE_TARGET;
  gcc_is_generated_code_target = GCC_IS_GENERATED_CODE_TARGET;
  gcc_or_clang_is_generated_code_target = gcc_is_generated_code_target ||
                                          clang_is_generated_code_target;
  gnu_target_version_number = GNU_TARGET_VERSION_NUMBER;
#if CLANG_IS_GENERATED_CODE_TARGET
  clang_target_version_number = CLANG_TARGET_VERSION_NUMBER;
#endif /* CLANG_IS_GENERATED_CODE_TARGET */
#ifdef SUN_TARGET_VERSION_NUMBER
  sun_target_version_number = SUN_TARGET_VERSION_NUMBER;
#endif /* ifdef SUN_TARGET_VERSION_NUMBER */
  gcc_builtin_varargs_in_generated_code =
                                         GCC_BUILTIN_VARARGS_IN_GENERATED_CODE;
#if BACK_END_IS_CP_GEN_BE && CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT
  /* Will be set to MSVC_IS_GENERATED_CODE_TARGET if the source dialect is
     Microsoft mode. */
  msvc_is_generated_code_target = FALSE;
#else /* !(BACK_END_IS_CP_GEN_BE && CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT) */
  msvc_is_generated_code_target = MSVC_IS_GENERATED_CODE_TARGET;
#endif /* BACK_END_IS_CP_GEN_BE && CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT */
  msvc_target_version_number = MSVC_TARGET_VERSION_NUMBER;
  microsoft_dialect_is_generated_code_target =
                                    MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
  cp_gen_be_target_matches_source_dialect =
                                       CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT;
#endif /* BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_C_GEN_BE
  use_empty_struct_in_generated_c = USE_EMPTY_STRUCT_IN_GENERATED_C;
#endif /* BACK_END_IS_C_GEN_BE */
  exc_spec_in_func_type = FALSE;
  targ_minimum_pack_alignment = TARG_MINIMUM_PACK_ALIGNMENT;
  targ_maximum_pack_alignment = TARG_MAXIMUM_PACK_ALIGNMENT;
  bitint_maxwidth_value = DEFAULT_BITINT_MAXWIDTH_VALUE;
}  /* target_early_init */


void target_one_time_init(void)
/*
Do one-time initialization of variables related to the target.  This is
executed once after command-line processing, and not again for each source
file.  Called after target configuration (if any) has been determined.
*/
{
  /* Record character sizes in an array that can be indexed by character
     kind. */
  init_character_sizes();
  character_size[(int)chk_char] = 1;
  character_size[(int)chk_wchar_t] = targ_sizeof_wchar_t;
  character_size[(int)chk_char8_t] = 1;
  character_size[(int)chk_char16_t] = targ_sizeof_char16_t;
  character_size[(int)chk_char32_t] = targ_sizeof_char32_t;
  if (targ_dual_alignments_for_builtin_types) {
    init_field_alignment_tables();
  }  /* if */
#if INT128_EXTENSIONS_ALLOWED
  targ_sizeof_largest_integer = targ_sizeof_int128;
#else /* !INT128_EXTENSIONS_ALLOWED */
#if LONG_LONG_ALLOWED
  targ_sizeof_largest_integer = targ_sizeof_long_long;
#else /* !LONG_LONG_ALLOWED */
  targ_sizeof_largest_integer = targ_sizeof_long;
#endif /* !LONG_LONG_ALLOWED */
#endif /* INT128_EXTENSIONS_ALLOWED */
#if CHECKING
  check_target_configuration();
#endif /* CHECKING */
}  /* target_one_time_init */


void target_init(void)
/*
Initialize target machine characteristics.  This is the per-compilation
initialization and must be done after command-line processing.
*/
{
  /* The signedness of characters can be set on the command line. */
  set_plain_char_int_kind(targ_has_signed_chars);
  /* Set the element of int_kind_is_signed that corresponds to "plain"
     char. */
  int_kind_is_signed[(int)ik_char] = targ_has_signed_chars;
  /* String literals are shared, except in pcc mode and in some Microsoft
     modes.  (pcc and Microsoft C allow string literals to be overwritten.) */
  string_literals_shared = (C_dialect != C_dialect_pcc &&
                            !(microsoft_mode &&
                              (C_mode() || microsoft_version < 1910)));
  /* Determine the integer kind for the largest integer types. */
#if LONG_LONG_ALLOWED
  targ_intmax_kind = (an_integer_kind)ik_long_long;
  targ_uintmax_kind = (an_integer_kind)ik_unsigned_long_long;
#else /* !LONG_LONG_ALLOWED */
  targ_intmax_kind = (an_integer_kind)ik_long;
  targ_uintmax_kind = (an_integer_kind)ik_unsigned_long;
#endif /* LONG_LONG_ALLOWED */
  /* Determine the maximum size of a class object. */
  if (targ_max_class_object_size == 0) {
    targ_max_class_object_size = targ_size_t_max;
  }  /* if */
  /* Determine the maximum base class offset. */
  if (targ_max_base_class_offset == 0) {
    targ_max_base_class_offset = targ_size_t_max;
#if DO_IL_LOWERING
  } else {
    /* Compute the maximum base class offset value that will fit in the
       delta field of a virtual function table. */
    a_targ_size_t		size;
    a_host_large_unsigned	temp;
    a_targ_alignment		alignment;
    a_host_large_unsigned	bits;

    /* Get the size of whatever integer kind is associated with delta field
       of the virtual function table. */
    get_integer_size_and_alignment(targ_delta_int_kind, &size, &alignment);
    /* Now given the size, compute the maximum integer value it will
       accommodate. */
    bits = size * targ_char_bit;
    if (int_kind_is_signed[targ_delta_int_kind]) bits -= 1;
    temp = ~((~(a_host_large_unsigned)0) << bits);
    if (temp > (a_host_large_unsigned)targ_size_t_max) {
      /* It shouldn't exceed the maximum that can fit in a_targ_size_t. */
      temp = (a_host_large_unsigned)targ_size_t_max;
    }  /* if */
    if (temp >= targ_max_base_class_offset) {
      /* Don't increase the maximum offset beyond what was specified. */
    } else {
      /* Set the maximum offset to the computed value. */
      targ_max_base_class_offset = (a_targ_size_t)temp;
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  init_microsoft_sized_int_types();
  /* Determine whether the target is a 64-bit target. */
  { a_targ_size_t     size;
    a_targ_alignment  alignment;
    get_integer_size_and_alignment(targ_size_t_int_kind, &size, &alignment);
    is_64bit_target = (size * targ_char_bit == 64);
  }
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  always_fold_calls_to_builtin_constant_p =
                              DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P;
}  /* target_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

