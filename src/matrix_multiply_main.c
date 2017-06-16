
#include <stdio.h>
#include "matrix_multiply.h"

int main(int argc, char *argv[])
{
  naive_matrix_multiply(0);
  long long res = naive_matrix_multiply_estimated_flops(0);
  printf("\nestimated FLOPS: %ld\n", res);
}

