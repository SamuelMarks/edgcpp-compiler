/**
 * @file test_aggregates.c
 * @brief Missing brief.
 * @details Missing details.
 */
/** @brief MyBitfield struct. */
struct MyBitfield {
/** @brief a member. */
    unsigned int a : 3;
/** @brief b member. */
    unsigned int b : 5;
/** @brief c member. */
    unsigned int c : 24;
};

/** @brief Nested struct. */
struct Nested {
/** @brief x member. */
    int x;
/** @brief y member. */
    int y;
};

/** @brief MyStruct struct. */
struct MyStruct {
/** @brief nested member. */
    struct Nested nested[2];
/** @brief bf member. */
    struct MyBitfield bf;
/** @brief the union */
    union {
        /** @brief i member. */
        int i;
        /** @brief f member. */
        float f;
    } u; /**< @brief u member */
};

/** @brief global_struct member. */
struct MyStruct global_struct = {
    { {1, 2}, {3, 4} },
    { 1, 2, 3 },
    { .i = 42 }
};

/** @brief str_arr member. */
char str_arr[10] = "hello";
/** @brief int_arr member. */
int int_arr[2][3] = { {1, 2, 3}, {4, 5, 6} };

/**
 * @brief Main entry point.
 * @details Runs tests.
 * @return 0 on success.
 */
int main() {
    if (global_struct.nested[1].y != 4) return 1;
    if (global_struct.bf.a != 1) return 1;
    if (global_struct.u.i != 42) return 1;
    if (str_arr[0] != 'h' || str_arr[5] != '\0') return 1;
    if (int_arr[1][2] != 6) return 1;
    return 0;
}
