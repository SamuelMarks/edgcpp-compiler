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

int main() {
    test_floats();
    test_special_floats();
    test_strings();
    return 0;
}
