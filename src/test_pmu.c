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

/******************************************************************************
 * perf event
 *****************************************************************************/
#include <linux/perf_event.h>

/******************************************************************************
 * perfmon
 *****************************************************************************/
#include "perfmon/pfmlib.h"

#define MATRIX_SIZE 512

static double a[MATRIX_SIZE][MATRIX_SIZE];
static double b[MATRIX_SIZE][MATRIX_SIZE];
static double c[MATRIX_SIZE][MATRIX_SIZE];

void naive_matrix_multiply(int quiet) {

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


#define TMSG(fd,...) do { if (!quiet) fprintf(fd, __VA_ARGS__); } while(0);

struct event_desc {
	uint64_t type;
	uint64_t config;
	char *   name;
};

static struct event_desc  events[] = {
	{.type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_CPU_CYCLES,   .name="snb::MEM_TRANS_RETIRED:LATENCY_ABOVE_THRESHOLD"},
	{.type=PERF_TYPE_HARDWARE, .config=PERF_COUNT_HW_INSTRUCTIONS, .name="OFFCORE_RESPONSE_0"}
};

typedef uint64_t u64;

/*
 * use with PFM_OS_PERF, PFM_OS_PERF_EXT for pfm_get_os_event_encoding()
 */
typedef struct {
        struct perf_event_attr *attr;   /* in/out: perf_event struct pointer */
        char **fstr;                    /* out/in: fully qualified event string */
        size_t size;                    /* sizeof struct */
        int idx;                        /* out: opaque event identifier */
        int cpu;                        /* out: cpu to program, -1 = not set */
        int flags;                      /* out: perf_event_open() flags */
        int pad0;                       /* explicit 64-bit mode padding */
} pfm_perf_encode_arg_t;

////////////////////////////////////////////////
//
static struct perf_event_attr event_attr[2];

/* Size of buffer data (must be power of 2 */
static int buffer_pages = 1;

static void *event_buf[] = {NULL, NULL};
static size_t event_pgmsk;

static int quiet = 1;
static int fd[2];
static int samples[2];


long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
		int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
			group_fd, flags);
	return ret;
}

/**
 * Initializing perfmon
 * return 1 if the initialization passes successfully
 **/
int
pfmu_init()
{
   /* to allow encoding of events from non detected PMU models */
   int ret = setenv("LIBPFM_ENCODE_INACTIVE", "1", 1);
   if (ret != PFM_SUCCESS) {
      fprintf(stderr, "cannot force inactive encoding");
      return 0;
   }

   ret = pfm_initialize();
   if (ret != PFM_SUCCESS) {
      fprintf(stderr, "cannot initialize libpfm: %s", pfm_strerror(ret));
      return 0;
   }

   return 1;
}

/*
 * end of perfmon
 */
void
pfmu_fini()
{
   pfm_terminate();
}


// return 0 or positive if the event exists, -1 otherwise
// if the event exist, code and type are the code and type of the event
int 
pfmu_getEventAttribute(const char *eventname, struct perf_event_attr *event_attr)
{
  pfm_perf_encode_arg_t arg;
  char *fqstr = NULL;

  arg.fstr = &fqstr;
  arg.size = sizeof(pfm_perf_encode_arg_t);
  struct perf_event_attr attr;
  memset(&attr, 0, sizeof(struct perf_event_attr));

  arg.attr = &attr;
  int ret = pfm_get_os_event_encoding(eventname, PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT, &arg);

  if (ret == PFM_SUCCESS) {
    memcpy(event_attr, &attr, sizeof(struct perf_event_attr));
    return 1;
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
static int read_from_perf_buffer(int index, void *buf, size_t sz)
{
	struct perf_event_mmap_page *header = event_buf[index];
	size_t pgmsk = event_pgmsk;
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


static int read_from_perf_buffer_64(int index, void *buf)
{
	return read_from_perf_buffer(index, buf, sizeof(uint64_t));
}


/*
 * Not the data we need? Skip the data.
 */
static void skip_perf_data(int index, size_t sz)
{
	struct perf_event_mmap_page *hdr = event_buf[index];

	if ((hdr->data_tail + sz) > hdr->data_head)
		sz = hdr->data_head - hdr->data_tail;

	hdr->data_tail += sz;
}

static int is_more_perf_data(int index)
{
	struct perf_event_mmap_page *hdr = event_buf[index];

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

static int parse_perf_switch(int index, struct perf_event_header *ehdr)
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

static void disable_all_events()
{
	int ret;

	ret = ioctl(fd[0], PERF_EVENT_IOC_DISABLE, 1);
	if (ret < 0) fprintf(stderr, "cannot disable perf event: %d\n", fd[0]);

	ret = ioctl(fd[1], PERF_EVENT_IOC_DISABLE, 1);
	if (ret < 0) fprintf(stderr, "cannot disable perf event: %d\n", fd[1]);
}

static void enable_all_events()
{
	int ret;

	ret = ioctl(fd[0], PERF_EVENT_IOC_ENABLE, 1);
	if (ret < 0) fprintf(stderr, "cannot enable perf event: %d\n", fd[0]);

	ret = ioctl(fd[1], PERF_EVENT_IOC_ENABLE, 1);
	if (ret < 0) fprintf(stderr, "cannot enable perf event: %d\n", fd[1]);
}


static void sigio_handler(int n, siginfo_t *info, void *uc)
{
	static int num_samples = 0;

	if (info->si_code < 0) {
		fprintf(stderr, "Required signal not generated\n");
		return;
	}
/*
	if (info->si_code != POLL_HUP) {
		fprintf(stderr, "POLL_HUP signal not generated by SIGPERF, %d\n", info->si_code);
		return;
	}*/

	disable_all_events();

	int i, index = -1;
	for(i=0; i<2; i++) {
		if (info->si_fd == fd[i]) {
			index = i;
			break;
		}
	}

	if (index < 0 || index > 1) {
		fprintf(stderr, "Wrong fd: %d\n", info->si_fd);
		enable_all_events();
		return;
	}

	TMSG(stderr, "%d. FD %d, SIGPERF: %d\n", index, fd[index], num_samples++);

	struct perf_event_header ehdr;
	int ret;

again:

	ret = read_from_perf_buffer(index, &ehdr, sizeof(ehdr));
	if (ret) {
		fprintf(stderr, "cannot read event header\n");
		return;
	}

	if (ehdr.type != PERF_RECORD_SAMPLE && ehdr.type != PERF_RECORD_SWITCH) {
		/* Not the sample we are looking for */
		fprintf(stderr, "skipping record type %d of %d bytes\n", ehdr.type, ehdr.size);
		skip_perf_data(index, ehdr.size);
	}

	if (ehdr.type == PERF_RECORD_SAMPLE) {
		ret = parse_perf_sample(index, &ehdr);

	} else if (ehdr.type == PERF_RECORD_SWITCH) {
		ret = parse_perf_switch(index, &ehdr);
	}

	samples[index]++;

	if (is_more_perf_data(index))
		goto again;

	enable_all_events();
}

int wait_loop(void)
{
	int i = 0;
	for(i=0; i<10; i++)
	  naive_matrix_multiply(1);
	return i;
}

int
setup_perf(int index, struct perf_event_attr *attr)
{
	attr->disabled = 1;
	attr->size     = sizeof(struct perf_event_attr);

	attr->sample_period = 100;
	attr->freq 		= 1;

	/* PERF_SAMPLE_STACK_USER may also be good to use */
	attr->sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID |
			PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_CPU |
			PERF_SAMPLE_PERIOD;

	attr->context_switch = 1;
	attr->sample_id_all = 1;

	int fd_group_leader = -1;

	fd[index] = perf_event_open(attr, 0, -1, fd_group_leader, 0);
	if (fd[index] == -1) {
		fprintf(stderr, "Error in perf_event_open : %d\n", errno);
		return -1;
	}

	size_t pagesize = sysconf(_SC_PAGESIZE);
	void *buf = mmap(NULL, (buffer_pages + 1) * pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd[index], 0);
	if (buf == MAP_FAILED) {
		fprintf(stderr, "Can't mmap buffer\n");
		return -1;
	}
	event_buf[index]    = buf;

	size_t pgmsk = (buffer_pages * pagesize) - 1;
	event_pgmsk  = pgmsk;

	return 0;
}

#define SIGPERF (SIGRTMIN+4)

int
setup_notification(int index)
{
	/*
	 * Setup notification on the file descriptor
	 */
	int ret = fcntl(fd[index], F_SETFL, fcntl(fd[index], F_GETFL, 0) | O_ASYNC);
	if (ret == -1) {
		fprintf(stderr, "Can't set notification\n");
		return -1;
	}

	ret = fcntl(fd[index], F_SETSIG, SIGPERF);
	if (ret == -1) {
		fprintf(stderr, "Cannot set sigio\n");
		return -1;
	}

	/* Get ownership of the descriptor */
	ret = fcntl(fd[index], F_SETOWN, getpid());
	if (ret == -1) {
		fprintf(stderr, "Error in setting owner\n");
		return -1;
	}
	return 0;
}

static void
get_event_attr(char *name, struct perf_event_attr *attr)
{
	memset(attr, 0, sizeof(struct perf_event_attr) );
	int res = pfmu_getEventAttribute(name, attr);
	if (res > 0) {
		printf("name: %s\n  type: %d\n", name, attr->type);
		printf("  config: %d\n  config1: %d\n  config2: %d  \n", attr->config, attr->config1, attr->config2);
	} else {
		printf("error ");
	}
}


int main(int argc, char *argv[])
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigio_handler;
	act.sa_flags     = SA_SIGINFO;
	sigaction(SIGPERF, &act, 0);

	pfmu_init();

	get_event_attr(events[0].name, &event_attr[0]);
	get_event_attr(events[1].name, &event_attr[1]);

	event_attr[0].precise_ip = 2;
	 
	pfmu_fini();

	setup_perf(0, &event_attr[0]);
	setup_perf(1, &event_attr[1]);

	setup_notification(0);
	setup_notification(1);

	enable_all_events();

	wait_loop();

	disable_all_events();

	close(fd[0]);
	close(fd[1]);

	printf("total samples %s: %d\n", events[0].name, samples[0]);
	printf("total samples %s: %d\n", events[1].name, samples[1]);
}
