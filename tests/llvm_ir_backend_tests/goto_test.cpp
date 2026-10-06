extern "C" void printf(const char*, ...);

void test_computed_goto(int idx) {
    void* arr[] = { &&label1, &&label2, &&label3 };
    if (idx < 0 || idx > 2) return;

    goto *arr[idx];

label1:
    printf("Label 1\n");
    return;
label2:
    printf("Label 2\n");
    return;
label3:
    printf("Label 3\n");
    return;
}

int main() {
    test_computed_goto(0);
    test_computed_goto(1);
    test_computed_goto(2);
    return 0;
}
