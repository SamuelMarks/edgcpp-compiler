/**
 * @file llvm_gen_be_opt.cpp
 * @brief Implementation of the LLVM Optimization Pipeline configuration and PassBuilder integration.
 * @details Provides functions for configuring the LLVM New Pass Manager,
 * specifying optimization levels, and assembling pass pipelines.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_opt.h"
#include <string.h>

#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/Error.h>

BEGIN_EDG_NAMESPACE

llvm_gen_be_error_t parse_opt_level_string(const char* opt_str, llvm_opt_options_t* out_opts) noexcept {
  if (!opt_str || !out_opts) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  out_opts->opt_level = llvm_opt_level_t::O0;
  out_opts->vectorize_loops = false;
  out_opts->vectorize_slp = false;
  out_opts->inlining_threshold = 0;
  out_opts->use_lto = false;

  if (strcmp(opt_str, "O0") == 0) {
    out_opts->opt_level = llvm_opt_level_t::O0;
  } else if (strcmp(opt_str, "O1") == 0) {
    out_opts->opt_level = llvm_opt_level_t::O1;
    out_opts->vectorize_slp = true;
  } else if (strcmp(opt_str, "O2") == 0) {
    out_opts->opt_level = llvm_opt_level_t::O2;
    out_opts->vectorize_loops = true;
    out_opts->vectorize_slp = true;
  } else if (strcmp(opt_str, "O3") == 0) {
    out_opts->opt_level = llvm_opt_level_t::O3;
    out_opts->vectorize_loops = true;
    out_opts->vectorize_slp = true;
    out_opts->inlining_threshold = 275;
  } else if (strcmp(opt_str, "Os") == 0) {
    out_opts->opt_level = llvm_opt_level_t::Os;
    out_opts->vectorize_slp = true;
  } else if (strcmp(opt_str, "Oz") == 0) {
    out_opts->opt_level = llvm_opt_level_t::Oz;
  } else {
    return llvm_gen_be_error_t::invalid_argument;
  }

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t run_optimization_pipeline(llvm::Module* module, const llvm_opt_options_t* opts) noexcept {
  if (!module || !opts) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::OptimizationLevel llvm_opt_level;
  switch (opts->opt_level) {
    case llvm_opt_level_t::O0: llvm_opt_level = llvm::OptimizationLevel::O0; break;
    case llvm_opt_level_t::O1: llvm_opt_level = llvm::OptimizationLevel::O1; break;
    case llvm_opt_level_t::O2: llvm_opt_level = llvm::OptimizationLevel::O2; break;
    case llvm_opt_level_t::O3: llvm_opt_level = llvm::OptimizationLevel::O3; break;
    case llvm_opt_level_t::Os: llvm_opt_level = llvm::OptimizationLevel::O2; break; // Map Os to O2 in LLVM 23+
    case llvm_opt_level_t::Oz: llvm_opt_level = llvm::OptimizationLevel::O2; break; // Map Oz to O2 in LLVM 23+
    default: llvm_opt_level = llvm::OptimizationLevel::O0; break;
  }

  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;

  llvm::PassInstrumentationCallbacks pic;
  llvm::StandardInstrumentations si(module->getContext(), /*DebugLogging=*/false);
  si.registerCallbacks(pic, &mam);

  llvm::PipelineTuningOptions pto;
  pto.LoopVectorization = opts->vectorize_loops;
  pto.SLPVectorization = opts->vectorize_slp;

  llvm::PassBuilder pb(nullptr, pto, std::nullopt, &pic);

  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);

  llvm::ModulePassManager mpm;
  if (opts->opt_level == llvm_opt_level_t::O0) {
    mpm = pb.buildO0DefaultPipeline(llvm_opt_level);
  } else {
    if (opts->use_lto) {
      mpm = pb.buildLTOPreLinkDefaultPipeline(llvm_opt_level);
    } else {
      mpm = pb.buildPerModuleDefaultPipeline(llvm_opt_level);
    }
  }

  mpm.run(*module, mam);

  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
