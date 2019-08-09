#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

static const ssize_t check_buffer_size = 16;

static void process_path(const char*);
static void process_fd(const char*, int);
static bool is_vip(const char*, ssize_t);

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i)
		process_path(argv[i]);
	return 0;
}

static void process_path(const char* path)
{
	int fd = open(path, O_RDONLY|O_CLOEXEC);
	if (fd == -1)
	{
		perror(path);
		return;
	}

	process_fd(path, fd);
	close(fd);
}

static void process_fd(const char* path, int fd)
{
	if (lseek(fd, -check_buffer_size, SEEK_END) == -1)
	{
		perror(path);
		return;
	}

	char buffer[check_buffer_size];
	ssize_t size = read(fd, buffer, check_buffer_size);

	if (size == -1)
	{
		perror(path);
		return;
	}

	if (is_vip(buffer, size))
		printf("%s\n", path);
}

static bool is_vip(const char* buffer, ssize_t size)
{
	for (; size > 0; --size)
	{
		switch (buffer[size - 1])
		{
			case 0x30:
			case 0x31:
			case 0x32:
			case 0x33:
			case 0x34:
			case 0x35:
			case 0x36:
			case 0x37:
			case 0x38:
			case 0x39:
				continue;
			case 0x0a:
				return true;
			default:
				return false;
		}
	}

	return false;
}
