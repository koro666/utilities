#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

static bool add_uid(const char*, size_t*, uid_t**);
static bool check_uid(uid_t, size_t, const uid_t*);
static int run_client(struct sockaddr_un*, socklen_t, int, char**);
static int run_server(struct sockaddr_un*, socklen_t, size_t, const uid_t*, size_t, const uid_t*);

#define AUTO_FREE __attribute__((cleanup(cleanup_free)))
#define AUTO_CLOSE __attribute__((cleanup(cleanup_close)))

static void cleanup_free(void*);
static void cleanup_close(int*);

int main(int argc, char** argv)
{
	bool server = false;

	const char* path = "/var/run/takeover.socket";

	size_t uidc = 0, duidc = 0;

	AUTO_FREE uid_t* uidv = NULL;
	AUTO_FREE uid_t* duidv = NULL;

	int opt;
	while ((opt = getopt(argc, argv, "ds:u:o:")) != -1)
	{
		switch (opt)
		{
			case 'd':
				server = true;
				break;
			case 's':
				path = optarg;
				break;
			case 'u':
				if (!add_uid(optarg, &uidc, &uidv))
					fprintf(stderr, "-u: invalid user %s\n", optarg);
				break;
			case 'o':
				if (!add_uid(optarg, &duidc, &duidv))
					fprintf(stderr, "-o: invalid user %s\n", optarg);
				break;
		}
	}

	struct sockaddr_un saddr;
	memset(&saddr, 0, sizeof(saddr));
	saddr.sun_family = AF_UNIX;
	strncpy(saddr.sun_path, path, sizeof(saddr.sun_path) - 1);
	socklen_t slen = offsetof(struct sockaddr_un, sun_path) + strlen(saddr.sun_path);
	if (saddr.sun_path[0] == '@')
		saddr.sun_path[0] = 0;
	else
		++slen;

	if (server)
		return run_server(&saddr, slen, uidc, uidv, duidc, duidv);
	else
		return run_client(&saddr, slen, argc - optind, argv + optind);
}

static bool add_uid(const char* s, size_t* c, uid_t** v)
{
	char* endptr;
	uid_t uid = strtoul(s, &endptr, 0);
	if (*endptr)
	{
		struct passwd* pw = getpwnam(s);
		if (!pw)
			return false;
		uid = pw->pw_uid;
	}

	*v = realloc(*v, ((*c) + 1) * sizeof(uid_t));
	(*v)[*c] = uid;
	++(*c);

	return true;
}

static bool check_uid(uid_t u, size_t c, const uid_t* v)
{
	for (size_t i = 0; i < c; ++i)
	{
		if (v[i] == u)
			return true;
	}

	return false;
}

static int run_client(struct sockaddr_un* saddr, socklen_t slen, int pathc, char** pathv)
{
	AUTO_CLOSE int sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sock == -1)
	{
		perror("socket");
		return EXIT_FAILURE;
	}

	if (connect(sock, (const struct sockaddr*)saddr, slen) == -1)
	{
		perror("connect");
		return EXIT_FAILURE;
	}

	for (int i = 0; i < pathc; ++i)
	{
		AUTO_CLOSE int fd = open(pathv[i], O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
		if (fd == -1)
		{
			perror(pathv[i]);
			continue;
		}

		char buffer[CMSG_SPACE(sizeof(int))];

		struct msghdr msg =
		{
			.msg_control = buffer,
			.msg_controllen = sizeof(buffer)
		};

		struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
		msg.msg_controllen = cmsg->cmsg_len;

		if (sendmsg(sock, &msg, 0) == -1)
			perror("sendmsg");
	}

	return EXIT_SUCCESS;
}

static int run_server(struct sockaddr_un* saddr, socklen_t slen, size_t uidc, const uid_t* uidv, size_t duidc, const uid_t* duidv)
{
	AUTO_CLOSE int sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sock == -1)
	{
		perror("socket");
		return EXIT_FAILURE;
	}

	if (bind(sock, (const struct sockaddr*)saddr, slen) == -1)
	{
		perror("bind");
		return EXIT_FAILURE;
	}

	int optval = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)) == -1)
	{
		perror("setsockopt");
		return EXIT_FAILURE;
	}

	while (true)
	{
		char buffer[CMSG_ALIGN(CMSG_SPACE(sizeof(struct ucred))) + CMSG_SPACE(sizeof(int))];

		struct msghdr msg =
		{
			.msg_control = buffer,
			.msg_controllen = sizeof(buffer)
		};

		if (recvmsg(sock, &msg, MSG_CMSG_CLOEXEC) == -1)
		{
			if (errno == EINTR)
				continue;

			perror("recvmsg");
			return EXIT_FAILURE;
		}

		AUTO_CLOSE int fd = -1;
		struct ucred cred = { -1, -1, -1 };

		for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg,cmsg))
		{
			if (cmsg->cmsg_level != SOL_SOCKET)
				continue;

			switch (cmsg->cmsg_type)
			{
				case SCM_CREDENTIALS:
					memcpy(&cred, CMSG_DATA(cmsg), sizeof(struct ucred));
					break;
				case SCM_RIGHTS:
					cleanup_close(&fd);
					memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
					break;
			}
		}

		if (fd == -1 || cred.uid == -1 || cred.gid == -1)
			continue;

		if (!check_uid(cred.uid, uidc, uidv))
		{
			fprintf(stderr, "%d: Unauthorized caller uid\n", cred.uid);
			continue;
		}

		struct stat buf;
		if (fstat(fd, &buf) == -1)
		{
			perror("fstat");
			continue;
		}

		if (!check_uid(buf.st_uid, duidc, duidv))
		{
			fprintf(stderr, "%d: Unauthorized owner uid\n", buf.st_uid);
			continue;
		}

		if (fchown(fd, cred.uid, cred.gid) == -1)
		{
			perror("fchown");
			continue;
		}
	}

	return EXIT_SUCCESS;
}

static void cleanup_free(void* pp)
{
	free(*(void**)pp);
}

static void cleanup_close(int* pfd)
{
	if (*pfd != -1)
		close(*pfd);
}
