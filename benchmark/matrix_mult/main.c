#include <stdlib.h>
#include "matrix_mult.h"

#define N 1300

int main(int argc, char **argv) {
	int i;
	int **A, **B, **C;

	A = (int**)malloc(sizeof(int*)*N);
	B = (int**)malloc(sizeof(int*)*N);
	C = (int**)malloc(sizeof(int*)*N);
	for (i = 0; i < N; i++) {
		A[i] = (int*)malloc(sizeof(int)*N);
		B[i] = (int*)malloc(sizeof(int)*N);
		C[i] = (int*)malloc(sizeof(int)*N);
	}

	matrix_mult(A, B, N, C);

	for (i = 0; i < N; i++) {
		free(A[i]);
		free(B[i]);
		free(C[i]);
	}
	free(A);
	free(B);
	free(C);

	return 0;
}
