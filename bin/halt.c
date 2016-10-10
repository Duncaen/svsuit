/*
 * To the extent possible under law, Christian Neukirchen has waived
 * all copyright and related or neighboring rights to this work.
 *
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
#include <sys/reboot.h>
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>

#include "sgetopt.h"
#include "error.h"
#include "strerr.h"
#include "str.h"
#include "sig.h"
#include "open.h"


extern char *__progname;

typedef enum {NOOP, HALT, REBOOT, POWEROFF} action_type;

static void
fatal(const char *s1, const char *s2)
{
	strerr_die5sys(1, __progname, ": ", s1, s2, ": ");
}

int
main(int argc, const char *const *argv)
{
	int do_sync = 1;
	int do_force = 0;
	int opt;
	action_type action = NOOP;

	if (str_equal(__progname, "halt"))
		action = HALT;
	else if (str_equal(__progname, "reboot"))
		action = REBOOT;
	else if (str_equal(__progname, "poweroff"))
		action = POWEROFF;
	else
		strerr_warn2(__progname,
		    ": no default behavior, needs to be called as halt/reboot/poweroff", 0);

	while ((opt = getopt(argc, argv, "dfhinw")) != opteof)
		switch (opt) {
			case 'f': do_force = 1; break;
			case 'n': do_sync = 0; break;
			case 'w': do_sync = 0; action = NOOP; break;
			case 'd':
			case 'h':
			case 'i': break;
			/* silently ignored */
			default: strerr_die3x(0, "usage: ", __progname, " [-nf]");
		}

	if (do_sync) sync();
	if (action == NOOP) _exit(0);

	if (do_force)
		switch (action) {
		case HALT: reboot(RB_HALT_SYSTEM); fatal("halt failed", 0);
		case POWEROFF: reboot(RB_POWER_OFF); fatal("poweroff failed", 0);
		case REBOOT: reboot(RB_AUTOBOOT); fatal("reboot failed", 0);
		default: _exit(0);
		}

	if (open_trunc(STOPIT_FILE) == -1)
		fatal("unable to create ", STOPIT_FILE);
	if (chmod(STOPIT_FILE, 0100) == -1)
		fatal("unable to chmod ", STOPIT_FILE);

	if (action == REBOOT && open_trunc(REBOOT_FILE) == -1)
		fatal("unable to create ", REBOOT_FILE);

	if (chmod(REBOOT_FILE, action == REBOOT ? 0100 : 0) == -1
	    && (action == REBOOT && errno != error_noent))
		fatal("unable to chmod ", REBOOT_FILE);

	_exit(0);
}
