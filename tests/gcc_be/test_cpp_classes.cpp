class Base {
public:
    int x;
    Base(int x) : x(x) {}
};
class Derived : public Base {
public:
    int y;
    Derived(int x, int y) : Base(x), y(y) {}
};
int main() {
    Derived d(1, 2);
    return d.x == 1 && d.y == 2 ? 0 : 1;
}
