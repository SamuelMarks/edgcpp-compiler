extern "C" void printf(const char*, ...);

void test_floats() {
    float f = 3.14159265f;
    double d = 2.718281828459045;
    long double ld = 1.6180339887498948482;
    printf("Float: %f\n", f);
    printf("Double: %lf\n", d);
}

void test_special_floats() {
    float inf = __builtin_inff();
    float nan = __builtin_nanf("");
    printf("Inf: %f, NaN: %f\n", inf, nan);
}

void test_strings() {
    const wchar_t* wstr = L"Wide String";
    const char16_t* u16str = u"UTF-16 String";
    const char32_t* u32str = U"UTF-32 String";
    printf("Wide string length: %zu\n", sizeof(L"Wide String") / sizeof(wchar_t) - 1);
    printf("UTF-16 string length: %zu\n", sizeof(u"UTF-16 String") / sizeof(char16_t) - 1);
    printf("UTF-32 string length: %zu\n", sizeof(U"UTF-32 String") / sizeof(char32_t) - 1);
}

// New tests for LLVM_IR_0_PLAN.md

#if __has_include(<stdfloat>)
#include <stdfloat>
#endif

// Some EDG/Clang builtins
_Float16 f16_val = (_Float16)1.0f;
__bf16 bf16_val = (__bf16)1.0f;
long double ld_val = 1.0L;

_Complex float cf_val;
_Complex double cd_val;

const char* s1 = "";
const wchar_t* s2 = L"wide";
const char16_t* s4 = u"utf16";
const char32_t* s5 = U"utf32";

void test_floats_new() {
    _Float16 x = f16_val;
    __bf16 y = bf16_val;
    long double z = ld_val;
}

void test_complex() {
    _Complex float a = cf_val;
    _Complex double b = cd_val;
}

void test_strings_new() {
    const char* p1 = s1;
    const wchar_t* p2 = s2;
    const char16_t* p4 = s4;
    const char32_t* p5 = s5;
}

int main() {
    test_floats();
    test_special_floats();
    test_strings();
    test_floats_new();
    test_complex();
    test_strings_new();
    return 0;
}
