struct Foo { int x; float y; };
int main() {
    struct Foo foo = { 1, 2.0f };
    return foo.x == 1 ? 0 : 1;
}
