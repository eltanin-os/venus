#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define CACHEDIR "/var/pkg/cache"
#define LOCALDB "/var/pkg/local"
#define REMOTEDB "/var/pkg/remote"
#define ETCFILE "/etc/venus.conf"
#define SUMFILE "/etc/chksum"
#define LOCALCONF ".config/venus.conf"

#define DBFILE "remote.vlz"
#define SMFILE "chksum"

enum {
	NOTAG,
	FILETAG,
	RDEPTAG,
	MDEPTAG,
};

struct package {
	ctype_ioq *fp;
	int tag;
	char *name;
	char *version;
	char *license;
	char *description;
	char *size;
};

static int rfd;

static char *arch;
static char *ext;
static char *root;
static char *url;

static char *fetch;
static char *fflags;
static char *uncompress;
static char *uflags;

/* env routines */
static void
conf_start(void)
{
	ctype_arr *ap;
	ctype_ioq *fp;
	char *s;

	s = concat(c_sys_getenv("HOME"), LOCALCONF);
	if (!(fp = ioq_new(s, C_OREAD, 0))) {
		if (errno != C_ENOENT)
			c_err_die(1, "ioq_new %s", s);
		if (!(fp = ioq_new(ETCFILE, C_OREAD, 0)))
			c_err_die(1, "ioq_new %s", ETCFILE);
	}

	while ((ap = getln(fp))) {
		s = c_arr_data(ap);
		s[c_arr_bytes(ap) - 1] = 0;
		if (*s == '#')
			continue;
		if (!(s = c_str_chr(s, C_USIZEMAX, '=')))
			continue;
		*s++ = 0;
		if (!STRCMP("arch", c_arr_data(ap)))
			assign(&arch, s);
		else if (!STRCMP("extension", c_arr_data(ap)))
			assign(&ext, s);
		else if (!STRCMP("fetch", c_arr_data(ap)))
			assign(&fetch, s);
		else if (!STRCMP("fflags", c_arr_data(ap)))
			assign(&fflags, s);
		else if (!STRCMP("root", c_arr_data(ap)))
			assign(&url, s);
		else if (!STRCMP("uflags", c_arr_data(ap)))
			assign(&uflags, s);
		else if (!STRCMP("uncompress", c_arr_data(ap)))
			assign(&uncompress, s);
		else if (!STRCMP("url", c_arr_data(ap)))
			assign(&url, s);
	}

	c_sys_close(fp->fd);
	c_std_free(fp);
}

/* pkg db routines */
static int
pkgdata(struct package *p)
{
	ctype_arr *ap;
	char *s, *v;

	while ((ap = getln(p->fp))) {
		v = c_arr_data(ap);
		v[c_arr_bytes(ap) - 1] = 0;
		if (*v != '#')
			break;
		if (!(v = c_str_chr(v, C_USIZEMAX, ':')))
			continue;
		*v++ = 0;
		while (*v == ' ')
			*v++ = 0;

		s = c_arr_data(ap);
		++s;
		while (*s == ' ')
			*s++ = 0;

		if (!STRCMP("name", s))
			assign(&p->name, v);
		else if (!STRCMP("version", s))
			assign(&p->version, v);
		else if (!STRCMP("license", s))
			assign(&p->license, v);
		else if (!STRCMP("description", s))
			assign(&p->description, v);
		else if (!STRCMP("size", s))
			assign(&p->size, v);

		s = c_ioq_peek(p->fp);
		if (*s != '#')
			break;
	}

	return 0;
}

static char *
pkgrdeps(struct package *p)
{
	ctype_arr *ap;
	char *s;

	while ((ap = getln(p->fp))) {
		s = c_arr_data(ap);
		s[c_arr_bytes(ap) - 1] = 0;
		if  (p->tag == NOTAG) {
			if (!STRCMP("rundeps:", c_arr_data(ap)))
				p->tag = RDEPTAG;
			continue;
		}
		s = c_arr_data(ap);
		if (*s != '\t')
			break;
		++s;
		return s;
	}

	p->tag = NOTAG;
	return nil;
}

static char *
pkgmdeps(struct package *p)
{
	ctype_arr *ap;
	char *s;

	while ((ap = getln(p->fp))) {
		s = c_arr_data(ap);
		s[c_arr_bytes(ap) - 1] = 0;
		if (p->tag == NOTAG) {
			if (!STRCMP("makedeps:", c_arr_data(ap)))
				p->tag = MDEPTAG;
			continue;
		}
		s = c_arr_data(ap);
		if (*s != '\t')
			break;
		++s;
		return s;
	}

	p->tag = NOTAG;
	return nil;
}

static char *
pkgfiles(struct package *p)
{
	ctype_arr *ap;
	char *s;

	while ((ap = getln(p->fp))) {
		s = c_arr_data(ap);
		s[c_arr_bytes(ap) - 1] = 0;
		if (p->tag == NOTAG) {
			if (!STRCMP("files:", c_arr_data(ap)))
				p->tag = FILETAG;
			continue;
		}
		s = c_arr_data(ap);
		if (*s != '\t')
			break;
		++s;
		return s;
	}

	p->tag = NOTAG;
	return nil;
}

static void
pkgfree(struct package *p)
{
	c_std_free(p->name);
	c_std_free(p->version);
	c_std_free(p->license);
	c_std_free(p->description);
	c_std_free(p->size);
}

/* pkg routines */
static int
pkgadd(struct package *p)
{
	ctype_stat st;
	ctype_arr arr;
	ctype_fssize siz;
	ctype_hst hs;
	usize n;
	ctype_fd fd;
	int rv;
	uint sum;
	char *ssum, *ssiz;
	char *pkg, *path;

	(void)c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s#%s/", CACHEDIR, p->name, p->version) < 0)
		c_err_die(1, "c_dyn_fmt");
	n = c_arr_bytes(&arr);

	rv = 0;
	while ((pkg = pkgfiles(p))) {
		if ((ssum = c_str_chr(pkg, C_USIZEMAX, ' ')))
			*ssum++ = 0;
		if ((ssiz = c_str_chr(ssum, C_USIZEMAX, ' ')))
			*ssiz++ = 0;
		if (c_dyn_fmt(&arr, "%s", pkg) < 0)
			c_err_die(1, "c_dyn_fmt");
		if ((fd = c_sys_open(c_arr_data(&arr), C_OREAD, 0)) < 0) {
			rv = c_err_warn("c_sys_open %s", c_arr_data(&arr));
			goto cont;
		}
		if (c_sys_fstat(&st, fd) < 0) {
			rv = c_err_warn("c_sys_fstat %s", c_arr_data(&arr));
			goto cont;
		}
		if (c_hsh_putfd(&hs, c_hsh_fletcher32, fd, st.size) < 0) {
			rv = c_err_warn("c_hsh_putfd %s", c_arr_data(&arr));
			goto cont;
		}
		sum = c_std_strtovl(ssum, 8, 0, C_UINTMAX, nil, nil);
		if (sum != c_hsh_state0(&hs)) {
			rv = c_err_warnx("%s: sum mismatch", c_arr_data(&arr));
			goto cont;
		}
		siz = c_std_strtovl(ssiz, 8, 0, C_VLONGMAX, nil, nil);
		if (siz != (ctype_fssize)st.size) {
			rv = c_err_warnx("%s: size mismatch", c_arr_data(&arr));
			goto cont;
		}
		path = concat(root, pkg);
		rv |= makepath(path);
		if (c_sys_rename(c_arr_data(&arr), path) < 0) {
			rv = c_err_warn("c_sys_rename %s %s",
			    c_arr_data(&arr), path);
		}
cont:
		if (fd != -1)
			(void)c_sys_close(fd);
		c_arr_trunc(&arr, n, sizeof(uchar));
	}

	c_dyn_free(&arr);

	return rv;
}

static int
pkgdel(struct package *p)
{
	int rv;
	char *pkg, *s;

	while ((pkg = pkgfiles(p))) {
		if ((s = c_str_chr(pkg, C_USIZEMAX, ' ')))
			*s++ = 0;
		s = concat(root, pkg);
		if (c_sys_unlink(s) < 0) {
			rv = c_err_warn("c_sys_unlink %s", s);
			continue;
		}
		rv |= destroypath(s, c_str_len(s, C_USIZEMAX));
	}

	return 0;
}

static int
pkgexplode(struct package *p)
{
	ctype_arr arr;
	ctype_fd fds[2];
	ctype_fd fd;
	char *s;

	(void)c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s#%s.v%s",
	    CACHEDIR, p->name, p->version, ext) < 0)
		c_err_die(1, "c_dyn_fmt");

	if ((fd = c_sys_open(c_arr_data(&arr), C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open %s", c_arr_data(&arr));

	s = c_arr_data(&arr);
	if ((s = c_str_rchr(s, c_arr_bytes(&arr), '.')))
		*s = 0;
	(void)c_sys_mkdir(c_arr_data(&arr), 0755);
	if (c_sys_chdir(c_arr_data(&arr)) < 0)
		c_err_die(1, "c_sys_chdir %s", c_arr_data(&arr));
	c_dyn_free(&arr);

	if (c_sys_pipe(fds) < 0)
		c_err_die(1, "c_sys_pipe");

	switch (c_sys_fork()) {
	case -1:
		c_err_die(1, "c_sys_fork");
	case 0:
		c_sys_dup(fd, 0);
		c_sys_dup(fds[1], 1);
		c_sys_close(fds[0]);
		c_sys_close(fds[1]);

		c_exc_run(uncompress, avmake2(uncompress, uflags));
		c_err_die(1, "c_exc_run %s", uncompress);
	}

	c_sys_close(fds[1]);
	unarchivefd(fds[0]);
	c_sys_close(fds[0]);
	c_sys_close(fd);

	return 0;
}

static int
pkgfetch(struct package *p)
{
	ctype_arr *ap, arr;
	ctype_hst hs;
	ctype_ioq *fp;
	usize n;
	int check;
	char *sum, *siz;

	(void)c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s/%s#%s.v%s",
	    url, arch, p->name, p->version, ext) < 0)
		c_err_die(1, "c_dyn_fmt");

	switch (c_sys_fork()) {
	case -1:
		c_err_die(1, "c_sys_fork");
	case 0:
		(void)c_sys_chdir(CACHEDIR);
		c_exc_run(fetch, avmake3(fetch, fflags, c_arr_data(&arr)));
		c_err_die(1, "c_exc_run %s", fetch);
	}
	(void)c_sys_wait(nil);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s#%s.v%s",
	    CACHEDIR, p->name, p->version, ext) < 0)
		c_err_die(1, "c_dyn_fmt");

	if (c_hsh_putfile(&hs, c_hsh_whirlpool, c_arr_data(&arr)) < 0)
		c_err_die(1, "c_hsh_putfile %s", c_arr_data(&arr));

	c_arr_trunc(&arr, 0, sizeof(uchar));
	(void)c_arr_fmt(&arr, "%s#%s.v%s", p->name, p->version, ext);

	if (!(fp = ioq_new(SUMFILE, C_OREAD, 0)))
		c_err_die(1, "ioq_new %s", SUMFILE);

	check = 0;

	while ((ap = getln(fp))) {
		sum = c_arr_data(ap);
		sum[c_arr_bytes(ap) - 1] = 0;
		if (!(sum = c_str_chr(sum, c_arr_bytes(ap), ' ')))
			c_err_diex(1, "%s: wrong format", SUMFILE);
		*sum++ = 0;
		if (c_str_cmp(c_arr_data(ap), C_USIZEMAX, c_arr_data(&arr)))
			continue;
		++check;
		if (!(siz = c_str_chr(sum, c_arr_bytes(ap), ' ')))
			c_err_diex(1, "%s: wrong format", SUMFILE);
		*siz++ = 0;
		n = c_std_strtovl(siz, 8, 0, C_USIZEMAX, nil, nil);
		if (n != hs.len)
			check = c_err_warnx("%s: size mismatch",
			    c_arr_data(&arr));
		if (check_sum(&hs, sum) < 0)
			check = c_err_warnx("%s: checksum mismatch",
			    c_arr_data(&arr));
		break;
	}

	if (!check)
		check = c_err_warnx("%s: have no checksum", c_arr_data(&arr));

	if (check < 0)
		(void)c_sys_unlink(c_arr_data(&arr));

	c_dyn_free(&arr);

	return 0;
}

static int
pkginfo(struct package *p)
{
	c_ioq_fmt(ioq1,
	    "Name: %s\n"
	    "Version: %s\n"
	    "License: %s\n"
	    "Description: %s\n"
	    "Size: %s\n",
	    p->name, p->version, p->license,
	    p->description, p->size);
	return 0;
}

static int
pkglfiles(struct package *p)
{
	char *pkg, *s;

	if (!(pkg = pkgfiles(p)))
		return 0;

	if ((s = c_str_chr(pkg, C_USIZEMAX, '\t')))
		*s++ = 0;

	(void)c_ioq_fmt(ioq1, "%s", pkg);

	while ((pkg = pkgfiles(p))) {
		if ((s = c_str_chr(pkg, C_USIZEMAX, '\t')))
			*s++ = 0;
		(void)c_ioq_fmt(ioq1, " %s", pkg);
	}

	(void)c_ioq_put(ioq1, "\n");

	return 0;
}

static int
pkglmdeps(struct package *p)
{
	char *pkg, *s;

	if (!(pkg = pkgmdeps(p)))
		return 0;

	if ((s = c_str_chr(pkg, C_USIZEMAX, '\t')))
		*s++ = 0;

	c_ioq_fmt(ioq1, "%s", pkg);

	while ((pkg = pkgmdeps(p))) {
		if ((s = c_str_chr(pkg, C_USIZEMAX, '\t')))
			*s++ = 0;
		c_ioq_fmt(ioq1, " %s", pkg);
	}

	(void)c_ioq_put(ioq1, "\n");

	return 0;
}

static int
pkgradd(struct package *p)
{
	return pkgfetch(p) || pkgexplode(p) || pkgadd(p);
}

static int
pkglrdeps(struct package *p)
{
	char *pkg, *s;

	if (!(pkg = pkgrdeps(p)))
		return 0;

	if ((s = c_str_chr(pkg, C_USIZEMAX, '\t')))
		*s++ = 0;

	(void)c_ioq_fmt(ioq1, "%s", pkg);

	while ((pkg = pkgrdeps(p))) {
		if ((s = c_str_chr(pkg, C_USIZEMAX, '\t')))
			*s++ = 0;
		(void)c_ioq_fmt(ioq1, " %s", pkg);
	}

	(void)c_ioq_put(ioq1, "\n");

	return 0;
}

static int
pkgupdate(struct package *p)
{
	ctype_arr arr;
	ctype_fd fds[2];
	ctype_fd fd;

	(void)p;
	(void)c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s", url, DBFILE) < 0)
		c_err_die(1, "c_dyn_fmt");

	switch (c_sys_fork()) {
	case -1:
		c_err_die(1, "c_sys_fork");
	case 0:
		(void)c_sys_chdir(CACHEDIR);
		c_exc_run(fetch, avmake3(fetch, fflags, c_arr_data(&arr)));
		c_err_die(1, "c_exc_run %s", fetch);
	}
	(void)c_sys_wait(nil);

	c_arr_trunc(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s", CACHEDIR, DBFILE) < 0)
		c_err_die(1, "c_dyn_fmt");

	if ((fd = c_sys_open(c_arr_data(&arr), C_OCREATE | C_OWRITE, 0)) < 0)
		c_err_die(1, "c_sys_open %s", c_arr_data(&arr));

	if (c_sys_pipe(fds) < 0)
		c_err_die(1, "c_sys_pipe");

	switch (c_sys_fork()) {
	case -1:
		c_err_die(1, "c_sys_fork");
	case 0:
		c_sys_dup(fd, 0);
		c_sys_dup(fds[1], 1);
		c_sys_close(fds[0]);
		c_sys_close(fds[1]);

		c_exc_run(uncompress, avmake2(uncompress, uflags));
		c_err_die(1, "c_exc_run %s", uncompress);
	}

	c_sys_close(fds[1]);
	c_sys_chdir(REMOTEDB);
	unarchivefd(fds[0]);
	c_sys_fchdir(rfd);
	c_sys_close(fds[0]);
	c_sys_close(fd);

	c_arr_trunc(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s", url, SMFILE) < 0)
		c_err_die(1, "c_dyn_fmt");

	switch (c_sys_fork()) {
	case -1:
		c_err_die(1, "c_sys_fork");
	case 0:
		(void)c_sys_chdir(CACHEDIR);
		c_exc_run(fetch, avmake3(fetch, fflags, c_arr_data(&arr)));
		c_err_die(1, "c_exc_run %s", fetch);
	}

	return 0;
}

/* main routines */
static void
usage(void)
{
	c_ioq_fmt(ioq2,
	    "usage: %s [-NLR] -Aadefi [pkg ...]\n"
	    "       %s [-NLR] -l files|mdeps|rdeps [pkg ...]\n"
	    "       %s -u\n",
	    c_std_getprogname(), c_std_getprogname(), c_std_getprogname());
	c_std_exit(1);
}

int
main(int argc, char **argv)
{
	struct package pkg;
	char *db, *tdb;
	char *tmp;
	int (*fn)(struct package *);
	int rv;

	c_std_setprogname(argv[0]);
	tdb = nil;
	fn = nil;

	C_ARGBEGIN {
	case 'A':
		db = REMOTEDB;
		fn = pkgradd;
		break;
	case 'N':
		tdb = "";
		break;
	case 'L':
		tdb = LOCALDB;
		break;
	case 'R':
		tdb = REMOTEDB;
		break;
	case 'a':
		db = REMOTEDB;
		fn = pkgadd;
		break;
	case 'd':
		db = LOCALDB;
		fn = pkgdel;
		break;
	case 'e':
		db = REMOTEDB;
		fn = pkgexplode;
		break;
	case 'f':
		db = REMOTEDB;
		fn = pkgfetch;
		break;
	case 'i':
		db = LOCALDB;
		fn = pkginfo;
		break;
	case 'l':
		db = LOCALDB;
		tmp = C_EARGF(usage());
		if (!c_mem_cmp("files", sizeof("files"), tmp))
			fn = pkglfiles;
		else if (!c_mem_cmp("mdeps", sizeof("mdeps"), tmp))
			fn = pkglmdeps;
		else if (!c_mem_cmp("rdeps", sizeof("rdeps"), tmp))
			fn = pkglrdeps;
		else
			usage();
		break;
	case 'u':
		fn = pkgupdate;
		break;
	default:
		usage();
	} C_ARGEND

	if ((tmp = c_sys_getenv("VENUS_ARCH")))
		arch = tmp;

	if ((tmp = c_sys_getenv("VENUS_EXTENSION")))
		ext = tmp;

	if ((tmp = c_sys_getenv("VENUS_FETCH")))
		fetch = tmp;

	if ((tmp = c_sys_getenv("VENUS_FFLAGS")))
		fflags = tmp;

	if ((tmp = c_sys_getenv("VENUS_ROOT")))
		root = tmp;

	if ((tmp = c_sys_getenv("VENUS_UFLAGS")))
		uflags = tmp;

	if ((tmp = c_sys_getenv("VENUS_UNCOMPRESS")))
		uncompress = tmp;

	if ((tmp = c_sys_getenv("VENUS_URL")))
		url = tmp;

	(void)c_sys_umask(0);
	conf_start();

	if (!fn)
		usage();
	if (fn == pkgupdate)
		c_std_exit(fn(nil));

	db = tdb ? tdb : db;
	(void)c_mem_set(&pkg, sizeof(pkg), 0);

	if ((rfd = c_sys_open(".", C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open <dot>");

	for (; *argv; --argc, ++argv) {
		tmp = concat(db, *argv);
		if (!(pkg.fp = ioq_new(tmp, C_OREAD, 0))) {
			rv = c_err_warn("ioq_new %s", tmp);
			continue;
		}
		(void)pkgdata(&pkg);
		rv |= fn(&pkg);
		pkgfree(&pkg);
		c_sys_close(pkg.fp->fd);
		c_std_free(pkg.fp);
		if (c_sys_fchdir(rfd) < 0)
			c_err_die(1, "c_sys_fchdir");
	}

	c_sys_close(rfd);
	c_ioq_flush(ioq1);

	return 0;
}
