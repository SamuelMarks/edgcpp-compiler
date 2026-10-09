#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_codegen.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace edg;
LLVMBackendState state; LLVMBackendState* edg::be_state = &state;



bool check_magic(const char* filename, const std::vector<uint8_t>& expected_magic) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) return false;
    std::vector<uint8_t> buffer(expected_magic.size());
    if (!ifs.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) return false;
    for (size_t i = 0; i < expected_magic.size(); ++i) {
        if (buffer[i] != expected_magic[i]) return false;
    }
    return true;
}

void test_triple_format(llvm::Module* module, const char* triple_str, const std::vector<uint8_t>& expected_magic) {
    llvm::TargetMachine* tm = nullptr;
    llvm_gen_be_error_t err = create_target_machine(triple_str, nullptr, nullptr, llvm::CodeGenOptLevel::None, &tm);
    if (err != llvm_gen_be_error_t::ok) {
        std::cerr << "Failed to create target machine for " << triple_str << ". Skipping format test.\n";
        return;
    }
    
    assert(tm != nullptr);
    module->setDataLayout(tm->createDataLayout());
    
    std::string obj_out = std::string("test_out_") + triple_str + ".o";
    err = emit_machine_code_to_file(module, tm, codegen_file_type_t::object_file, obj_out.c_str());
    assert(err == llvm_gen_be_error_t::ok);
    
    bool magic_matches = check_magic(obj_out.c_str(), expected_magic);
    if (!magic_matches) {
        std::cerr << "Magic number mismatch for " << triple_str << "\n";
        std::exit(1);
    }
    
    llvm::sys::fs::remove(obj_out);
    delete tm;
}

int main() {
    assert(initialize_llvm_targets() == llvm_gen_be_error_t::ok);

    llvm::LLVMContext context;
    llvm::Module module("codegen_test_module", context);

    // Create a dummy function to emit
    llvm::FunctionType* func_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
    llvm::Function* func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, "test_func", &module);
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "entry", func);
    llvm::IRBuilder<> builder(bb);
    builder.CreateRetVoid();

    llvm::TargetMachine* tm = nullptr;
    std::string default_triple = llvm::sys::getDefaultTargetTriple();

    llvm_gen_be_error_t err = create_target_machine(default_triple.c_str(), nullptr, nullptr, llvm::CodeGenOptLevel::None, &tm);
    if (err != llvm_gen_be_error_t::ok) {
        std::cerr << "Failed to create target machine for " << default_triple << "\n";
        return 1;
    }

    assert(tm != nullptr);
    module.setDataLayout(tm->createDataLayout());

    // Test emission of LLVM IR text
    const char* text_out = "test_out.ll";
    err = emit_machine_code_to_file(&module, tm, codegen_file_type_t::llvm_ir_text, text_out);
    assert(err == llvm_gen_be_error_t::ok);

    // Test emission of LLVM Bitcode
    const char* bc_out = "test_out.bc";
    err = emit_machine_code_to_file(&module, tm, codegen_file_type_t::bitcode_file, bc_out);
    assert(err == llvm_gen_be_error_t::ok);

    // Test emission of Assembly
    const char* asm_out = "test_out.s";
    err = emit_machine_code_to_file(&module, tm, codegen_file_type_t::assembly_file, asm_out);
    assert(err == llvm_gen_be_error_t::ok);

    // Test emission of Object File
    const char* obj_out = "test_out.o";
    err = emit_machine_code_to_file(&module, tm, codegen_file_type_t::object_file, obj_out);
    assert(err == llvm_gen_be_error_t::ok);

    // Clean up
    llvm::sys::fs::remove(text_out);
    llvm::sys::fs::remove(bc_out);
    llvm::sys::fs::remove(asm_out);
    llvm::sys::fs::remove(obj_out);
    delete tm;

    // --- Object Format Verification ---
    // ELF: 0x7F 'E' 'L' 'F'
    std::vector<uint8_t> elf_magic = {0x7f, 0x45, 0x4c, 0x46};
    test_triple_format(&module, "x86_64-pc-linux-gnu", elf_magic);

    // Mach-O (64-bit): 0xCF 0xFA 0xED 0xFE
    std::vector<uint8_t> macho_magic = {0xcf, 0xfa, 0xed, 0xfe};
    test_triple_format(&module, "x86_64-apple-darwin", macho_magic);

    // COFF (x86_64): 0x64 0x86
    std::vector<uint8_t> coff_magic = {0x64, 0x86};
    test_triple_format(&module, "x86_64-pc-windows-msvc", coff_magic);
    
    // Test error cases
    llvm_gen_be_error_t err_invalid = create_target_machine(nullptr, nullptr, nullptr, llvm::CodeGenOptLevel::None, &tm);
    assert(err_invalid == llvm_gen_be_error_t::invalid_argument);

    llvm_gen_be_error_t err_unknown = create_target_machine("invalid-triple-that-does-not-exist", nullptr, nullptr, llvm::CodeGenOptLevel::None, &tm);
    assert(err_unknown == llvm_gen_be_error_t::invalid_argument);

    // Test null arguments to emit_machine_code_to_file
    err = emit_machine_code_to_file(nullptr, nullptr, codegen_file_type_t::object_file, "out.o");
    assert(err == llvm_gen_be_error_t::invalid_argument);
    err = emit_machine_code_to_file(&module, nullptr, codegen_file_type_t::object_file, nullptr);
    assert(err == llvm_gen_be_error_t::invalid_argument);

    // Test missing TM when emitting obj
    err = emit_machine_code_to_file(&module, nullptr, codegen_file_type_t::object_file, "out.o");
    assert(err == llvm_gen_be_error_t::invalid_argument);

    // Test IO error with invalid path
    err = create_target_machine(default_triple.c_str(), nullptr, nullptr, llvm::CodeGenOptLevel::None, &tm);
    assert(err == llvm_gen_be_error_t::ok);
    err = emit_machine_code_to_file(&module, tm, codegen_file_type_t::object_file, "/invalid/path/that/does/not/exist/out.o");
    assert(err == llvm_gen_be_error_t::io_error);
    
    // Test unknown file type
    err = emit_machine_code_to_file(&module, tm, static_cast<codegen_file_type_t>(999), "test_out_unknown.o");
    assert(err == llvm_gen_be_error_t::invalid_argument);

    delete tm;

    std::cout << "Codegen tests passed successfully!\n";
    return 0;
}
