/* Public domain. */

#include "byte.h"

void
byte_copyr(void *dest, size_t n, const void *src)
{
	uint8_t *to = dest;
	const uint8_t *from = src;

	to += n;
	from += n;

	for (;;) {
		if (!n) return;
		*--to = *--from;
		--n;

		if (!n) return;
		*--to = *--from;
		--n;

		if (!n) return;
		*--to = *--from;
		--n;

		if (!n) return;
		*--to = *--from;
		--n;
	}
}
