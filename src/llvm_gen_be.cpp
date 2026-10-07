#include <type_traits>

/* llvm_gen_be.cpp - LLVM IR-generating back end core */
#include "basic_hdrs.h"
#include "fe_common.h"
#include "il_write.h"
#include "il_read.h"


#include "llvm_gen_be_internal.h"
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

BEGIN_EDG_NAMESPACE

LLVMBackendState* be_state = nullptr;

std::string build_data_layout() {
  std::string dl = "";
  // Endianness
#if targ_little_endian
  dl += "e-";
#else
  dl += "E-";
#endif

#if 0
  dl += "m:o-";
#elif TARG_MICROSOFT
  dl += "m:w-";
#else
  dl += "m:e-";
#endif

  // Pointer size
  dl += "p:" + std::to_string((int)(targ_sizeof_pointer * targ_char_bit)) + ":" + std::to_string((int)(targ_sizeof_pointer * targ_char_bit)) + "-";

  // Data layout construction based on EDG target macros
  dl += "i8:" + std::to_string((int)(targ_char_bit)) + "-";
  dl += "i16:" + std::to_string((int)(targ_sizeof_short * targ_char_bit)) + "-";
  dl += "i32:" + std::to_string((int)(targ_sizeof_int * targ_char_bit)) + "-";
  dl += "i64:" + std::to_string((int)(targ_sizeof_long_long * targ_char_bit)) + "-";
  dl += "f32:" + std::to_string((int)(targ_sizeof_float * targ_char_bit)) + "-";
  dl += "f64:" + std::to_string((int)(targ_sizeof_double * targ_char_bit)) + "-";
  dl += "f128:" + std::to_string((int)(targ_sizeof_long_double * targ_char_bit));

  return dl;
}

void generate_llvm_output_file(const char* base_name) {
  std::string file_name;
  bool emit_bc = false; // Add CLI toggle logic later if needed

  if (gen_llvm_file_name) {
    file_name = gen_llvm_file_name;
  } else if (base_name) {
    const char* derived = derived_name(base_name, emit_bc ? ".bc" : ".ll");
    if (derived) file_name = derived;
  }

  if (file_name.empty()) {
    file_name = emit_bc ? "output.bc" : "output.ll";
  }

  std::string err_str;
  llvm::raw_string_ostream os(err_str);
  if (llvm::verifyModule(*be_state->module, &os)) {
    // If it fails, report via EDG error handling
    internal_error(err_str.c_str());
  }

  std::error_code EC;
  llvm::raw_fd_ostream dest(file_name, EC, llvm::sys::fs::OF_None);

  if (EC) {
    internal_error(file_name.c_str());
    return;
  }

  if (emit_bc) {
    llvm::WriteBitcodeToFile(*be_state->module, dest);
  } else {
    be_state->module->print(dest, nullptr);
  }
}

void llvm_gen_be(void)
{
  if (!be_state) {
    be_state = new LLVMBackendState();
  }
  be_state->context = std::make_unique<llvm::LLVMContext>();
  be_state->module = std::make_unique<llvm::Module>("edg_module", *be_state->context);
  be_state->builder = std::make_unique<llvm::IRBuilder<>>(*be_state->context);
  
  // Set TargetTriple
#if 0
  #if TARG_AARCH64
    be_state->module->setTargetTriple(llvm::Triple("aarch64-apple-darwin"));
  #else
    be_state->module->setTargetTriple(llvm::Triple("x86_64-apple-darwin"));
  #endif
#elif TARG_MICROSOFT
  #if TARG_AARCH64
    be_state->module->setTargetTriple(llvm::Triple("aarch64-pc-windows-msvc"));
  #else
    be_state->module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
  #endif
#else
  #if TARG_AARCH64
    be_state->module->setTargetTriple(llvm::Triple("aarch64-unknown-linux-gnu"));
  #else
    be_state->module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
  #endif
#endif

  be_state->module->setDataLayout(build_data_layout());

  emit_global_variables();
  emit_function_declarations();
  emit_function_definitions();
  emit_global_ctors_and_dtors();

  generate_llvm_output_file(primary_source_file_name);
}

#if !STANDALONE_UTILITY_PROGRAM

void back_end(void)
/*
Simple "back end" that generates LLVM IR. This version is for use as a
subroutine called in the same program as the front end.
*/
{
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* If the intermediate language was written to a file, read it back in. */
  primary_source_file_name = NULL;
  if (skip_il_read) {
    /* The IL should still be in memory. */
  } else {
    il_read(f_il_output);
  }  /* if */
  primary_source_file_name = il_header.primary_source_file->file_name;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

  /* Generate LLVM IR. */
  llvm_gen_be();
}

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MAKE_FRONT_END_CALLABLE

void llvm_gen_be_cleanup(void)
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason. It performs any cleanup operations
required.
*/
{
  if (be_state) {
    be_state->builder.reset();
    be_state->module.reset();
    be_state->context.reset();
    be_state->type_cache.clear();
    be_state->local_vars.clear();
    be_state->break_blocks.clear();
    be_state->continue_blocks.clear();
    be_state->current_landing_pads.clear();
    be_state->label_blocks.clear();
    be_state->case_blocks.clear();
    delete be_state;
    be_state = nullptr;
  }
}

#endif /* MAKE_FRONT_END_CALLABLE */

END_EDG_NAMESPACE
#endif /* BACK_END_IS_LLVM_GEN_BE */
