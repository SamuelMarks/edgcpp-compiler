// RUN: cpfe --c99 --edg_base_dir include_c++ --gen_llvm_file_name %t.ll %s
// RUN: cat %t.ll | FileCheck %s

struct Bitfields {
    int a : 3;
    int b : 5;
};

int read_b(struct Bitfields* s) {
    // CHECK: load i32
    // CHECK: and i32
    // CHECK: shl i32
    // CHECK: ashr i32
    return s->b;
}

void write_b(struct Bitfields* s, int val) {
    // CHECK: load i32
    // CHECK: and i32
    // CHECK: and i32
    // CHECK: or i32
    // CHECK: store i32
    s->b = val;
}
