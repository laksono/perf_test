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
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <asm/unistd.h>

#include <sched.h>

#define TMSG(fd,...) do { if (!quiet) fprintf(fd, __VA_ARGS__); } while(0);

struct perf_event_attr event_attr[2];
/* Size of buffer data (must be power of 2 */
int buffer_pages = 1;
void *event_buf[] = {NULL, NULL};
size_t event_pgmsk;

int quiet = 1;
int fd[2];
int samples[2];


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

	int i, index = -1;
	for(i=0; i<2; i++) {
		if (info->si_fd == fd[i]) {
			index = i;
			break;
		}
	}

	if (index < 0 || index > 1) {
		fprintf(stderr, "Wrong fd: %d\n", info->si_fd);
		return;
	}

	TMSG(stderr, "%d. FD %d, SIGIO: %d\n", index, fd[index], num_samples++);
	samples[index]++;

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
		fprintf(stderr, "skipping record type %d\n", ehdr.type);
		skip_perf_data(index, ehdr.size);
	}

	if (ehdr.type == PERF_RECORD_SAMPLE) {
		ret = parse_perf_sample(index, &ehdr);

	} else if (ehdr.type == PERF_RECORD_SWITCH) {
		ret = parse_perf_switch(index, &ehdr);
	}

	if (is_more_perf_data(index))
		goto again;

	ret = ioctl(fd[index], PERF_EVENT_IOC_REFRESH, 1);
	if (ret == -1)
		fprintf(stderr, "Error enable counter in IOC_REFRESH\n");
}

int instructions_million(void) {

#if defined(__i386__) || (defined __x86_64__)
	asm(	"	xor	%%ecx,%%ecx\n"
		"	mov	$1499999,%%ecx\n"
		"test_loop:\n"
		"	dec	%%ecx\n"
		"	jnz	test_loop\n"
		: /* no output registers */
		: /* no inputs */
		: "cc", "%ecx" /* clobbered */
	);
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
int wait_loop(void)
{
	int i=0, j = 0;

	for(i=0; i<1000; i++)
	  j += instructions_million();
	return j;
}

int
setup_perf(int process, int index, unsigned int type, unsigned int config)
{
	memset(&event_attr[index], 0, sizeof(event_attr));

	event_attr[index].disabled = 1;
	event_attr[index].size 	   = sizeof(struct perf_event_attr);
	event_attr[index].type 	   = type;
	event_attr[index].config   = config;

	event_attr[index].sample_period = 4000;
	event_attr[index].freq 		= 1;

	/* PERF_SAMPLE_STACK_USER may also be good to use */
	event_attr[index].sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID |
			PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_CPU |
			PERF_SAMPLE_PERIOD;

	event_attr[index].context_switch = 1;
	event_attr[index].sample_id_all = 1;

	struct perf_event_attr *attr = &(event_attr[index]);
	fd[index] = perf_event_open(attr, process, -1, -1, 0);
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

int
setup_notification(int index)
{
	/*
	 * Setup notification on the file descriptor
	 */
	int ret = fcntl(fd[index], F_SETFL, fcntl(fd[0], F_GETFL, 0) | O_ASYNC);
	if (ret == -1) {
		fprintf(stderr, "Can't set notification\n");
		return -1;
	}

	ret = fcntl(fd[index], F_SETSIG, SIGIO);
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

	/* Enable the event for one period */
	ret = ioctl(fd[index], PERF_EVENT_IOC_REFRESH, 1);
	if (ret == -1) {
		fprintf(stderr, "Error in IOC_REFRESH\n");
		return -1;
	}
	return 0;
}

int
disable_counter(int index)
{
	/* Disable the event counter */
	int ret = ioctl(fd[index], PERF_EVENT_IOC_DISABLE, 1);
	if (ret == -1) {
		fprintf(stderr, "Error in IOC_DISABLE\n");
		return -1;
	}

	close(fd[index]);
	return 0;
}

void
parent_action(int child)
{
	struct sigaction act;
	int ret;
	int wstat;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigio_handler;
	act.sa_flags     = SA_SIGINFO;
	sigaction(SIGIO, &act, 0);

	setup_perf(child, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
	setup_perf(child, 1, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

	setup_notification(0);
	setup_notification(1);

wait:
	/* Wait for the signal */
	if ((ret = wait(&wstat)) == -1)
		if (errno == EINTR)
			goto wait;

	disable_counter(0);
	disable_counter(1);

	printf("total samples cycles: %d\n", samples[0]);
	printf("total samples instructions: %d\n", samples[1]);
}

int main(int argc, char *argv[])
{
	pid_t child;

	child = fork();
	if (child == 0) {
	  wait_loop();

	} else if (child >0) {
	  parent_action(child);

	} else {
	  fprintf(stderr, "Fail to fork\n");
	}
}
