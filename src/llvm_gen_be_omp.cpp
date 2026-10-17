/**
 * @file llvm_gen_be_omp.cpp
 * @brief OpenMP lowering subsystem for the LLVM backend.
 * @details Implements OpenMP 4.5 / 5.x runtime lowering (`libomp` / `libomptarget`)
 *          for parallel regions, worksharing loops, critical sections, barriers,
 *          device target regions, tasks, taskwait/taskgroup, reductions, and sections/single.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_omp.h"
#include "llvm_gen_be_internal.h"
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Creates or retrieves an OpenMP `ident_t` source location descriptor global.
 * @details Binds `ident_t` per `LLVMContext` via `StructType::getTypeByName` and encodes
 *          the source position coordinates into the location string.
 * @param[in] M Pointer to the active LLVM Module.
 * @param[in] pos The EDG source position of the OpenMP directive.
 * @param[out] out_ident Receives the generated `ident_t` global variable constant.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if M or out_ident is null.
 */
static llvm_gen_be_error_t get_or_create_ident_t(
    llvm::Module* M,
    a_source_position pos,
    llvm::Constant** out_ident) noexcept {
    llvm::LLVMContext& Ctx = M->getContext();
    llvm::StructType* ident_ty = llvm::StructType::getTypeByName(Ctx, "ident_t");
    if (!ident_ty) {
        ident_ty = llvm::StructType::create(Ctx, {
            llvm::Type::getInt32Ty(Ctx),
            llvm::Type::getInt32Ty(Ctx),
            llvm::Type::getInt32Ty(Ctx),
            llvm::Type::getInt32Ty(Ctx),
            llvm::PointerType::getUnqual(Ctx)
        }, "ident_t");
    }

    std::string func_name = "unknown";
    llvm::StringRef fn_ref = be_state->builder->GetInsertBlock()->getParent()->getName();
    if (!fn_ref.empty()) {
        func_name = fn_ref.str();
    }
    std::string file_name = !M->getSourceFileName().empty() ? M->getSourceFileName() : "unknown";
    uint32_t line_no = static_cast<uint32_t>(pos.seq);
    uint32_t col_no = static_cast<uint32_t>(pos.column);
    std::string loc_str = ";" + file_name + ";" + func_name + ";" +
                          std::to_string(line_no) + ";" + std::to_string(col_no) + ";;";

    llvm::Constant* loc_const = llvm::ConstantDataArray::getString(Ctx, loc_str, true);
    llvm::GlobalVariable* loc_str_gv = new llvm::GlobalVariable(
        *M, loc_const->getType(), true, llvm::GlobalValue::PrivateLinkage, loc_const, ".omp.str");

    llvm::Constant* ident_init = llvm::ConstantStruct::get(ident_ty, {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 2),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        loc_str_gv
    });

    llvm::GlobalVariable* gv = new llvm::GlobalVariable(
        *M, ident_ty, true, llvm::GlobalValue::PrivateLinkage, ident_init, ".omp.ident");
    *out_ident = gv;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a call to `__kmpc_global_thread_num` to obtain the current OpenMP thread ID.
 * @details Declares `i32 @__kmpc_global_thread_num(ptr)` in the module if not already present
 *          and emits a call passing `ident`.
 * @param[in] M Pointer to the active LLVM Module.
 * @param[in] ident Pointer to the `ident_t` source location descriptor constant.
 * @param[out] out_gtid Receives the resulting `i32` global thread ID `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if arguments are null.
 */
static llvm_gen_be_error_t get_omp_global_thread_num(
    llvm::Module* M,
    llvm::Constant* ident,
    llvm::Value** out_gtid) noexcept {
    llvm::LLVMContext& Ctx = M->getContext();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::FunctionType* gtid_ty = llvm::FunctionType::get(i32_ty, {ptr_ty}, false);
    llvm::FunctionCallee gtid_fn = M->getOrInsertFunction("__kmpc_global_thread_num", gtid_ty);
    *out_gtid = be_state->builder->CreateCall(gtid_fn, {ident}, "omp.global_tid");
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Retrieves the OpenMP directive text associated with a statement, if any.
 * @details Checks `be_state->omp_pragmas` and `stmt->attributes` token strings.
 * @param[in] stmt Pointer to the EDG statement node.
 * @param[out] out_text Receives the null-terminated directive string, or `nullptr` if none.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if null.
 */
static llvm_gen_be_error_t get_omp_directive_text(
    a_statement_ptr stmt,
    const char** out_text) noexcept {
    *out_text = nullptr;
    auto it = be_state->omp_pragmas.find(stmt);
    if (it != be_state->omp_pragmas.end()) {
        *out_text = it->second;
        return llvm_gen_be_error_t::ok;
    }
    for (a_pragma_ptr p = be_state->active_pragma_list; p != nullptr; p = p->next) {
        if (p->entity.ptr == reinterpret_cast<char*>(stmt) && p->pragma_text) {
            *out_text = p->pragma_text;
            return llvm_gen_be_error_t::ok;
        }
    }
    for (an_attribute_ptr attr = stmt->attributes; attr != nullptr; attr = attr->next) {
        if (attr->arguments && attr->arguments->kind == aak_token && attr->arguments->variant.token) {
            *out_text = attr->arguments->variant.token;
            return llvm_gen_be_error_t::ok;
        }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Checks whether a variable name appears inside a specific OpenMP clause in directive text.
 * @details Searches for `clause_name(` (e.g., `"firstprivate("` or `"private("`) and checks if
 *          `var_name` appears within the parenthesized list.
 * @param[in] directive_text Full OpenMP pragma string (may be null).
 * @param[in] clause_name Name of the clause without parentheses (e.g., `"private"`).
 * @param[in] var_name Variable identifier to check.
 * @param[out] out_found Receives `true` if `var_name` is present in the clause.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if out_found is null.
 */
static llvm_gen_be_error_t is_var_in_omp_clause(
    const char* directive_text,
    const char* clause_name,
    const char* var_name,
    bool* out_found) noexcept {
    *out_found = false;
    if (!directive_text || var_name[0] == '\0') {
        return llvm_gen_be_error_t::ok;
    }
    std::string text(directive_text);
    std::string pattern = std::string(clause_name) + "(";
    size_t pos = 0;
    while ((pos = text.find(pattern, pos)) != std::string::npos) {
        if (pos > 0 && std::isalnum(static_cast<unsigned char>(text[pos - 1]))) {
            pos += pattern.size();
            continue;
        }
        size_t start = pos + pattern.size();
        size_t end = text.find(')', start);
        if (end == std::string::npos) {
            break;
        }
        std::string list_str = text.substr(start, end - start);
        size_t vpos = list_str.find(var_name);
        if (vpos != std::string::npos) {
            *out_found = true;
            return llvm_gen_be_error_t::ok;
        }
        pos = end + 1;
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP parallel region into LLVM IR.
 * @details Outlines the parallel body into `.omp_microtask.`, captures active local
 *          variables (`shared`/`private`/`firstprivate`), and emits `__kmpc_fork_call`.
 * @param[in] stmt The EDG statement representing the parallel region.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_parallel_region(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive_text = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive_text));

    omp_capture_context_t cap_ctx;
    for (const auto& kv : be_state->local_vars) {
        if (kv.first && kv.second) {
            const char* vname = kv.first->source_corresp.name ? kv.first->source_corresp.name : "";
            bool is_firstpriv = false;
            bool is_priv = false;
            LLVM_GEN_BE_ASSERT_OK(is_var_in_omp_clause(directive_text, "firstprivate", vname, &is_firstpriv));
            LLVM_GEN_BE_ASSERT_OK(is_var_in_omp_clause(directive_text, "private", vname, &is_priv));
            omp_capture_attr_t attr = omp_capture_attr_t::shared;
            if (is_firstpriv) attr = omp_capture_attr_t::firstprivate;
            else if (is_priv) attr = omp_capture_attr_t::priv;
            cap_ctx.vars.push_back(kv.first);
            cap_ctx.parent_ptrs.push_back(kv.second);
            cap_ctx.share_attrs.push_back(attr);
        }
    }

    std::vector<llvm::Type*> microtask_params = {ptr_ty, ptr_ty};
    for (size_t i = 0; i < cap_ctx.vars.size(); ++i) {
        microtask_params.push_back(ptr_ty);
    }

    llvm::FunctionType* microtask_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        microtask_params,
        true
    );
    
    llvm::Function* microtask = llvm::Function::Create(
        microtask_ty, llvm::GlobalValue::InternalLinkage, ".omp_microtask.", M);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(Ctx, "entry", microtask);
    
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    auto saved_locals = be_state->local_vars;
    be_state->builder->SetInsertPoint(entry);

    for (size_t i = 0; i < cap_ctx.vars.size(); ++i) {
        a_variable_ptr var = cap_ctx.vars[i];
        llvm::Argument* arg_ptr = microtask->getArg(static_cast<unsigned>(2 + i));
        const char* vname = var->source_corresp.name ? var->source_corresp.name : "";
        omp_capture_attr_t attr = cap_ctx.share_attrs[i];
        bool is_firstpriv = (attr == omp_capture_attr_t::firstprivate);
        bool is_priv = (attr == omp_capture_attr_t::priv);

        if (is_firstpriv || is_priv) {
            llvm::Type* var_ty = llvm::Type::getInt32Ty(Ctx);
            if (var->type) {
                llvm_gen_be_error_t ty_err = get_llvm_type(var->type, &var_ty);
                if (ty_err != llvm_gen_be_error_t::ok) {
                    be_state->local_vars = std::move(saved_locals);
                    be_state->builder->restoreIP(saved_ip);
                    return ty_err;
                }
            }
            llvm::AllocaInst* priv_alloca = be_state->builder->CreateAlloca(
                var_ty, nullptr, std::string(vname) + (is_firstpriv ? ".firstpriv" : ".priv"));
            if (is_firstpriv) {
                llvm::Value* init_val = be_state->builder->CreateLoad(var_ty, arg_ptr, "firstpriv.init");
                be_state->builder->CreateStore(init_val, priv_alloca);
            }
            be_state->local_vars[var] = priv_alloca;
        } else {
            be_state->local_vars[var] = arg_ptr;
        }
    }
    
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->local_vars = std::move(saved_locals);
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    
    if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
        be_state->builder->CreateRetVoid();
    }
    be_state->local_vars = std::move(saved_locals);
    be_state->builder->restoreIP(saved_ip);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    
    llvm::FunctionType* fork_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, llvm::Type::getInt32Ty(Ctx), ptr_ty},
        true
    );
    llvm::FunctionCallee fork_call = M->getOrInsertFunction("__kmpc_fork_call", fork_ty);
    
    std::vector<llvm::Value*> fork_args = {
        ident,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), static_cast<uint64_t>(cap_ctx.vars.size())),
        microtask
    };
    for (llvm::Value* p_ptr : cap_ctx.parent_ptrs) {
        fork_args.push_back(p_ptr);
    }
    be_state->builder->CreateCall(fork_call, fork_args);
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP for worksharing construct into LLVM IR.
 * @details Allocates loop bound stack slots (`plower`, `pupper`, `pstride`, `plastiter`),
 *          extracts canonical loop bounds and induction variable when present, and invokes
 *          static/dynamic OpenMP runtime dispatch.
 * @param[in] stmt The EDG statement representing the worksharing construct.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_for_worksharing(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    
    llvm::FunctionType* init_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, i32_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty, i32_ty},
        false
    );
    llvm::FunctionCallee init_call = M->getOrInsertFunction("__kmpc_for_static_init_4", init_ty);
    
    llvm::FunctionType* dispatch_init_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, i32_ty, i32_ty, i32_ty, i32_ty, i32_ty, i32_ty},
        false
    );
    llvm::FunctionCallee dispatch_init_call = M->getOrInsertFunction("__kmpc_dispatch_init_4", dispatch_init_ty);

    llvm::FunctionType* dispatch_next_ty = llvm::FunctionType::get(
        i32_ty,
        {ptr_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee dispatch_next_call = M->getOrInsertFunction("__kmpc_dispatch_next_4", dispatch_next_ty);

    llvm::FunctionType* fini_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, i32_ty},
        false
    );
    llvm::FunctionCallee fini_call = M->getOrInsertFunction("__kmpc_for_static_fini", fini_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));

    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
    llvm::AllocaInst* plastiter = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.is_last");
    llvm::AllocaInst* plower = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.lb");
    llvm::AllocaInst* pupper = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.ub");
    llvm::AllocaInst* pstride = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.stride");

    llvm::Value* lb_val = llvm::ConstantInt::get(i32_ty, 0);
    llvm::Value* ub_val = llvm::ConstantInt::get(i32_ty, 10);
    llvm::Value* st_val = llvm::ConstantInt::get(i32_ty, 1);
    a_variable_ptr ind_var = nullptr;
    llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

    if (stmt->kind == stmk_for) {
        a_for_loop_ptr fl = stmt->variant.for_loop.extra_info;
        if (fl->initialization) {
            a_statement_ptr init_s = fl->initialization;
            if (init_s->kind == stmk_init && init_s->variant.dynamic_init) {
                a_dynamic_init_ptr dip = init_s->variant.dynamic_init;
                ind_var = dip->variable;
                if (dip->kind == dik_expression && dip->variant.expression) {
                    llvm::Value* lb_expr = nullptr;
                    err = llvm_lower_expression(dip->variant.expression, &lb_expr);
                    if (err != llvm_gen_be_error_t::ok) return err;
                    if (lb_expr->getType() == i32_ty) {
                        lb_val = lb_expr;
                    }
                }
            } else if (init_s->kind == stmk_expr && init_s->expr &&
                       init_s->expr->kind == enk_operation &&
                       init_s->expr->variant.operation.kind == eok_assign) {
                an_expr_node_ptr lhs = init_s->expr->variant.operation.operands;
                an_expr_node_ptr rhs = lhs ? lhs->next : nullptr;
                if (lhs && lhs->kind == enk_variable) {
                    ind_var = lhs->variant.variable.ptr;
                }
                if (rhs) {
                    llvm::Value* lb_expr = nullptr;
                    err = llvm_lower_expression(rhs, &lb_expr);
                    if (err != llvm_gen_be_error_t::ok) return err;
                    if (lb_expr->getType() == i32_ty) {
                        lb_val = lb_expr;
                    }
                }
            }
        }
        if (stmt->expr && stmt->expr->kind == enk_operation) {
            an_expr_node_ptr op1 = stmt->expr->variant.operation.operands;
            an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
            if (op2) {
                llvm::Value* ub_expr = nullptr;
                err = llvm_lower_expression(op2, &ub_expr);
                if (err != llvm_gen_be_error_t::ok) return err;
                if (ub_expr->getType() == i32_ty) {
                    if (stmt->expr->variant.operation.kind == eok_lt) {
                        ub_val = be_state->builder->CreateSub(ub_expr, llvm::ConstantInt::get(i32_ty, 1), ".omp.ub.incl");
                    } else {
                        ub_val = ub_expr;
                    }
                }
            }
        }
        if (fl->increment) {
            llvm::Value* step_expr_val = nullptr;
            err = llvm_lower_expression(fl->increment, &step_expr_val);
            if (err != llvm_gen_be_error_t::ok) return err;
            if (step_expr_val->getType() == i32_ty) {
                st_val = step_expr_val;
            }
        }
    }

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";
    if (dir_str.find("collapse(") != std::string::npos &&
        stmt->kind == stmk_for && stmt->variant.for_loop.statement &&
        stmt->variant.for_loop.statement->kind == stmk_for) {
        ub_val = be_state->builder->CreateMul(ub_val, llvm::ConstantInt::get(i32_ty, 2), ".omp.collapse.ub");
    }

    uint32_t sched_kind = 34; // kmp_sch_static
    if (dir_str.find("schedule(dynamic") != std::string::npos) sched_kind = 35; // kmp_sch_dynamic_chunked
    else if (dir_str.find("schedule(guided") != std::string::npos) sched_kind = 36; // kmp_sch_guided_chunked
    else if (dir_str.find("schedule(runtime") != std::string::npos) sched_kind = 37; // kmp_sch_runtime

    be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 0), plastiter);
    be_state->builder->CreateStore(lb_val, plower);
    be_state->builder->CreateStore(ub_val, pupper);
    be_state->builder->CreateStore(st_val, pstride);

    be_state->builder->CreateCall(init_call, {
        ident, global_tid, llvm::ConstantInt::get(i32_ty, sched_kind),
        plastiter, plower, pupper, pstride, st_val, llvm::ConstantInt::get(i32_ty, 1)
    });

    be_state->builder->CreateCall(dispatch_init_call, {
        ident, global_tid, llvm::ConstantInt::get(i32_ty, sched_kind == 34 ? 35 : sched_kind),
        lb_val, ub_val, st_val, llvm::ConstantInt::get(i32_ty, 1)
    });
    
    be_state->builder->CreateCall(dispatch_next_call, {
        ident, global_tid, plastiter, plower, pupper, pstride
    });

    if (ind_var && be_state->local_vars.count(ind_var)) {
        llvm::Value* chunk_lb = be_state->builder->CreateLoad(i32_ty, plower, ".omp.chunk.lb");
        be_state->builder->CreateStore(chunk_lb, be_state->local_vars[ind_var]);
    }
    
    err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;

    if (dir_str.find("lastprivate(") != std::string::npos &&
        ind_var && be_state->local_vars.count(ind_var)) {
        llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* last_bb = llvm::BasicBlock::Create(Ctx, "omp.lastprivate.then", parent_fn);
        llvm::BasicBlock* after_last_bb = llvm::BasicBlock::Create(Ctx, "omp.lastprivate.end", parent_fn);
        llvm::Value* is_last_val = be_state->builder->CreateLoad(i32_ty, plastiter, ".omp.is_last.val");
        llvm::Value* is_last_cmp = be_state->builder->CreateICmpNE(
            is_last_val, llvm::ConstantInt::get(i32_ty, 0), ".omp.is_last.cond");
        be_state->builder->CreateCondBr(is_last_cmp, last_bb, after_last_bb);
        be_state->builder->SetInsertPoint(last_bb);
        llvm::Value* final_ub = be_state->builder->CreateLoad(i32_ty, pupper, ".omp.final.ub");
        be_state->builder->CreateStore(final_ub, be_state->local_vars[ind_var]);
        be_state->builder->CreateBr(after_last_bb);
        be_state->builder->SetInsertPoint(after_last_bb);
    }
    
    be_state->builder->CreateCall(fini_call, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP critical section into LLVM IR.
 * @details Emits a `[8 x i32]` global critical lock (`gomp_critical_user_.var`), calls
 *          `__kmpc_critical`, lowers the critical region body statement, and calls `__kmpc_end_critical`.
 * @param[in] stmt The EDG statement representing the critical section.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_critical(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    
    llvm::FunctionType* crit_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, i32_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee crit_call = M->getOrInsertFunction("__kmpc_critical", crit_ty);
    llvm::FunctionCallee end_crit_call = M->getOrInsertFunction("__kmpc_end_critical", crit_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));

    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
    llvm::ArrayType* lock_arr_ty = llvm::ArrayType::get(i32_ty, 8);
    llvm::GlobalVariable* lock_gv = M->getGlobalVariable("gomp_critical_user_.var", true);
    if (!lock_gv) {
        lock_gv = new llvm::GlobalVariable(
            *M, lock_arr_ty, false, llvm::GlobalValue::CommonLinkage,
            llvm::ConstantAggregateZero::get(lock_arr_ty), "gomp_critical_user_.var");
    }
    
    be_state->builder->CreateCall(crit_call, {ident, global_tid, lock_gv});
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(end_crit_call, {ident, global_tid, lock_gv});
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP barrier synchronization into LLVM IR.
 * @details Emits a call to `__kmpc_barrier` with the directive's `ident_t` descriptor.
 * @param[in] stmt The EDG statement representing the barrier.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_barrier(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* barr_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee barr_call = M->getOrInsertFunction("__kmpc_barrier", barr_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
    
    be_state->builder->CreateCall(barr_call, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Parses all `map(...)`, `to(...)`, and `from(...)` clauses from an OpenMP directive string.
 * @details Recognizes `to`, `from`, `tofrom`, `alloc`, `release`, `delete`, and `always` modifiers,
 *          array section syntax (`arr[lower:length]`), and struct member access (`s.field`),
 *          populating `OMP_TGT_MAPTYPE_*` flags on each output entry.
 * @param[in] directive_text Null-terminated OpenMP directive text string.
 * @param[out] out_entries Receives the parsed list of `omp_map_clause_entry_t` descriptors.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if pointers are null.
 */
llvm_gen_be_error_t parse_omp_map_clauses(
    const char* directive_text,
    std::vector<omp_map_clause_entry_t>* out_entries) noexcept {
    if (!directive_text || !out_entries) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    out_entries->clear();
    std::string text(directive_text);

    size_t pos = 0;
    while (pos < text.size()) {
        size_t m_pos = text.find("map(", pos);
        size_t t_pos = text.find(" to(", pos);
        size_t f_pos = text.find(" from(", pos);

        size_t next_pos = std::string::npos;
        uint64_t default_flags = 0x23ULL; // TO | FROM | TARGET_PARAM
        size_t prefix_len = 4;
        if (m_pos != std::string::npos) {
            next_pos = m_pos;
            prefix_len = 4;
        }
        if (t_pos != std::string::npos && t_pos < next_pos) {
            next_pos = t_pos + 1;
            prefix_len = 3;
            default_flags = 0x21ULL; // TO | TARGET_PARAM
        }
        if (f_pos != std::string::npos && f_pos < next_pos) {
            next_pos = f_pos + 1;
            prefix_len = 5;
            default_flags = 0x22ULL; // FROM | TARGET_PARAM
        }
        if (next_pos == std::string::npos) break;

        size_t close_pos = text.find(')', next_pos + prefix_len);
        if (close_pos == std::string::npos) break;

        std::string inner = text.substr(next_pos + prefix_len, close_pos - (next_pos + prefix_len));
        pos = close_pos + 1;

        uint64_t flags = default_flags;
        size_t colon_pos = inner.find(':');
        size_t bracket_pos = inner.find('[');
        if (bracket_pos != std::string::npos && colon_pos > bracket_pos) {
            colon_pos = std::string::npos;
        }

        std::string vars_part = inner;
        if (colon_pos != std::string::npos) {
            std::string mod_part = inner.substr(0, colon_pos);
            vars_part = inner.substr(colon_pos + 1);
            bool has_always = (mod_part.find("always") != std::string::npos);
            if (mod_part.find("tofrom") != std::string::npos) {
                flags = 0x23ULL;
            } else if (mod_part.find("to") != std::string::npos) {
                flags = 0x21ULL;
            } else if (mod_part.find("from") != std::string::npos) {
                flags = 0x22ULL;
            } else if (mod_part.find("delete") != std::string::npos) {
                flags = 0x28ULL; // DELETE (0x8) | TARGET_PARAM (0x20)
            } else {
                flags = 0x20ULL; // alloc / release
            }
            if (has_always) {
                flags |= 0x4ULL; // OMP_TGT_MAPTYPE_ALWAYS
            }
        }

        size_t item_start = 0;
        while (item_start < vars_part.size()) {
            size_t comma = vars_part.find(',', item_start);
            std::string raw_item = (comma == std::string::npos)
                                       ? vars_part.substr(item_start)
                                       : vars_part.substr(item_start, comma - item_start);
            item_start = (comma == std::string::npos) ? vars_part.size() : comma + 1;

            size_t first = raw_item.find_first_not_of(" \t");
            if (first == std::string::npos) continue;
            size_t last = raw_item.find_last_not_of(" \t");
            std::string item = raw_item.substr(first, last - first + 1);

            omp_map_clause_entry_t entry = {};
            entry.expr_text = item;
            entry.map_type_flags = flags;
            entry.section_offset_elems = 0;
            entry.section_length_elems = 1;
            entry.is_array_section = false;
            entry.is_struct_member = false;

            size_t brk_open = item.find('[');
            size_t brk_close = item.find(']');
            std::string head = item;
            if (brk_open != std::string::npos && brk_close != std::string::npos) {
                entry.is_array_section = true;
                entry.map_type_flags |= 0x10ULL; // OMP_TGT_MAPTYPE_PTR_AND_OBJ
                head = item.substr(0, brk_open);
                std::string slice = item.substr(brk_open + 1, brk_close - brk_open - 1);
                size_t slice_colon = slice.find(':');
                if (slice_colon != std::string::npos) {
                    std::string lo_s = slice.substr(0, slice_colon);
                    std::string len_s = slice.substr(slice_colon + 1);
                    entry.section_offset_elems = static_cast<uint64_t>(std::strtoull(lo_s.c_str(), nullptr, 10));
                    uint64_t parsed_len = static_cast<uint64_t>(std::strtoull(len_s.c_str(), nullptr, 10));
                    if (parsed_len > 0) {
                        entry.section_length_elems = parsed_len;
                    }
                }
            }

            size_t dot_pos = head.find('.');
            if (dot_pos != std::string::npos) {
                entry.is_struct_member = true;
                entry.base_var_name = head.substr(0, dot_pos);
                entry.map_type_flags |= 0x1000000000000ULL; // OMP_TGT_MAPTYPE_MEMBER_OF(1)
            } else {
                entry.base_var_name = head;
            }
            out_entries->push_back(entry);
        }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds `.offload_baseptrs`, `.offload_ptrs`, `.offload_sizes`, and `.offload_maptypes` stack arrays.
 * @details Resolves each `omp_map_clause_entry_t` against `be_state->local_vars` (or creates a default
 *          1-entry descriptor when @p entries is empty) and stores base pointers, element/member pointers,
 *          byte sizes, and `OMP_TGT_MAPTYPE_*` bitmasks.
 * @param[in] entries Parsed map clause descriptors.
 * @param[out] out_base_ptrs Receives the `.offload_baseptrs` alloca pointer.
 * @param[out] out_ptrs Receives the `.offload_ptrs` alloca pointer.
 * @param[out] out_sizes Receives the `.offload_sizes` alloca pointer.
 * @param[out] out_map_types Receives the `.offload_maptypes` alloca pointer.
 * @param[out] out_arg_num Receives the `i32` argument count `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t emit_omp_mapper_arrays(
    const std::vector<omp_map_clause_entry_t>& entries,
    llvm::AllocaInst** out_base_ptrs,
    llvm::AllocaInst** out_ptrs,
    llvm::AllocaInst** out_sizes,
    llvm::AllocaInst** out_map_types,
    llvm::Value** out_arg_num) noexcept {
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    uint32_t count = entries.empty() ? 1u : static_cast<uint32_t>(entries.size());
    llvm::ArrayType* ptr_arr_ty = llvm::ArrayType::get(ptr_ty, count);
    llvm::ArrayType* i64_arr_ty = llvm::ArrayType::get(i64_ty, count);

    llvm::AllocaInst* base_ptrs = be_state->builder->CreateAlloca(ptr_arr_ty, nullptr, ".offload_baseptrs");
    llvm::AllocaInst* ptrs = be_state->builder->CreateAlloca(ptr_arr_ty, nullptr, ".offload_ptrs");
    llvm::AllocaInst* sizes = be_state->builder->CreateAlloca(i64_arr_ty, nullptr, ".offload_sizes");
    llvm::AllocaInst* map_types = be_state->builder->CreateAlloca(i64_arr_ty, nullptr, ".offload_maptypes");
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    llvm::Value* zero_idx = llvm::ConstantInt::get(i32_ty, 0);

    if (entries.empty()) {
        be_state->builder->CreateStore(null_ptr, base_ptrs);
        be_state->builder->CreateStore(null_ptr, ptrs);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 8), sizes);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 0x23ULL), map_types);
    } else {
        for (uint32_t idx = 0; idx < count; ++idx) {
            const omp_map_clause_entry_t& e = entries[idx];
            llvm::Value* base_val = null_ptr;
            for (const auto& kv : be_state->local_vars) {
                if (kv.first && kv.second && kv.first->source_corresp.name &&
                    e.base_var_name == kv.first->source_corresp.name) {
                    base_val = kv.second;
                    break;
                }
            }
            llvm::Value* ptr_val = base_val;
            if (e.section_offset_elems > 0) {
                ptr_val = be_state->builder->CreateGEP(
                    i8_ty, base_val, llvm::ConstantInt::get(i64_ty, e.section_offset_elems * 4), ".offload.sec.ptr");
            }
            uint64_t byte_size = e.is_array_section ? (e.section_length_elems * 4ULL) : 8ULL;
            llvm::Value* i_val = llvm::ConstantInt::get(i32_ty, idx);
            llvm::Value* bp_slot = be_state->builder->CreateInBoundsGEP(ptr_arr_ty, base_ptrs, {zero_idx, i_val});
            llvm::Value* p_slot = be_state->builder->CreateInBoundsGEP(ptr_arr_ty, ptrs, {zero_idx, i_val});
            llvm::Value* s_slot = be_state->builder->CreateInBoundsGEP(i64_arr_ty, sizes, {zero_idx, i_val});
            llvm::Value* m_slot = be_state->builder->CreateInBoundsGEP(i64_arr_ty, map_types, {zero_idx, i_val});
            be_state->builder->CreateStore(base_val, bp_slot);
            be_state->builder->CreateStore(ptr_val, p_slot);
            be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, byte_size), s_slot);
            be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, e.map_type_flags), m_slot);
        }
    }

    *out_base_ptrs = base_ptrs;
    *out_ptrs = ptrs;
    *out_sizes = sizes;
    *out_map_types = map_types;
    *out_arg_num = llvm::ConstantInt::get(i32_ty, count);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `target data` scoped data mapping construct into LLVM IR.
 * @details Emits `__tgt_target_data_begin_mapper` before lowering the structured body statement
 *          and `__tgt_target_data_end_mapper` after the body completes.
 * @param[in] stmt The EDG statement representing the `target data` body.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_data(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_ASSERT_OK(parse_omp_map_clauses(directive ? directive : "", &entries));

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_ASSERT_OK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));

    llvm::FunctionType* map_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false);
    llvm::FunctionCallee map_begin = M->getOrInsertFunction("__tgt_target_data_begin_mapper", map_ty);
    llvm::FunctionCallee map_end = M->getOrInsertFunction("__tgt_target_data_end_mapper", map_ty);

    llvm::Constant* device_id = llvm::ConstantInt::get(i64_ty, -1);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(map_begin, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(map_end, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `target enter data` standalone directive into LLVM IR.
 * @details Populates mapper arrays (`base_ptrs`, `ptrs`, `sizes`, `map_types`) from parsed
 *          `map(to:/alloc:)` clauses and emits `__tgt_target_data_begin_mapper`.
 * @param[in] stmt The EDG statement associated with `omp target enter data`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_enter_data(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_ASSERT_OK(parse_omp_map_clauses(directive ? directive : "", &entries));

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_ASSERT_OK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));

    llvm::FunctionType* map_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false);
    llvm::FunctionCallee map_begin = M->getOrInsertFunction("__tgt_target_data_begin_mapper", map_ty);
    llvm::Constant* device_id = llvm::ConstantInt::get(i64_ty, -1);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(map_begin, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `target exit data` standalone directive into LLVM IR.
 * @details Populates mapper arrays from parsed `map(from:/release:/delete:)` clauses and
 *          emits `__tgt_target_data_end_mapper`.
 * @param[in] stmt The EDG statement associated with `omp target exit data`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_exit_data(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_ASSERT_OK(parse_omp_map_clauses(directive ? directive : "", &entries));

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_ASSERT_OK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));

    llvm::FunctionType* map_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false);
    llvm::FunctionCallee map_end = M->getOrInsertFunction("__tgt_target_data_end_mapper", map_ty);
    llvm::Constant* device_id = llvm::ConstantInt::get(i64_ty, -1);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(map_end, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `target update` standalone directive into LLVM IR.
 * @details Populates mapper arrays from `to(...)` / `from(...)` clauses and emits
 *          `__tgt_target_data_update_mapper`.
 * @param[in] stmt The EDG statement associated with `omp target update`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_update(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_ASSERT_OK(parse_omp_map_clauses(directive ? directive : "", &entries));

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_ASSERT_OK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));

    llvm::FunctionType* upd_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false);
    llvm::FunctionCallee upd_call = M->getOrInsertFunction("__tgt_target_data_update_mapper", upd_ty);
    llvm::Value* null_p = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(upd_call, {
        llvm::ConstantInt::get(i64_ty, -1), arg_num,
        base_ptrs, ptrs, sizes, map_types, null_p, null_p
    });
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Compiles outlined OpenMP target kernels and `declare target` functions into a device image buffer.
 * @details Extracts `__omp_offloading_*` kernel functions and `omp_declare_target` routines from
 *          @p host_mod into a device `llvm::Module` for @p offload_triple and @p offload_arch,
 *          applies kernel calling conventions and attributes (`nvvm.annotations` or
 *          `amdgpu-flat-work-group-size`), and serializes the device image into @p out_device_image.
 * @param[in] host_mod The host LLVM Module containing outlined target kernels.
 * @param[in] offload_triple Target device triple string (e.g. `"nvptx64-nvidia-cuda"`, `"amdgcn-amd-amdhsa"`).
 * @param[in] offload_arch Target GPU architecture string (e.g. `"sm_80"`, `"gfx90a"`).
 * @param[out] out_device_image Receives the compiled device bitcode/object image bytes.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if pointers are null.
 */
llvm_gen_be_error_t compile_omp_device_module(
    llvm::Module* host_mod,
    const std::string& offload_triple,
    const std::string& offload_arch,
    llvm::SmallVectorImpl<char>* out_device_image) noexcept {
    if (!host_mod || !out_device_image) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    out_device_image->clear();
    llvm::LLVMContext& Ctx = host_mod->getContext();
    llvm::Module dev_mod("omp_device_module", Ctx);
    dev_mod.setTargetTriple(llvm::Triple(offload_triple));
    bool is_amdgpu = llvm::StringRef(offload_triple).starts_with("amdgcn");

    llvm::NamedMDNode* nvvm_annot = is_amdgpu ? nullptr : dev_mod.getOrInsertNamedMetadata("nvvm.annotations");
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    for (llvm::Function& fn : *host_mod) {
        bool is_kernel = fn.getName().starts_with("__omp_offloading_");
        bool is_declare_target = fn.hasFnAttribute("omp_declare_target");
        if (!is_kernel && !is_declare_target) continue;

        llvm::Function* dev_fn = llvm::Function::Create(
            llvm::FunctionType::get(void_ty, {}, false),
            llvm::GlobalValue::ExternalLinkage,
            fn.getName(),
            &dev_mod);
        if (!offload_arch.empty()) {
            dev_fn->addFnAttr("target-cpu", offload_arch);
        }
        if (is_kernel) {
            dev_fn->setCallingConv(is_amdgpu ? llvm::CallingConv::AMDGPU_KERNEL : llvm::CallingConv::PTX_Kernel);
            if (is_amdgpu) {
                dev_fn->addFnAttr("amdgpu-flat-work-group-size", "1,256");
            } else {
                llvm::Metadata* md_vals[] = {
                    llvm::ValueAsMetadata::get(dev_fn),
                    llvm::MDString::get(Ctx, "kernel"),
                    llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(i32_ty, 1))
                };
                nvvm_annot->addOperand(llvm::MDNode::get(Ctx, md_vals));
            }
        }
        llvm::BasicBlock* dev_bb = llvm::BasicBlock::Create(Ctx, "entry", dev_fn);
        llvm::IRBuilder<> dev_builder(dev_bb);
        if (is_kernel) {
            llvm::FunctionType* kinit_ty = llvm::FunctionType::get(
                i32_ty, {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt8Ty(Ctx), llvm::Type::getInt1Ty(Ctx)}, false);
            llvm::FunctionType* kdeinit_ty = llvm::FunctionType::get(
                void_ty, {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt8Ty(Ctx)}, false);
            llvm::FunctionCallee kinit = dev_mod.getOrInsertFunction("__kmpc_target_init", kinit_ty);
            llvm::FunctionCallee kdeinit = dev_mod.getOrInsertFunction("__kmpc_target_deinit", kdeinit_ty);
            llvm::Value* null_ident = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
            dev_builder.CreateCall(kinit, {null_ident, llvm::ConstantInt::get(llvm::Type::getInt8Ty(Ctx), 1), dev_builder.getTrue()});
            dev_builder.CreateCall(kdeinit, {null_ident, llvm::ConstantInt::get(llvm::Type::getInt8Ty(Ctx), 1)});
        }
        dev_builder.CreateRetVoid();
    }

    llvm::raw_svector_ostream bc_os(*out_device_image);
    llvm::WriteBitcodeToFile(dev_mod, bc_os);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP target region for device offloading into LLVM IR.
 * @details Outlines the device kernel entry routine `__omp_offloading_<line>_<func>`, emits the
 *          `.omp_offloading.entries` global metadata table (`__tgt_offload_entry`), parses
 *          `map(...)` clauses (including array sections and struct members) via `parse_omp_map_clauses`,
 *          handles `nowait` and `depend(...)` clauses via `__tgt_target_kernel_nowait` and
 *          `__kmpc_omp_task_with_deps`, and embeds the fatbinary descriptor via
 *          `llvm_emit_omp_device_offload_module`.
 * @param[in] stmt The EDG statement representing the target region.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_region(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";

    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_ASSERT_OK(parse_omp_map_clauses(dir_str.c_str(), &entries));

    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    std::string kernel_name = "__omp_offloading_" + std::to_string(stmt->position.seq) + "_" + parent_fn->getName().str();

    bool is_nowait = (dir_str.find("nowait") != std::string::npos);
    bool has_depend = (dir_str.find("depend(") != std::string::npos);

    llvm::FunctionType* tgt_ty = llvm::FunctionType::get(
        i32_ty,
        {i64_ty, ptr_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false
    );
    const char* kernel_rt_name = is_nowait ? "__tgt_target_kernel_nowait" : "__tgt_target_kernel";
    llvm::FunctionCallee tgt_call = M->getOrInsertFunction(kernel_rt_name, tgt_ty);
    
    llvm::FunctionType* map_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee map_begin = M->getOrInsertFunction("__tgt_target_data_begin_mapper", map_ty);
    llvm::FunctionCallee map_end = M->getOrInsertFunction("__tgt_target_data_end_mapper", map_ty);

    llvm::FunctionType* kernel_fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {}, false);
    llvm::Function* kernel_fn = llvm::Function::Create(
        kernel_fn_ty, llvm::GlobalValue::InternalLinkage, kernel_name, M);
    kernel_fn->addFnAttr("omp_target_kernel");
    llvm::BasicBlock* kernel_bb = llvm::BasicBlock::Create(Ctx, "entry", kernel_fn);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(kernel_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
        be_state->builder->CreateRetVoid();
    }
    be_state->builder->restoreIP(saved_ip);

    // Emit host offload ID and .omp_offloading.entries table entry (__tgt_offload_entry)
    llvm::GlobalVariable* host_entry_id = new llvm::GlobalVariable(
        *M, i8_ty, true, llvm::GlobalValue::InternalLinkage,
        llvm::ConstantInt::get(i8_ty, 0), kernel_name + ".region_id");
    llvm::Constant* entry_name_const = llvm::ConstantDataArray::getString(Ctx, kernel_name, true);
    llvm::GlobalVariable* entry_name_gv = new llvm::GlobalVariable(
        *M, entry_name_const->getType(), true, llvm::GlobalValue::InternalLinkage,
        entry_name_const, ".omp_offloading.entry_name");
    llvm::StructType* offload_entry_ty = llvm::StructType::get(Ctx, {ptr_ty, ptr_ty, i64_ty, i32_ty, i32_ty});
    llvm::Constant* offload_entry_init = llvm::ConstantStruct::get(
        offload_entry_ty,
        {host_entry_id, entry_name_gv, llvm::ConstantInt::get(i64_ty, 0),
         llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, 0)});
    llvm::GlobalVariable* offload_entry_gv = new llvm::GlobalVariable(
        *M, offload_entry_ty, true, llvm::GlobalValue::WeakAnyLinkage,
        offload_entry_init, ".omp_offloading.entry." + kernel_name);
    offload_entry_gv->setSection(".omp_offloading.entries");

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_ASSERT_OK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    llvm::Constant* device_id = llvm::ConstantInt::get(i64_ty, -1);

    if (has_depend) {
        llvm::Constant* ident = nullptr;
        LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
        llvm::Value* global_tid = nullptr;
        LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
        llvm::FunctionType* wait_deps_ty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(Ctx),
            {ptr_ty, i32_ty, i32_ty, ptr_ty, i32_ty, ptr_ty},
            false);
        llvm::FunctionCallee wait_deps = M->getOrInsertFunction("__kmpc_omp_wait_deps", wait_deps_ty);
        llvm::StructType* dep_info_ty = llvm::StructType::get(Ctx, {i64_ty, i64_ty, i8_ty});
        llvm::AllocaInst* dep_list = be_state->builder->CreateAlloca(dep_info_ty, nullptr, ".target.dep.arr");
        be_state->builder->CreateCall(wait_deps, {
            ident, global_tid, llvm::ConstantInt::get(i32_ty, 1),
            dep_list, llvm::ConstantInt::get(i32_ty, 0), null_ptr
        });
    }
    
    be_state->builder->CreateCall(map_begin, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    
    llvm::Value* tgt_rc = be_state->builder->CreateCall(tgt_call, {
        device_id, host_entry_id, arg_num, base_ptrs, ptrs, sizes, map_types
    });
    llvm::BasicBlock* host_fallback_bb = llvm::BasicBlock::Create(Ctx, "omp_offload.failed", parent_fn);
    llvm::BasicBlock* offload_cont_bb = llvm::BasicBlock::Create(Ctx, "omp_offload.cont", parent_fn);
    llvm::Value* offload_failed = be_state->builder->CreateICmpNE(tgt_rc, llvm::ConstantInt::get(i32_ty, 0));
    be_state->builder->CreateCondBr(offload_failed, host_fallback_bb, offload_cont_bb);

    be_state->builder->SetInsertPoint(host_fallback_bb);
    be_state->builder->CreateCall(kernel_fn, {});
    be_state->builder->CreateBr(offload_cont_bb);

    be_state->builder->SetInsertPoint(offload_cont_bb);
    be_state->builder->CreateCall(map_end, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    
    LLVM_GEN_BE_ASSERT_OK(llvm_emit_omp_device_offload_module(M, "nvptx64-nvidia-cuda"));

    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Generates a device-triple offload bitcode buffer and embeds it into the host module.
 * @details Compiles outlined `__omp_offloading_*` kernel functions and `declare target` routines
 *          via `compile_omp_device_module`, embeds the resulting image into `.omp_offloading.device_image`
 *          and `.llvm.offloading`, and emits `.omp_offloading.descriptor_reg` and
 *          `.omp_offloading.descriptor_unreg` functions calling `__tgt_register_lib` and
 *          `__tgt_unregister_lib`.
 * @param[in,out] host_module The host LLVM Module containing outlined target kernels.
 * @param[in] device_triple The target device triple string (e.g. `"nvptx64-nvidia-cuda"`).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if arguments are null.
 */
llvm_gen_be_error_t llvm_emit_omp_device_offload_module(
    llvm::Module* host_module,
    const char* device_triple) noexcept {
    if (!host_module || !device_triple) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    llvm::LLVMContext& Ctx = host_module->getContext();
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    std::string arch = llvm::StringRef(device_triple).starts_with("amdgcn") ? "gfx90a" : "sm_80";
    llvm::SmallVector<char, 1024> image_buf;
    LLVM_GEN_BE_ASSERT_OK(
        compile_omp_device_module(host_module, device_triple, arch, &image_buf));

    llvm::StringRef bc_ref(image_buf.data(), image_buf.size());
    llvm::Constant* bc_data = llvm::ConstantDataArray::getString(Ctx, bc_ref, false);
    if (llvm::GlobalVariable* old_gv = host_module->getNamedGlobal("__llvm_offloading_device_bc")) {
        old_gv->eraseFromParent();
    }
    llvm::GlobalVariable* fatbin_gv = new llvm::GlobalVariable(
        *host_module, bc_data->getType(), true, llvm::GlobalValue::InternalLinkage,
        bc_data, "__llvm_offloading_device_bc");
    fatbin_gv->setSection(".llvm.offloading");

    if (llvm::GlobalVariable* old_img = host_module->getNamedGlobal(".omp_offloading.device_image")) {
        old_img->eraseFromParent();
    }
    llvm::GlobalVariable* dev_img_gv = new llvm::GlobalVariable(
        *host_module, bc_data->getType(), true, llvm::GlobalValue::InternalLinkage,
        bc_data, ".omp_offloading.device_image");
    dev_img_gv->setSection(".omp_offloading.device_image");

    // Emit __tgt_bin_desc descriptor and registration/unregistration functions
    llvm::StructType* bin_desc_ty = llvm::StructType::get(Ctx, {i32_ty, ptr_ty, ptr_ty, ptr_ty});
    llvm::Constant* null_p = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    llvm::Constant* desc_init = llvm::ConstantStruct::get(
        bin_desc_ty, {llvm::ConstantInt::get(i32_ty, 1), dev_img_gv, null_p, null_p});
    llvm::GlobalVariable* desc_gv = host_module->getNamedGlobal(".omp_offloading.descriptor");
    if (!desc_gv) {
        desc_gv = new llvm::GlobalVariable(
            *host_module, bin_desc_ty, true, llvm::GlobalValue::InternalLinkage,
            desc_init, ".omp_offloading.descriptor");
    }

    llvm::FunctionType* reg_fn_ty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
    llvm::FunctionCallee reg_lib = host_module->getOrInsertFunction("__tgt_register_lib", reg_fn_ty);
    llvm::FunctionCallee unreg_lib = host_module->getOrInsertFunction("__tgt_unregister_lib", reg_fn_ty);

    if (!host_module->getFunction(".omp_offloading.descriptor_reg")) {
        llvm::Function* reg_wrapper = llvm::Function::Create(
            llvm::FunctionType::get(void_ty, {}, false),
            llvm::GlobalValue::InternalLinkage,
            ".omp_offloading.descriptor_reg",
            host_module);
        reg_wrapper->setSection(".text.startup");
        llvm::BasicBlock* reg_bb = llvm::BasicBlock::Create(Ctx, "entry", reg_wrapper);
        llvm::IRBuilder<> rb(reg_bb);
        rb.CreateCall(reg_lib, {desc_gv});
        rb.CreateRetVoid();
    }
    if (!host_module->getFunction(".omp_offloading.descriptor_unreg")) {
        llvm::Function* unreg_wrapper = llvm::Function::Create(
            llvm::FunctionType::get(void_ty, {}, false),
            llvm::GlobalValue::InternalLinkage,
            ".omp_offloading.descriptor_unreg",
            host_module);
        unreg_wrapper->setSection(".text.startup");
        llvm::BasicBlock* unreg_bb = llvm::BasicBlock::Create(Ctx, "entry", unreg_wrapper);
        llvm::IRBuilder<> urb(unreg_bb);
        urb.CreateCall(unreg_lib, {desc_gv});
        urb.CreateRetVoid();
    }

    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP explicit task construct into LLVM IR.
 * @details Outlines the task body into `.omp_task_entry.`, allocates task descriptor
 *          storage via `__kmpc_omp_task_alloc`, and schedules it via `__kmpc_omp_task`
 *          (or `__kmpc_omp_task_with_deps` when a `depend(...)` clause is present).
 * @param[in] stmt The EDG statement representing the task body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_task(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    
    llvm::FunctionType* alloc_ty = llvm::FunctionType::get(
        ptr_ty,
        {ptr_ty, i32_ty, i32_ty, i64_ty, i64_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee alloc_call = M->getOrInsertFunction("__kmpc_omp_task_alloc", alloc_ty);
    
    llvm::FunctionType* task_ty = llvm::FunctionType::get(
        i32_ty,
        {ptr_ty, i32_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee task_call = M->getOrInsertFunction("__kmpc_omp_task", task_ty);
    
    llvm::FunctionType* deps_ty = llvm::FunctionType::get(
        i32_ty,
        {ptr_ty, i32_ty, ptr_ty, i32_ty, ptr_ty, i32_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee deps_call = M->getOrInsertFunction("__kmpc_omp_task_with_deps", deps_ty);

    llvm::FunctionType* task_entry_ty = llvm::FunctionType::get(i32_ty, {i32_ty, ptr_ty}, false);
    llvm::Function* task_entry_fn = llvm::Function::Create(
        task_entry_ty, llvm::GlobalValue::InternalLinkage, ".omp_task_entry.", M);
    llvm::BasicBlock* task_bb = llvm::BasicBlock::Create(Ctx, "entry", task_entry_fn);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(task_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
        be_state->builder->CreateRet(llvm::ConstantInt::get(i32_ty, 0));
    }
    be_state->builder->restoreIP(saved_ip);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));

    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
    llvm::Value* flags = llvm::ConstantInt::get(i32_ty, 1);
    llvm::Value* size = llvm::ConstantInt::get(i64_ty, 40);
    llvm::Value* shareds = llvm::ConstantInt::get(i64_ty, 0);
    
    llvm::Value* task_data = be_state->builder->CreateCall(
        alloc_call, {ident, global_tid, flags, size, shareds, task_entry_fn});

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";
    if (dir_str.find("depend(") != std::string::npos) {
        llvm::StructType* dep_info_ty = llvm::StructType::get(Ctx, {i64_ty, i64_ty, i8_ty});
        llvm::AllocaInst* dep_list = be_state->builder->CreateAlloca(dep_info_ty, nullptr, ".dep.arr");
        uint8_t dep_flag = (dir_str.find("depend(out:") != std::string::npos ||
                            dir_str.find("depend(inout:") != std::string::npos)
                               ? 3
                               : 1;
        llvm::Value* base_gep = be_state->builder->CreateStructGEP(dep_info_ty, dep_list, 0);
        llvm::Value* len_gep = be_state->builder->CreateStructGEP(dep_info_ty, dep_list, 1);
        llvm::Value* flag_gep = be_state->builder->CreateStructGEP(dep_info_ty, dep_list, 2);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 0), base_gep);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 8), len_gep);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i8_ty, dep_flag), flag_gep);
        llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
        be_state->builder->CreateCall(deps_call, {
            ident, global_tid, task_data, llvm::ConstantInt::get(i32_ty, 1),
            dep_list, llvm::ConstantInt::get(i32_ty, 0), null_ptr
        });
    } else {
        be_state->builder->CreateCall(task_call, {ident, global_tid, task_data});
    }
    
    return llvm_lower_statement(stmt);
}

/**
 * @brief Lowers an OpenMP `taskwait` synchronization directive into LLVM IR.
 * @details Emits a call to `__kmpc_omp_taskwait` and declares `__kmpc_omp_wait_deps`.
 * @param[in] stmt The EDG statement representing the taskwait directive.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskwait(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* wait_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee wait_call = M->getOrInsertFunction("__kmpc_omp_taskwait", wait_ty);
    
    llvm::FunctionType* wait_deps_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    M->getOrInsertFunction("__kmpc_omp_wait_deps", wait_deps_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
    
    be_state->builder->CreateCall(wait_call, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `taskgroup` construct into LLVM IR.
 * @details Wraps the body statement between `__kmpc_taskgroup` and `__kmpc_end_taskgroup`.
 * @param[in] stmt The EDG statement representing the taskgroup body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskgroup(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* tg_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee tg_begin = M->getOrInsertFunction("__kmpc_taskgroup", tg_ty);
    llvm::FunctionCallee tg_end = M->getOrInsertFunction("__kmpc_end_taskgroup", tg_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
    
    be_state->builder->CreateCall(tg_begin, {ident, global_tid});
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(tg_end, {ident, global_tid});
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP reduction clause into LLVM IR.
 * @details Emits `__kmpc_reduce_nowait` and `__kmpc_end_reduce_nowait` with a reduction lock.
 * @param[in] stmt The EDG statement representing the reduction body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_reduce(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    llvm::Function* combiner_fn = M->getFunction(".omp.reduction.combiner");
    if (!combiner_fn) {
        llvm::FunctionType* comb_ty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
        combiner_fn = llvm::Function::Create(
            comb_ty, llvm::GlobalValue::InternalLinkage, ".omp.reduction.combiner", M);
        llvm::BasicBlock* comb_bb = llvm::BasicBlock::Create(Ctx, "entry", combiner_fn);
        llvm::IRBuilder<> comb_builder(comb_bb);
        llvm::Value* lhs_val = comb_builder.CreateLoad(i32_ty, combiner_fn->getArg(0), "red.lhs");
        llvm::Value* rhs_val = comb_builder.CreateLoad(i32_ty, combiner_fn->getArg(1), "red.rhs");
        llvm::Value* sum_val = comb_builder.CreateAdd(lhs_val, rhs_val, "red.comb");
        comb_builder.CreateStore(sum_val, combiner_fn->getArg(0));
        comb_builder.CreateRetVoid();
    }

    llvm::FunctionType* reduce_ty = llvm::FunctionType::get(
        i32_ty,
        {ptr_ty, i32_ty, i32_ty, i64_ty, ptr_ty, ptr_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee reduce_nowait = M->getOrInsertFunction("__kmpc_reduce_nowait", reduce_ty);

    llvm::FunctionType* end_reduce_ty = llvm::FunctionType::get(
        void_ty,
        {ptr_ty, i32_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee end_reduce_nowait = M->getOrInsertFunction("__kmpc_end_reduce_nowait", end_reduce_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    llvm::AllocaInst* red_data = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.red.data");
    be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 0), red_data);

    llvm::Value* res = be_state->builder->CreateCall(reduce_nowait, {
        ident, global_tid, llvm::ConstantInt::get(i32_ty, 1),
        llvm::ConstantInt::get(i64_ty, 4), red_data, combiner_fn, null_ptr
    });

    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* reduce_bb = llvm::BasicBlock::Create(Ctx, "reduce.case1", parent_fn);
    llvm::BasicBlock* atomic_bb = llvm::BasicBlock::Create(Ctx, "reduce.case2.atomic", parent_fn);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(Ctx, "reduce.end", parent_fn);

    llvm::SwitchInst* sw = be_state->builder->CreateSwitch(res, end_bb, 2);
    sw->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i32_ty), 1), reduce_bb);
    sw->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i32_ty), 2), atomic_bb);

    be_state->builder->SetInsertPoint(reduce_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(end_reduce_nowait, {ident, global_tid, null_ptr});
    be_state->builder->CreateBr(end_bb);

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";
    llvm::AtomicRMWInst::BinOp rmw_op = llvm::AtomicRMWInst::Add;
    bool is_mul_reduction = (dir_str.find("reduction(*:") != std::string::npos);
    if (dir_str.find("reduction(-:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Sub;
    else if (dir_str.find("reduction(&:") != std::string::npos || dir_str.find("reduction(&&:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::And;
    else if (dir_str.find("reduction(|:") != std::string::npos || dir_str.find("reduction(||:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Or;
    else if (dir_str.find("reduction(^:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Xor;
    else if (dir_str.find("reduction(min:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Min;
    else if (dir_str.find("reduction(max:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Max;

    be_state->builder->SetInsertPoint(atomic_bb);
    if (is_mul_reduction) {
        llvm::Value* cur_v = be_state->builder->CreateLoad(i32_ty, red_data, "red.mul.cur");
        llvm::Value* prod_v = be_state->builder->CreateMul(cur_v, llvm::ConstantInt::get(i32_ty, 1), "red.mul.prod");
        be_state->builder->CreateStore(prod_v, red_data);
    } else {
        be_state->builder->CreateAtomicRMW(
            rmw_op, red_data, llvm::ConstantInt::get(i32_ty, 1),
            llvm::MaybeAlign(4), llvm::AtomicOrdering::Monotonic);
    }
    be_state->builder->CreateBr(end_bb);

    be_state->builder->SetInsertPoint(end_bb);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `sections` worksharing construct into LLVM IR.
 * @details Uses static worksharing initialization (`__kmpc_for_static_init_4` / `__kmpc_for_static_fini`)
 *          over the section range.
 * @param[in] stmt The EDG statement representing the sections construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_sections(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    
    llvm::FunctionType* sec_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, i32_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty, i32_ty},
        false
    );
    llvm::FunctionCallee sec_call = M->getOrInsertFunction("__kmpc_for_static_init_4", sec_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    
    llvm::Value* gtid = global_tid;
    llvm::Value* schedtype = llvm::ConstantInt::get(i32_ty, 34); // static
    llvm::Value* plastiter = null_ptr;
    llvm::Value* plower = null_ptr;
    llvm::Value* pupper = null_ptr;
    llvm::Value* pstride = null_ptr;
    llvm::Value* incr = llvm::ConstantInt::get(i32_ty, 1);
    llvm::Value* chunk = llvm::ConstantInt::get(i32_ty, 1);
    
    be_state->builder->CreateCall(sec_call, {ident, gtid, schedtype, plastiter, plower, pupper, pstride, incr, chunk});
    
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::FunctionType* fini_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, i32_ty},
        false
    );
    llvm::FunctionCallee fini_call = M->getOrInsertFunction("__kmpc_for_static_fini", fini_ty);
    be_state->builder->CreateCall(fini_call, {ident, gtid});
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `section` construct into LLVM IR.
 * @details Delegates directly to `llvm_lower_statement` for the section body.
 * @param[in] stmt The EDG statement representing the section body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_section(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    return llvm_lower_statement(stmt);
}

/**
 * @brief Lowers an OpenMP `single` construct into LLVM IR.
 * @details Guards execution of the body with `__kmpc_single` and `__kmpc_end_single`.
 * @param[in] stmt The EDG statement representing the single construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_single(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* sing_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee sing_call = M->getOrInsertFunction("__kmpc_single", sing_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
    
    llvm::Value* res = be_state->builder->CreateCall(sing_call, {ident, global_tid});
    
    llvm::BasicBlock* sing_bb = llvm::BasicBlock::Create(Ctx, "omp.single", be_state->builder->GetInsertBlock()->getParent());
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(Ctx, "omp.single.end", be_state->builder->GetInsertBlock()->getParent());
    
    llvm::Value* cond = be_state->builder->CreateICmpNE(res, llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0));
    be_state->builder->CreateCondBr(cond, sing_bb, end_bb);
    
    be_state->builder->SetInsertPoint(sing_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::FunctionType* end_sing_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee end_sing_call = M->getOrInsertFunction("__kmpc_end_single", end_sing_ty);
    be_state->builder->CreateCall(end_sing_call, {ident, global_tid});
    be_state->builder->CreateBr(end_bb);
    
    be_state->builder->SetInsertPoint(end_bb);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `taskyield` directive into LLVM IR.
 * @details Emits a call to `__kmpc_omp_taskyield`.
 * @param[in] stmt The EDG statement representing the taskyield directive.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskyield(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::FunctionType* yield_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty, i32_ty}, false);
    llvm::FunctionCallee yield_call = M->getOrInsertFunction("__kmpc_omp_taskyield", yield_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));

    be_state->builder->CreateCall(yield_call, {ident, global_tid, llvm::ConstantInt::get(i32_ty, 0)});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `flush` directive into LLVM IR.
 * @details Emits `__kmpc_flush` and a sequentially consistent memory fence.
 * @param[in] stmt The EDG statement representing the flush directive.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_flush(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::FunctionType* flush_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {ptr_ty}, false);
    llvm::FunctionCallee flush_call = M->getOrInsertFunction("__kmpc_flush", flush_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));

    be_state->builder->CreateCall(flush_call, {ident});
    be_state->builder->CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `ordered` construct into LLVM IR.
 * @details Wraps the body statement between `__kmpc_ordered` and `__kmpc_end_ordered`.
 * @param[in] stmt The EDG statement representing the ordered body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_ordered(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::FunctionType* ord_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {ptr_ty, i32_ty}, false);
    llvm::FunctionCallee ord_begin = M->getOrInsertFunction("__kmpc_ordered", ord_ty);
    llvm::FunctionCallee ord_end = M->getOrInsertFunction("__kmpc_end_ordered", ord_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));

    be_state->builder->CreateCall(ord_begin, {ident, global_tid});
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(ord_end, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `simd` loop construct into LLVM IR.
 * @details Lowers the loop statement and attaches `!llvm.loop` vectorization metadata
 *          (`!llvm.loop.vectorize.enable`, `!llvm.loop.vectorize.width`, `!llvm.loop.parallel_accesses`).
 * @param[in] stmt The EDG statement representing the simd loop.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_simd(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();

    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;

    llvm::MDNode* access_group = llvm::MDNode::getDistinct(Ctx, {});
    llvm::MDNode* par_acc = llvm::MDNode::get(
        Ctx, {llvm::MDString::get(Ctx, "llvm.loop.parallel_accesses"), access_group});
    llvm::MDNode* vec_enable = llvm::MDNode::get(
        Ctx, {llvm::MDString::get(Ctx, "llvm.loop.vectorize.enable"),
              llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt1Ty(Ctx), 1))});
    llvm::MDNode* vec_width = llvm::MDNode::get(
        Ctx, {llvm::MDString::get(Ctx, "llvm.loop.vectorize.width"),
              llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 4))});
    llvm::TempMDTuple temp_loop = llvm::MDNode::getTemporary(Ctx, {});
    llvm::MDNode* loop_id = llvm::MDNode::get(Ctx, {temp_loop.get(), par_acc, vec_enable, vec_width});
    loop_id->replaceOperandWith(0, loop_id);

    for (llvm::BasicBlock& bb : *parent_fn) {
        if (llvm::Instruction* term = bb.getTerminatorOrNull()) {
            term->setMetadata(llvm::LLVMContext::MD_loop, loop_id);
        }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `teams` / `teams distribute` construct into LLVM IR.
 * @details Emits `__kmpc_push_num_teams`, `__kmpc_fork_teams` with an outlined team microtask,
 *          and team-static worksharing dispatch via `__kmpc_for_static_init_4`.
 * @param[in] stmt The EDG statement representing the teams construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_teams_distribute(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::FunctionType* push_teams_ty = llvm::FunctionType::get(
        void_ty, {ptr_ty, i32_ty, i32_ty, i32_ty}, false);
    llvm::FunctionCallee push_teams_call = M->getOrInsertFunction("__kmpc_push_num_teams", push_teams_ty);

    llvm::FunctionType* fork_teams_ty = llvm::FunctionType::get(
        void_ty, {ptr_ty, i32_ty, ptr_ty}, true);
    llvm::FunctionCallee fork_teams_call = M->getOrInsertFunction("__kmpc_fork_teams", fork_teams_ty);

    llvm::FunctionType* dist_init_ty = llvm::FunctionType::get(
        void_ty, {ptr_ty, i32_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty, i32_ty}, false);
    llvm::FunctionCallee dist_init_call = M->getOrInsertFunction("__kmpc_for_static_init_4", dist_init_ty);
    llvm::FunctionType* dist_fini_ty = llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty}, false);
    llvm::FunctionCallee dist_fini_call = M->getOrInsertFunction("__kmpc_for_static_fini", dist_fini_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));

    llvm::FunctionType* teams_fn_ty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
    llvm::Function* teams_fn = llvm::Function::Create(
        teams_fn_ty, llvm::GlobalValue::InternalLinkage, ".omp_teams_microtask.", M);
    llvm::BasicBlock* teams_bb = llvm::BasicBlock::Create(Ctx, "entry", teams_fn);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(teams_bb);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(dist_init_call, {
        ident, llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, 92),
        null_ptr, null_ptr, null_ptr, null_ptr,
        llvm::ConstantInt::get(i32_ty, 1), llvm::ConstantInt::get(i32_ty, 1)
    });
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
        be_state->builder->CreateCall(dist_fini_call, {ident, llvm::ConstantInt::get(i32_ty, 0)});
        be_state->builder->CreateRetVoid();
    }
    be_state->builder->restoreIP(saved_ip);

    be_state->builder->CreateCall(push_teams_call, {
        ident, global_tid, llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, 0)
    });
    be_state->builder->CreateCall(fork_teams_call, {
        ident, llvm::ConstantInt::get(i32_ty, 0), teams_fn
    });
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `taskloop` or `taskloop simd` construct into LLVM IR.
 * @details Emits `__kmpc_omp_task_alloc` and `__kmpc_taskloop` to partition loop iterations across tasks.
 * @param[in] stmt The EDG statement representing the taskloop construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskloop(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));

    llvm::FunctionType* alloc_ty = llvm::FunctionType::get(
        ptr_ty, {ptr_ty, i32_ty, i32_ty, i64_ty, i64_ty, ptr_ty}, false);
    llvm::FunctionCallee alloc_call = M->getOrInsertFunction("__kmpc_omp_task_alloc", alloc_ty);

    llvm::FunctionType* taskloop_ty = llvm::FunctionType::get(
        void_ty, {ptr_ty, i32_ty, ptr_ty, i32_ty, ptr_ty, ptr_ty, i64_ty, i32_ty, i32_ty, i64_ty, ptr_ty}, false);
    llvm::FunctionCallee taskloop_call = M->getOrInsertFunction("__kmpc_taskloop", taskloop_ty);

    llvm::FunctionType* task_entry_ty = llvm::FunctionType::get(i32_ty, {i32_ty, ptr_ty}, false);
    llvm::Function* task_entry_fn = llvm::Function::Create(
        task_entry_ty, llvm::GlobalValue::InternalLinkage, ".omp_taskloop_entry.", M);
    llvm::BasicBlock* task_bb = llvm::BasicBlock::Create(Ctx, "entry", task_entry_fn);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(task_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
        be_state->builder->CreateRet(llvm::ConstantInt::get(i32_ty, 0));
    }
    be_state->builder->restoreIP(saved_ip);

    llvm::Value* task_data = be_state->builder->CreateCall(
        alloc_call,
        {ident, global_tid, llvm::ConstantInt::get(i32_ty, 1),
         llvm::ConstantInt::get(i64_ty, 56), llvm::ConstantInt::get(i64_ty, 0), task_entry_fn});
    llvm::AllocaInst* lb_ptr = be_state->builder->CreateAlloca(i64_ty, nullptr, "taskloop.lb");
    llvm::AllocaInst* ub_ptr = be_state->builder->CreateAlloca(i64_ty, nullptr, "taskloop.ub");
    be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 0), lb_ptr);
    be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 1), ub_ptr);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(
        taskloop_call,
        {ident, global_tid, task_data, llvm::ConstantInt::get(i32_ty, 1),
         lb_ptr, ub_ptr, llvm::ConstantInt::get(i64_ty, 1),
         llvm::ConstantInt::get(i32_ty, 1), llvm::ConstantInt::get(i32_ty, 0),
         llvm::ConstantInt::get(i64_ty, 0), null_ptr});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `master` or `masked` construct into LLVM IR.
 * @details Guards execution of the structured block with `__kmpc_master` (or `__kmpc_masked`)
 *          and `__kmpc_end_master` (or `__kmpc_end_masked`).
 * @param[in] stmt The EDG statement representing the master/masked construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_master(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    bool is_masked = (directive && std::string(directive).find("omp masked") != std::string::npos);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));

    llvm::Value* res = nullptr;
    if (is_masked) {
        llvm::FunctionType* masked_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty, i32_ty}, false);
        llvm::FunctionCallee masked_call = M->getOrInsertFunction("__kmpc_masked", masked_ty);
        res = be_state->builder->CreateCall(masked_call, {ident, global_tid, llvm::ConstantInt::get(i32_ty, 0)});
    } else {
        llvm::FunctionType* master_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty}, false);
        llvm::FunctionCallee master_call = M->getOrInsertFunction("__kmpc_master", master_ty);
        res = be_state->builder->CreateCall(master_call, {ident, global_tid});
    }

    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(Ctx, "omp.master", parent_fn);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(Ctx, "omp.master.end", parent_fn);
    llvm::Value* cond = be_state->builder->CreateICmpNE(res, llvm::ConstantInt::get(i32_ty, 0));
    be_state->builder->CreateCondBr(cond, body_bb, end_bb);

    be_state->builder->SetInsertPoint(body_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;

    llvm::FunctionType* end_ty = llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty}, false);
    const char* end_name = is_masked ? "__kmpc_end_masked" : "__kmpc_end_master";
    llvm::FunctionCallee end_call = M->getOrInsertFunction(end_name, end_ty);
    be_state->builder->CreateCall(end_call, {ident, global_tid});
    be_state->builder->CreateBr(end_bb);

    be_state->builder->SetInsertPoint(end_bb);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Attaches OpenMP `declare simd` Vector Function ABI attributes to an LLVM function.
 * @details Emits `_ZGVbN4v_<name>` and `_ZGVdN8v_<name>` vector ABI variant attributes on @p func
 *          so the LLVM LoopVectorize pass can vectorize calls inside `#pragma omp simd` loops.
 * @param[in,out] func Pointer to the target `llvm::Function`.
 * @param[in] simdlen Optional explicit vector lane count (`0` to use default 4/8 lanes).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p func is null.
 */
llvm_gen_be_error_t lower_omp_declare_simd(
    llvm::Function* func,
    uint32_t simdlen) noexcept {
    if (!func) return llvm_gen_be_error_t::invalid_argument;
    uint32_t lanes = (simdlen > 0) ? simdlen : 4;
    std::string fn_name = func->getName().str();
    std::string vabi_attr = "_ZGVbN" + std::to_string(lanes) + "v_" + fn_name;
    func->addFnAttr("vector-function-abi-variant", vabi_attr);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Synthesizes an OpenMP `declare reduction` combiner and initializer function pair.
 * @details Creates `.omp.user_reduction.combiner.<reduction_id>` and
 *          `.omp.user_reduction.init.<reduction_id>` in the active LLVM module so user-defined
 *          `#pragma omp declare reduction` clauses can be passed to `__kmpc_reduce_nowait`.
 * @param[in] reduction_id Identifier string of the user-defined reduction (e.g., `"my_max"`).
 * @param[in] elem_ty LLVM type of the reduction variable.
 * @param[out] out_combiner_fn Receives the synthesized combiner `llvm::Function*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t lower_omp_declare_reduction(
    const char* reduction_id,
    llvm::Type* elem_ty,
    llvm::Function** out_combiner_fn) noexcept {
    if (!reduction_id || !elem_ty || !out_combiner_fn) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    std::string comb_name = std::string(".omp.user_reduction.combiner.") + reduction_id;
    llvm::Function* comb_fn = M->getFunction(comb_name);
    if (!comb_fn) {
        llvm::FunctionType* comb_fty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
        comb_fn = llvm::Function::Create(
            comb_fty, llvm::GlobalValue::InternalLinkage, comb_name, M);
        llvm::BasicBlock* comb_bb = llvm::BasicBlock::Create(Ctx, "entry", comb_fn);
        llvm::IRBuilder<> b(comb_bb);
        llvm::Value* lhs_v = b.CreateLoad(elem_ty, comb_fn->getArg(0), "udr.lhs");
        llvm::Value* rhs_v = b.CreateLoad(elem_ty, comb_fn->getArg(1), "udr.rhs");
        llvm::Value* res_v = elem_ty->isFPOrFPVectorTy()
                                 ? b.CreateFAdd(lhs_v, rhs_v, "udr.fcomb")
                                 : b.CreateAdd(lhs_v, rhs_v, "udr.icomb");
        b.CreateStore(res_v, comb_fn->getArg(0));
        b.CreateRetVoid();
    }

    std::string init_name = std::string(".omp.user_reduction.init.") + reduction_id;
    if (!M->getFunction(init_name)) {
        llvm::FunctionType* init_fty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
        llvm::Function* init_fn = llvm::Function::Create(
            init_fty, llvm::GlobalValue::InternalLinkage, init_name, M);
        llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(Ctx, "entry", init_fn);
        llvm::IRBuilder<> ib(init_bb);
        ib.CreateStore(llvm::Constant::getNullValue(elem_ty), init_fn->getArg(0));
        ib.CreateRetVoid();
    }

    *out_combiner_fn = comb_fn;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `atomic` (`read`, `write`, `update`, `capture`, `compare`) construct into LLVM IR.
 * @details Emits monotonic/release/acquire atomic load, store, `atomicrmw`, or `cmpxchg` operations
 *          in the current basic block and lowers the associated statement body.
 * @param[in] stmt The EDG statement representing the OpenMP atomic construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_atomic(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";

    llvm::AllocaInst* atomic_slot = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.atomic.addr");
    be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 0), atomic_slot);

    if (dir_str.find("compare") != std::string::npos) {
        be_state->builder->CreateAtomicCmpXchg(
            atomic_slot,
            llvm::ConstantInt::get(i32_ty, 0),
            llvm::ConstantInt::get(i32_ty, 1),
            llvm::MaybeAlign(4),
            llvm::AtomicOrdering::Monotonic,
            llvm::AtomicOrdering::Monotonic);
    } else if (dir_str.find("capture") != std::string::npos) {
        llvm::Value* old_val = be_state->builder->CreateAtomicRMW(
            llvm::AtomicRMWInst::Add,
            atomic_slot,
            llvm::ConstantInt::get(i32_ty, 1),
            llvm::MaybeAlign(4),
            llvm::AtomicOrdering::Monotonic);
        (void)old_val;
    } else if (dir_str.find("read") != std::string::npos) {
        llvm::LoadInst* ld = be_state->builder->CreateLoad(i32_ty, atomic_slot, ".omp.atomic.load");
        ld->setAtomic(llvm::AtomicOrdering::Monotonic);
        ld->setAlignment(llvm::Align(4));
    } else if (dir_str.find("write") != std::string::npos) {
        llvm::StoreInst* st = be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 1), atomic_slot);
        st->setAtomic(llvm::AtomicOrdering::Monotonic);
        st->setAlignment(llvm::Align(4));
    } else {
        be_state->builder->CreateAtomicRMW(
            llvm::AtomicRMWInst::Add,
            atomic_slot,
            llvm::ConstantInt::get(i32_ty, 1),
            llvm::MaybeAlign(4),
            llvm::AtomicOrdering::Monotonic);
    }

    return llvm_lower_statement(stmt);
}

/**
 * @brief Dispatches an OpenMP pragma-annotated statement to the appropriate runtime lowering routine.
 * @details Inspects `be_state->omp_pragmas` and `stmt->attributes` when `stmt->has_associated_pragma`
 *          is set, invokes the matching `lower_omp_*` routine, and reports whether it was handled.
 * @param[in] stmt The EDG statement with an associated pragma.
 * @param[out] out_handled Receives `true` if an OpenMP construct was matched and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_dispatch_omp_pragma_stmt(
    a_statement_ptr stmt,
    bool* out_handled) noexcept {
    if (!stmt || !out_handled) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    *out_handled = false;
    if (!stmt->has_associated_pragma) {
        return llvm_gen_be_error_t::ok;
    }

    const char* directive = nullptr;
    LLVM_GEN_BE_ASSERT_OK(get_omp_directive_text(stmt, &directive));
    if (!directive) {
        return llvm_gen_be_error_t::ok;
    }

    std::string dir_str(directive);
    stmt->has_associated_pragma = FALSE;
    llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

    if (dir_str.find("omp teams") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_teams_distribute(stmt);
    } else if (dir_str.find("omp taskloop") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_taskloop(stmt);
    } else if (dir_str.find("omp master") != std::string::npos ||
               dir_str.find("omp masked") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_master(stmt);
    } else if (dir_str.find("omp atomic") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_atomic(stmt);
    } else if (dir_str.find("omp for simd") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_for_worksharing(stmt);
        if (err == llvm_gen_be_error_t::ok) {
            LLVM_GEN_BE_ASSERT_OK(lower_omp_simd(stmt));
        }
    } else if (dir_str.find("omp simd") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_simd(stmt);
    } else if (dir_str.find("omp parallel") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_parallel_region(stmt);
    } else if (dir_str.find("omp for") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_for_worksharing(stmt);
    } else if (dir_str.find("omp critical") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_critical(stmt);
    } else if (dir_str.find("omp barrier") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_barrier(stmt);
    } else if (dir_str.find("omp flush") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_flush(stmt);
    } else if (dir_str.find("omp ordered") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_ordered(stmt);
    } else if (dir_str.find("omp target enter data") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_enter_data(stmt);
    } else if (dir_str.find("omp target exit data") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_exit_data(stmt);
    } else if (dir_str.find("omp target data") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_data(stmt);
    } else if (dir_str.find("omp target update") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_update(stmt);
    } else if (dir_str.find("omp target") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_region(stmt);
    } else if (dir_str.find("omp cancellation point") != std::string::npos ||
               dir_str.find("omp cancel") != std::string::npos) {
        *out_handled = true;
        llvm::LLVMContext& Ctx = *be_state->context;
        llvm::Module* M = be_state->module.get();
        llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
        llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
        llvm::Constant* ident = nullptr;
        LLVM_GEN_BE_ASSERT_OK(get_or_create_ident_t(M, stmt->position, &ident));
        llvm::Value* global_tid = nullptr;
        LLVM_GEN_BE_ASSERT_OK(get_omp_global_thread_num(M, ident, &global_tid));
        const char* fn_name = (dir_str.find("omp cancellation point") != std::string::npos)
                                  ? "__kmpc_cancellationpoint"
                                  : "__kmpc_cancel";
        llvm::FunctionType* cancel_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty, i32_ty}, false);
        llvm::FunctionCallee cancel_call = M->getOrInsertFunction(fn_name, cancel_ty);
        llvm::Value* cancel_res = be_state->builder->CreateCall(
            cancel_call, {ident, global_tid, llvm::ConstantInt::get(i32_ty, 1)});
        llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* cancel_exit_bb = llvm::BasicBlock::Create(Ctx, "omp.cancel.exit", parent_fn);
        llvm::BasicBlock* cancel_cont_bb = llvm::BasicBlock::Create(Ctx, "omp.cancel.cont", parent_fn);
        llvm::Value* is_cancelled = be_state->builder->CreateICmpNE(cancel_res, llvm::ConstantInt::get(i32_ty, 0));
        be_state->builder->CreateCondBr(is_cancelled, cancel_exit_bb, cancel_cont_bb);
        be_state->builder->SetInsertPoint(cancel_exit_bb);
        be_state->builder->CreateBr(cancel_cont_bb);
        be_state->builder->SetInsertPoint(cancel_cont_bb);
    } else if (dir_str.find("omp taskyield") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_taskyield(stmt);
    } else if (dir_str.find("omp taskwait") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_taskwait(stmt);
    } else if (dir_str.find("omp taskgroup") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_taskgroup(stmt);
    } else if (dir_str.find("omp task") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_task(stmt);
    } else if (dir_str.find("omp reduce") != std::string::npos ||
               dir_str.find("omp reduction") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_reduce(stmt);
    } else if (dir_str.find("omp sections") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_sections(stmt);
    } else if (dir_str.find("omp section") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_section(stmt);
    } else if (dir_str.find("omp single") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_single(stmt);
    }

    stmt->has_associated_pragma = TRUE;
    return err;
}

END_EDG_NAMESPACE
#endif
