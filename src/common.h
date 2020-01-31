#include "config.h"

#define CSTRCMP(a, b) c_mem_cmp((a), sizeof((a)), (b))
#define STRCMP(a, b) c_mem_cmp((a), sizeof((a)) - 1, (b))

#define ROPTS C_OREAD
#define RMODE 0
#define WOPTS (C_OCREATE | C_OWRITE)
#define WMODE C_DEFFILEMODE

/* fail routines */
static inline void
efchdir(ctype_fd fd)
{
	if (c_sys_fchdir(fd) < 0)
		c_err_die(1, "c_sys_fchdir");
}

static inline vlong
estrtovl(char *p, int b, vlong l, vlong h)
{
	vlong rv;
	int e;

	rv = c_std_strtovl(p, b, l, h, nil, &e);
	if (e < 0)
		c_err_die(1, "c_std_strtovl %s", p);
	return rv;
}

/* archive routines */
ctype_status archive(char *, char **);
ctype_status archivefd(ctype_fd, char **av);
ctype_status unarchive(char *);
ctype_status unarchivefd(ctype_fd);

/* chksum routines */
void chksumetc(ctype_fd, char *);
ctype_status chksumfile(char *, char *);
void putsize(ctype_hst *, ctype_hmd *);

/* dyn routines */
char **av1make(char *);
char **av3make(char *, char *, char *);
char *estrdup(char *);
ctype_arr *getdbln(ctype_ioq *);
ctype_arr *getln(ctype_ioq *);

/* fs routines */
ctype_status makepath(char *);
ctype_status removepath(char *);

/* utils routines */
void dofetch(char *);
void douncompress(ctype_fd);

/* main routines */
int ar_main(int, char **);
int cksum_main(int, char **);
int venus_main(int, char **);

/* global variables */
extern char *fetch;
extern char *uncompress;
