#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "levels.h"
#include "util.h"

char *lvlimpl_onboarding(int readmefd, int filesdir, unsigned lvlno) {
	dprintf(readmefd,
		"Welcome to keyhunt!\nThe game consists of a series"
		" of levels that will get progressively harder as you advance."
		"\n\nTo pass each level, you must find the level's \"secret key\""
		" - a randomized string of text that has been obfuscated/hidden"
		" somewhere in the filesystem. Each level has a README (like the"
		" one you are reading now). The README will tell you how"
		" the secret key has been hidden, but it is up to you to actually"
		" find it.\n\nWhen you find the key, execute the"
		" `runme` program (the program you just used to start the game) like this:\n"
		"    runme claim PUT_SECRET_KEY_HERE\n\n"
		"This first level is easy; your secret key has been written to a file in the files/"
		" directory. Use `cd`, `ls`, and `cat` to find it, and then claim the key using the"
		" `runme` program as described above."
		"\n"
	);

	static char secret[15];
	randalnum(secret, ARRAY_LEN(secret));

	int thefile = MUST(openat(filesdir, "hello", O_CREAT|O_WRONLY, 0644));
	secret[sizeof(secret)-1] = '\n';
	write(thefile, secret, ARRAY_LEN(secret));
	secret[sizeof(secret)-1] = '\0';
	close(thefile);

	return secret;
}

char *lvlimpl_digitline(int readmefd, int filesdir, unsigned lvlno) {
	dprintf(readmefd,
		"A file called 'list' has been created in the files/ directory."
		" There is exactly one line in that file that contains only"
		" digits. Find that line - it is your secret key."
		"\n"
	);

	int listfile = MUST(openat(filesdir, "list", O_CREAT|O_WRONLY, 0644));
	int nbefore = rand_lt(500);
	int nafter = rand_lt(500);
	char buf[25];
	static char secret[25];
	for (int i = 0; i < nbefore; i++) {
		randalnum_guaranteed_alpha(buf, sizeof(buf));
		buf[sizeof(buf)-1] = '\n';
		write(listfile, buf, sizeof(buf));
	}
	randdigits(secret, sizeof(secret));
	secret[sizeof(secret)-1] = '\n';
	write(listfile, secret, sizeof(secret));
	secret[sizeof(secret)-1] = '\0';
	for (int i = 0; i < nafter; i++) {
		randalnum_guaranteed_alpha(buf, sizeof(buf));
		buf[sizeof(buf)-1] = '\n';
		write(listfile, buf, sizeof(buf));
	}
	close(listfile);

	return secret;
}