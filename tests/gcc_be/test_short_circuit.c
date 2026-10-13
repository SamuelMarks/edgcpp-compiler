/**
 * @file test_short_circuit.c
 * @brief Missing brief.
 * @details Missing details.
 */
/**
 * @brief Main entry point.
 * @details Runs tests.
 * @return 0 on success.
 */
int main() {
    int x = 0;
    if (1 || (x = 1)) {}
    return x;
}
