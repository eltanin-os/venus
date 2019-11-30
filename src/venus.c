#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"
#include "config.h"

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

/* dirs */
static ctype_fd fd_cache;
static ctype_fd fd_etc;
static ctype_fd fd_remote;
static ctype_fd fd_root;
static ctype_fd fd_chksum;

static char *arch;
static char *ext;
static char *root;
static char *url;

static int hardreg;

ctype_fd fd_dot;
char *fetch;
char *inflate;

/* env routines */
static void
conf_start(void)
{
	ctype_arr *ap;
	ctype_fd fd;
	ctype_ioq *fp;
	char *s;

	if (!(s = c_sys_getenv("HOME")))
		goto open_configfile;

	s = concat(s, LOCALCONF);
	if ((fd = c_sys_open(s, C_OREAD, 0)) < 0) {
		if (errno != C_ENOENT)
			c_err_die(1, "c_sys_open %s", s);
open_configfile:
		if ((fd = c_sys_open(CONFIGFILE, C_OREAD, 0)) < 0)
			c_err_die(1, "c_sys_open " CONFIGFILE);
	}

	if (!(fp = c_ioq_new(C_BIOSIZ, fd, c_sys_read)))
		c_err_die(1, "c_ioq_new");

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
		else if (!STRCMP("root", c_arr_data(ap)))
			assign(&root, s);
		else if (!STRCMP("uncompress", c_arr_data(ap)))
			assign(&inflate, s);
		else if (!STRCMP("url", c_arr_data(ap)))
			assign(&url, s);
	}

	c_std_free(fp);
	c_sys_close(fd);
}

/* pkg db routines */
static void
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
}

static char *
pkgtag(struct package *p, char *strtag, int tag)
{
	ctype_arr *ap;
	usize n;
	char *s;

	while ((ap = getln(p->fp))) {
		s = c_arr_data(ap);
		n = c_arr_bytes(ap) - 1;
		s[n] = 0;
		if (p->tag == NOTAG) {
			if (!c_str_cmp(strtag, n, c_arr_data(ap)))
				p->tag = tag;
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

static int
pkglist(struct package *p, char *strtag, int tag)
{
	char *pkg, *s;

	if (!(pkg = pkgtag(p, strtag, tag)))
		return 0;

	if ((s = c_str_chr(pkg, C_USIZEMAX, ' ')))
		*s++ = 0;

	c_ioq_fmt(ioq1, "%s", pkg);

	while ((pkg = pkgtag(p, s, tag))) {
		if ((s = c_str_chr(pkg, C_USIZEMAX, ' ')))
			*s++ = 0;
		c_ioq_fmt(ioq1, " %s", pkg);
	}

	c_ioq_put(ioq1, "\n");

	return 0;
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
pkgdel(struct package *p)
{
	ctype_arr arr;
	int rv;
	char *pkg, *s;

	while ((pkg = pkgtag(p, "files:", FILETAG))) {
		if ((s = c_str_chr(pkg, C_USIZEMAX, ' ')))
			*s++ = 0;
		s = concat(root, pkg);
		if (c_sys_unlink(s) < 0) {
			rv = c_err_warn("c_sys_unlink %s", s);
			continue;
		}
		rv |= destroypath(s, c_str_len(s, C_USIZEMAX));
	}

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s", LOCALDB, p->name) < 0)
		c_err_die(1, "c_dyn_fmt");

	c_sys_unlink(c_arr_data(&arr));
	c_dyn_free(&arr);
	return rv;
}

static int
pkgexplode(struct package *p)
{
	ctype_arr arr, dest;
	ctype_ioq *fp;
	ctype_fd fd;

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s#%s.v%s",
	    CACHEDIR, p->name, p->version, ext) < 0)
		c_err_die(1, "c_dyn_fmt");

	if ((fd = c_sys_open(c_arr_data(&arr), C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open %s", c_arr_data(&arr));

	uncompress(fd_root, fd);
	c_sys_close(fd);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s", REMOTEDB, p->name) < 0)
		c_err_die(1, "c_dyn_fmt");

	c_mem_set(&dest, sizeof(dest), 0);
	if (c_dyn_fmt(&dest, "%s/%s", LOCALDB, p->name) < 0)
		c_err_die(1, "c_dyn_fmt");

	if (hardreg) {
		if ((fd = c_sys_open(c_arr_data(&dest),
		     C_OCREATE | C_OWRITE, C_DEFFILEMODE)) < 0)
			c_err_die(1, "c_sys_open %s", c_arr_data(&dest));
		if (!(fp = c_ioq_new(C_BIOSIZ, fd, c_sys_write)))
			c_err_die(1, "c_ioq_new");
		if (c_ioq_putfile(fp, c_arr_data(&arr)) < 0)
			c_err_die(1, "c_ioq_putfile");
		c_std_free(fp);
		c_sys_close(fd);
	} else {
		if (c_sys_symlink(c_arr_data(&arr), c_arr_data(&dest)) < 0)
			c_err_die(1, "c_sys_symlink %s %s",
			    c_arr_data(&arr), c_arr_data(&dest));
	}

	c_dyn_free(&arr);
	return 0;
}

static int
pkgfetch(struct package *p)
{
	ctype_arr arr;

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s/%s#%s.v%s",
	    url, arch, p->name, p->version, ext) < 0)
		c_err_die(1, "c_dyn_fmt");

	dofetch(fd_cache, c_arr_data(&arr));
	c_sys_seek(fd_chksum, 0, SEEK_SET);
	efchdir(fd_cache);
	checksum_whirlpool(fd_chksum, c_gen_basename(c_arr_data(&arr)));
	efchdir(fd_dot);

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
pkginstall(struct package *p)
{
	return pkgfetch(p) || pkgexplode(p);
}

static int
pkglfiles(struct package *p)
{
	return pkglist(p, "files:", FILETAG);
}

static int
pkglmdeps(struct package *p)
{
	return pkglist(p, "makedeps:", MDEPTAG);
}

static int
pkglrdeps(struct package *p)
{
	return pkglist(p, "rundeps:", RDEPTAG);
}

static int
pkgupdate(struct package *p)
{
	ctype_arr arr;
	ctype_fd fd;

	(void)p;
	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/%s/%s", url, arch, RDBNAME) < 0)
		c_err_die(1, "c_dyn_fmt");

	dofetch(fd_cache, c_arr_data(&arr));

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s", CACHEDIR, RDBNAME) < 0)
		c_err_die(1, "c_dyn_fmt");

	if ((fd = c_sys_open(c_arr_data(&arr), C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open %s", c_arr_data(&arr));

	uncompress(fd_remote, fd);
	c_sys_close(fd);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s/%s", url, arch, SNAME) < 0)
		c_err_die(1, "c_dyn_fmt");

	dofetch(fd_etc, c_arr_data(&arr));

	c_dyn_free(&arr);
	return 0;
}

/* main routines */
static void
usage(void)
{
	c_ioq_fmt(ioq2,
	    "usage: %s [-HNLR] -Aadefi [pkg ...]\n"
	    "       %s [-NLR] -l files|mdeps|rdeps [pkg ...]\n"
	    "       %s -u\n",
	    c_std_getprogname(), c_std_getprogname(), c_std_getprogname());
	c_std_exit(1);
}

int
venus_main(int argc, char **argv)
{
	struct package pkg;
	ctype_fd fd;
	ctype_ioq ioq;
	int (*fn)(struct package *);
	int rv;
	char *db, *tdb, *tmp;
	char buf[C_BIOSIZ];

	c_std_setprogname(argv[0]);
	tdb = nil;
	fn = nil;

	C_ARGBEGIN {
	case 'H':
		hardreg = 1;
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
		fn = pkginstall;
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

	if ((tmp = c_sys_getenv("VENUS_ROOT")))
		root = tmp;

	if ((tmp = c_sys_getenv("VENUS_UNCOMPRESS")))
		inflate = tmp;

	if ((tmp = c_sys_getenv("VENUS_URL")))
		url = tmp;

	conf_start();

	if (!fn)
		usage();

	if ((fd_cache = c_sys_open(CACHEDIR, C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open " CACHEDIR);

	if ((fd_etc = c_sys_open(ETCDIR, C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open " ETCDIR);

	if ((fd_remote = c_sys_open(REMOTEDB, C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open " REMOTEDB);

	if ((fd_dot = c_sys_open(".", C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open <dot>");

	if (fn == pkgupdate)
		c_std_exit(fn(nil));

	if ((fd_chksum = c_sys_open(CHKSUMFILE, C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open " CHKSUMFILE);

	if ((fd_root = c_sys_open(root, C_OREAD, 0)) < 0)
		c_err_die(1, "c_sys_open %s", root);

	if (tdb)
		db = tdb;

	c_mem_set(&pkg, sizeof(pkg), 0);
	pkg.fp = &ioq;

	for (; *argv; --argc, ++argv) {
		tmp = concat(db, *argv);
		if ((fd = c_sys_open(tmp, C_OREAD, 0)) < 0) {
			rv = c_err_warn("c_sys_open %s", tmp);
			continue;
		}
		c_ioq_init(&ioq, fd, buf, sizeof(buf), c_sys_read);

		pkgdata(&pkg);
		rv |= fn(&pkg);

		pkgfree(&pkg);
		pkg.tag = 0;
		c_sys_close(fd);
	}

	c_ioq_flush(ioq1);

	return 0;
}
