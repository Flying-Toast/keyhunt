#include <string.h>
#include <sys/random.h>

#include "util.h"

#define LCALPHA "qwertyuiopasdfghjklzxcvbnm"
#define UCALPHA "QWERTYUIOPASDFGHJKLZXCVBNM"
#define DIGITS "0123456789"

unsigned rand_lt(unsigned lt) {
	char buf[sizeof(unsigned)];
	getrandom(buf, sizeof(buf), 0);
	return *((unsigned *)buf) % lt;
}

static char randchr(char *choices) {
	return choices[rand_lt(strlen(choices))];
}

static void randstr(char *legal, char *buf, size_t len) {
	getrandom(buf, len - 1, 0);
	buf[len - 1] = '\0';
	for (size_t i = 0; i < len - 1; i++)
		buf[i] = legal[buf[i] % strlen(legal)];
}

// random alphanumeric string
void randalnum(char *buf, size_t len) {
	static char legal[] = LCALPHA UCALPHA DIGITS;
	randstr(legal, buf, len);
}

void randalnum_guaranteed_alpha(char *buf, size_t len) {
	randalnum(buf, len);
	buf[rand_lt(len)] = randchr(LCALPHA UCALPHA);
}

void randdigits(char *buf, size_t len) {
	randstr(DIGITS, buf, len);
}
