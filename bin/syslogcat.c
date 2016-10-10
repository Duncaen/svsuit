#include <sys/klog.h>

#include "buffer.h"
#include "strerr.h"

static char buf[16384];

int
main()
{
	int len;
	for (;;) {
		if ((len = klogctl(2, buf, sizeof buf)) == -1)
			strerr_die1sys(111, "klogcat: klogctl: ");
		buffer_putflush(buffer_2, buf, len);
	}
}
