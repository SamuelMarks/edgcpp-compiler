/**
 * @file test_types.c
 * @brief Missing brief.
 * @details Missing details.
 */
/** @brief y member. */
/** @brief x member. */
/** @brief Foo struct. */
/** @brief Foo struct */
struct Foo {
    /** @brief x */
    int x;
    /** @brief y */
    float y;
};
/**
 * @brief Main entry point.
 * @details Runs tests.
 * @return 0 on success.
 */
int main() {
    struct Foo foo = { 1, 2.0f };
    return foo.x == 1 ? 0 : 1;
}
