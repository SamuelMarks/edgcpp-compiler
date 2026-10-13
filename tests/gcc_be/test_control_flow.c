/**
 * @file test_control_flow.c
 * @brief Missing brief.
 * @details Missing details.
 */
/**
 * @brief Main entry point.
 * @details Runs tests.
 * @return 0 on success.
 */
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
