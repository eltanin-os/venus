#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define CMP(a, b, c) (!(a) && !CSTRCMP((b), (c)))

#define LARG(a) \
(!CSTRCMP("files", (a)) ? &pkglfiles  :\
 (!CSTRCMP("mdeps", (a)) ? &pkglmdeps :\
 (!CSTRCMP("rdeps", (a)) ? &pkglrdeps :\
 (usage(), (int (*)(struct package *))nil))))

struct package {
	struct cfg cfg;
	ctype_arr name;
	ctype_arr version;
	ctype_arr license;
	ctype_arr description;
	ctype_arr size;
	char *path;
};

/* path fd */
ctype_fd fd_cache;
ctype_fd fd_dot;
ctype_fd fd_etc;
ctype_fd fd_remote;
ctype_fd fd_root;
ctype_fd fd_chksum;

/* config */
char *arch;
char *fetch;
char *root;
char *safeurl;
char *uncompress;
char *url;

/* global options */
static int regmode = 1;

/* env routines */
static void
readcfg(void)
{
	struct cfg cfg;
	ctype_arr arr;
	ctype_fd fd;
	char *s;

	fd = -1;
	if ((s = c_std_getenv("HOME"))) {
		c_mem_set(&arr, sizeof(arr), 0);
		if (c_dyn_fmt(&arr, "%s/%s", s, LOCALCONF) < 0)
			c_err_die(1, "c_dyn_fmt");
		if ((fd = c_sys_open(c_arr_data(&arr), ROPTS, RMODE)) < 0) {
			if (errno != C_ENOENT)
				c_err_die(1, "c_sys_open %s", c_arr_data(&arr));
		}
		c_dyn_free(&arr);
	}
	if (fd == -1)
		fd = eopen(CONFIGFILE, ROPTS, RMODE);

	cfginit(&cfg, fd);
	arch = estrdup(ecfgfind(&cfg, "arch"));
	fetch = estrdup(ecfgfind(&cfg, "fetch"));
	root = epathdup(ecfgfind(&cfg, "root"));
	safeurl = estrdup(ecfgfind(&cfg, "safeurl"));
	uncompress = estrdup(ecfgfind(&cfg, "uncompress"));
	url = estrdup(ecfgfind(&cfg, "url"));
	cfgclose(&cfg);
	c_sys_close(fd);
}

static void
startfd(void)
{
	fd_dot = eopen(".", ROPTS, RMODE);
	fd_root = eopen(root, C_OREAD, 0);
	efchdir(fd_root);
	fd_cache = eopen(CACHEDIR, ROPTS, RMODE);
	fd_chksum = eopen(CHKSUMFILE, ROPTS | C_OCREATE, RMODE);
	fd_etc = eopen(ETCDIR, ROPTS, RMODE);
	fd_remote = eopen(REMOTEDB, ROPTS, RMODE);
}

/* pkg db routines */
static void
pkgdata(struct package *pkg)
{
	c_arr_trunc(&pkg->name, 0, sizeof(uchar));
	c_dyn_fmt(&pkg->name, "%s", cfgfind(&pkg->cfg, "name"));
	c_arr_trunc(&pkg->version, 0, sizeof(uchar));
	c_dyn_fmt(&pkg->version, "%s", cfgfind(&pkg->cfg, "version"));
	c_arr_trunc(&pkg->license, 0, sizeof(uchar));
	c_dyn_fmt(&pkg->license, "%s", cfgfind(&pkg->cfg, "license"));
	c_arr_trunc(&pkg->description, 0, sizeof(uchar));
	c_dyn_fmt(&pkg->description, "%s", cfgfind(&pkg->cfg, "description"));
	c_arr_trunc(&pkg->size, 0, sizeof(uchar));
	c_dyn_fmt(&pkg->size, "%s", cfgfind(&pkg->cfg, "size"));
}

static char *
pkglist(struct package *pkg, char *key)
{
	char *s, *p;

	if (!(s = cfgfindtag(&pkg->cfg, key)))
		return 0;
	if ((p = c_str_chr(s, C_USIZEMAX, ' ')))
		*p = 0;
	return s;
}

static void
writepkglist(struct package *pkg, char *key)
{
	char *s;

	if (!(s = pkglist(pkg, key)))
		return;

	c_ioq_fmt(ioq1, "%s", s);
	while ((s = pkglist(pkg, key)))
		c_ioq_fmt(ioq1, " %s", s);
	c_ioq_put(ioq1, "\n");
}

/* pkg routines */
static ctype_status
pkgcheck(struct package *pkg)
{
	ctype_status r;
	char *p, *s;

	r = 0;
	efchdir(fd_root);
	while ((s = ecfgfindtag(&pkg->cfg, "files{"))) {
		if (!(p = c_str_chr(s, C_USIZEMAX, ' ')))
			c_err_diex(1, "%s: malformed database file", pkg->path);
		*p++ = 0;
		if (chksumfile(s, p) < 0)
			r = c_err_warnx("%s: checksum mismatch", s);
	}
	return r;
}

static ctype_status
pkgdel(struct package *pkg)
{
	ctype_status r;
	char *s;

	r = 0;
	efchdir(fd_root);
	while ((s = pkglist(pkg, "files{")))
		r |= removepath(s);

	efchdir(fd_dot);
	c_sys_unlink(pkg->path);
	return r;
}

static void
doregister(char *src, char *dest)
{
	ctype_ioq ioq;
	ctype_fd fd;

	if (regmode) {
		fd = eopen(dest, WOPTS, WMODE);
		c_ioq_init(&ioq, fd, nil, 0, &c_sys_write);
		if (c_ioq_putfile(&ioq, src) < 0)
			c_err_die(1, "c_ioq_putfile");
	} else {
		if (c_sys_symlink(src, dest) < 0)
			c_err_die(1, "c_sys_symlink %s %s", src, dest);
	}
}

static ctype_status
pkgexplode(struct package *pkg)
{
	ctype_arr arr;

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s%s/%s#%s.v%s",
	    root, CACHEDIR, c_arr_data(&pkg->name),
	    c_arr_data(&pkg->version), EXTCOMP) < 0)
		c_err_die(1, "c_dyn_fmt");

	efchdir(fd_root);
	douncompress(c_arr_data(&arr));

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s%s/%s", root,
	    LOCALDB, c_arr_data(&pkg->name)) < 0)
		c_err_die(1, "c_dyn_fmt");

	efchdir(fd_dot);
	doregister(pkg->path, c_arr_data(&arr));
	c_dyn_free(&arr);
	return 0;
}

static ctype_status
pkgfetch(struct package *pkg)
{
	ctype_arr arr;

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s/%s#%s.v%s",
	    url, arch, c_arr_data(&pkg->name),
	    c_arr_data(&pkg->version), EXTCOMP) < 0)
		c_err_die(1, "c_dyn_fmt");

	efchdir(fd_cache);
	dofetch(c_arr_data(&arr));
	chksumetc(fd_chksum, c_gen_basename(c_arr_data(&arr)));
	c_dyn_free(&arr);
	return 0;
}

static ctype_status
pkgadd(struct package *pkg)
{
	return pkgfetch(pkg) || pkgexplode(pkg);
}

static ctype_status
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

static ctype_status
pkglfiles(struct package *pkg)
{
	writepkglist(pkg, "files{");
	return 0;
}

static ctype_status
pkglmdeps(struct package *pkg)
{
	writepkglist(pkg, "makedeps{");
	return 0;
}

static ctype_status
pkglrdeps(struct package *pkg)
{
	writepkglist(pkg, "rdeps{");
	return 0;
}

static ctype_status
pkgupdate(struct package *pkg)
{
	ctype_arr arr;

	(void)pkg;
	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s/%s", safeurl, arch, RDBNAME) < 0)
		c_err_die(1, "c_dyn_fmt");

	efchdir(fd_cache);
	dofetch(c_arr_data(&arr));

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s%s/%s", root, CACHEDIR, RDBNAME) < 0)
		c_err_die(1, "c_dyn_fmt");

	efchdir(fd_remote);
	douncompress(c_arr_data(&arr));

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s/%s", safeurl, arch, SNAME) < 0)
		c_err_die(1, "c_dyn_fmt");

	efchdir(fd_etc);
	dofetch(c_arr_data(&arr));
	c_dyn_free(&arr);
	return 0;
}

static void
usage(void)
{
	c_ioq_fmt(ioq2,
	    "usage: %s [-HLNR] -acdefi [pkg ...]\n"
	    "       %s [-LNR] -l files|mdeps|rdeps [pkg ...]\n"
	    "       %s -u\n",
	    c_std_getprogname(), c_std_getprogname(), c_std_getprogname());
	c_std_exit(1);
}

ctype_status
venus_main(int argc, char **argv)
{
	struct package pkg;
	ctype_arr arr;
	ctype_fd fd;
	ctype_status (*func)(struct package *);
	ctype_status r;
	char *tmp;
	char *db, *dbflag;

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	dbflag = nil;

	while (c_std_getopt(argmain, argc, argv, "LNRSacdefil:u")) {
		switch (argmain->opt) {
		case 'L':
			dbflag = LOCALDB;
			break;
		case 'N':
			dbflag = "";
			break;
		case 'R':
			dbflag = REMOTEDB;
			break;
		case 'S':
			regmode = 0;
			break;
		case 'a':
			db = REMOTEDB;
			func = &pkgadd;
			break;
		case 'c':
			db = LOCALDB;
			func = &pkgcheck;
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
			tmp = argmain->arg;
			func = LARG(tmp);
			db = LOCALDB;
			break;
		case 'u':
			func = &pkgupdate;
			break;
		default:
			usage();
		}
	}
	argc -= argmain->idx;
	argv += argmain->idx;

	if (!func)
		usage();

	readcfg();
	startfd();
	if (func == pkgupdate)
		c_std_exit(func(nil));
	if (dbflag)
		db = dbflag;

	r = 0;
	c_mem_set(&arr, sizeof(arr), 0);
	c_mem_set(&pkg, sizeof(pkg), 0);
	for (; *argv; ++argv) {
		c_arr_trunc(&arr, 0, sizeof(uchar));
		if (c_dyn_fmt(&arr, "%s%s%s", *db ? root : "", db, *argv) < 0)
			c_err_die(1, "c_dyn_fmt");
		pkg.path = c_arr_data(&arr);
		if ((fd = c_sys_open(pkg.path, ROPTS, RMODE)) < 0) {
			r = c_err_warn("c_sys_open %s", pkg.path);
			continue;
		}
		cfginit(&pkg.cfg, fd);
		pkgdata(&pkg);
		r |= func(&pkg);
		c_sys_close(fd);
	}
	c_ioq_flush(ioq1);
	return r;
}
