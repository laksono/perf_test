
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define N 10000000

int main(int argc, char *argv[])
{
  char *name_template = "/tmp/lakstemp-XXXXXX";
  char filename[256];
  strncpy(filename, name_template, 20);

  int fd = mkstemp(filename);
  if (fd<0) {
	fprintf(stderr, "Error creating temp file: %d %s\n", errno, strerror(errno));
	return -1;
  }
  FILE *fp = fdopen(fd, "w");
  if (fp == NULL) {
	fprintf(stderr, "Error creating file: %d %s\n", errno, strerror(errno));
	return -2;
  }

  char buffer[] = "this is just a test.\nPlease ignore it.\n";
  int i;

  for(i=0; i<N; i++) {
	  fprintf(fp, buffer);
  }
  fclose(fp);
  return 1;
}

