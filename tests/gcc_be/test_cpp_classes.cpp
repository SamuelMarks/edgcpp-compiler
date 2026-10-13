/**
 * @file test_cpp_classes.cpp
 * @brief Test file.
 * @details Tests functionality.
 */
/**
 * @brief Base class.
 * @details Helper class for testing.
 */
class Base {
public:
    /** @brief x member. */
    int x;
/**
 * @brief Base function.
 * @details Auto-generated documentation.
 * @param[in] x argument.
 * @return Return value.
 */
    Base(int x) : x(x) {}
};
/**
 * @brief Derived class.
 * @details Helper class for testing.
 */
class Derived : public Base {
public:
    /** @brief y member. */
    int y;
/**
 * @brief Derived function.
 * @details Auto-generated documentation.
 * @param[in] x argument.
 * @param[in] y argument.
 * @return Return value.
 */
    Derived(int x, int y) : Base(x), y(y) {}
};
/**
 * @brief Main entry point.
 * @details Runs tests.
 * @return 0 on success.
 */
int main() {
/**
 * @brief d function.
 * @details Test helper function d.
 * @param[in] 1 Input parameter.
 * @param[in] 2 Input parameter.
 * @return Expected return value.
 */
    Derived d(1, 2);
    return d.x == 1 && d.y == 2 ? 0 : 1;
}
