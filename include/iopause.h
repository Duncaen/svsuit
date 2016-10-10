/* Public domain. */

#ifndef IOPAUSE_H
#define IOPAUSE_H

#ifdef IOPAUSE_POLL
#include <sys/types.h>
#include <poll.h>

typedef struct pollfd iopause_fd;
#define IOPAUSE_READ POLLIN
#define IOPAUSE_WRITE POLLOUT
#endif /* IOPAUSE_POLL */

#include "taia.h"

extern void iopause(iopause_fd *, size_t, struct taia *, struct taia *);

#endif
