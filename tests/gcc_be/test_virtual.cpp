class Base {
public:
    virtual int get() { return 1; }
};
class Derived : public Base {
public:
    int get() override { return 2; }
};
int main() {
    Derived d;
    Base* b = &d;
    return b->get() == 2 ? 0 : 1;
}
