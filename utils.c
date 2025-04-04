#include "utils.h"



void print_progress(int n, int totalSteps) {
    printf("\r[");
    for(int i = 0; i < (n * 50 / totalSteps); i++) printf("=");
    printf(">");
    for(int i = (n * 50 / totalSteps); i < 50; i++) printf(" ");
    printf("] %d%%", (n * 100 / totalSteps) + 1);
}