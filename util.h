#ifndef __HAVE_UTIL_H
#define __HAVE_UTIL_H

#include <errno.h>
#include <stdio.h>

#define ARRAY_LEN(ARR) (sizeof(ARR) / sizeof(ARR[0]))

#define _STRINGIFY1(X) #X
#define _STRINGIFY2(X) _STRINGIFY1(X)
#define MUST(CALL) \
	({ \
		errno = 0; \
		typeof(CALL) __ret = CALL; \
		if (errno) { \
			perror("MUST(" __FILE__ ":" _STRINGIFY2(__LINE__) ")"); \
			exit(1); \
		} \
		__ret; \
	})


void randstr(char *buf, size_t len);

#endif
