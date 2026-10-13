/**
 * @file test_expr_ops.c
 * @brief Missing brief.
 * @details Missing details.
 */
/**
 * @brief Main entry point.
 * @details Runs tests.
 * @return 0 on success.
 */
int main() {
    int a = 1, b = 2;
    a += b;
    if (a != 3) return 1;
    a <<= 1;
    if (a != 6) return 1;
    a >>= 1;
    if (a != 3) return 1;
    a &= 1;
    if (a != 1) return 1;
    a ^= 3;
    if (a != 2) return 1;
    a |= 4;
    if (a != 6) return 1;
    return 0;
}
