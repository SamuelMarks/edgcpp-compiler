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

int main() {
    test_basic_asm();
    test_complex_asm();
    test_clobber_asm();
    return 0;
}
