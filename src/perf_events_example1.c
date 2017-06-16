/*
 * perf events self profiling example.
 *
 * This version uses the kernel to read the counter data. This is much
 * simpler but has a higher cost than doing it in userspace. The added
 * cost should only matter if you are profiling very small sections of
 * code.
 *
 * Requires perf_event.h from a recent kernel tree, or download from 
 * git via:
 *
 * wget -O perf_event.h "http://git.kernel.org/?p=linux/kernel/git/torvalds/linux.git;a=blob_plain;f=include/uapi/linux/perf_event.h;hb=HEAD"
 *
 * Build with:
 * gcc -O2 -o perf_events_example1 perf_events_example1.c
 *
 * Copyright 2012 Anton Blanchard, IBM Corporation <anton@au.ibm.com>
 */

#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>

/* Profile everything (kernel/hypervisor/idle/user) or just user? */
#undef USERSPACE_ONLY

#ifndef __NR_perf_event_open
#if defined(__PPC__)
#define __NR_perf_event_open	319
#elif defined(__i386__)
#define __NR_perf_event_open	336
#elif defined(__x86_64__)
#define __NR_perf_event_open	298
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
static int instructions_fd;

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
}

static void start_counters(void)
{
	/* Only need to start and stop the group leader */
	ioctl(cycles_fd, PERF_EVENT_IOC_ENABLE);
}

static void stop_counters(void)
{
	ioctl(cycles_fd, PERF_EVENT_IOC_DISABLE);
}

static void read_counters(void)
{
	size_t res;
	unsigned long long cycles;
	unsigned long long instructions;

	res = read(cycles_fd, &cycles, sizeof(unsigned long long));
	assert(res == sizeof(unsigned long long));

	res = read(instructions_fd, &instructions, sizeof(unsigned long long));
	assert(res == sizeof(unsigned long long));

	printf("cycles:\t\t%lld\n", cycles);
	printf("instructions:\t%lld\n", instructions);
	if (instructions > 0)
		printf("CPI:\t\t%0.2f\n", (float)cycles/instructions);
}

int main(int argc, char *argv[])
{
	setup_counters();

	start_counters();

	/* Do something */
	int i, rem = 0;
	for(i=0; i<5; i++) {
	  rem += sleep(1);
	}

	stop_counters();
	printf("Remainder of sleep: %d\n", rem);
	read_counters();

	return 0;
}
