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

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <sys/syscall.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/prctl.h>

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

#define PERF_SIGNAL (SIGRTMIN+4)

typedef void * perf_buffer_t;

static int count_total[] = {0,0};
static int event_fd[2];
static perf_buffer_t event_buf[2];

#define buffer_pages 1
static size_t pagesize;
static size_t pgmsk;

static uint64_t sample_type = PERF_SAMPLE_PERIOD | PERF_SAMPLE_IP | PERF_SAMPLE_ADDR;

static inline
int sys_perf_event_open(struct perf_event_attr *attr, pid_t pid,
				      int cpu, int group_fd,
				      unsigned long flags)
{
	attr->size = sizeof(*attr);
	return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}


static int
start_counters(int fd)
{
	return ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
}

static int
stop_counters(int fd)
{
	return ioctl(fd, PERF_EVENT_IOC_DISABLE);
}

static int
stop_all()
{
	int ret  = stop_counters(event_fd[0]);
	ret 	+= stop_counters(event_fd[1]);

	return ret;
}

static int
start_all()
{
	int ret = ioctl(event_fd[0], PERF_EVENT_IOC_ENABLE);
	ret    += ioctl(event_fd[1], PERF_EVENT_IOC_ENABLE);
}

static int
get_fd(int sig_fd)
{
	for(int i=0; i<2; i++) {
		if (event_fd[i] == sig_fd)
			return i;
	}
	return -1;
}

/*
 * event_buf already has the mmap'ed address for perf buffer,
 * So, go ahead and read the data.
 *
 * The payload data (event data) starts after the first page of control
 * information.
 */
static int read_from_perf_buffer(void *event_buf, void *buf, size_t sz)
{
	struct perf_event_mmap_page *header = event_buf;
	void *data;
	unsigned long tail;
	size_t avail_sz, min, c;

	/*
	 * data points to beginning of buffer payload
	 */
	data = ((void *)header) + sysconf(_SC_PAGESIZE);

	/*
	 * position of tail within the buffer payload.
	 */
	tail = header->data_tail & pgmsk;

	/*
	 * Size of available data.
	 */
	avail_sz = header->data_head - header->data_tail;
	if (sz > avail_sz) {
		fprintf(stderr, "Needed size is more than available size\n");
		return -1;
	}

	/*
	 * c = size till the end of buffer
	 *
	 * buffer size is a power of two.
	 */
	c = pgmsk + 1 - tail;

	/*
	 * min with requested size.
	 */
	min = c < sz ? c : sz;

	/* copy beginning */
	memcpy(buf, data + tail, min);

	/* copy wrapped around leftover */
	if (sz > min)
		memcpy(buf + min, data, sz - min);

	/* header->data_tail should reflect the last read data */
	header->data_tail += sz;

	return 0;
}


static int read_from_perf_buffer_64(void *event_buf, void *buf)
{
	return read_from_perf_buffer(event_buf, buf, sizeof(uint64_t));
}



/*
 * The below parser is a stripped down version of perf source. To keep it
 * simple, it only looks for PERF_SAMPLE_IDENTIFIER, PERF_SAMPLE_CALLCHAIN,
 * PERF_SAMPLE_IP, PERF_SAMPLE_RAW and PERF_SAMPLE_TIME.
 * More parsers can be added if needed.
 */
static int parse_perf_sample(void *event_buf, struct perf_event_header *ehdr)
{
	size_t sz;
	uint64_t type, val64;
	uint64_t time_enabled, time_running;
	const char *str;
	int ret;
	uint64_t value;
	uint64_t id;
	struct { uint32_t pid, tid; } pid;

	if (!ehdr)
		return -1;

	sz = ehdr->size - sizeof(*ehdr);

	type = sample_type;

	if (type & PERF_SAMPLE_IDENTIFIER) {
		ret = read_from_perf_buffer_64(event_buf, &val64);
		if (ret) {
			fprintf(stderr, "cannot read IP");
			return -1;
		}
		fprintf(stderr, "ID :%llu ", val64);
		sz -= sizeof(val64);
	}

	/*
	 * the sample_type information is laid down
	 * based on the PERF_RECORD_SAMPLE format specified
	 * in the perf_event.h header file.
	 * That order is different from the enum perf_event_sample_format.
	 */
	if (type & PERF_SAMPLE_IP) {
		ret = read_from_perf_buffer_64(event_buf, &val64);
		if (ret) {
			fprintf(stderr, "cannot read IP");
			return -1;
		}

		/*
		 * MISC_EXACT_IP indicates that kernel is returning
		 * th  IIP of an instruction which caused the event, i.e.,
		 * no skid
		 */
		//fprintf(stderr, "IIP:%#016llu  ", val64);
		sz -= sizeof(val64);
	}

	if (type & PERF_SAMPLE_TID) {
		ret = read_from_perf_buffer(event_buf, &pid, sizeof(pid));
		if (ret) {
			fprintf(stderr, "cannot read PID");
			return -1;
		}

		//fprintf(stderr, "PID:%d  TID:%d  ", pid.pid, pid.tid);
		sz -= sizeof(pid);
	}

	if (type & PERF_SAMPLE_TIME) {
		ret = read_from_perf_buffer_64(event_buf, &val64);
		if (ret) {
			fprintf(stderr, "cannot read time");
			return -1;
		}

		//fprintf(stderr, "TIME:%llu  ", val64);
		sz -= sizeof(val64);
	}

	if (type & PERF_SAMPLE_ADDR) {
		ret = read_from_perf_buffer_64(event_buf, &val64);
		if (ret) {
			fprintf(stderr, "cannot read time");
			return -1;
		}

		//fprintf(stderr, "ADDR:%llu  ", val64);
		sz -= sizeof(val64);
	}
	if (type & PERF_SAMPLE_CPU) {
		struct { uint32_t cpu, reserved; } cpu;
		ret = read_from_perf_buffer(event_buf, &cpu, sizeof(cpu));
		if (ret) {
			fprintf(stderr, "cannot read cpu");
			return -1;
		}
		//fprintf(stderr, "CPU:%u  ", cpu.cpu);
		sz -= sizeof(cpu);
	}

	if (type & PERF_SAMPLE_PERIOD) {
		ret = read_from_perf_buffer_64(event_buf, &val64);
		if (ret) {
			fprintf(stderr, "cannot read period");
			return -1;
		}
		//fprintf(stderr, "PERIOD:%llu  ", val64);
		sz -= sizeof(val64);
	}

	if (type & PERF_SAMPLE_CALLCHAIN) {
		uint64_t nr, ip;

		ret = read_from_perf_buffer_64(event_buf, &nr);
		if (ret) {
			fprintf(stderr, "cannot read callchain nr");
			return -1;
		}
		sz -= sizeof(nr);

		//fprintf(stderr, "\n  CALLCHAIN :\n");
		while(nr--) {
			ret = read_from_perf_buffer_64(event_buf, &ip);
			if (ret) {
				fprintf(stderr, "cannot read ip");
				return -1;
			}

			sz -= sizeof(ip);

			//fprintf(stderr, "\t0x%llu\n", ip);
		}
	}

	fprintf(stderr, ".");
	return 0;
}


/*
 * Not the data we need? Skip the data.
 */
static void skip_perf_data(void *event_buf, size_t sz)
{
	struct perf_event_mmap_page *hdr = event_buf;

	if ((hdr->data_tail + sz) > hdr->data_head)
		sz = hdr->data_head - hdr->data_tail;

	hdr->data_tail += sz;
}

static int is_more_perf_data(void *event_buf)
{
	struct perf_event_mmap_page *hdr = event_buf;

	if (hdr->data_tail < hdr->data_head)
		return 1;

	return 0;
}


static 
void event_handler(int signum, siginfo_t *info, void *uc)
{
	int ret;

	if (info->si_code < 0) {
		fprintf(stderr, "Required signal not generated\n");
		return;
	}

	if (info->si_code != POLL_HUP) {
		fprintf(stderr, "POLL_HUP signal not generated by PERF_SIGNAL , %d\n", info->si_code);
		return;
	}

	int fd = info->si_fd;
	int index = get_fd(fd);
	if (index < 0) {
		fprintf(stderr, "unknown fd: %d\n", fd);
	}

	struct perf_event_header ehdr;
again:
	ret = read_from_perf_buffer(event_buf[index], &ehdr, sizeof(ehdr));
	if (ret) {
		fprintf(stderr, "cannot read event header\n");
		return;
	}
	if (ehdr.type == PERF_RECORD_SAMPLE) {
		//fprintf(stderr, "[%d] fd: %d    ", index, fd);
		ret = parse_perf_sample(event_buf[index], &ehdr);

	}  else {
		skip_perf_data(event_buf[index], ehdr.size);
	}

	if (is_more_perf_data(event_buf))
		goto again;

	count_total[index]++;

	ret=start_counters(fd);

	(void) ret;
}

static 
int setup_notification(int fd)
{
	/*
	 * Setup notification on the file descriptor
	 */
	int ret = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_ASYNC);
	if (ret == -1) {
		fprintf(stderr, "Can't set notification\n");
		return -1;
	}

	ret = fcntl(fd, F_SETSIG, PERF_SIGNAL);
	if (ret == -1) {
		fprintf(stderr, "Cannot set sigio\n");
		return -1;
	}

	/* Get ownership of the descriptor */
	ret = fcntl(fd, F_SETOWN, getpid());
	if (ret == -1) {
		fprintf(stderr, "Error in setting owner\n");
		return -1;
	}

	/* Enable the event for one period */
	ret = ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
	if (ret == -1) {
		fprintf(stderr, "Error in IOC_REFRESH\n");
		return -1;
	}
	return ret;
}

static void*
setup_buffer(int fd)
{
	void *buf = mmap(NULL, (buffer_pages + 1) * pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		fprintf(stderr, "Can't mmap buffer\n");
		return NULL;
	}

	return buf;
}


static
int setup_counters(uint64_t type, uint64_t config)
{
	static int index = 0;
	struct perf_event_attr attr;

	memset(&attr, 0, sizeof(attr));
#ifdef USERSPACE_ONLY
	attr.exclude_kernel = 1;
	attr.exclude_hv = 1;
	attr.exclude_idle = 1;
#endif

	attr.disabled 	 = 1;
	

	attr.type 	 = type;
	attr.config 	 = config;
	attr.sample_freq = 4000;
	attr.freq 	 = 1;
	attr.wakeup_events = 1;
	attr.size	   = sizeof(struct perf_event_attr);
	attr.sample_type   = sample_type;

	event_fd[index] = sys_perf_event_open(&attr, 0, -1, -1, 0);
	if (event_fd[index] < 0) {
		perror("sys_perf_event_open");
	}
	int fd = event_fd[index];

	event_buf[index] = setup_buffer(fd);
	if (event_buf[index] == NULL) {
		exit(2);
	}

	setup_notification(fd);

	index++;
	return fd;
}


static void
read_counters(int index)
{
	size_t res;
	unsigned long long counter_result;

	res = read(event_fd[index], &counter_result, sizeof(unsigned long long));
	assert(res == sizeof(unsigned long long));

	printf("[%d] counter:\t\t%lld\n[%d] Num counter: %d\n\n", index, counter_result, index, count_total[index]);
}

static void
setup_handler()
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = event_handler;
	act.sa_flags     = SA_SIGINFO;
	sigaction(PERF_SIGNAL, &act, 0);
}

#ifndef asm
#define asm __asm__
#endif

int instructions_million(void) {

#if defined(__i386__) || (defined __x86_64__)
	asm(	"	xor	%%ecx,%%ecx\n"
		"	mov	$499999999,%%ecx\n"
		"test_loop%=:\n"
		"	dec	%%ecx\n"
		"	jnz	test_loop%=\n"
		: /* no output registers */
		: /* no inputs */
		: "cc", "%ecx" /* clobbered */
	);
	printf("instruction millions: OK\n");
	return 0;
#elif defined(__PPC__)
	asm(	"	nop			# to give us an even million\n"
		"	lis	15,499997@ha	# load high 16-bits of counter\n"
		"	addi	15,15,499997@l	# load low 16-bits of counter\n"
		"55:\n"
		"	addic.  15,15,-1              # decrement counter\n"
		"	bne     0,55b                  # loop until zero\n"
		: /* no output registers */
		: /* no inputs */
		: "cc", "15" /* clobbered */
	);
	return 0;
#elif defined(__ia64__)

	asm(	"	mov	loc6=166666	// below is 6 instr.\n"
		"	;;			// because of that we count 4 too few\n"
		"55:\n"
		"	add	loc6=-1,loc6	// decrement count\n"
		"	;;\n"
		"	cmp.ne	p2,p3=0,loc6\n"
		"(p2)	br.cond.dptk	55b	// if not zero, loop\n"
		: /* no output registers */
		: /* no inputs */
		: "p2", "loc6" /* clobbered */
	);
	return 0;
#elif defined(__sparc__)
	asm(	"	sethi	%%hi(333333), %%l0\n"
		"	or	%%l0,%%lo(333333),%%l0\n"
		"test_loop:\n"
		"	deccc	%%l0		! decrement count\n"
		"	bnz	test_loop	! repeat until zero\n"
		"	nop			! branch delay slot\n"
		: /* no output registers */
		: /* no inputs */
		: "cc", "l0" /* clobbered */
	);
	return 0;
#elif defined(__arm__)
	asm(	"	ldr	r2,count	@ set count\n"
		"	b       test_loop\n"
		"count:	.word 333332\n"
		"test_loop:\n"
		"	add	r2,r2,#-1\n"
		"	cmp	r2,#0\n"
		"	bne	test_loop	@ repeat till zero\n"
		: /* no output registers */
		: /* no inputs */
		: "cc", "r2" /* clobbered */
	);
	return 0;
#elif defined(__aarch64__)
	asm(	"	ldr	x2,=333332	// set count\n"
		"test_loop:\n"
		"	add	x2,x2,#-1\n"
		"	cmp	x2,#0\n"
		"	bne	test_loop	// repeat till zero\n"
		: /* no output registers */
		: /* no inputs */
		: "cc", "r2" /* clobbered */
	);
	return 0;
#endif

	return -1;

}

int
main(int argc, char *argv[])
{
	pagesize = sysconf(_SC_PAGESIZE);
	pgmsk = (buffer_pages * pagesize) - 1;

	setup_handler();

	int fd = setup_counters(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
	if (fd < 0) {
		perror("setup counter cyc");
		exit(1);
	}
	printf("fd cycles: %d\n", fd);

	fd = setup_counters(PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND);
	//fd = setup_counters(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS);
	if (fd < 0) {
		perror("setup counter ins");
		exit(1);
	}
	printf("fd page-faults: %d\n", fd);

	start_counters(fd);

	/* Do something */
	instructions_million();

	stop_counters(fd);
	fprintf(stderr, "\n");
	read_counters(0);
	read_counters(1);

	return 0;
}
