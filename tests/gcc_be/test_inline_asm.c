int main() {
    int out = 0;
#if defined(__aarch64__)
    __asm__("mov %w0, 1" : "=r"(out));
#elif defined(__x86_64__)
    __asm__("mov $1, %0" : "=r"(out));
#else
    out = 1;
#endif
    return out == 1 ? 0 : 1;
}
