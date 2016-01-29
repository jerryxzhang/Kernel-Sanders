/**
 * A mini-library of fixed point arithmetic functions. Fixed point numbers are
 * represented with 32 bits: 1 sign, 17 integer, and 14 fractional.
 */

#include <stdint.h>

#define NUM_INTEGER_BITS 17
#define NUM_FRACTIONAL_BITS 14
#define FIXED_POINT_MULTIPLIER (1 << NUM_FRACTIONAL_BITS)

typedef int32_t fixed_point;

fixed_point to_fp(int n);
int to_int(fixed_point x);
int round_fp(fixed_point x);

// To add two fixed point numbers just use '+' as normal

// Use this function for subtraction too
fixed_point add(fixed_point x, int n);

// To multiply or divide a fixed point with an int just use '*' or '/'

fixed_point multiply(fixed_point x, fixed_point y);

fixed_point divide(fixed_point x, fixed_point y);

//Prints out the message and the representation of the fixed point number on one line
void print_fp(fixed_point x, const char* msg);


