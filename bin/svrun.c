static void
updatesv(struct svdir *s)
{
	char *fstatusnew = "supervise/status.new";
	char *fstatus = "supervise/status";
	char *fstatnew = "supervise/stat.new";
	char *fstat = "supervise/stat";
	char *fpidnew = "supervise/pid.new";
	char *fpid = "supervise/pid";
	char *p;
	char bspace[64];
	char status[20];
  char spid[FMT_ULONG];
	uint64_t l;
	ssize_t n;
	buffer b;
	int fd;

	(void)s;

	/* pid */
	if (s->pidchanged) {
		if ((fd =open_trunc(fpidnew)) == -1) {
			strerr_warn3("svdir: unable to open ", fpidnew, ": ", &strerr_sys);
			return;
		}
		buffer_init(&b, buffer_unixwrite, fd, bspace, sizeof bspace);
		spid[fmt_ulong(spid, (unsigned long)s->pid)] =0;
		if (s->pid) {
			buffer_put(&b, spid, sizeof spid);
			buffer_putflush(&b, "\n", 1);
		}
		close(fd);
		if (rename(fpidnew, fpid) == -1) {
			strerr_warn5("svdir: unable to renmae ", fpidnew, " to ", fpid, ": ",
			    &strerr_sys);
			return;
		}
		s->pidchanged = 0;
	}

	/* stat */
	if ((fd = open_trunc(fstatnew)) == -1) {
		strerr_warn3("svdir: unable to open ", fstatnew, ": ", &strerr_sys);
		return;
	}
  buffer_init(&b, buffer_unixwrite, fd, bspace, sizeof bspace);
	switch (s->state) {
	case S_DOWN: p = "down"; break;
	case S_RUN: p = "run"; break;
	case S_FINISH: p = "finish"; break;
	}
	buffer_puts(&b, p);
	if (s->ctrl & C_PAUSE) buffer_puts(&b, ", paused");
	if (s->ctrl & C_TERM) buffer_puts(&b, ", got TERM");
	if (s->state != S_DOWN) {
		switch (s->want) {
		case W_DOWN: p = "down"; break;
		case W_EXIT: p = "exit"; break;
		default: p = 0; break;
		}
		if (p) {
			buffer_puts(&b, ", want ");
			buffer_puts(&b, p);
		}
	}
	buffer_putflush(&b, "\n", 1);
	close(fd);
	if (rename(fstatnew, fstat) == -1)
		strerr_warn5("svdir: unable to renmae ", fstatnew, " to ", fstat, ": ",
		    &strerr_sys);

	/* status */
	taia_pack(status, &s->start);
  l = (unsigned long)s->pid;
	status[12] = l; l >>= 8;
	status[13] = l; l >>= 8;
	status[14] = l; l >>= 8;
	status[15] = l;
	status[16] = (s->ctrl & C_PAUSE);
	status[17] = (s->want & W_UP) ? 'u' : 'd';
	status[18] = (s->ctrl & W_UP);
	status[19] = s->state;
	if ((fd = open_trunc(fstatusnew)) == -1) {
		strerr_warn3("svdir: unable to open ", fstatusnew, ": ", &strerr_sys);
		return;
	}
	if ((n = write(fd, status, sizeof status)) == -1) {
		strerr_warn3("svdir: unable to write ", fstatusnew, ": ", &strerr_sys);
		close(fd);
		unlink(fstatusnew);
		return;
	}
	close(fd);
	if (n < (ssize_t)sizeof status) {
		strerr_warn3("svdir: unable to write ", fstatusnew, ": partial write.", 0);
		return;
	}
	if (rename(fstatnew, fstat) == -1)
		strerr_warn5("svdir: unable to renmae ", fstatusnew, " to ", fstatus, ": ",
		    &strerr_sys);
}

uint32_t
custom(sv_t *s, uint8_t c)
{
	int w;
	pid_t pid;
	struct stat st;
	char prog[10];
	char *cargv[2];

	if (s->type == T_LOG) return 0;
	memcpy(prog, "control/?", sizeof prog);
	prog[8] = c;

	if ((stat(prog, &st)) == -1) {
		if (errno == ENOENT) return 0;
		strerr_warn3("svdir: unable to stat ", prog, ": ", &strerr_sys);
	}

	if (!(st.st_mode & S_IXUSR))
		return 0;

	switch ((pid = fork())) {
	case -1:
		strerr_warn3("svdir: fork for ", prog, ": ", &strerr_sys);
		return 0;
	case 0:
		if (s->log && fd_copy(1, s->logpipe[1]) == -1)
			strerr_warn3("svdir: stdout for ", prog, ": ", &strerr_sys);
		cargv[0] = prog;
		cargv[1] = 0;
		execv(prog, cargv);
		strerr_die3sys(111, "svdir: exec svwatch ", prog, ": ");
	}

	while (wait_pid(&w, pid) == -1) {
		if (errno == EINTR) continue;
		strerr_warn3("svdir: unable to wait for ", prog, ": ", &strerr_sys);
		return 0;
	}

	return !wait_exitcode(w);
}

static void
handlectrl(int fd, uint8_t c)
{
	struct svdir *s;
	int sig;

	if (fd == ctrlfd[1])
		switch (c) {
		case '0': finish_arg = "halt"; break;
		case '6': finish_arg = "halt"; term(); return;
		case 'a': /* rescan */ break;
		case 'h': hup(); return;
		default: return;
		}

	s = 0;
	/*
	for (i = 0; i < 500; i++) {
		s = &services[i];
		if (s->ctrlfd[1] == fd)
			break;
	}
	*/
	
	/* service actions */
	switch (c) {
	case 'd':
		s->want = W_DOWN;
		updatesv(s);
		if (s->state == S_RUN) stopsv(s);
		break;
	case 'u':
		s->want = W_UP;
		updatesv(s);
		if (s->state == S_DOWN) stopsv(s);
		break;
	case 'x':
		s->want = W_EXIT;
		updatesv(s);
		if (s->state == S_RUN) stopsv(s);
		break;
	case 't': stopsv(s);
		if (s->state == S_RUN) stopsv(s);
		break;
	case 'o':
		s->want = W_DOWN;
		updatesv(s);
		if (s->state == S_DOWN) startsv(s);
		break;
	}

	/* send signals to running services */

	/* XXX: error warn? */
	if ((s->state & S_RUN) != S_RUN)
		return;

	switch (c) {
	case 'k': sig = SIGKILL; break;
	case 'p': sig = SIGSTOP; break;
	case 'c': sig = SIGCONT; break;
	case 'a': sig = SIGALRM; break;
	case 'h': sig = SIGHUP; break;
	case 'i': sig = SIGINT; break;
	case 'q': sig = SIGQUIT; break;
	case '1': sig = SIGUSR1; break;
	case '2': sig = SIGUSR2; break;
	default: /* XXX: error/warn about unabled ctrl */ return;
	}

	/**/
	if (!custom(s, c))
		kill(s->pid, sig);

	/* update status immediately on specific signals */
	switch (sig) {
	case SIGKILL: s->state |= S_DOWN; break;
	case SIGSTOP: s->ctrl |= C_PAUSE; break;
	case SIGCONT: s->ctrl &= ~C_PAUSE; break;
	default: return; /* status is updated later */
	}

	updatesv(s);
}

int
main(int argc, char **argv)
{
	char opt;
	int once, verbose,

	once = verbose = 0;
	while ((opt = getopt(argc, argv, "ov")) != opteof)
		switch (opt) {
		case 'o': once++; break;
		case 'v': verbose++; break;
		}
	argv =+ optind;
	if (*argv) {
		if (chdir())
	}
		strerr_warn3("svrun: ", fpidnew, ": ", &strerr_sys);

}
