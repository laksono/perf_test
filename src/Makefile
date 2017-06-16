all: cs_dual mmul pe_dual pe_dual_group cs_dual_fork cs_multi

clean:
	rm -rf *.o *.hpcstruct hpctoolkit-*
	rm -f cs_dual mmul pe_dual pe_dual_group cs_dual_fork cs_multi

pe_dual_group: pe_dual_group.c
	gcc -g -std=gnu99 -O0 ./pe_dual_group.c -o pe_dual_group

cs_multi: cs_multi.c
	gcc -g -std=gnu99 -O0 ./cs_multi.c -o cs_multi

pe_dual: pe_dual.c
	gcc -g -std=gnu99 -O0 ./pe_dual.c -o pe_dual

cs_dual: cs_dual.c matrix_multiply.c matrix_multiply.h
	gcc -g -std=gnu99 -O0 ./cs_dual.c -o cs_dual matrix_multiply.c

cs_dual_fork: cs_dual_fork.c 
	gcc -g -std=gnu99 -O0 ./cs_dual_fork.c -o cs_dual_fork 

mmul: matrix_multiply_main.c matrix_multiply.c matrix_multiply.h
	gcc -g -std=gnu99 -O0 matrix_multiply_main.c -o mmul matrix_multiply.c
