all: memleak test2 raja cs_dual mmul pe_dual pe_dual_group cs_dual_fork cs_multi

memleak: memleak.c++
	g++ -g -O0 memleak.c++ -o memleak

test2: test2.c
	gcc -fopenmp -std=c99 -g -O0 test2.c -o test2

clean:
	rm -rf *.o memleak test2 *.hpcstruct hpctoolkit-*
	cd raja/test/LULESH; make clean
	cd ../../..
	rm -f lulesh-RAJA-parallel.exe

raja: raja/test/LULESH/luleshRAJA-parallel.cxx
	push raja/test/LULESH
	make parallel
	pop
	ln -s raja/test/LULESH/lulesh-RAJA-parallel.exe .

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
