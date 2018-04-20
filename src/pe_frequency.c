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

struct perf_event_attr event_attr;
/* Size of buffer data (must be power of 2 */
int buffer_pages = 1;
void *event_buf = NULL;
size_t event_pgmsk;

int fd;

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
	static int num_samples = 0;

	if (info->si_code < 0) {
		fprintf(stderr, "Required signal not generated\n");
		return;
	}

	if (info->si_code != POLL_HUP) {
		fprintf(stderr, "POLL_HUP signal not generated by SIGIO, %d\n", info->si_code);
		return;
	}

	if (info->si_fd != fd) {
		fprintf(stderr, "Wrong fd\n");
		return;
	}

	fprintf(stderr, "\nSIGIO: %d\n", num_samples++);

	struct perf_event_header ehdr;
	int ret;

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

	} else if (ehdr.type == PERF_RECORD_SWITCH) {
		ret = parse_perf_switch(&ehdr);
	}

	if (is_more_perf_data())
		goto again;

	ret = ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
	if (ret == -1)
		fprintf(stderr, "Error in IOC_REFRESH\n");
}

#define MAX_BYTES (1024 * 1024 * 1024 * 1024)
int wait_loop(void)
{
	int j = 0;

#if 0
	char *buffer;
	printf("\n================ Malloc\n");
	buffer = (char*)malloc(MAX_BYTES);
	if (buffer == NULL) {
		printf("error allocating memory\n");
		return;
	}

	printf("\n================ WRITE\n");
	int fd = open("/tmp/trto", O_CREAT | O_WRONLY,S_IRUSR );
	if (fd >= 0) {
		printf("writing to fd %d, %ld bytes\n", fd, MAX_BYTES);
		write(fd, buffer, MAX_BYTES);
	}
#else
	// simple loop with sleep
	for(int i=0; i<4 ; i++ ) {
		j += sleep(1);
		printf("%d sleep %d\n", i, j);
	} 
#endif
	return j;
}


int main(int argc, char *argv[])
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigio_handler;
	act.sa_flags     = SA_SIGINFO;
	sigaction(SIGIO, &act, 0);

	memset(&event_attr, 0, sizeof(event_attr));
	event_attr.disabled = 1;

	event_attr.size = sizeof(struct perf_event_attr);
	event_attr.type = PERF_TYPE_HARDWARE;
	event_attr.config = PERF_COUNT_HW_CPU_CYCLES;
	event_attr.sample_period = 4000;
	event_attr.freq = 1;

	/* PERF_SAMPLE_STACK_USER may also be good to use */
	event_attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID |
			PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_CPU |
			PERF_SAMPLE_PERIOD;

	fd = perf_event_open(&event_attr, 0, -1, -1, 0);
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

	size_t pgmsk = (buffer_pages * pagesize) - 1;
	event_pgmsk  = pgmsk;
	event_buf    = buf;
	/*
	 * Setup notification on the file descriptor
	 */
	int ret = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_ASYNC);
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

	wait_loop();

	/* Disable the event counter */
	ret = ioctl(fd, PERF_EVENT_IOC_DISABLE, 1);
	if (ret == -1) {
		fprintf(stderr, "Error in IOC_DISABLE\n");
		return -1;
	}

	close(fd);

}
