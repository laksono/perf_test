#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <omp.h>

#define N  1000000000
#define REPS 32


int main() 
{
	omp_set_num_threads(2);
	int pagesize = sysconf(_SC_PAGESIZE);
	printf("page size = %d\n", pagesize);
	int nextpage = pagesize/sizeof(int);

	for (int i=0; i < REPS; i++) {
		int *v = malloc(sizeof(int) * N);
#pragma omp parallel
		{
#pragma omp for
			for(int j = 0; j < N; j += nextpage) {
				v[j] = 5;
			}
		}
		free(v);
	}
	return 0;
}
