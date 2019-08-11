#include <fcntl.h>
#include <unistd.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <ftw.h>

static int scan(const char*);
static int scan_cb(const char*, const struct stat*, int, struct FTW*);
static int try_get_extent_count(const char*, off_t);

int main(int argc, char** argv)
{
	int result = EXIT_SUCCESS;

	puts("DEVICE\tINODE\tMODE\tLINKS\tUID\tGID\tSIZE\tATIME\tMTIME\tCTIME\tEXTENTS\tNAME");

	if (argc > 1)
	{
		for (int i = 1; i < argc; ++i)
		{
			if (scan(argv[i]) != EXIT_SUCCESS)
				result = EXIT_FAILURE;
		}
	}
	else
	{
		result = scan(".");
	}

	return result;
}

static int scan(const char* path)
{
	if (nftw(path, scan_cb, sysconf(_SC_OPEN_MAX) - 4, FTW_PHYS) == -1)
	{
		perror("nftw");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int scan_cb(const char* path, const struct stat* sb, int typeflag, struct FTW* ftwbuf)
{
	if (typeflag != FTW_F)
		return 0;

	printf(
		"%ld\t%ld\t%04o\t%lu\t%d\t%d\t%ld\t%ld\t%ld\t%ld\t%d\t%s\n",
		sb->st_dev,
		sb->st_ino,
		sb->st_mode & ~S_IFMT,
		sb->st_nlink,
		sb->st_uid,
		sb->st_gid,
		sb->st_size,
		sb->st_atime,
		sb->st_mtime,
		sb->st_ctime,
		try_get_extent_count(path, sb->st_size),
		path
	);
	return 0;
}

static int try_get_extent_count(const char* path, off_t size)
{
	int fd = open(path, O_RDONLY);
	if (fd == -1)
	{
		perror(path);
		return -1;
	}

	struct fiemap fm = { .fm_length = size };
	if (ioctl(fd, FS_IOC_FIEMAP, &fm) == -1)
	{
		perror("ioctl");
		close(fd);
		return -1;
	}

	close(fd);
	return fm.fm_mapped_extents;
}
