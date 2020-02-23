#include "config.h"

#define CSTRCMP(a, b) c_mem_cmp((a), sizeof((a)), (b))
#define STRCMP(a, b) c_mem_cmp((a), sizeof((a)) - 1, (b))

#define ROPTS C_OREAD
#define RMODE 0
#define WOPTS (C_OCREATE | C_OWRITE)
#define WMODE C_DEFFILEMODE

struct cfg {
	ctype_arr arr;
	ctype_ioq ioq;
	ctype_fd fd;
	int tag;
	char buf[C_BIOSIZ];
};

/* archive routines */
ctype_status archive(char *, char **);
ctype_status archivefd(ctype_fd, char **av);
ctype_status unarchive(char *);
ctype_status unarchivefd(ctype_fd);

/* chksum routines */
void chksumetc(ctype_fd, char *);
ctype_status chksumfile(char *, char *);
void putsize(ctype_hst *, ctype_hmd *);

/* conf routines */
void cfginit(struct cfg *, ctype_fd);
char *cfgfind(struct cfg *, char *);
char *cfgfindtag(struct cfg *, char *);
void cfgclose(struct cfg *);

/* fs routines */
ctype_status makepath(char *);
ctype_status removepath(char *);

/* utils routines */
void dofetch(char *);
void douncompress(char *);

/* main routines */
ctype_status ar_main(int, char **);
ctype_status cksum_main(int, char **);
ctype_status venus_main(int, char **);

/* global variables */
extern char *fetch;
extern char *uncompress;

/* fail inline routies */
static inline char *
ecfgfind(struct cfg *cfg, char *k)
{
	char *s;

	if ((s = cfgfind(cfg, k)) == (void *)-1)
		c_err_die(1, "cfgfind");
	if (!s)
		c_err_diex(1, "cfgfind %s: key not found", k);
	return s;
}

static inline char *
ecfgfindtag(struct cfg *cfg, char *k)
{
	char *s;

	if ((s = cfgfindtag(cfg, k)) == (void *)-1)
		c_err_die(1, "cfgfindtag");
	if (!s)
		c_err_diex(1, "cfgfindtag %s: key not found", k);
	return s;
}

static inline void
efchdir(ctype_fd fd)
{
	if (c_sys_fchdir(fd) < 0)
		c_err_die(1, "c_sys_fchdir");
}

static inline ctype_fd
eopen(char *s, uint opts, uint mode)
{
	ctype_fd fd;

	if ((fd = c_sys_open(s, opts, mode)) < 0)
		c_err_die(1, "c_sys_open %s", s);
	return fd;
}

static inline char *
estrdup(char *s)
{
	char *p;

	if (!(p = c_str_dup(s, C_USIZEMAX)))
		c_err_die(1, "c_str_dup");
	return p;
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
