/**
 * A mini-library of fixed point arithmetic functions. Fixed point numbers are
 * represented with 32 bits: 1 sign, 17 integer, and 14 fractional.
 */

#include "fixed_point.h"
#include <stdio.h>

fixed_point to_fp(int n) {
    return n * FIXED_POINT_MULTIPLIER;
}

int to_int(fixed_point x) {
    return x / FIXED_POINT_MULTIPLIER;
}

int round_fp(fixed_point x) {
    return x > 0 ? (x + FIXED_POINT_MULTIPLIER / 2) / FIXED_POINT_MULTIPLIER :
        (x - FIXED_POINT_MULTIPLIER / 2) / FIXED_POINT_MULTIPLIER;
}

int add(fixed_point x, int n) {
    return x + to_fp(n);
}

fixed_point multiply(fixed_point x, fixed_point y) {
    return ((int64_t) x) * y / FIXED_POINT_MULTIPLIER;

}

fixed_point divide(fixed_point x, fixed_point y) {
    return ((int64_t) x) * FIXED_POINT_MULTIPLIER / y;
}

void print_fp(fixed_point x, const char* msg) {
    printf("%s%d.%d\n", msg, to_int(x), to_int(100000 * add(x, -to_int(x))));
}

/*
int main(int argc, char** argv) {
    fixed_point x = to_fp(80) + to_fp(10);

    print_fp(x, "80 + 10:  ");
    print_fp(to_fp(10), "10:  ");
    print_fp(divide(to_fp(100), to_fp(3)), "100/3: ");
    print_fp(divide(to_fp(999), to_fp(1000)), "999/1000: ");
    print_fp(add(to_fp(99), 50), "99 + 50: ");
    print_fp(multiply(to_fp(12), to_fp(12)), "12 * 12: ");
    print_fp(multiply(divide(to_fp(3), to_fp(2)), divide(to_fp(3), to_fp(2))), "1.25**2: ");

    printf("Round down %d\n", round_fp(divide(to_fp(54), to_fp(10))));
    printf("Round up %d\n", round_fp(divide(to_fp(56), to_fp(10))));

    return 0;
}*/ // Enable to run tests on the library
