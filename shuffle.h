#ifndef SHUFFLE_H
#define SHUFFLE_H

#include <stdlib.h>

static void
shuffle(void *data, size_t n, size_t elem_size) {
	size_t  i, j, k;
	char    buf[256];
	char   *d;

	if (elem_size > sizeof(buf) || n <= 1) return;

	d = (char *) data;
	for (i = 0, k = 0; i < n - 1; i++, k += elem_size) {
		j = i + (unsigned) (rand() / (RAND_MAX / (int)(n-i) + 1));
		j *= elem_size;
		memcpy(buf, &d[j], elem_size);
		memcpy(&d[j], &d[k], elem_size);
		memcpy(&d[k], buf, elem_size);
	}
}

#endif
