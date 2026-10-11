namespace std {
    class bad_alloc {};
}

struct MyException {
    int code;
    MyException(int c) : code(c) {}
    ~MyException() {}
};

struct AutoCleanup {
    int* p;
    AutoCleanup(int* ptr) : p(ptr) { *p = 1; }
    ~AutoCleanup() { *p = 0; }
};

void func(int *flag) {
    AutoCleanup cleanup(flag);
    throw MyException(42);
}

int main() {
    int cleanup_flag = 0;
    try {
        func(&cleanup_flag);
    } catch (const MyException& e) {
        if (e.code == 42 && cleanup_flag == 0) {
            return 0; /* Success */
        }
        return 1;
    } catch (const std::bad_alloc& e) {
        return 1;
    } catch (...) {
        try {
            throw;
        } catch (...) {
            return 1;
        }
    }
    return 1;
}
