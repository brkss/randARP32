#include "bjorklund.h"

static void build_pattern(int *pattern, int *counts, int *remainders, int level, int *index) {
    if (level == -1) {
        return;
    }
    if (level == 0) {
        pattern[(*index)++] = 1;
        return;
    }
    for (int i = 0; i < counts[level]; i++) {
        build_pattern(pattern, counts, remainders, level - 1, index);
    }
    if (remainders[level] != 0) {
        build_pattern(pattern, counts, remainders, level - 1, index);
    }
}

void bjorklund_euclid(int steps, int pulses, int *pattern) {
    // Clean up out-of-range
    if (pulses > steps) pulses = steps;
    if (pulses < 0) pulses = 0;

    // Clear pattern
    for (int i = 0; i < steps; i++) {
        pattern[i] = 0;
    }
    // If no pulses, done
    if (pulses == 0) {
        return;
    }

    int counts[32] = {0};
    int remainders[32] = {0};

    int divisor = steps - pulses;
    int level = 0;
    int remainder = pulses;
    counts[0] = divisor;
    remainders[0] = pulses;
    
    int numerator = steps;
    int denominator = pulses;

    // The iterative part to fill counts[] and remainders[]
    while (remainders[level] > 1) {
        level++;
        counts[level] = numerator / denominator;
        remainders[level] = numerator % denominator;
        numerator = denominator;
        denominator = remainders[level];
    }

    // Now the pattern is built from the bottom up
    int index = 0;
    build_pattern(pattern, counts, remainders, level, &index);

    // index will be 'pulses' so far, repeated expansions
    // Finally, rotate until the first element is 1
    while (pattern[0] != 1) {
        int tmp = pattern[0];
        for (int i = 0; i < steps - 1; i++) {
            pattern[i] = pattern[i + 1];
        }
        pattern[steps - 1] = tmp;
    }
}