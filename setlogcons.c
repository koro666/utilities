#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	unsigned long int vt = 0;
	if (argc >= 2)
	{
		char* nptr = argv[1];
		char* endptr;

		vt = strtoul(nptr, &endptr, 10);

		if (!*nptr || *endptr)
		{
			fprintf(stderr, "%s: invalid number\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	char devname[32];
	sprintf(devname, "/dev/tty%lu", vt);

	int fd = open(devname, O_RDONLY);
	if (fd == -1)
	{
		perror("open");
		return EXIT_FAILURE;
	}

	char arg[2] = { 11, vt };
	if (ioctl(fd, TIOCLINUX, arg) == -1)
	{
		perror("ioctl");
		close(fd);
		return EXIT_FAILURE;
	}

	close(fd);
	return EXIT_SUCCESS;
}
