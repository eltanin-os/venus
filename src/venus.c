#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define LARG(a) \
(!CSTRCMP("files", (a)) ? &pkglfiles  :\
 (!CSTRCMP("mdeps", (a)) ? &pkglmdeps :\
 (!CSTRCMP("rdeps", (a)) ? &pkglrdeps :\
 (usage(), (int (*)(struct package *))nil))))


enum {
	NOTAG,
	FILETAG,
	RDEPTAG,
	MDEPTAG,
};

struct package {
	ctype_ioq *p;
	ctype_arr name;
	ctype_arr version;
	ctype_arr license;
	ctype_arr description;
	ctype_arr size;
	int tag;
	char *path;
};

/* path fd */
ctype_fd fd_cache;
ctype_fd fd_etc;
ctype_fd fd_remote;
ctype_fd fd_root;
ctype_fd fd_chksum;

/* config */
char *arch;
char *extension;
char *fetch;
char *root;
char *uncompress;
char *url;

/* global options */
static int regmode;

/* env routines */
static void
readcfg(void)
{
	ctype_arr *ap;
	ctype_arr arr;
	ctype_ioq ioq;
	ctype_fd fd;
	char buf[C_BIOSIZ];
	char *s;

	if ((s = c_sys_getenv("HOME"))) {
		c_mem_set(&arr, sizeof(arr), 0);
		if (c_dyn_fmt(&arr, "%s/%s", s, LOCALCONF) < 0)
			c_err_die(1, "c_dyn_fmt");
		if ((fd = c_sys_open(c_arr_data(&arr), ROPTS, RMODE)) < 0) {
			if (errno != C_ENOENT)
				c_err_die(1, "c_sys_open %s", c_arr_data(&arr));
			s = nil;
		}
		c_dyn_free(&arr);
	}
	if (!s) {
		if ((fd = c_sys_open(CONFIGFILE, ROPTS, RMODE)) < 0)
			c_err_die(1, "c_sys_open " CONFIGFILE);
	}

	c_ioq_init(&ioq, fd, buf, sizeof(buf), &c_sys_read);
	while ((ap = getln(&ioq))) {
		s = c_arr_data(ap);
		if (*s == '#')
			continue;
		if (!(s = c_str_chr(s, C_USIZEMAX, '=')))
			continue;
		*s++ = 0;
		if (!arch && !CSTRCMP("arch", c_arr_data(ap)))
			arch = estrdup(s);
		else if (!extension && !CSTRCMP("extension", c_arr_data(ap)))
			extension = estrdup(s);
		else if (!fetch && !CSTRCMP("fetch", c_arr_data(ap)))
			fetch = estrdup(s);
		else if (!root && !CSTRCMP("root", c_arr_data(ap)))
			root = estrdup(s);
		else if (!uncompress && !CSTRCMP("uncompress", c_arr_data(ap)))
			uncompress = estrdup(s);
		else if (!url && !CSTRCMP("url", c_arr_data(ap)))
			url = estrdup(s);

	}
	c_sys_close(fd);
}

static void
readenv(void)
{
	char *tmp;

	if ((tmp = c_sys_getenv("VENUS_ARCH")))
		arch = tmp;
	if ((tmp = c_sys_getenv("VENUS_EXTENSION")))
		extension = tmp;
	if ((tmp = c_sys_getenv("VENUS_FETCH")))
		fetch = tmp;
	if ((tmp = c_sys_getenv("VENUS_ROOT")))
		root = tmp;
	if ((tmp = c_sys_getenv("VENUS_UNCOMPRESS")))
		uncompress = tmp;
	if ((tmp = c_sys_getenv("VENUS_URL")))
		url = tmp;
}

static void
startfd(void)
{
	if ((fd_cache = c_sys_open(CACHEDIR, ROPTS, RMODE)) < 0)
		c_err_die(1, "c_sys_open " CACHEDIR);
	if ((fd_chksum = c_sys_open(CHKSUMFILE, ROPTS, RMODE)) < 0)
		c_err_die(1, "c_sys_open " CHKSUMFILE);
	if ((fd_etc = c_sys_open(ETCDIR, ROPTS, RMODE)) < 0)
		c_err_die(1, "c_sys_open " ETCDIR);
	if ((fd_remote = c_sys_open(REMOTEDB, ROPTS, RMODE)) < 0)
		c_err_die(1, "c_sys_open " REMOTEDB);
	if ((fd_root = c_sys_open(root, C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open %s", root);
}

/* pkg db routines */
static void
pkgdata(struct package *pkg)
{
	ctype_arr *ap, *target;
	char *p, *s;

	c_arr_trunc(&pkg->name, 0, sizeof(uchar));
	c_arr_trunc(&pkg->version, 0, sizeof(uchar));
	c_arr_trunc(&pkg->license, 0, sizeof(uchar));
	c_arr_trunc(&pkg->description, 0, sizeof(uchar));
	c_arr_trunc(&pkg->size, 0, sizeof(uchar));

	while ((ap = getln(pkg->p))) {
		s = c_arr_data(ap);
		if (!(p = c_str_chr(s, C_USIZEMAX, ':')))
			continue;
		*p = 0;

		p += 2; /*:<blank>*/
		s += 2; /*#<blank>*/

		if (!CSTRCMP("name", s))
			target = &pkg->name;
		else if (!CSTRCMP("version", s))
			target = &pkg->version;
		else if (!CSTRCMP("license", s))
			target = &pkg->license;
		else if (!CSTRCMP("description", s))
			target = &pkg->description;
		else if (!CSTRCMP("size", s))
			target = &pkg->size;
		else
			continue;

		c_dyn_fmt(target, "%s", p);
		s = c_ioq_peek(pkg->p);
		if (*s != '#')
			break;
	}
}

static char *
pkgtag(struct package *pkg, char *strtag, usize n, int tag)
{
	ctype_arr *ap;
	char *s;

	while ((ap = getln(pkg->p))) {
		s = c_arr_data(ap);
		if (pkg->tag == NOTAG) {
			if (!c_mem_cmp(strtag, n, s))
				pkg->tag = tag;
			continue;
		}
		if (*s != '\t')
			break;
		return ++s;
	}

	pkg->tag = NOTAG;
	return nil;
}

static char *
pkglist(struct package *pkg, char *strtag, usize n, int tag)
{
	char *s, *p;

	if (!(s = pkgtag(pkg, strtag, n, tag)))
		return 0;

	if ((p = c_str_chr(s, C_USIZEMAX, ' ')))
		*p = 0;

	return s;
}

static int
writepkglist(struct package *pkg, char *strtag, usize n, int tag)
{
	char *s;

	if (!(s = pkglist(pkg, strtag, n, tag)))
		return 0;

	c_ioq_fmt(ioq1, "%s", s);
	while ((s = pkglist(pkg, strtag, n, tag)))
		c_ioq_fmt(ioq1, " %s", s);
	c_ioq_put(ioq1, "\n");
	return 0;
}

/* pkg routines */
static int
pkgdel(struct package *pkg)
{
	int r;
	char *s;

	r = 0;
	efchdir(fd_root);
	while ((s = pkglist(pkg, "files:", sizeof("files:"), FILETAG)))
		r |= removepath(s);

	c_sys_unlink(pkg->path);
	return r;
}

static void
doregister(char *src, char *dest)
{
	ctype_ioq ioq;
	ctype_fd fd;

	if (regmode) {
		if ((fd = c_sys_open(dest, WOPTS, WMODE)) < 0)
			c_err_die(1, "c_sys_open %s", dest);
		c_ioq_init(&ioq, fd, nil, 0, &c_sys_write);
		if (c_ioq_putfile(&ioq, src) < 0)
			c_err_die(1, "c_ioq_putfile");
	} else {
		if (c_sys_symlink(src, dest) < 0)
			c_err_die(1, "c_sys_symlink %s %s", src, dest);
	}
}

static int
pkgexplode(struct package *pkg)
{
	ctype_arr arr;
	ctype_fd fd;

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s#%s.v%s",
	    CACHEDIR, c_arr_data(&pkg->name),
	    c_arr_data(&pkg->version), extension) < 0)
		c_err_die(1, "c_dyn_fmt");

	if ((fd = c_sys_open(c_arr_data(&arr), ROPTS, RMODE)) < 0)
		c_err_die(1, "c_sys_open %s", c_arr_data(&arr));

	douncompress(fd_root, fd);
	c_sys_close(fd);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s", LOCALDB, c_arr_data(&pkg->name)) < 0)
		c_err_die(1, "c_dyn_fmt");

	doregister(pkg->path, c_arr_data(&arr));
	c_dyn_free(&arr);
	return 0;
}

static int
pkgfetch(struct package *pkg)
{
	ctype_arr arr;

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s/%s#%s.v%s",
	    url, arch, c_arr_data(&pkg->name),
	    c_arr_data(&pkg->version), extension) < 0)
		c_err_die(1, "c_dyn_fmt");

	dofetch(fd_cache, c_arr_data(&arr));
	chksum_whirlpool(fd_cache, fd_chksum, c_gen_basename(c_arr_data(&arr)));
	c_dyn_free(&arr);
	return 0;
}

static int
pkgadd(struct package *pkg)
{
	return pkgfetch(pkg) || pkgexplode(pkg);
}

static int
pkginfo(struct package *pkg)
{
	c_ioq_fmt(ioq1,
	    "Name: %s\n"
	    "Version: %s\n"
	    "License: %s\n"
	    "Description: %s\n"
	    "Size: %s\n",
	    c_arr_data(&pkg->name), c_arr_data(&pkg->version),
	    c_arr_data(&pkg->license), c_arr_data(&pkg->description),
	    c_arr_data(&pkg->size));
	return 0;
}

static int
pkglfiles(struct package *pkg)
{
	return writepkglist(pkg, "files:", sizeof("files:"), MDEPTAG);
}

static int
pkglmdeps(struct package *pkg)
{
	return writepkglist(pkg, "mdeps:", sizeof("mdeps:"), MDEPTAG);
}

static int
pkglrdeps(struct package *pkg)
{
	return writepkglist(pkg, "rdeps:", sizeof("rdeps:"), RDEPTAG);
}

static int
pkgupdate(struct package *pkg)
{
	ctype_arr arr;
	ctype_fd fd;

	(void)pkg;
	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s/%s", url, arch, RDBNAME) < 0)
		c_err_die(1, "c_dyn_fmt");

	dofetch(fd_cache, c_arr_data(&arr));

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s", CACHEDIR, RDBNAME) < 0)
		c_err_die(1, "c_dyn_fmt");

	if ((fd = c_sys_open(c_arr_data(&arr), ROPTS, RMODE)) < 0)
		c_err_die(1, "c_sys_open %s", c_arr_data(&arr));

	douncompress(fd_remote, fd);
	c_sys_close(fd);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s/%s", url, arch, SNAME) < 0)
		c_err_die(1, "c_dyn_fmt");

	dofetch(fd_etc, c_arr_data(&arr));
	c_dyn_free(&arr);
	return 0;
}

static void
usage(void)
{
	c_ioq_fmt(ioq2,
	    "usage: %s [-HLNR] -adefi [pkg ...]\n"
	    "       %s [-LNR] -l files|mdeps|rdeps [pkg ...]\n"
	    "       %s -u\n",
	    c_std_getprogname(), c_std_getprogname(), c_std_getprogname());
	c_std_exit(1);
}

int
venus_main(int argc, char **argv)
{
	struct package pkg;
	ctype_ioq ioq;
	ctype_arr arr;
	ctype_fd fd;
	int (*func)(struct package *);
	int r;
	char *tmp;
	char *db, *dbflag;
	char buf[C_BIOSIZ];

	c_std_setprogname(argv[0]);

	dbflag = nil;

	C_ARGBEGIN {
	case 'H':
		regmode = 1;
		break;
	case 'L':
		dbflag = LOCALDB;
		break;
	case 'N':
		dbflag = "";
		break;
	case 'R':
		dbflag = REMOTEDB;
		break;
	case 'a':
		db = REMOTEDB;
		func = &pkgadd;
		break;
	case 'd':
		db = LOCALDB;
		func = &pkgdel;
		break;
	case 'e':
		db = REMOTEDB;
		func = &pkgexplode;
		break;
	case 'f':
		db = REMOTEDB;
		func = &pkgfetch;
		break;
	case 'i':
		db = LOCALDB;
		func = &pkginfo;
		break;
	case 'l':
		tmp = C_EARGF(usage());
		func = LARG(tmp);
		db = LOCALDB;
		break;
	case 'u':
		func = &pkgupdate;
		break;
	default:
		usage();
	} C_ARGEND

	readenv();
	readcfg();
	startfd();

	if (func == pkgupdate)
		c_std_exit(func(nil));
	if (dbflag)
		db = dbflag;

	c_mem_set(&arr, sizeof(arr), 0);
	c_mem_set(&pkg, sizeof(pkg), 0);
	pkg.p = &ioq;
	r = 0;

	for (; *argv; ++argv) {
		c_arr_trunc(&arr, 0, sizeof(uchar));
		if (c_dyn_fmt(&arr, "%s/%s", db, *argv) < 0)
			c_err_die(1, "c_dyn_fmt");
		pkg.path = c_arr_data(&arr);
		if ((fd = c_sys_open(pkg.path, ROPTS, RMODE)) < 0) {
			r = c_err_warn("c_sys_open %s", pkg.path);
			continue;
		}
		c_ioq_init(&ioq, fd, buf, sizeof(buf), &c_sys_read);
		pkgdata(&pkg);
		r |= func(&pkg);
		c_sys_close(fd);
	}
	c_ioq_flush(ioq1);
	return 0;
}
