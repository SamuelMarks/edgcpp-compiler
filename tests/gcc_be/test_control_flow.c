int main() {
    int i;
    int sum = 0;
    for (i = 0; i < 10; ++i) {
        if (i % 2 == 0) continue;
        if (i == 7) break;
        sum += i;
    }
    return sum == (1 + 3 + 5) ? 0 : 1;
}
