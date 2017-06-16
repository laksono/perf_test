
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <asm/unistd.h>

/******************************************************************************
 * perf event
 *****************************************************************************/
#include <linux/perf_event.h>

/******************************************************************************
 * perfmon
 *****************************************************************************/
#include "perfmon/pfmlib.h"


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


long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
		int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
			group_fd, flags);
	return ret;
}

/*
 * return 0 or positive if the event exists, -1 otherwise
 * if the event exist, code and type are the code and type of the event
 */
int
pfmu_getEventType(const char *eventname, uint64_t *code, uint64_t *type)
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
    *type = arg.attr->type;
    *code = arg.attr->config;
    return 1;
  }
  return -1;
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

/*
 * test if the kernel can create the given event
 * @param code: config, @type: type of event
 * @return: file descriptor if successful, -1 otherwise
 *  caller needs to check errno for the root cause
 */ 
static int
test_pmu(uint64_t code, uint64_t type) 
{
	struct perf_event_attr event_attr;
	memset(&event_attr, 0, sizeof(event_attr));
	event_attr.disabled = 1;

	event_attr.size = sizeof(struct perf_event_attr);
	event_attr.type = type;
	event_attr.config = code;

	int fd = perf_event_open(&event_attr, 0, -1, -1, 0);
	if (fd == -1) {
		return -1;
	}
	close(fd);
	return fd;
}

/*
 * test all the pmus listed in perfmon
 *
 */
static int
browse_pmus()
{
  static char *event =  ".*";
  pfm_pmu_info_t pinfo;
  pfm_event_info_t info;
  int i, j, ret, pname;

  memset(&pinfo, 0, sizeof(pinfo));
  memset(&info, 0, sizeof(info));

  pinfo.size = sizeof(pinfo);
  info.size = sizeof(info);

  int num_events = 0, num_fail = 0;

 /*
  * scan all supported events, incl. those
  * from undetected PMU models
  */
  pfm_for_all_pmus(j) {

    ret = pfm_get_pmu_info(j, &pinfo);
    if (ret != PFM_SUCCESS)
	continue;

    for (i = pinfo.first_event; i != -1; i = pfm_get_event_next(i)) {

	ret = pfm_get_event_info(i, PFM_OS_NONE, &info);

	if (ret != PFM_SUCCESS) {
	  fprintf( stderr, "cannot get event info: %s", pfm_strerror(ret));
	}
        else {
  	  uint64_t code, type;
          if (pfmu_getEventType(info.name, &code, &type) > 0) {

	    // test if we can create the event
     	    int result = test_pmu(code, type);
	    if (result<0) { 
	        printf("type: %d \tcode: %d \t \tname: %s.", 
				type, code, info.name);
		printf(" \t Error : %d (%s)\n", errno, strerror(errno));
		num_fail++;
	    }
	  }
	  num_events++;
	}
    } 
  }
  printf("\nNumber of events: %d\nNumber of Perf event failures: %d (%.2f \%)\n", 
		  num_events, num_fail, (float) 100.0*((float)num_fail/num_events));
  return num_events;
}

int
main(int argc, char *argv[])
{
  char *event_name;
  uint64_t code, type;

  if (pfmu_init() == 0) {
    exit(1);
  }

  if (argc > 1) {
	 
    event_name = argv[1];

    if (pfmu_getEventType(event_name, &code, &type) > 0) {
      // test if we can create the event
      int result = test_pmu(code, type);
      printf("type: %d \tcode: %d \tname: %s \t %s\n", 
				type, code, event_name, 
				result<0? "FAIL": "PASS" );
    } else
    {
	fprintf(stderr, "Event not recognized: %s\n", event_name);
    }
  } else {
    browse_pmus();
  }
  pfmu_fini();
}
