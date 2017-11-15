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
#include <errno.h>

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
#define MAX_EVENTS  3

#define buffer_pages 1

#define MAX_MALLOC  3

struct mem_alloc_s {
  void *address;
  size_t size;
  char *var_name;
  uint64_t num_samples;
};

struct perf_mmap_data_s {
  uint64_t period;
  uint64_t ip;
  uint64_t address;
  uint32_t cpu;
  uint32_t res;
  uint32_t pid;
  uint32_t tid;
};

struct event_data_s {
  void *buffer;
  int   fd;
  int   total;
};

struct event_info_s {
  uint64_t config;
  uint64_t type;
  uint64_t threshold;
  uint64_t freq;
};

struct event_info_s event_info[] = {
    {.config = PERF_COUNT_HW_CPU_CYCLES,  .type = PERF_TYPE_HARDWARE, .threshold = 4000, .freq = 1},
    {.config = (1ULL<<19),  .type = 7, .threshold = 40, .freq = 1},
    {.config = PERF_COUNT_SW_PAGE_FAULTS, .type = PERF_TYPE_SOFTWARE, .threshold = 1,    .freq = 0}
};

static struct event_data_s events[MAX_EVENTS];

static size_t pagesize;
static size_t pgmsk;

static uint64_t sample_type = PERF_SAMPLE_PERIOD | PERF_SAMPLE_IP 
			    | PERF_SAMPLE_ADDR   | PERF_SAMPLE_CPU
			    | PERF_SAMPLE_TID;

static char *var_names[] = {"A", "B", "C"};
static int num_mallocs = 0;

static struct mem_alloc_s  mem_allocation[MAX_MALLOC];

static struct perf_mmap_data_s   mmap_data[MAX_EVENTS];

static int
get_num_events()
{
  int num_events = sizeof(event_info) / sizeof(struct event_info_s);
  return num_events;
}

static void*
wrap_malloc(size_t size)
{
	void *var = malloc(size);
	for (int i=0; i<MAX_MALLOC; i++) {
		if (mem_allocation[i].address == NULL) {
			mem_allocation[num_mallocs].address = var;
			mem_allocation[num_mallocs].size    = size;
      mem_allocation[num_mallocs].var_name = var_names[i];
			num_mallocs++;

			return var;
		}
	}
	return NULL; // out of memory
}


static void
wrap_free(void *address)
{
	for(int i=0; i<MAX_MALLOC; i++) {
		if (mem_allocation[i].address == address) {
			mem_allocation[i].address = NULL;
			mem_allocation[i].size	  = 0;
      mem_allocation[i].var_name = NULL;
			num_mallocs--;
		}
	}
}

static int
update(void *address)
{
  for(int i=0; i<MAX_MALLOC; i++) {
      if (mem_allocation[i].address != NULL    &&
          address >= mem_allocation[i].address &&
          address <  mem_allocation[i].address+mem_allocation[i].size) {
          mem_allocation[i].num_samples++;
      }
  }
}


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
  int ret = 0;
  for(int i=0; i<MAX_EVENTS; i++) {
      	if (events[i].fd >= 0)
		ret += stop_counters(events[i].fd);
  }

	return ret;
}

static int
start_all()
{
  int ret = 0;
  for(int i=0; i<MAX_EVENTS; i++) {
      	if (events[i].fd >= 0)
      		ret    += ioctl(events[i].fd, PERF_EVENT_IOC_ENABLE);
  }

  return ret;
}

static int
get_fd(int sig_fd)
{
	for(int i=0; i<MAX_EVENTS; i++) {
		if (events[i].fd == sig_fd)
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
static int parse_perf_sample(void *event_buf, struct perf_event_header *ehdr,
                             struct perf_mmap_data_s *data, int verbose)
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
		ret = read_from_perf_buffer_64(event_buf, &(data->ip));
		if (ret) {
			fprintf(stderr, "cannot read IP");
			return -1;
		}

		/*
		 * MISC_EXACT_IP indicates that kernel is returning
		 * th  IIP of an instruction which caused the event, i.e.,
		 * no skid
		 */
		if (verbose) fprintf(stderr, "IP: 0x%x  ", data->ip);
		sz -= sizeof(val64);
	}

	if (type & PERF_SAMPLE_TID) {
		ret = read_from_perf_buffer(event_buf, &pid, sizeof(pid));
		if (ret) {
			fprintf(stderr, "cannot read PID");
			return -1;
		}
		data->pid = pid.pid;
		data->tid = pid.tid;

		if (verbose) fprintf(stderr, "PID:%d  TID:%d  ", pid.pid, pid.tid);
		sz -= sizeof(pid);
	}

	if (type & PERF_SAMPLE_TIME) {
		ret = read_from_perf_buffer_64(event_buf, &val64);
		if (ret) {
			fprintf(stderr, "cannot read time");
			return -1;
		}

		if (verbose) fprintf(stderr, "TIME:%llu  ", val64);
		sz -= sizeof(val64);
	}

	if (type & PERF_SAMPLE_ADDR) {
		ret = read_from_perf_buffer_64(event_buf, &(data->address));
		if (ret) {
			fprintf(stderr, "cannot read time");
			return -1;
		}

		if (verbose) fprintf(stderr, "ADDR: 0x%x  ", data->address);
		sz -= sizeof(val64);
	}
	if (type & PERF_SAMPLE_CPU) {
		struct { uint32_t cpu, reserved; } cpu;
		ret = read_from_perf_buffer(event_buf, &cpu, sizeof(cpu));
		if (ret) {
			fprintf(stderr, "cannot read cpu");
			return -1;
		}
		data->cpu = cpu.cpu;
		data->res = cpu.reserved;

		if (verbose) fprintf(stderr, "CPU:%u  ", cpu.cpu);
		sz -= sizeof(cpu);
	}

	if (type & PERF_SAMPLE_PERIOD) {
		ret = read_from_perf_buffer_64(event_buf, &(data->period));
		if (ret) {
			fprintf(stderr, "cannot read period");
			return -1;
		}

		if (verbose) fprintf(stderr, "PERIOD:%llu  ", data->period);
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

		if (verbose) fprintf(stderr, "\n  CALLCHAIN :\n");
		while(nr--) {
			ret = read_from_perf_buffer_64(event_buf, &ip);
			if (ret) {
				fprintf(stderr, "cannot read ip");
				return -1;
			}

			sz -= sizeof(ip);

			if (verbose) fprintf(stderr, "\t0x%llu\n", ip);
		}
	}

	if (verbose) fprintf(stderr, "\n");
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
	ret = read_from_perf_buffer(events[index].buffer, &ehdr, sizeof(ehdr));
	if (ret) {
		fprintf(stderr, "cannot read event header\n");
		return;
	}
	if (ehdr.type == PERF_RECORD_SAMPLE) {
		ret = parse_perf_sample(events[index].buffer, &ehdr, &mmap_data[index], 0);
		update((void*)mmap_data[index].address);

	}  else {
		skip_perf_data(events[index].buffer, ehdr.size);
	}

	if (is_more_perf_data(events[index].buffer))
		goto again;

	events[index].total++;

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
int setup_counters(uint64_t type, uint64_t config, uint64_t period, uint64_t freq)
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
	attr.sample_freq = period;
	attr.freq 	 = freq;
	attr.wakeup_events = 0;
	attr.size	   = sizeof(struct perf_event_attr);
	attr.sample_type   = 17417; //PERF_SAMPLE_RAW | PERF_SAMPLE_CPU; //sample_type;

	printf("Creating event %d: ", config);
	events[index].fd = sys_perf_event_open(&attr, 0, -1, -1, 0);
	printf("%d\n", events[index].fd);
	if (events[index].fd < 0) {
		fprintf(stderr, "Error: %s\n", strerror(errno));
		perror("sys_perf_event_open");
		exit(1);
	}
	int fd = events[index].fd;

	events[index].buffer = setup_buffer(fd);
	if (events[index].buffer == NULL) {
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

	res = read(events[index].fd, &counter_result, sizeof(unsigned long long));
	assert(res == sizeof(unsigned long long));

	printf("[%d] counter:\t\t%lld\n[%d] Num counter: %d\n\n", index, counter_result, index, events[index].total);
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

void gemm_omp(double *A, double *B, double *C, int n) 
{   
    #pragma omp parallel
    {
        int i, j, k;
        #pragma omp for
        for (i = 0; i < n; i++) { 
            for (j = 0; j < n; j++) {
                double dot  = 0;
                for (k = 0; k < n; k++) {
                    dot += A[i*n+k]*B[k*n+j];
                } 
                C[i*n+j ] = dot;
            }
        }

    }
}



int
main(int argc, char *argv[])
{
	const unsigned int  n=164;
	const unsigned int  nn=n*n;

	memset(mem_allocation, 0, sizeof(mem_allocation));

	pagesize = sysconf(_SC_PAGESIZE);
	pgmsk = (buffer_pages * pagesize) - 1;

	setup_handler();

	int i;
	int num_events = get_num_events();
	for(i=0; i<num_events; i++) {
	    int fd = setup_counters(event_info[i].type, event_info[i].config,
	                            event_info[i].threshold, event_info[i].freq);
	    printf("event %d, fd: %d\n", i, fd);
	}

	/* Do something */
	double *A, *B, *C, dtime;

  	A = (double*)wrap_malloc(sizeof(double)*nn);
  	B = (double*)wrap_malloc(sizeof(double)*nn);
  	C = (double*)wrap_malloc(sizeof(double)*nn);

	printf("A: %p - %p   B: %p - %p     C: %p - %p\n", A, A+nn, B, B+nn, C, C+nn);

	for(i=0; i<MAX_EVENTS; i++) {
	    start_counters(events[i].fd);
	}

	for(i=0; i<n*n; i++) {
		A[i] = rand()/RAND_MAX; 
		B[i] = rand()/RAND_MAX;
	}

	gemm_omp(A, B, C, n);


  for(i=0; i<MAX_EVENTS; i++) {
      stop_counters(events[i].fd);
      read_counters(i);
  }

	for (i=0; i<MAX_MALLOC; i++) {
	    printf("Var: %s, address: %p-%p, size: %d, samples: %d\n", mem_allocation[i].var_name, mem_allocation[i].address,
	           mem_allocation[i].address+mem_allocation[i].size, mem_allocation[i].size, mem_allocation[i].num_samples);
	}

	wrap_free(A);
	wrap_free(B);
	wrap_free(B);

	return 0;
}
