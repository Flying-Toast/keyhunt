#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

struct dbent {
	char kind;

	union {
		// kind 'u':
		struct {
			uid_t uid;
			int lvl;
			char *secret;
		} ku;
	};
};

// global variables :-)
static int g_dbfd = -1;
static char *g_dbcontent;
static size_t g_dbsize;
static uid_t g_myuid;
static int g_playdirfd;

static void opendb(void) {
	if (g_dbfd == -1) {
		g_dbfd = MUST(open("db", O_CREAT|O_APPEND|O_RDWR, 0600));
		struct flock lk = {
			.l_type = F_WRLCK,
			.l_whence = SEEK_SET,
			.l_start = 0,
			.l_len = 0,
		};
		if (fcntl(g_dbfd, F_SETLK, &lk) == -1) {
			fputs("Waiting for db lock...\n", stderr);
			MUST(fcntl(g_dbfd, F_SETLKW, &lk));
		}
	} else {
		fputs("dbfd already opened!\n", stderr);
		exit(1);
	}

	struct stat st;
	MUST(fstat(g_dbfd, &st));
	g_dbsize = st.st_size;
	if (g_dbcontent) {
		fputs("dbcontent already allocated!\n", stderr);
		exit(1);
	}
	if (g_dbsize != 0) {
		g_dbcontent = MUST(malloc(g_dbsize));
		read(g_dbfd, g_dbcontent, g_dbsize);
	}
}

static void insertdb(struct dbent *ent) {
	// TODO: g_dbfd should really be FILE*...
	// TODO: this is bad unbuffered like this
	if (ent->kind == 'u') {
		dprintf(g_dbfd, "u%lu", (unsigned long)ent->ku.uid);
		MUST(write(g_dbfd, "\0", 1));

		dprintf(g_dbfd, "%u", ent->ku.lvl);
		MUST(write(g_dbfd, "\0", 1));

		write(g_dbfd, ent->ku.secret, strlen(ent->ku.secret));
		MUST(write(g_dbfd, "\0", 1));

		MUST(write(g_dbfd, "\n", 1));
	} else {
		fprintf(stderr, "Unknown kind '%c' for inserted ent\n", ent->kind);
		exit(1);
	}
}

/*
db format (each line):

	uUID\000LVL\000SECRET_KEY\000\n
	^
	| 'u' = "unlock" event (user started a new level)
*/
static void iter_db(void (*fn)(struct dbent *, void *), void *arg) {
	if (g_dbsize == 0)
		return;
	// points to the last byte of the db contents
	char *end = g_dbcontent + g_dbsize - 1;
	char *cur = g_dbcontent;

#define ADD(CUR, AMNT, END) \
	do { \
		if (((CUR) += (AMNT)) > (END)) { \
			fputs("ADDed past END\n", stderr); \
			exit(1); \
		} \
	} while (0)

	while (cur < end) {
		char evt = *cur;
		ADD(cur, 1, end);
		// after this if block, cur points to the newline at the end of the just-processed line
		if (evt == 'u') { // 'unlock' event
			char *uidstr = cur;
			ADD(cur, 1 + strlen(uidstr), end);

			char *lvlstr = cur;
			ADD(cur, 1 + strlen(lvlstr), end);

			char *keystr = cur;
			ADD(cur, 1 + strlen(keystr), end);

			char *nendptr;
			uid_t uid = MUST(strtoul(uidstr, &nendptr, 10));
			if (*nendptr != '\0') {
				fprintf(stdout, "invalid uid: '%s'\n", uidstr);
				exit(1);
			}
			int lvl = strtoul(lvlstr, &nendptr, 10);
			if (*nendptr != '\0') {
				fprintf(stdout, "invalid lvl: '%s'\n", lvlstr);
				exit(1);
			}
			struct dbent ent;
			ent.kind = 'u';
			ent.ku.uid = uid;
			ent.ku.lvl = lvl;
			ent.ku.secret = keystr;
			(*fn)(&ent, arg);
		} else {
			fprintf(stderr, "Unknown db event '%c'\n", evt);
			exit(1);
		}
		// now cur points to the \n

		// increment past the \n, for the next iteration of the loop
		if (cur <= end && *cur == '\n')
			cur++;
	}
}

struct _numunlocked_iter_arg {
	uid_t uid;
	int n;
};
static void _numunlocked_iter(struct dbent *ent, void *uarg) {
	struct _numunlocked_iter_arg *arg = uarg;
	if (ent->kind == 'u') {
		if (ent->ku.uid == arg->uid)
			arg->n++;
	}
}
static int usr_numunlocked(uid_t uid) {
	struct _numunlocked_iter_arg arg = {
		.uid = uid,
		.n = 0,
	};
	iter_db(_numunlocked_iter, &arg);
	return arg.n;
}

static int usr_is_new(uid_t uid) {
	return usr_numunlocked(uid) == 0;
}

// returned string is only valid until this function is called again
static char *usrnameof(uid_t uid) {
	// TODO: handle deleted users
	return MUST(getpwuid(uid))->pw_name;
}
// see usrnameof() above
static char *myname(void) {
	return usrnameof(g_myuid);
}

int main(void) {
	umask(0072);
	MUST(chdir("/home/simon/dirhunt"));
	// TODO: prevent someone from holding db lock indefinitely
	// TODO: by e.g. suspending runme with ^Z
	opendb();

	mkdir("play", 0705);
	g_playdirfd = MUST(open("play", O_DIRECTORY));

	g_myuid = getuid();

	if (usr_is_new(g_myuid)) {
		mkdirat(g_playdirfd, myname(), 0705);
	} else {
		printf("Welcome back %s!", myname());
	}
}
