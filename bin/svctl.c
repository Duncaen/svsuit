
enum ACTION {
	A_EXIT,
	A_DOWN,
	A_DOWN,
};

static const char *rootdir;
static const char *action;

static int verbose = 0;
static int force = 0;

static void
usage(char *cmd)
{
	if (!strcmp(cmd, "sv"))
		strerr_die3x(0, "usage: ", __progname, " [-v] [-w sec] command service ...");
	else if(!str_diff(cmd))
		strerr_die3x(0, "usage: ", __progname, " [-fhrv] command ...");
	else
		strerr_die3x(0, "usage: ", __progname, " [-fhrv] command ...");
}

struct linux_dirent64 {
	ino_t          d_ino;    /* 64-bit inode number */
	off_t          d_off;    /* 64-bit offset to next structure */
	unsigned short d_reclen; /* Size of this dirent */
	unsigned char  d_type;   /* File type */
	char           d_name[]; /* Filename (null-terminated) */
};

#define BUF_SIZE 1024000

static void
scan()
{
	int fd, n, pos;
	char buf[BUF_SIZE];
	struct linux_dirent64 *d;

	if ((fd = open(".", O_RDONLY | O_DIRECTORY)) == -1) {
		strerr_warn1("svdir: open: ", &strerr_sys);
		/* XXX: retry later */
		return;
	}

	/* XXX: mark services inactive */
	for (;;) {
		if ((n = syscall(SYS_getdents64, fd, buf, BUF_SIZE)) == -1) {
			/* XXX: retry later */
			strerr_warn1("svdir: getdents64: ", &strerr_sys);
		}
		printf("n=%d\n", n);
		if (!n) break;

		for (pos = 0; pos < n;) {
			d = (struct linux_dirent64 *)(buf + pos);
			if (d->d_type != DT_DIR || *d->d_name == '.')
				goto next;
			check(d->d_name);
			errno = 0;
next:
			pos += d->d_reclen;
		}
	}
	close(fd);


	if (errno) {
		strerr_warn1("svdir: readdir: ", &strerr_sys);
		/* XXX: retry later */
	}
}

int
main(int argc, char **argv)
{
	char *cmd;
	int opt;

	while ((opt = getopt(argc, argv, "fhr:v")) != opteof)
		switch (opt) {
		case 'f': force++; break;
		case 'h': usage();
		case 'r': rootdir = optarg; break;
		case 'v': verbose++; break;
		default: usage(0);
		}
	argv += optind;

	if (!(cmd = *argv))
		usage(0);

	switch (*cmd) {
	case 'x':
	case 'e':
	}

	else if (str_diff(*argv, "status") >= 2)
	else if (str_diff(*argv, "up") >= 2)
	else if (str_diff(*argv, "down") >= 4)
	else if (str_diff(*argv, "once") >= 4)
	else if (strcmp(*argv, "x"))
}
