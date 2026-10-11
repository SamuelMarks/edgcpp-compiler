struct MyBitfield {
    unsigned int a : 3;
    unsigned int b : 5;
    unsigned int c : 24;
};

struct Nested {
    int x;
    int y;
};

struct MyStruct {
    struct Nested nested[2];
    struct MyBitfield bf;
    union {
        int i;
        float f;
    } u;
};

struct MyStruct global_struct = {
    { {1, 2}, {3, 4} },
    { 1, 2, 3 },
    { .i = 42 }
};

char str_arr[10] = "hello";
int int_arr[2][3] = { {1, 2, 3}, {4, 5, 6} };

int main() {
    if (global_struct.nested[1].y != 4) return 1;
    if (global_struct.bf.a != 1) return 1;
    if (global_struct.u.i != 42) return 1;
    if (str_arr[0] != 'h' || str_arr[5] != '\0') return 1;
    if (int_arr[1][2] != 6) return 1;
    return 0;
}
