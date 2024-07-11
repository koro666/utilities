#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static bool process_path(const char*);
static bool process_fd(const char*, int);
static void convert_timespec(const struct statx_timestamp*, struct timespec*);

int main(int argc, char** argv)
{
	int result = EXIT_SUCCESS;

	for (int i = 1; i < argc; ++i)
	{
		if (!process_path(argv[i]))
			result = EXIT_FAILURE;
	}

	return result;
}

static bool process_path(const char* path)
{
	int fd = open(path, O_PATH|O_NOFOLLOW|O_CLOEXEC);
	if (fd < 0)
	{
		perror(path);
		return false;
	}

	bool result = process_fd(path, fd);
	close(fd);

	return result;
}

static bool process_fd(const char* path, int fd)
{
	struct statx stx;
	if (statx(fd, "", AT_EMPTY_PATH, STATX_ATIME|STATX_BTIME, &stx) < 0)
	{
		perror(path);
		return false;
	}

	if (!(stx.stx_mask & STATX_BTIME))
	{
		fprintf(stderr, "%s: no btime\n", path);
		return false;
	}

	struct timespec times[2];

	convert_timespec(&stx.stx_btime, times + 1);
	if (stx.stx_mask & STATX_ATIME)
		convert_timespec(&stx.stx_atime, times);
	else
		times[0] = times[1];

	if (utimensat(fd, "", times, AT_EMPTY_PATH) < 0)
	{
		perror(path);
		return false;
	}

	return true;
}

static void convert_timespec(const struct statx_timestamp* it, struct timespec* ot)
{
	ot->tv_sec = it->tv_sec;
	ot->tv_nsec = it->tv_nsec;
}
