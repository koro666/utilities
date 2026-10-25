#include <fcntl.h>
#include <unistd.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#include <sqlite3.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>

static sqlite3* database;
static sqlite3_stmt* statement;

static bool opendb(const char*);
static bool bind_null(int);
static bool bind_int64(int, sqlite3_int64);
static bool bind_blob(int, const char*);
static void closedb();

static bool scan(const char*);
static int scan_cb(const char*, const struct stat*, int, struct FTW*);
static bool try_get_extent_count(const char*, const struct stat*, unsigned int*);

int main(int argc, char** argv)
{
	if (!opendb(argc > 1 ? argv[1] : NULL))
		return EXIT_FAILURE;

	int result;
	if (argc > 2)
	{
		result = EXIT_SUCCESS;
		for (int i = 2; i < argc; ++i)
		{
			if (!scan(argv[i]))
				result = EXIT_FAILURE;
		}
	}
	else
	{
		result = scan(".") ?
			EXIT_SUCCESS :
			EXIT_FAILURE; 
	}

	closedb();
	return result;
}

static bool opendb(const char* path)
{
	int error = path ?
		sqlite3_open_v2(path, &database, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_EXRESCODE, NULL) :
		sqlite3_open_v2(":memory:", &database, SQLITE_OPEN_READWRITE|SQLITE_OPEN_MEMORY|SQLITE_OPEN_EXRESCODE, NULL);
	
	if (error)
	{
		fprintf(stderr, "sqlite3_open_v2: %s\n", sqlite3_errstr(error));
		return false;
	}

	static const char sql0[] =
		"PRAGMA journal_mode = WAL;\n"
		"PRAGMA synchronous = OFF;\n"
		"PRAGMA foreign_keys = ON;\n"
		"CREATE TABLE IF NOT EXISTS files ("
			"device INTEGER NOT NULL, "
			"inode INTEGER NOT NULL, "
			"mode INTEGER NOT NULL, "
			"links INTEGER NOT NULL, "
			"uid INTEGER NOT NULL, "
			"gid INTEGER NOT NULL, "
			"size INTEGER NOT NULL, "
			"atime INTEGER NOT NULL, "
			"mtime INTEGER NOT NULL, "
			"ctime INTEGER NOT NULL, "
			"extents INTEGER, "
			"name BLOB) STRICT;\n";

	char* errmsg;
	error = sqlite3_exec(database, sql0, NULL, NULL, &errmsg);
	if (error)
	{
		fprintf(stderr, "sqlite3_exec: %s\n", errmsg);
		sqlite3_close_v2(database);
		database = NULL;
		return false;
	}

	static const char sql1[] =
		"INSERT INTO files "
		"(device, inode, mode, links, uid, gid, size, atime, mtime, ctime, extents, name) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

	error = sqlite3_prepare_v2(database, sql1, -1, &statement, NULL);
	if (error)
	{
		fprintf(stderr, "sqlite3_prepare_v2: %s\n", sqlite3_errmsg(database));
		sqlite3_close_v2(database);
		database = NULL;
		return false;
	}

	return true;
}

static bool bind_null(int i)
{
	int error = sqlite3_bind_null(statement, i);
	if (error)
	{
		fprintf(stderr, "sqlite3_bind_null: %s\n", sqlite3_errmsg(database));
		return false;
	}

	return true;
}

static bool bind_int64(int i, sqlite3_int64 v)
{
	int error = sqlite3_bind_int64(statement, i, v);
	if (error)
	{
		fprintf(stderr, "sqlite3_bind_int64: %s\n", sqlite3_errmsg(database));
		return false;
	}

	return true;
}

static bool bind_blob(int i, const char* v)
{
	int error = sqlite3_bind_blob(statement, i, v, strlen(v), SQLITE_STATIC);
	if (error)
	{
		fprintf(stderr, "sqlite3_bind_blob: %s\n", sqlite3_errmsg(database));
		return false;
	}

	return true;
}

static void closedb()
{
	sqlite3_finalize(statement);
	statement = NULL;

	sqlite3_close_v2(database);
	database = NULL;
}

static bool scan(const char* path)
{
	int result = nftw(path, scan_cb, sysconf(_SC_OPEN_MAX) - 16, FTW_PHYS);
	if (result == -1)
	{
		perror("nftw");
		return false;
	}

	return result == 0;
}

static int scan_cb(const char* path, const struct stat* sb, int typeflag, struct FTW* ftwbuf)
{
	if (typeflag != FTW_F)
		return 0;

	unsigned int extents;
	if (!bind_int64(1, sb->st_dev) ||
		!bind_int64(2, sb->st_ino) ||
		!bind_int64(3, sb->st_mode & ~S_IFMT) ||
		!bind_int64(4, sb->st_nlink) ||
		!bind_int64(5, sb->st_uid) ||
		!bind_int64(6, sb->st_gid) ||
		!bind_int64(7, sb->st_size) ||
		!bind_int64(8, sb->st_atime) ||
		!bind_int64(9, sb->st_mtime) ||
		!bind_int64(10, sb->st_ctime) ||
		!(try_get_extent_count(path, sb, &extents) ? bind_int64(11, extents) : bind_null(11)) ||
		!bind_blob(12, path))
		return 1;

	int error;
	while (true)
	{
		error = sqlite3_step(statement);
		if (!error)
			continue;
		else if (error == SQLITE_DONE)
			break;

		fprintf(stderr, "sqlite3_step: %s\n", sqlite3_errmsg(database));
		sqlite3_reset(statement);
		return 1;
	}

	error = sqlite3_reset(statement);
	if (error)
	{
		fprintf(stderr, "sqlite3_reset: %s\n", sqlite3_errmsg(database));
		return 1;
	}

	return 0;
}

static bool try_get_extent_count(const char* path, const struct stat* sb, unsigned int* result)
{
	if (!S_ISREG(sb->st_mode))
		return false;

	int fd = open(path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
	if (fd == -1)
	{
		perror(path);
		return false;
	}

	struct fiemap fm = { .fm_length = sb->st_size };
	if (ioctl(fd, FS_IOC_FIEMAP, &fm) == -1)
	{
		if (!((errno == EINVAL && !sb->st_size) || (errno == ENOTSUP)))
			perror("ioctl");
		close(fd);
		return false;
	}

	close(fd);

	*result = fm.fm_mapped_extents;
	return true;
}

