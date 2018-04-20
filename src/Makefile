PERFMON_ROOT=/home/la5/git/perfmon2-libpfm4

all: cs_dual mmul pe_dual pe_dual_group cs_dual_fork cs_multi context_switches cs_switch

clean:
	rm -rf *.o *.hpcstruct hpctoolkit-*
	rm -f cs_dual mmul pe_dual pe_dual_group cs_dual_fork cs_multi

cs_switch: cs_switch.c
	gcc -g -std=gnu99 -O0 cs_switch.c -o cs_switch

context_switches: context_switches.c
	gcc -g -std=gnu99 -O0 ./context_switches.c -o context_switches

pe_dual_group: pe_dual_group.c
	gcc -g -std=gnu99 -O0 ./pe_dual_group.c -o pe_dual_group

cs_multi: cs_multi.c
	gcc -g -std=gnu99 -O0 ./cs_multi.c -o cs_multi

pe_dual: pe_dual.c
	gcc -g -std=gnu99 -O0 ./pe_dual.c -o pe_dual

cs_dual: cs_dual.c matrix_multiply.c matrix_multiply.h
	gcc -g -std=gnu99 -O0 ./cs_dual.c -o cs_dual matrix_multiply.c

gen_sample: gen_sample.c 
	gcc -g -std=gnu99 -O0 ./gen_sample.c -o gen_sample

cs_dual_fork: cs_dual_fork.c 
	gcc -g -std=gnu99 -O0 ./cs_dual_fork.c -o cs_dual_fork 

mmul: matrix_multiply_main.c matrix_multiply.c matrix_multiply.h
	gcc -g -std=gnu99 -O0 matrix_multiply_main.c -o mmul matrix_multiply.c

test_pmu: test_pmu.c
	gcc -g -O0 test_pmu.c  -I ${PERFMON_ROOT}/include/ -L $(PERFMON_ROOT)/lib/ -lpfm -o test_pmu
