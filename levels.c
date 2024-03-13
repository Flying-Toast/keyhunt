#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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
		"You can also pipe in your key instead of giving it as an argument:\n"
		"    echo SECRET_KEY | runme claim\n\n"
		"This first level is easy; your secret key has been written to a file in the files/"
		" directory. Use `cd`, `ls`, and `cat` to find it, and then claim the key using the"
		" `runme` program as described above."
		"\n"
	);

	static char secret[15];
	randalnum(secret, ARRAY_LEN(secret));

	int thefile = MUST(openat(filesdir, "secret", O_CREAT|O_WRONLY, 0644));
	secret[sizeof(secret)-1] = '\n';
	write(thefile, secret, ARRAY_LEN(secret));
	secret[sizeof(secret)-1] = '\0';
	close(thefile);

	return secret;
}

char *lvlimpl_digitline(int readmefd, int filesdir, unsigned lvlno) {
	dprintf(readmefd,
		"Inspect the contents of `files/lines`."
		" One line in that file that contains only"
		" digits. Find that line - it is your secret key."
		"\n"
	);

	int listfile = MUST(openat(filesdir, "lines", O_CREAT|O_WRONLY, 0644));
	int nbefore = rand_between(100, 500);
	int nafter = rand_between(100, 500);
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

char *lvlimpl_fixedkeylinelen(int readmefd, int filesdir, unsigned lvlno) {
#define MAXLINESIZE 75
	unsigned secretsize = rand_between(25, MAXLINESIZE);
	dprintf(readmefd,
		"A file called 'lines' has been created in the files/ directory."
		" There is one line in that file that is exactly %u"
		" characters long. That line is your secret key."
		"\n"
		, secretsize-1
	);

	int listfile = MUST(openat(filesdir, "lines", O_CREAT|O_WRONLY, 0644));

	int nbefore = rand_between(100, 500);
	int nafter = rand_between(100, 500);

	char buf[MAXLINESIZE];
	for (int i = 0; i < nbefore; i++) {
		int sz = rand_between(25, MAXLINESIZE);
		if (sz == secretsize)
			sz--;
		randalnum(buf, sz);
		buf[sz-1] = '\n';
		write(listfile, buf, sz);
	}
	static char secret[MAXLINESIZE] = {0};
	randalnum(secret, secretsize + 1);
	secret[secretsize-1] = '\n';
	write(listfile, secret, strlen(secret));
	for (int i = 0; i < nafter; i++) {
		int sz = rand_between(25, MAXLINESIZE);
		if (sz == secretsize)
			sz--;
		randalnum(buf, sz);
		buf[sz-1] = '\n';
		write(listfile, buf, sz);
	}

	close(listfile);

	secret[secretsize-1] = '\0';
	return secret;
#undef MAXLINESIZE
}

char *lvlimpl_longestline(int readmefd, int filesdir, unsigned lvlno) {
#define LINEBUFSIZE 250
	dprintf(readmefd,
		"Examine the lines of the file files/lines."
		" Your secret key is the longest line in that file."
		"\n"
	);

	// secretsize = num chars excl NUL
	int secretsize = rand_between(100, LINEBUFSIZE);
	static char secret[LINEBUFSIZE] = {0};
	randalnum(secret, secretsize+1);
	char buf[LINEBUFSIZE];

	int listfile = MUST(openat(filesdir, "lines", O_CREAT|O_WRONLY, 0644));

	int nbefore = rand_between(500, 1000);
	int nafter = rand_between(500, 1000);
	for (int i = 0; i < nafter; i++) {
		int nch = rand_between(10, secretsize);
		randalnum(buf, nch+1);
		buf[nch] = '\n';
		write(listfile, buf, nch+1);
	}
	secret[secretsize] = '\n';
	write(listfile, secret, secretsize+1);
	secret[secretsize] = '\0';
	for (int i = 0; i < nbefore; i++) {
		int nch = rand_between(10, secretsize);
		randalnum(buf, nch+1);
		buf[nch] = '\n';
		write(listfile, buf, nch+1);
	}

	close(listfile);
	return secret;
#undef LINEBUFSIZE
}

char *lvlimpl_mostrecentfile(int readmefd, int filesdir, unsigned lvlno) {
#define NAMEBUFSIZE 16
	unsigned nfiles = rand_between(100, 200);
	static char namebuf[NAMEBUFSIZE];

	time_t now = time(NULL);
	for (int i = 0; i < nfiles; i++) {
		randalnum(namebuf, NAMEBUFSIZE);
		// pfft, what are the chances we make the same
		// random name twice? let's assume it won't happen.
		int fd = MUST(openat(filesdir, namebuf, O_CREAT|O_EXCL|O_WRONLY, 0644));
		// dont change the secret file, let it keep the current time
		if (i != nfiles - 1) {
			unsigned sec_offset = rand_between(60*2, 60*60*48);
			struct timespec ts[2];
			ts[0].tv_nsec = UTIME_OMIT;
			ts[1].tv_nsec = 0;
			ts[1].tv_sec = now - sec_offset;
			MUST(futimens(fd, ts));
		}
		close(fd);
	}

	dprintf(readmefd,
		"%u empty files have been created in the files/ directory."
		" Find the file with the most recent modification time. The"
		" name of that file is your secret key."
		"\n"
		, nfiles
	);
	return namebuf;
#undef NAMEBUFSIZE
}

char *lvlimpl_concatposns(int readmefd, int filesdir, unsigned lvlno) {
	unsigned nlines = rand_between(50, 75);
	char *secret = malloc(nlines + 1);
	randalnum(secret, nlines + 1);
	int listfile = MUST(openat(filesdir, "lines", O_CREAT|O_WRONLY, 0644));

	char *linebuf = malloc(nlines + 1);
	for (int i = 0; i < nlines; i++) {
		randalnum(linebuf, nlines + 1);
		linebuf[nlines] = '\n';
		linebuf[i] = secret[i];
		write(listfile, linebuf, nlines + 1);
	}
	free(linebuf);
	close(listfile);

	dprintf(readmefd,
		"%u lines of equal length have been written to 'files/lines'. Concatenate"
		" the first character of the first line, the 2nd character of"
		" the 2nd line, the 3rd character of the 3rd line, etc."
		" This should leave you with a string that is %u characters long."
		" That string is your secret key."
		"\n"
		, nlines
		, nlines
	);
	return secret;
}

char *lvlimpl_evenline(int readmefd, int filesdir, unsigned lvlno) {
#define MAXLINECHARS 100
	static char secret[MAXLINECHARS + 1];
	int seclen = rand_between(6, MAXLINECHARS);
	if (seclen & 1)
		seclen--;
	randalnum(secret, seclen + 1);
	secret[seclen] = '\n';
	int nbefore = rand_between(100, 200);
	int nafter = rand_between(100, 200);
	int listfile = MUST(openat(filesdir, "lines", O_CREAT|O_WRONLY, 0644));
	char buf[MAXLINECHARS];
	while (nbefore--) {
		int sz = rand_between(6, MAXLINECHARS);
		if (!(sz & 1))
			sz--;
		randalnum(buf, sz + 1);
		buf[sz] = '\n';
		write(listfile, buf, sz + 1);
	}
	write(listfile, secret, seclen + 1);
	while (nafter--) {
		int sz = rand_between(6, MAXLINECHARS);
		if (!(sz & 1))
			sz--;
		randalnum(buf, sz + 1);
		buf[sz] = '\n';
		write(listfile, buf, sz + 1);
	}
	dprintf(readmefd,
		"There is a single line in `files/lines` that is of even length. Find that line."
		"\n"
	);
	secret[seclen] = '\0';
	return secret;
#undef MAXLINECHARS
}

char *lvlimpl_filenamesuffix(int readmefd, int filesdir, unsigned lvlno) {
	#define NAMELEN 10
	int nbefore = rand_between(100, 150);
	int nafter = rand_between(100, 150);
	static char secret[NAMELEN+1];
	randalnum(secret, NAMELEN+1);
	secret[NAMELEN-1] = 'c';
	secret[NAMELEN-2] = 'b';
	secret[NAMELEN-3] = 'a';
	char buf[NAMELEN+1];
	while (--nbefore) {
		randalnum(buf, NAMELEN+1);
		if (buf[NAMELEN-1]=='c'&&buf[NAMELEN-2]=='b'&&buf[NAMELEN-3]=='a')
			buf[NAMELEN-1] = 'z';
		int fd = openat(filesdir, buf, O_CREAT|O_RDWR|O_EXCL, 0644);
		close(fd);
	}
	MUST(openat(filesdir, secret, O_CREAT|O_RDWR|O_EXCL, 0644));
	while (--nafter) {
		randalnum(buf, NAMELEN+1);
		if (buf[NAMELEN-1]=='c'&&buf[NAMELEN-2]=='b'&&buf[NAMELEN-3]=='a')
			buf[NAMELEN-1] = 'z';
		int fd = openat(filesdir, buf, O_CREAT|O_RDWR|O_EXCL, 0644);
		close(fd);
	}
	dprintf(readmefd,
		"Several files have been created in the files/ directory. Exactly ONE of those"
		" files has a filename that ends with \"abc\". That filename is your secret key."
		"\n(hint: try to do this using only `ls`)"
		"\n"
	);
	return secret;
}
