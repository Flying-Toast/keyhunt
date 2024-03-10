#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "levels.h"
#include "util.h"

#define ESCBOLD "\e[1m"
#define ESCCLEAR "\e[m"
#define BOLD(STR) ESCBOLD STR ESCCLEAR

struct dbent {
	char kind;

	union {
		// kind 'u':
		struct {
			uid_t uid;
			unsigned lvl;
			char *secret;
		} ku;

		// kind 'c':
		struct {
			uid_t uid;
			unsigned lvl;
		} kc;
	};
};

// global variables :-)
static int g_dbfd = -1;
static char *g_dbcontent;
static size_t g_dbsize;
static uid_t g_myuid;
static int g_playerdir;

// NOTE: new levels *must* be added to the end,
// otherwise it will bump people's most recently completed
// level and they will end up having to redo it.
static lvl_impl_t levelimpls[] = {
	lvlimpl_onboarding,
	lvlimpl_fixedkeylinelen,
	lvlimpl_digitline,
	lvlimpl_longestline,
};

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
		lseek(g_dbfd, 0, SEEK_SET);
	}

	struct stat st;
	MUST(fstat(g_dbfd, &st));
	g_dbsize = st.st_size;
	if (g_dbcontent)
		free(g_dbcontent);
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
	} else if (ent->kind == 'c') {
		dprintf(g_dbfd, "c%lu", (unsigned long)ent->kc.uid);
		MUST(write(g_dbfd, "\0", 1));

		dprintf(g_dbfd, "%u", ent->kc.lvl);
		MUST(write(g_dbfd, "\0", 1));

		MUST(write(g_dbfd, "\n", 1));
	} else {
		fprintf(stderr, "Unknown kind '%c' for inserted ent\n", ent->kind);
		exit(1);
	}

	opendb();
}

/*
db format (each line):

	uUID\000LVL\000SECRET_KEY\000\n
	^
	| 'u' = "unlock" event (user started a new level)

	cUID\000LVL\000\n
	^
	| 'c' = "completed" event (level passed)
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
		struct dbent ent;
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
			unsigned lvl = strtoul(lvlstr, &nendptr, 10);
			if (*nendptr != '\0') {
				fprintf(stdout, "invalid lvl: '%s'\n", lvlstr);
				exit(1);
			}
			ent.kind = 'u';
			ent.ku.uid = uid;
			ent.ku.lvl = lvl;
			ent.ku.secret = keystr;
		} else if (evt == 'c') { // 'completed' event
			char *uidstr = cur;
			ADD(cur, 1 + strlen(uidstr), end);

			char *lvlstr = cur;
			ADD(cur, 1 + strlen(lvlstr), end);

			char *nendptr;
			uid_t uid = MUST(strtoul(uidstr, &nendptr, 10));
			if (*nendptr != '\0') {
				fprintf(stdout, "invalid uid: '%s'\n", uidstr);
				exit(1);
			}
			unsigned lvl = strtoul(lvlstr, &nendptr, 10);
			if (*nendptr != '\0') {
				fprintf(stdout, "invalid lvl: '%s'\n", lvlstr);
				exit(1);
			}

			ent.kind = 'c';
			ent.kc.uid = uid;
			ent.kc.lvl = lvl;
		} else {
			fprintf(stderr, "Unknown db event '%c'\n", evt);
			exit(1);
		}
		(*fn)(&ent, arg);
		// now cur points to the \n

		// increment past the \n, for the next iteration of the loop
		if (cur <= end && *cur == '\n')
			cur++;
	}
}

struct _numcnt_iter_arg {
	uid_t uid;
	int n;
};
static void _numunlocked_iter(struct dbent *ent, void *uarg) {
	struct _numcnt_iter_arg *arg = uarg;
	if (ent->kind == 'u') {
		if (ent->ku.uid == arg->uid)
			arg->n++;
	}
}
static int usr_numunlocked(uid_t uid) {
	struct _numcnt_iter_arg arg = {
		.uid = uid,
		.n = 0,
	};
	iter_db(_numunlocked_iter, &arg);
	return arg.n;
}

struct _get_levelent_arg {
	uid_t uid;
	unsigned lvl;
	char *foundkey;
};
static void _get_levelent_iter(struct dbent *ent, void *uarg) {
	struct _get_levelent_arg *arg = uarg;
	if (ent->kind == 'u') {
		if (ent->ku.uid == arg->uid && ent->ku.lvl == arg->lvl)
			arg->foundkey = ent->ku.secret;
	}
}
static char *get_secret(uid_t uid, unsigned lvl) {
	struct _get_levelent_arg arg = {
		.uid = uid,
		.lvl = lvl,
		.foundkey = NULL,
	};
	iter_db(_get_levelent_iter, &arg);
	return arg.foundkey;
}

static int usr_curlevel(uid_t uid) {
	return usr_numunlocked(uid);
}

static int usr_is_new(uid_t uid) {
	return usr_numunlocked(uid) == 0;
}

static void _numcomplete_iter(struct dbent *ent, void *uarg) {
	struct _numcnt_iter_arg *arg = uarg;
	if (ent->kind == 'c') {
		if (ent->ku.uid == arg->uid)
			arg->n++;
	}
}
static int usr_numcomplete(uid_t uid) {
	struct _numcnt_iter_arg arg = {
		.uid = uid,
		.n = 0,
	};
	iter_db(_numcomplete_iter, &arg);
	return arg.n;
}
static int usr_won(uid_t uid) {
	return usr_numcomplete(uid) == ARRAY_LEN(levelimpls);
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

static void printent_iter(struct dbent *ent, void *_unused) {
	if (ent->kind == 'u') {
		printf(
			"unlocked:\n"
			"\tuid: %lu (%s)\n"
			"\tlvl: %u\n"
			"\tsecret: %s\n"
			, (unsigned long)ent->ku.uid
			, usrnameof(ent->ku.uid)
			, ent->ku.lvl
			, ent->ku.secret
		);
	} else if (ent->kind == 'c') {
		printf(
			"completed:\n"
			"\tuid: %lu (%s)\n"
			"\tlvl: %u\n"
			, (unsigned long)ent->ku.uid
			, usrnameof(ent->ku.uid)
			, ent->ku.lvl
		);
	} else {
		fprintf(stderr, "Unknown dbent kind '%c'\n", ent->kind);
		exit(1);
	}
}

// is the user currently in a level?
static int usr_has_inprogress(uid_t uid) {
	return usr_numcomplete(uid) != usr_numunlocked(uid);
}

static void rmfiles(int dirfd) {
	DIR *dirent = MUST(fdopendir(MUST(dup(dirfd))));
	struct dirent *i;
	while ((i = MUST(readdir(dirent))) != NULL) {
		struct stat st;
		MUST(fstatat(dirfd, i->d_name, &st, 0));
		if (S_ISREG(st.st_mode))
			MUST(unlinkat(dirfd, i->d_name, 0));
	}
	MUST(closedir(dirent));
}

static void activate_level(int playerdir, unsigned lvlno, uid_t playeruid) {
	char pathbuf[100] = {0};
	int nwritten = snprintf(pathbuf, sizeof(pathbuf), "README.lvl-%u", lvlno);
	if (nwritten >= sizeof(pathbuf)) {
		fputs("pathbuf overflow :(\n", stderr);
		exit(1);
	}

	// clear existing files in player dir
	mkdirat(playerdir, "files", 0755); // allowed to fail if it already exists
	int filesdir = MUST(openat(playerdir, "files", O_DIRECTORY));
	rmfiles(playerdir);
	rmfiles(filesdir);

	struct dbent newlvl;
	newlvl.kind = 'u';
	newlvl.ku.uid = playeruid;
	newlvl.ku.lvl = lvlno;
	int lvlidx = lvlno - 1;
	if (lvlidx >= ARRAY_LEN(levelimpls)) {
		fprintf(stderr, "Tried to activate out-of-bounds level %u.\n", lvlno);
		exit(1);
	}
	int readmefd = MUST(openat(playerdir, pathbuf, O_CREAT|O_EXCL|O_WRONLY, 0644));
	newlvl.ku.secret = (*levelimpls[lvlno - 1])(readmefd, filesdir, lvlno);
	insertdb(&newlvl);
}

void tryclaim(uid_t puid, char *trycode) {
	if (!usr_has_inprogress(puid)) {
		puts("You have nothing to claim right now...");
		return;
	}

	unsigned curlvl = usr_curlevel(puid);
	char *realcode = get_secret(puid, curlvl);
	if (realcode == NULL) {
		fputs("secret not in db\n", stderr);
		exit(1);
	}

	if (!strcmp(realcode, trycode)) {
		struct dbent completed;
		completed.kind = 'c';
		completed.kc.uid = puid;
		completed.kc.lvl = curlvl;
		insertdb(&completed);

		if (usr_won(puid)) {
			// TODO: A leaderboard would be cool
			printf("You win! You completed all %u levels. (More levels coming soon...)\n", curlvl);
		} else {
			activate_level(g_playerdir, curlvl + 1, puid);
			printf(
				"Congrats! You passed level %u. The next level"
				" has now been started in your play/ directory."
				" see the README for more information.\n"
				, curlvl
			);
		}
	} else {
		puts("Hmmm, that doesn't look like the correct key.");
	}
}

int main(int argc, char **argv) {
	umask(0022);
	MUST(chdir("/home/simon/keyhunt"));
	// TODO: prevent someone from holding db lock indefinitely
	// TODO: by e.g. suspending runme with ^Z
	opendb();

	// database dump
	if (argc == 2 && !strcmp(argv[1], "db") && geteuid() == getuid()) {
		iter_db(printent_iter, NULL);
		return 0;
	}

	int isclaim;
	char *claimcode;
	if (argc == 3 && !strcmp(argv[1], "claim")) {
		isclaim = 1;
		claimcode = argv[2];
	} else if (argc != 1) {
		fputs("Incorrect usage.\n", stderr);
		exit(1);
	}

	mkdir("play", 0755);
	int gamedirfd = MUST(open("play", O_DIRECTORY));

	g_myuid = getuid();

	mkdirat(gamedirfd, myname(), 0755);
	g_playerdir = MUST(openat(gamedirfd, myname(), O_DIRECTORY));

	if (usr_is_new(g_myuid)) {
		activate_level(g_playerdir, 1, g_myuid);
		printf(
			"== Hello %s! ==\n"
			"Welcome to keyhunt - a puzzle game designed to help you exercise"
			" your scripting skills.\nA personal instance of the game has just been"
			" started for you. To begin, run this command:\n"
			BOLD("cd play/%s") "\nand take a look at the file named README.lvl-1\n"
			, myname()
			, myname()
		);
	} else if (isclaim) {
		tryclaim(g_myuid, claimcode);
	} else if (usr_has_inprogress(g_myuid)) {
		printf(
			"Welcome back %s. cd into play/%s to continue where you left off."
			" when you find the secret key, come back and run this program again"
			" like this:\n"
			"    ./runme claim PUT_SECRET_KEY_HERE\n"
			, myname()
			, myname()
		);
	} else if (!usr_won(g_myuid)) {
		unsigned newlvl = usr_numunlocked(g_myuid) + 1;
		activate_level(g_playerdir, newlvl, g_myuid);
		printf("Welcome back! Level %u has been started in your play/ directory.\n", newlvl);
	} else {
		puts("You've finished all the levels so far! More levels are coming soon...");
	}
}
