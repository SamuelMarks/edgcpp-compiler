/**
 * @file test_functions.c
 * @brief Missing brief.
 * @details Missing details.
 */
/**
 * @brief add function.
 * @details Auto-generated documentation.
 * @param[in] a argument.
 * @param[in] b argument.
 * @return Return value.
 */
/**
 * @brief Adds two numbers.
 * @details Used for testing.
 * @param[in] a First.
 * @param[in] b Second.
 * @return sum.
 */
int add(int a, int b) { return a + b; }
/**
 * @brief Main entry point.
 * @details Runs tests.
 * @return 0 on success.
 */
int main() {
    int (*fn)(int, int) = add;
    return fn(2, 3) == 5 ? 0 : 1;
}
