int add(int a, int b) { return a + b; }
int main() {
    int (*fn)(int, int) = add;
    return fn(2, 3) == 5 ? 0 : 1;
}
