/*
 * perf events self profiling example.
 *
 * This version reads the counter data in userspace. This is more
 * complicated but has a lower cost than doing it in userspace. 
 *
 * Requires perf_event.h from a recent kernel tree, or download from 
 * git via:
 *
 * wget -O perf_event.h "http://git.kernel.org/?p=linux/kernel/git/torvalds/linux.git;a=blob_plain;f=include/uapi/linux/perf_event.h;hb=HEAD"
 *
 * Build with:
 * gcc -O2 -o perf_events_example2 perf_events_example2.c
 *
 * Copyright 2012 Anton Blanchard, IBM Corporation <anton@au.ibm.com>
 */

#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "perf_event.h"

/* Profile everything (kernel/hypervisor/idle/user) or just user? */
#undef USERSPACE_ONLY

#ifndef __NR_perf_event_open
#if defined(__PPC__)
#define __NR_perf_event_open	319
#define rmb()	asm volatile("lwsync":::"memory")

#define SPRN_UPMC1	771
#define SPRN_UPMC2	772
#define SPRN_UPMC3	773
#define SPRN_UPMC4	774
#define SPRN_UPMC5	775
#define SPRN_UPMC6	776

static unsigned long pmc_read(int val)
{
	unsigned long ret;

	switch (val) {
	case 1:
		asm volatile("mfspr %0,%1" : "=r" (ret): "i" (SPRN_UPMC1));
		break;
	case 2:
		asm volatile("mfspr %0,%1" : "=r" (ret): "i" (SPRN_UPMC2));
		break;
	case 3:
		asm volatile("mfspr %0,%1" : "=r" (ret): "i" (SPRN_UPMC3));
		break;
	case 4:
		asm volatile("mfspr %0,%1" : "=r" (ret): "i" (SPRN_UPMC4));
		break;
	case 5:
		asm volatile("mfspr %0,%1" : "=r" (ret): "i" (SPRN_UPMC5));
		break;
	case 6:
		asm volatile("mfspr %0,%1" : "=r" (ret): "i" (SPRN_UPMC6));
		break;
	default:
		fprintf(stderr, "Bad PMC read %d", val);
		exit(1);
	}

	return ret;
}

#elif defined(__i386__)
#define __NR_perf_event_open	336
#define rmb()	asm volatile("":::"memory")
#elif defined(__x86_64__)
#define __NR_perf_event_open	298
#define rmb()	asm volatile("":::"memory")
#else
#error __NR_perf_event_open must be defined
#endif
#endif

static inline int sys_perf_event_open(struct perf_event_attr *attr, pid_t pid,
				      int cpu, int group_fd,
				      unsigned long flags)
{
	attr->size = sizeof(*attr);
	return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

static int cycles_fd;
static void *cycles_mmap;
static int instructions_fd;
static void *instructions_mmap;

static void setup_counters(void)
{
	struct perf_event_attr attr;

	memset(&attr, 0, sizeof(attr));
#ifdef USERSPACE_ONLY
	attr.exclude_kernel = 1;
	attr.exclude_hv = 1;
	attr.exclude_idle = 1;
#endif

	attr.disabled = 1;
	attr.type = PERF_TYPE_HARDWARE;
	attr.config = PERF_COUNT_HW_CPU_CYCLES;
	cycles_fd = sys_perf_event_open(&attr, 0, -1, -1, 0);
	if (cycles_fd < 0) {
		perror("sys_perf_event_open");
		exit(1);
	}

	cycles_mmap = mmap(NULL, getpagesize(), PROT_READ, MAP_SHARED,
			   cycles_fd, 0);
	if (cycles_mmap == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	/*
	 * We use cycles_fd as the group leader in order to ensure
	 * both counters run at the same time and our CPI statistics are
	 * valid.
	 */
	attr.disabled = 0; /* The group leader will start/stop us */
	attr.type = PERF_TYPE_HARDWARE;
	attr.config = PERF_COUNT_HW_INSTRUCTIONS;
	instructions_fd = sys_perf_event_open(&attr, 0, -1, cycles_fd, 0);
	if (instructions_fd < 0) {
		perror("sys_perf_event_open");
		exit(1);
	}

	instructions_mmap = mmap(NULL, getpagesize(), PROT_READ, MAP_SHARED,
				 instructions_fd, 0);
	if (instructions_mmap == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	/* Fault in both pages so we don't incur a penalty */
	*(volatile unsigned int *)cycles_mmap;
	*(volatile unsigned int *)instructions_mmap;
}

static void start_counters(void)
{
	/* Only need to start and stop the group leader */
	ioctl(cycles_fd, PERF_EVENT_IOC_ENABLE);
}

static unsigned long long read_counter(int fd, struct perf_event_mmap_page *pc)
{
	unsigned int seq;
	unsigned long long count;

	do {
		seq = pc->lock;
		rmb();
		if (pc->index) {
			count = pmc_read(pc->index);
			count += pc->offset;
		} else {
			int res;
			res = read(fd, &count, sizeof(unsigned long long));
			assert(res == sizeof(unsigned long long));
		}
		rmb();
	} while (pc->lock != seq);

	return count;
}

static void stop_counters(void)
{
	ioctl(cycles_fd, PERF_EVENT_IOC_DISABLE);
}

static unsigned long long cycles_start;
static unsigned long long instructions_start;

static void init_counters(void)
{
	cycles_start = read_counter(cycles_fd, cycles_mmap);
	instructions_start = read_counter(instructions_fd, instructions_mmap);
}

static void read_counters(void)
{
	unsigned long long cycles;
	unsigned long long instructions;

	cycles = read_counter(cycles_fd, cycles_mmap) - cycles_start;
	instructions = read_counter(instructions_fd, instructions_mmap) - instructions_start;

	printf("cycles:\t\t%lld\n", cycles);
	printf("instructions:\t%lld\n", instructions);
	if (instructions > 0)
		printf("CPI:\t\t%0.2f\n", (float)cycles/instructions);
}

int main(int argc, char *argv[])
{
	setup_counters();

	start_counters();
	init_counters();

	/* Do something */

	read_counters();
	stop_counters();

	return 0;
}
