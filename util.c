#include <string.h>
#include <sys/random.h>

#include "util.h"

// random alphanumeric string
void randstr(char *buf, size_t len) {
	static char legal[] = "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890";
	getrandom(buf, len - 1, 0);
	buf[len - 1] = '\0';
	for (size_t i = 0; i < len - 1; i++)
		buf[i] = legal[buf[i] % strlen(legal)];
}
