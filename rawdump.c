#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
  int gfd, i, result, len;
  unsigned char buffer[4096];

  if (argc != 2)
  {
	fprintf(stderr, "Usage: rawdump raw-data-file\n");
	exit(1);
  }
  gfd = open(argv[1], O_RDONLY);
  result = 1;
  while (result > 0)
  {
	result = read(gfd, &len, sizeof(len));
	if (result > 0)
	{
		result = read(gfd, buffer, len);
		if (result > 0)
		{
			printf("%3d:", len);
			for (i = 0; i < (len / 1); ++i)
				printf(" %2.2x", buffer[i]);
			printf("\n");
		}
	}
  }
  close(gfd);
  return 0;
}

