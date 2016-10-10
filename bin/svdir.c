#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include <dirent.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "buffer.h"
#include "coe.h"
#include "open.h"
#include "byte.h"
#include "strerr.h"
#include "sig.h"
#include "str.h"
#include "fd.h"
#include "wait.h"
#include "sgetopt.h"
#include "pathexec.h"
#include "taia.h"
#include "fmt.h"
#include "fifo.h"
#include "getln.h"
#include "iopause.h"
#include "ndelay.h"

static const char *dir;

static int verbose = 0;
static int pid1 = 0;
static int ex = 0;

static int in = -1;
static int selfpipe[2] = { -1, -1 };

#define LOGBUFFER_SIZE (1024 << 16)
static char logbuf[LOGBUFFER_SIZE];

#ifndef CONFDIR
#define CONFDIR "/etc/init"
#endif

#define STAGE_PATH "/home/duncan/repos/svsuit/example/sv"
static char *stages[3] = {
	STAGE_PATH "/1",
	STAGE_PATH "/2",
	STAGE_PATH "/3"
};

static void
sig_term_handler()
{
	ex = 1;
	(void)write(selfpipe[1], "", 1);
}

static void
sig_hangup_handler()
{
	ex = 2;
	(void)write(selfpipe[1], "", 1);
}

static void
sig_child_handler()
{
	strerr_warn1("svdir: reap", 0);
	while (wait_nohang(0) > 0);
}

static void
start(char *name)
{
	pid_t pid;
	char *cargv[3] = { "svrun", 0, 0 };
	
	switch ((pid = fork())) {
	case -1:
		strerr_warn3("svdir: fork for ", name, ": ", &strerr_sys);
		/* XXX: retry? */
		return;
	case 0: break;
	default: return;
	}

	cargv[1] = name;
	execvp(*cargv, cargv);
	strerr_die3sys(111, "svdir: exec svrun ", name, ": ");
}

static pid_t
runstage(int n, char **argv, int fd)
{
	pid_t pid;
	char *s = stages[n-1];
	char *defargv[] = { 0, 0 };

	if (!argv) argv = defargv;

	if (verbose)
		strerr_warn2("svdir: running stage: ", s, 0);

	switch ((pid = fork())) {
	case -1:
		strerr_warn3("svdir: fork stage ", s, ": ", &strerr_sys);
		/* XXX: retry? */
		return 0;
	case 0: break;
	default: return pid;
	}
	argv[0] = s;
	if (fd) {
		fd_move(1, fd);
		close(fd);
	}
	execv(s, argv);
	if (errno == ENOENT) {
		if (verbose)
			strerr_warn3("svdir: stage ", s, ": ", &strerr_sys);
		_exit(0);
	}
	strerr_die3sys(111, "svdir: exec stage ", s, ": ");
	return 0;
}

static int
stage1()
{
	buffer buf_log;
	pid_t pid;
	int fd = 0;
	int log[2];

	/* run and wait for stage1 */
	if (pid1) {
		buffer_init(&buf_log, buffer_unixread, 1, logbuf, sizeof logbuf);
		if (pipe(log) == -1)
			strerr_die1sys(1, "svdir: pipe: ");
		coe(log[0]);
		coe(log[1]);
		ndelay_on(log[0]);
		ndelay_on(log[1]);
		fd = log[1];
	}

	int rv;
	for (;;) {
		pid = runstage(1, 0, fd);
		wait_pid(&rv, pid);
		if (wait_exitcode(rv) == 0)
			break;
		sleep(1);
	}

	/* XXX: how to handle this, exit into stage3? */
	if (chdir(dir) == -1)
		strerr_warn1("svdir: chdir: ", &strerr_sys);

	return 0;
}

static int
stage2()
{

	/* setup in fifo */
	if (fifo_make("./in", 0) == -1)
		strerr_warn1("svdir: fifo_make: ", &strerr_sys);
	if ((in = open_read("./in")) == -1)
		strerr_warn1("svdir: open ./in: ", &strerr_sys);
	coe(in);

}

static int
stage3()
{
	pid_t pid;
	int rv;

	/* run and wait for stage3 */
	for (;;) {
		pid = runstage(3, 0, 0);
		wait_pid(&rv, pid);
		if (wait_exitcode(rv) == 0)
			return rv;
		sleep(1);
	}
	return 0;
}

#define MAX_EVENTS 10

static int
loop()
{
	struct epoll_event ev, events[MAX_EVENTS];
	int epollfd, nfds;
	stralloc line;
	buffer b;
	int i, m;
	int curstage;
	pid_t pid;

	curstage = 1;
	/* run stage2 */
	pid = runstage(2, 0, 0);

	iofds[0].fd = selfpipe[0];
	iofds[0].events = IOPAUSE_READ;
	iofds[1].fd = in;
	iofds[1].events = IOPAUSE_READ;

	do {
		epollfd = epoll_create1(0);
	} while (epollfd == -1 && errno == EINTR);
	if (epollfd == -1)
		strerr_die1sys(1, "svdir: epoll_create: ");

	ev.events = POLLIN;
	ev.data.fd = selfpipe[0];
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, &ev) == -1)
		strerr_die1sys(1, "svdir: epoll_ctl: ");

	char ch;
	for (;;) {
		if (verbose)
			strerr_warn1("svdir: wait for events", 0);
		iopause(iofds, 2, 0, 0);
		if (verbose)
			strerr_warn1("svdir: something happened", 0);
		for (i = 0; i < 2; i++) {
			if (!iofds[i].revents)
				continue;
			if (iofds[i].fd == selfpipe[0]) {
				while (read(selfpipe[0], &ch, 1) == 1)
					;
			} else if (iofds[i].fd == in) {
				if (getln(&b, &line, &m, '\n') == -1) break;
				start(line.s);
			}
		}
		if (ex) return ex;
	}
	return 0;
}

int
main(int argc, const char *const *argv)
{
	int opt;
	int tflag, Pflag;

	pid1 = getpid() == 1;
	tflag = Pflag = verbose = 0;

	while ((opt = getopt(argc, argv, "tPv")) != opteof)
		switch (opt) {
		case 't': tflag++; break;
		case 'P': Pflag++; break;
		case 'v': verbose++; break;
		}

	(void)tflag;

	if (Pflag)
		setsid();

	argv += optind;
	dir = *argv;

	if (verbose && pid1)
		strerr_warn1("svdir: running as pid 1", 0);

	if (verbose)
		strerr_warn1("svdir: setup signal handlers", 0);

	if (pipe(selfpipe) == -1)
		strerr_die1sys(1, "svdir: pipe: ");
	coe(selfpipe[0]);
	coe(selfpipe[1]);
	ndelay_on(selfpipe[0]);
	ndelay_on(selfpipe[1]);

	if (verbose)
		strerr_warn1("svdir: setup signal handlers", 0);

	sig_catch(sig_term, sig_term_handler);
	sig_catch(sig_int, sig_term_handler);
	sig_catch(sig_hangup, sig_hangup_handler);
	if (pid1) sig_catch(sig_child, sig_child_handler);

	(void)stage1();
	(void)stage2();
	(void)stage3();

	_exit(0);
}
