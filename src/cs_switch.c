/*
 * Use perf_event interface to sample for software event context-switches and
 * PERF_RECORD_SWITCH events. Also record the callchain at the time of the
 * context-switch event for further analysis on the reason for the context
 * switch.
 *
 * Dump the data in the signal handler for each event and also dump the current
 * time stamp. The difference between the current time stamp (obtained from the
 * signal handler) and the event time stamp gives us the approximate off-cpu
 * time.
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <asm/unistd.h>
#include <time.h>
#include <sched.h>
#include <sys/wait.h>

/* How many signals do we want? */
#define NR_COUNT 10

/* Size of buffer data (must be power of 2 */
int buffer_pages = 1;

int event_fd = -1;
size_t event_pgmsk;
void *event_buf = NULL;

struct perf_event_attr event_attr;

/* This will keep track of the no. of signals delivered */
static unsigned long nr_count = 0;

long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
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
static int read_from_perf_buffer(void *buf, size_t sz)
{
	struct perf_event_mmap_page *header = event_buf;
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

static int read_from_perf_buffer_64(void *buf)
{
	return read_from_perf_buffer(buf, sizeof(uint64_t));
}

/*
 * Not the data we need? Skip the data.
 */
static void skip_perf_data(size_t sz)
{
	struct perf_event_mmap_page *hdr = event_buf;

	if ((hdr->data_tail + sz) > hdr->data_head)
		sz = hdr->data_head - hdr->data_tail;

	hdr->data_tail += sz;
}

static int is_more_perf_data(void)
{
	struct perf_event_mmap_page *hdr = event_buf;

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
static int parse_perf_sample(struct perf_event_header *ehdr)
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

	type = event_attr.sample_type;
	fmt = event_attr.read_format;

	fprintf(stderr, "CONTEXT SWITCH: SW_EVENT\n");
	fprintf(stderr, "  ");

	if (type & PERF_SAMPLE_IDENTIFIER) {
		ret = read_from_perf_buffer_64(&val64);
		if (ret) {
			fprintf(stderr, "cannot read IP");
			return -1;
		}
		fprintf(stderr, "ID :%"PRIu64" ", val64);
		sz -= sizeof(val64);
	}

	/*
	 * the sample_type information is laid down
	 * based on the PERF_RECORD_SAMPLE format specified
	 * in the perf_event.h header file.
	 * That order is different from the enum perf_event_sample_format.
	 */
	if (type & PERF_SAMPLE_IP) {
		ret = read_from_perf_buffer_64(&val64);
		if (ret) {
			fprintf(stderr, "cannot read IP");
			return -1;
		}

		/*
		 * MISC_EXACT_IP indicates that kernel is returning
		 * th  IIP of an instruction which caused the event, i.e.,
		 * no skid
		 */
		fprintf(stderr, "IIP:%#016"PRIx64"  ", val64);
		sz -= sizeof(val64);
	}
	
	if (type & PERF_SAMPLE_TID) {
		ret = read_from_perf_buffer(&pid, sizeof(pid));
		if (ret) {
			fprintf(stderr, "cannot read PID");
			return -1;
		}

		fprintf(stderr, "PID:%d  TID:%d  ", pid.pid, pid.tid);
		sz -= sizeof(pid);
	}

	if (type & PERF_SAMPLE_TIME) {
		ret = read_from_perf_buffer_64(&val64);
		if (ret) {
			fprintf(stderr, "cannot read time");
			return -1;
		}

		fprintf(stderr, "TIME:%'"PRIu64"  ", val64);
		sz -= sizeof(val64);
	}
	if (type & PERF_SAMPLE_CPU) {
		struct { uint32_t cpu, reserved; } cpu;
		ret = read_from_perf_buffer(&cpu, sizeof(cpu));
		if (ret) {
			fprintf(stderr, "cannot read cpu");
			return -1;
		}
		fprintf(stderr, "CPU:%u  ", cpu.cpu);
		sz -= sizeof(cpu);
	}

	if (type & PERF_SAMPLE_PERIOD) {
		ret = read_from_perf_buffer_64(&val64);
		if (ret) {
			fprintf(stderr, "cannot read period");
			return -1;
		}
		fprintf(stderr, "PERIOD:%'"PRIu64"  ", val64);
		sz -= sizeof(val64);
	}

	if (type & PERF_SAMPLE_CALLCHAIN) {
		uint64_t nr, ip;

		ret = read_from_perf_buffer_64(&nr);
		if (ret) {
			fprintf(stderr, "cannot read callchain nr");
			return -1;
		}
		sz -= sizeof(nr);

		fprintf(stderr, "\n  CALLCHAIN :\n");
		while(nr--) {
			ret = read_from_perf_buffer_64(&ip);
			if (ret) {
				fprintf(stderr, "cannot read ip");
				return -1;
			}

			sz -= sizeof(ip);

			fprintf(stderr, "\t0x%"PRIx64"\n", ip);
		}
	}

	return 0;
}

static int parse_perf_switch(struct perf_event_header *ehdr)
{
	size_t sz;
	uint64_t type, fmt, val64;
	int ret;
	struct { uint32_t pid, tid; } pid;
	
	if (!ehdr)
		return -1;

	sz = ehdr->size - sizeof(*ehdr);

	type = event_attr.sample_type;
	fmt = event_attr.read_format;

	if (ehdr->misc == PERF_RECORD_MISC_SWITCH_OUT)
		fprintf(stderr, "CONTEXT SWITCH: OUT\n");
	else
		fprintf(stderr, "CONTEXT SWITCH: IN\n");

	fprintf(stderr, "  ");
	if (type & PERF_SAMPLE_TID) {
		ret = read_from_perf_buffer(&pid, sizeof(pid));
		if (ret) {
			fprintf(stderr, "cannot read PID");
			return -1;
		}

		fprintf(stderr, "PID:%d  TID:%d  ", pid.pid, pid.tid);
		sz -= sizeof(pid);
	}

	if (type & PERF_SAMPLE_TIME) {
		ret = read_from_perf_buffer_64(&val64);
		if (ret) {
			fprintf(stderr, "cannot read time");
			return -1;
		}

		fprintf(stderr, "TIME:%'"PRIu64"  ", val64);
		sz -= sizeof(val64);
	}
	if (type & PERF_SAMPLE_CPU) {
		struct { uint32_t cpu, reserved; } cpu;
		ret = read_from_perf_buffer(&cpu, sizeof(cpu));
		if (ret) {
			fprintf(stderr, "cannot read cpu");
			return -1;
		}
		fprintf(stderr, "CPU:%u\n", cpu.cpu);
		sz -= sizeof(cpu);
	}

	return 0;
}

static void sigio_handler(int n, siginfo_t *info, void *uc)
{
	struct perf_event_header ehdr;
	int ret;

	fprintf(stderr, "SIGIO %lu\n", nr_count);

	/*
	 * Check the si_code, if its positive, then kernel generated it
	 * for SIGIO
	 */
	if (info->si_code < 0) {
		fprintf(stderr, "Required signal not generated\n");
		return;
	}

	/*
	 * SIGPOLL = SIGIO
	 * expect POLL_HUP instead of POLL_IN
	 */
	if (info->si_code != POLL_HUP) {
		fprintf(stderr, "POLL_HUP signal not generated by SIGIO, %d\n", info->si_code);
		return;
	}

	if (info->si_fd != event_fd) {
		fprintf(stderr, "Wrong fd\n");
		return;
	}

again:
	ret = read_from_perf_buffer(&ehdr, sizeof(ehdr));
	if (ret) {
		fprintf(stderr, "cannot read event header\n");
		return;
	}

	if (ehdr.type != PERF_RECORD_SAMPLE && ehdr.type != PERF_RECORD_SWITCH) {
		/* Not the sample we are looking for */
		fprintf(stderr, "skipping record type %d\n", ehdr.type);
		skip_perf_data(ehdr.size);
	}

	if (ehdr.type == PERF_RECORD_SAMPLE) {
		ret = parse_perf_sample(&ehdr);
		nr_count++;
	} else if (ehdr.type == PERF_RECORD_SWITCH) {
		ret = parse_perf_switch(&ehdr);
	}

	if (is_more_perf_data())
		goto again;

	fprintf(stderr, "\n");

	/*
	 * refresh the counter again
	 */
	ret = ioctl(info->si_fd, PERF_EVENT_IOC_REFRESH, 1);
	if (ret == -1)
		fprintf(stderr, "Error in IOC_REFRESH\n");
}

void wait_loop(void)
{
	int j;
	for(int i=0; i<NR_COUNT ; i++ ) {
          j = sleep(1);
	  /* 
	   * The below print may end up resulting in additional
	   * context switches due to the process being blocked
	   * for I/O
	   */
	  /*printf ("WORKER: sleep %d returned: %d\n", i, j);*/
        }
}

int main(int argc, char *argv[])
{
	struct sigaction act;
	size_t pagesize;
	int ret;
	int fd;
	void *buf;
	size_t pgmsk;
	pid_t pid;
	int wstat;
	
	pid = fork();
	if (pid == 0) {
		sleep(1);
		wait_loop();
		return 0;
	} else if (pid == -1) {
		perror("fork");
		return -1;
	}

	pagesize = sysconf(_SC_PAGESIZE);

	/*
	 * Register the sig handler (SIGIO)
	 */
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigio_handler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGIO, &act, 0);

	memset(&event_attr, 0, sizeof(event_attr));
	event_attr.disabled = 1;

	event_attr.size = sizeof(struct perf_event_attr);
	event_attr.type = PERF_TYPE_SOFTWARE;
	event_attr.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
	event_attr.sample_period = 1;
	/* PERF_SAMPLE_STACK_USER may also be good to use */
	event_attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID |
		PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_CPU |
		PERF_SAMPLE_PERIOD;
	event_attr.context_switch = 1;
	event_attr.sample_id_all = 1;

	/*
	 * To correlate with the user space events, we need to sync the
	 * perf events and user space with the same clock. In this case,
	 * the signal handler gets the clock_time from CLOCK_MONOTONIC
	 * and for the very same purpose, event_attr sets the same value for
	 * clockid.
	 * Please comment out the following two lines if running this on
	 * ubuntu 14.04.4. The default clock value for ubuntu 14.04.4 is
	 * CLOCK_MONOTONIC_RAW.
	 */
	// event_attr.use_clockid = 1;
	// event_attr.clockid = 1;

	fd = perf_event_open(&event_attr, pid, -1, -1, 0);
	if (fd == -1) {
		fprintf(stderr, "Error in perf_event_open : %d\n", errno);
		return -1;
	}

	event_fd = fd;

	/*
	 * map the perf buffer with buf here. But, there wouldn't be any data at this
	 * point. So, put the parsing logic for the buffer in signal handler, since
	 * that will called once an event occurs.
	 */
	buf = mmap(NULL, (buffer_pages + 1) * pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		fprintf(stderr, "Can't mmap buffer\n");
		return -1;
	}

	pgmsk = (buffer_pages * pagesize) - 1;

	/* We will need these two things in the signal handler */
	event_buf = buf;
	event_pgmsk = pgmsk;

	/*
	 * Setup notification on the file descriptor
	 */
	ret = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_ASYNC);
	if (ret == -1) {
		fprintf(stderr, "Can't set notification\n");
		return -1;
	}

	ret = fcntl(fd, F_SETSIG, SIGIO);
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

wait:
	/* Wait for the signal */
	if ((ret = wait(&wstat)) == -1)
		if (errno == EINTR)
			goto wait;

	/* Disable the event counter */
	ret = ioctl(fd, PERF_EVENT_IOC_DISABLE, 1);
	if (ret == -1) {
		fprintf(stderr, "Error in IOC_DISABLE\n");
		return -1;
	}

	/* That's it, done. Close the fd */
	close(fd);

	return 0;
}
