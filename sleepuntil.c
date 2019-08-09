#include <unistd.h>
#include <time.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	struct tm input;
	const char* endptr;

	if (argc == 2)
		endptr = strptime(argv[1], "%X", &input);
	else
		endptr = NULL;

	if (!endptr || *endptr)
	{
		fprintf(stderr, "Usage: %s <time>\n", argv[0]);
		return 1;
	}

	time_t now = time(NULL);

	struct tm target;
	localtime_r(&now, &target);

	target.tm_hour = input.tm_hour;
	target.tm_min = input.tm_min;
	target.tm_sec = input.tm_sec;

	time_t future = mktime(&target);
	if (future < now)
		future += 86400;

	char buffer[32];
	sprintf(buffer, "%ld", future - now);

	execl("/bin/sleep", "sleep", buffer, NULL);
	return 2;
}
