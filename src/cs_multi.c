#define _GNU_SOURCE

#include <sys/types.h>
#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <errno.h>

#include<linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <asm/unistd.h>

#include <sched.h>

#define TMSG(fd,...) do { if (!quiet) fprintf(fd, __VA_ARGS__); } while(0);

#define FREQUENCY_SAMPLE 4000
#define PERIOD_SAMPLE 1000000

/*
 * event type storage
 */
struct event_counter_s {
	const char *name;
	__u32 type;
	__u64 config;
	__u64 sample_period;
	__u64 freq;
};

struct event_data_s {
	unsigned int samples;
	int fd;
	void *event_buff;
};

struct event_counter_s events_period[] = {
	{.name="cycles", .type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_CPU_CYCLES,
			.sample_period=PERIOD_SAMPLE, .freq=0},
	{.name="cache-ll", .type=PERF_TYPE_HW_CACHE, .config=PERF_COUNT_HW_CACHE_LL,
			.sample_period=1000, .freq=0},
	{.name="instructions", .type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_INSTRUCTIONS,
			.sample_period=PERIOD_SAMPLE, .freq=0},
	{.name="branches", .type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
			.sample_period=10000, .freq=0},
	{.name="branch-misses", .type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_BRANCH_MISSES,
			.sample_period=100, .freq=0},
	{.name="cache-dtlb", .type=PERF_TYPE_HW_CACHE, .config=PERF_COUNT_HW_CACHE_DTLB,
			.sample_period=100, .freq=0},
	{.name="cache-l1d", .type=PERF_TYPE_HW_CACHE, .config=PERF_COUNT_HW_CACHE_L1D,
			.sample_period=1000, .freq=0},
	{.name="context-switches", .type=PERF_TYPE_SOFTWARE, .config=PERF_COUNT_SW_CONTEXT_SWITCHES,
			.sample_period=1, .freq=0},
};

struct event_counter_s events_freq[] = {
	{.name="cycles", .type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_CPU_CYCLES,
			.sample_period=FREQUENCY_SAMPLE, .freq=1},
	{.name="cache-ll", .type=PERF_TYPE_HW_CACHE, .config=PERF_COUNT_HW_CACHE_LL,
			.sample_period=FREQUENCY_SAMPLE, .freq=1},
	{.name="instructions", .type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_INSTRUCTIONS,
			.sample_period=FREQUENCY_SAMPLE, .freq=1},
	{.name="branches", .type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
			.sample_period=FREQUENCY_SAMPLE, .freq=1},
	{.name="branch-misses", .type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_BRANCH_MISSES,
			.sample_period=FREQUENCY_SAMPLE, .freq=1},
	{.name="cache-dtlb", .type=PERF_TYPE_HW_CACHE, .config=PERF_COUNT_HW_CACHE_DTLB,
			.sample_period=FREQUENCY_SAMPLE, .freq=1},
	{.name="cache-l1d", .type=PERF_TYPE_HW_CACHE, .config=PERF_COUNT_HW_CACHE_L1D,
			.sample_period=FREQUENCY_SAMPLE, .freq=1},
	{.name="context-switches", .type=PERF_TYPE_SOFTWARE, .config=PERF_COUNT_SW_CONTEXT_SWITCHES,
			.sample_period=1, .freq=0},
};

const unsigned int num_events = sizeof(events_freq)/sizeof(struct event_counter_s);

struct event_data_s    *event_data;
struct perf_event_attr *event_attr;

/* Size of buffer data (must be power of 2 */
int buffer_pages = 1;
size_t event_pgmsk;

int quiet = 1;


static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
		int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
			group_fd, flags);
	return ret;
}


/*
 * event_buf already has the mmap'ed address for perf buffer,
 * So, go ahead and read the data.
 *
 * The payload data (event data) starts after the first page of control
 * information.
 */
static int
read_from_perf_buffer(int index, void *buf, size_t sz)
{
	struct perf_event_mmap_page *header = event_data[index].event_buff;
	size_t pgmsk = event_pgmsk;
	void *data;
	unsigned long tail;
	size_t avail_sz, min, c;

	if (header == NULL) return -1;

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
		TMSG(stderr, "Needed size is more than available size\n");
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


static int
read_from_perf_buffer_64(int index, void *buf)
{
	return read_from_perf_buffer(index, buf, sizeof(uint64_t));
}


/*
 * Not the data we need? Skip the data.
 */
static void
skip_perf_data(int index, size_t sz)
{
	struct perf_event_mmap_page *hdr = event_data[index].event_buff;

	if ((hdr->data_tail + sz) > hdr->data_head)
		sz = hdr->data_head - hdr->data_tail;

	hdr->data_tail += sz;
}

static int
is_more_perf_data(int index)
{
	struct perf_event_mmap_page *hdr = event_data[index].event_buff;

	if (hdr->data_tail < hdr->data_head)
		return 1;

	return 0;
}


/*
 * The below parser is a stripped down version of perf source. To keep it
 * simple, it only looks for PERF_SAMPLE_IDENTIFIER, PERF_SAMPLE_CALLCHAIN,
 * PERF_SAMPLE_IP, PERF_SAMPLE_RAW and PERF_SAMPLE_TIME.
 * More parsers can be added if needed.
 */
static int parse_perf_sample(int index, struct perf_event_header *ehdr)
{
	size_t sz;
	uint64_t type, fmt, val64;
	uint64_t time_enabled, time_running;
	const char *str;
	int ret;
	uint64_t value;
	uint64_t id;
	struct { uint32_t pid, tid; } pid;

	if (!ehdr)
		return -1;

	sz = ehdr->size - sizeof(*ehdr);

	type = event_attr[index].sample_type;
	fmt = event_attr[index].read_format;

	TMSG(stderr, "CONTEXT SWITCH: SW_EVENT\n");
	TMSG(stderr, "  ");

	if (type & PERF_SAMPLE_IDENTIFIER) {
		ret = read_from_perf_buffer_64(index, &val64);
		if (ret) {
			TMSG(stderr, "cannot read IP");
			return -1;
		}
		TMSG(stderr, "ID :%"PRIu64" ", val64);
		sz -= sizeof(val64);
	}

	/*
	 * the sample_type information is laid down
	 * based on the PERF_RECORD_SAMPLE format specified
	 * in the perf_event.h header file.
	 * That order is different from the enum perf_event_sample_format.
	 */
	if (type & PERF_SAMPLE_IP) {
		ret = read_from_perf_buffer_64(index, &val64);
		if (ret) {
			TMSG(stderr, "cannot read IP");
			return -1;
		}

		/*
		 * MISC_EXACT_IP indicates that kernel is returning
		 * th  IIP of an instruction which caused the event, i.e.,
		 * no skid
		 */
		TMSG(stderr, "IIP:%#016"PRIx64"  ", val64);
		sz -= sizeof(val64);
	}

	if (type & PERF_SAMPLE_TID) {
		ret = read_from_perf_buffer(index, &pid, sizeof(pid));
		if (ret) {
			TMSG(stderr, "cannot read PID");
			return -1;
		}

		TMSG(stderr, "PID:%d  TID:%d  ", pid.pid, pid.tid);
		sz -= sizeof(pid);
	}

	if (type & PERF_SAMPLE_TIME) {
		ret = read_from_perf_buffer_64(index, &val64);
		if (ret) {
			TMSG(stderr, "cannot read time");
			return -1;
		}

		TMSG(stderr, "TIME:%'"PRIu64"  ", val64);
		sz -= sizeof(val64);
	}
	if (type & PERF_SAMPLE_CPU) {
		struct { uint32_t cpu, reserved; } cpu;
		ret = read_from_perf_buffer(index, &cpu, sizeof(cpu));
		if (ret) {
			TMSG(stderr, "cannot read cpu");
			return -1;
		}
		TMSG(stderr, "CPU:%u  ", cpu.cpu);
		sz -= sizeof(cpu);
	}

	if (type & PERF_SAMPLE_PERIOD) {
		ret = read_from_perf_buffer_64(index, &val64);
		if (ret) {
			TMSG(stderr, "cannot read period");
			return -1;
		}
		TMSG(stderr, "PERIOD:%'"PRIu64"  ", val64);
		sz -= sizeof(val64);
	}

	if (type & PERF_SAMPLE_CALLCHAIN) {
		uint64_t nr, ip;

		ret = read_from_perf_buffer_64(index, &nr);
		if (ret) {
			TMSG(stderr, "cannot read callchain nr");
			return -1;
		}
		sz -= sizeof(nr);

		TMSG(stderr, "\n  CALLCHAIN :\n");
		while(nr--) {
			ret = read_from_perf_buffer_64(index, &ip);
			if (ret) {
				TMSG(stderr, "cannot read ip");
				return -1;
			}

			sz -= sizeof(ip);

			TMSG(stderr, "\t0x%"PRIx64"\n", ip);
		}
	}

	return 0;
}

static int
parse_perf_switch(int index, struct perf_event_header *ehdr)
{
	size_t sz;
	uint64_t type, fmt, val64;
	int ret;
	struct { uint32_t pid, tid; } pid;

	if (!ehdr)
		return -1;

	sz = ehdr->size - sizeof(*ehdr);

	type = event_attr[index].sample_type;
	fmt = event_attr[index].read_format;

	if (ehdr->misc == PERF_RECORD_MISC_SWITCH_OUT) {
		TMSG(stderr, "CONTEXT SWITCH: OUT\n");
	} else {
		TMSG(stderr, "CONTEXT SWITCH: IN\n");
	}

	TMSG(stderr, "  ");
	if (type & PERF_SAMPLE_TID) {
		ret = read_from_perf_buffer(index, &pid, sizeof(pid));
		if (ret) {
			TMSG(stderr, "cannot read PID");
			return -1;
		}

		TMSG(stderr, "PID:%d  TID:%d  ", pid.pid, pid.tid);
		sz -= sizeof(pid);
	}

	if (type & PERF_SAMPLE_TIME) {
		ret = read_from_perf_buffer_64(index, &val64);
		if (ret) {
			TMSG(stderr, "cannot read time");
			return -1;
		}

		TMSG(stderr, "TIME:%'"PRIu64"  ", val64);
		sz -= sizeof(val64);
	}
	if (type & PERF_SAMPLE_CPU) {
		struct { uint32_t cpu, reserved; } cpu;
		ret = read_from_perf_buffer(index, &cpu, sizeof(cpu));
		if (ret) {
			TMSG(stderr, "cannot read cpu");
			return -1;
		}
		TMSG(stderr, "CPU:%u\n", cpu.cpu);
		sz -= sizeof(cpu);
	}

	return 0;
}

static void
sigio_handler(int n, siginfo_t *info, void *uc)
{
	if (info->si_code < 0) {
		fprintf(stderr, "Required signal not generated\n");
		return;
	}

	if (info->si_code != POLL_HUP) {
		fprintf(stderr, "POLL_HUP signal not generated by SIGIO, %d\n", info->si_code);
		return;
	}

	int i, index = -1;
	for(i=0; i<num_events; i++) {
		if (info->si_fd == event_data[i].fd) {
			index = i;
			break;
		}
	}

	if (index < 0 || index >= num_events ) {
		fprintf(stderr, "Wrong fd: %d\n", info->si_fd);
		return;
	}

	TMSG(stderr, "%d. FD %d, SIGIO: %d\n", index, info->si_fd, event_data[index].samples);

	struct perf_event_header ehdr;
	int ret;

	i = index;
again:

	ret = read_from_perf_buffer(i, &ehdr, sizeof(ehdr));
	if (ret != 0) {
		return;
	}

	event_data[i].samples++;

	if (ehdr.type != PERF_RECORD_SAMPLE && ehdr.type != PERF_RECORD_SWITCH) {
		/* Not the sample we are looking for */
		//fprintf(stderr, "%d skipping record type %x of %d bytes\n", i, ehdr.type, ehdr.size);
		skip_perf_data(index, ehdr.size);
	}

	if (ehdr.type == PERF_RECORD_SAMPLE) {
		ret = parse_perf_sample(i, &ehdr);

	} else if (ehdr.type == PERF_RECORD_SWITCH) {
		ret = parse_perf_switch(i, &ehdr);
	}

	if (is_more_perf_data(i))
		goto again;

	ret = ioctl(info->si_fd, PERF_EVENT_IOC_REFRESH, 1);
	if (ret == -1) {
		fprintf(stderr, "fd %d: Error enable counter in IOC_REFRESH: %s\n",
				info->si_fd, strerror(errno));
	}
}

#define MATRIX_SIZE 512
static double a[MATRIX_SIZE][MATRIX_SIZE];
static double b[MATRIX_SIZE][MATRIX_SIZE];
static double c[MATRIX_SIZE][MATRIX_SIZE];

static void
naive_matrix_multiply(int quiet) {

  double s;
  int i,j,k;

  for(i=0;i<MATRIX_SIZE;i++) {
    for(j=0;j<MATRIX_SIZE;j++) {
      a[i][j]=(double)i*(double)j;
      b[i][j]=(double)i/(double)(j+5);
    }
  }

  for(j=0;j<MATRIX_SIZE;j++) {
     for(i=0;i<MATRIX_SIZE;i++) {
        s=0;
        for(k=0;k<MATRIX_SIZE;k++) {
	   s+=a[i][k]*b[k][j];
	}
        c[i][j] = s;
     }
  }

  s=0.0;
  for(i=0;i<MATRIX_SIZE;i++) {
    for(j=0;j<MATRIX_SIZE;j++) {
      s+=c[i][j];
    }
  }

  if (!quiet) printf("Matrix multiply sum: s=%lf\n",s);

  return;
}

static int
wait_loop(void)
{
	int j = 0;

	for(j=0; j<10; j++)
		naive_matrix_multiply(1);
	return j;
}


static int
setup_perf(struct event_counter_s *event, struct event_data_s *event_data)
{
	static int index = 0;

	memset(&event_attr[index], 0, sizeof(struct perf_event_attr));

	event_attr[index].disabled = 1;
	event_attr[index].size 	   = sizeof(struct perf_event_attr);
	event_attr[index].type 	   = event->type;
	event_attr[index].config   = event->config;

	event_attr[index].sample_period = event->sample_period;
	event_attr[index].freq 		= event->type;

	/* PERF_SAMPLE_STACK_USER may also be good to use */
	event_attr[index].sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID |
			PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_CPU |
			PERF_SAMPLE_PERIOD;

	event_attr[index].context_switch = 1;
	event_attr[index].sample_id_all = 1;

	struct perf_event_attr *attr = &(event_attr[index]);
	int fd = perf_event_open(attr, 0, -1, -1, 0);
	if (fd == -1) {
		fprintf(stderr, "Error in perf_event_open : %d\n", errno);
		return -1;
	}

	size_t pagesize = sysconf(_SC_PAGESIZE);
	void *buf = mmap(NULL, (buffer_pages + 1) * pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		fprintf(stderr, "Can't mmap buffer\n");
		return -1;
	}
	event_data->event_buff = buf;
	event_data->fd 		   = fd;
	event_data->samples    = 0;

	printf("setup %d: %s, code: %d, type: %d, thresh: %d, freq: %d, fd: %d.\n",
			index, event->name, event->config, event->type, event->sample_period, event->freq,
			 event_data->fd);
	index++;
	return index;
}

static int
setup_notification(int index)
{
	/*
	 * Setup notification on the file descriptor
	 */
	int ret = fcntl(event_data[index].fd, F_SETFL, fcntl(event_data[index].fd, F_GETFL, 0) | O_ASYNC);
	if (ret == -1) {
		fprintf(stderr, "Can't set notification\n");
		return -1;
	}

	ret = fcntl(event_data[index].fd, F_SETSIG, SIGIO);
	if (ret == -1) {
		fprintf(stderr, "Cannot set sigio\n");
		return -1;
	}

	/* Get ownership of the descriptor */
	ret = fcntl(event_data[index].fd, F_SETOWN, getpid());
	if (ret == -1) {
		fprintf(stderr, "Error in setting owner\n");
		return -1;
	}

	/* Enable the event for one period */
	ret = ioctl(event_data[index].fd, PERF_EVENT_IOC_REFRESH, 1);
	if (ret == -1) {
		fprintf(stderr, "%d Error in IOC_REFRESH (fd: %d): %s\n", index, event_data[index].fd, strerror(errno));
		return -1;
	}
	return 0;
}

static int
disable_counter(int index)
{
	/* Disable the event counter */
	int ret = ioctl(event_data[index].fd, PERF_EVENT_IOC_DISABLE, 1);
	if (ret == -1) {
		fprintf(stderr, "%d: Error in IOC_DISABLE: %s\n", index, strerror(errno));
		return -1;
	}
	close(event_data[index].fd);
	return 0;
}

static void
main_test(struct event_counter_s *event, unsigned int num_events)
{
	event_data = (struct event_data_s*)    malloc(sizeof(struct event_data_s)   * num_events);
	event_attr = (struct perf_event_attr*) malloc(sizeof(struct perf_event_attr)* num_events);

	size_t pagesize = sysconf(_SC_PAGESIZE);
	size_t pgmsk = (buffer_pages * pagesize) - 1;
	event_pgmsk  = pgmsk;

	// setup all the event counters
	for(int i=0; i<num_events; i++) {
		setup_perf(&event[i], &event_data[i]);
	}

	// start the event
	for(int i=0; i<num_events; i++) {
		setup_notification(i);
	}

	// computation or waiting loop
	wait_loop();

	// stop the counter
	for(int i=0; i<num_events; i++) {
		disable_counter(i);
		printf("total samples for %s: %d\n", event[i].name, event_data[i].samples);
	}
	free(event_data);
	free(event_attr);
}

int main(int argc, char *argv[])
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigio_handler;
	act.sa_flags     = SA_SIGINFO;
	sigaction(SIGIO, &act, 0);

	printf("Testing with frequency sampling\n");
	const unsigned int num_events = sizeof(events_freq)/sizeof(struct event_counter_s);
	main_test(events_freq, num_events);

	printf("\n\nTesting with period sampling\n");
	main_test(events_period, num_events);
}
