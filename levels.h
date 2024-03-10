#ifndef __HAVE_LEVELS_H
#define __HAVE_LEVELS_H

// return value is the secret key
typedef char *(*lvl_impl_t)(int readmefd, int filesdir, unsigned lvlno);

char *lvlimpl_onboarding(int readmefd, int filesdir, unsigned lvlno);
char *lvlimpl_digitline(int readmefd, int filesdir, unsigned lvlno);
char *lvlimpl_fixedkeylinelen(int readmefd, int filesdir, unsigned lvlno);
char *lvlimpl_longestline(int readmefd, int filesdir, unsigned lvlno);

#endif
