#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char** argv)
{
	off_t offset = lseek(STDIN_FILENO, 0, SEEK_CUR);
	if (offset == -1)
		offset = 0;

	struct stat sb;
	if (fstat(STDIN_FILENO, &sb) == -1)
		memset(&sb, 0, sizeof(sb));

	int tty = isatty(STDOUT_FILENO);
	while (1)
	{
		unsigned char in_buffer[16];
		ssize_t in_size;

		do { in_size = read(STDIN_FILENO, in_buffer, sizeof(in_buffer)); }
		while (in_size == -1 && errno == EINTR);

		if (!in_size)
			return 0;

		if (in_size < 0)
			return 1;

		unsigned char out_buffer[4+16+4+2+(16*(2+1))+1+16+1];
		size_t out_size = 0;

		if (tty)
		{
			out_buffer[out_size++] = '\x1B';
			out_buffer[out_size++] = '[';
			out_buffer[out_size++] = '2';
			out_buffer[out_size++] = 'm';
		}

		static const char hex[] = "0123456789ABCDEF";

		for (int shift = (sb.st_size > 0xffffffff ? 60 : 28); shift >= 0; shift -= 4)
			out_buffer[out_size++] = hex[(offset >> shift) & 0xf];

		if (tty)
		{
			out_buffer[out_size++] = '\x1B';
			out_buffer[out_size++] = '[';
			out_buffer[out_size++] = '0';
			out_buffer[out_size++] = 'm';
		}

		out_buffer[out_size++] = ' ';
		out_buffer[out_size++] = ' ';

		for (int i = 0; i < 16; ++i)
		{
			if (i < in_size)
			{
				unsigned char c = in_buffer[i];
				out_buffer[out_size++] = hex[(c >> 4) & 0x0f];
				out_buffer[out_size++] = hex[c & 0x0f];
			}
			else
			{
				out_buffer[out_size++] = ' ';
				out_buffer[out_size++] = ' ';
			}

			out_buffer[out_size++] = ' ';
		}

		out_buffer[out_size++] = ' ';

		for (int i = 0; i < in_size; ++i)
		{
			unsigned char c = in_buffer[i];
			out_buffer[out_size++] = c < 0x20 || c >= 0x7f ? '.' : c;
		}

		out_buffer[out_size++] = '\n';

		for (off_t cursor = 0; cursor < out_size;)
		{
			ssize_t result;
			do { result = write(STDOUT_FILENO, out_buffer + cursor, out_size - cursor); }
			while (out_size == -1 && errno == EINTR);

			if (result < 0)
				return 1;

			cursor += result;
		}

		offset += in_size;
	}
}
