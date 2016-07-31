#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main() {
  vector test;
  VectorNew(&test, sizeof(int), NULL, 6);
  for (int i = 0; i < 10; i++)
    VectorAppend(&test, &i);
  return 0;
}
