extern "C" void printf(const char*, ...);
extern "C" void exit(int);

struct PolymorphicBase {
    virtual ~PolymorphicBase() {}
    virtual void print() const { printf("PolymorphicBase\n"); }
};

struct Derived : PolymorphicBase {
    void print() const override { printf("Derived\n"); }
};

struct DestructorCheck {
    int id;
    DestructorCheck(int i) : id(i) { printf("Construct %d\n", id); }
    ~DestructorCheck() { printf("Destruct %d\n", id); }
};

void throw_int() {
    throw 42;
}

void throw_derived() {
    throw Derived();
}

void rethrow() {
    try {
        throw_int();
    } catch (...) {
        printf("Caught and rethrowing\n");
        throw;
    }
}

void test_basic_try_catch() {
    try {
        throw_int();
    } catch (int e) {
        printf("Caught int: %d\n", e);
    }
}

void test_polymorphic_catch() {
    try {
        throw_derived();
    } catch (const PolymorphicBase& b) {
        printf("Caught polymorphic base\n");
        b.print();
    }
}

void test_rethrow() {
    try {
        rethrow();
    } catch (int e) {
        printf("Caught rethrown int: %d\n", e);
    }
}

void test_destructors() {
    try {
        DestructorCheck d1(1);
        DestructorCheck d2(2);
        throw_int();
    } catch (int) {
        printf("Caught exception after destructors\n");
    }
}

int main() {
    test_basic_try_catch();
    test_polymorphic_catch();
    test_rethrow();
    test_destructors();
    return 0;
}
