extern "C" void printf(const char*, ...);

void test_basic_asm() {
    __asm__ volatile ("nop");
    printf("Executed basic asm\n");
}

void test_complex_asm() {
    int src = 1;
    int dst;
#if defined(__x86_64__) || defined(__i386__)
    __asm__ ("mov %1, %0\n\t"
             "add $1, %0"
             : "=r" (dst)
             : "r" (src));
#elif defined(__aarch64__)
    __asm__ ("mov %w0, %w1\n\t"
             "add %w0, %w0, 1"
             : "=r" (dst)
             : "r" (src));
#else
    dst = src + 1; // Fallback
#endif
    printf("Complex asm result: %d\n", dst);
}

void test_clobber_asm() {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile ("" : : : "memory");
#elif defined(__aarch64__)
    __asm__ volatile ("" : : : "memory");
#endif
    printf("Executed clobber asm\n");
}

void test_multiple_outputs_and_memory() {
    int out1 = 0, out2 = 0;
    int mem_var = 10;
#if defined(__x86_64__) || defined(__i386__)
    __asm__ ("mov %3, %0\n\t"
             "add $1, %0\n\t"
             "mov %0, %1\n\t"
             "mov %0, %2"
             : "=r" (out1), "=r" (out2), "=m" (mem_var)
             : "m" (mem_var));
#elif defined(__aarch64__)
    __asm__ ("ldr %w0, %3\n\t"
             "add %w0, %w0, 1\n\t"
             "mov %w1, %w0\n\t"
             "str %w0, %2"
             : "=r" (out1), "=r" (out2), "=m" (mem_var)
             : "m" (mem_var));
#endif
    printf("Multi-output result: out1=%d, out2=%d, mem_var=%d\n", out1, out2, mem_var);
}

int main() {
    test_basic_asm();
    test_complex_asm();
    test_clobber_asm();
    test_multiple_outputs_and_memory();
    return 0;
}

// New tests for LLVM_IR_0_PLAN.md

void test_multiple_outputs() {
    int a, b;
    __asm__("mov %0, 1\nmov %1, 2" : "=r"(a), "=r"(b));
}

void test_mixed_outputs() {
    int a; float b;
    __asm__("mov %0, 1\nmov %1, 2.0" : "=r"(a), "=r"(b));
}

void test_memory_constraint() {
    int x;
    __asm__("mov %0, 42" : "=m"(x));
}

void test_read_write_memory() {
    int x = 0;
    __asm__("add %0, 1" : "+m"(x));
}

void test_inputs_and_clobbers() {
    int x = 5;
    __asm__("add %0, 1" : "=r"(x) : "0"(x) : "memory", "cc");
}
