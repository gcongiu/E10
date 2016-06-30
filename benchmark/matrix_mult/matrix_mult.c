#include "matrix_mult.h"

int matrix_mult(int **A, int **B, int N, int **C) {
	
	int i, j, k;
	
	for (i = 0; i < N; i++) {
		for (j = 0; j < N; j++) {
			for (k = 0; k < N; k++) {
				C[i][j] += (int)(A[i][k] * B[k][j]);
			}
		}
	}

	return 0;
}
