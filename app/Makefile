all: memleak test2 raja 

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
	cd raja/test/LULESH; make parallel
	ln -s raja/test/LULESH/lulesh-RAJA-parallel.exe .
