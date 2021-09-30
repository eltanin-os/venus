#include <tertium/cpu.h>
#include <tertium/std.h>

#define STRCAT(a, b) c_dyn_cat((a), (b), sizeof((b)) - 1, sizeof(uchar))
#define STRALLOC(a) \
{ \
if (!(a = getkey(#a))) c_err_diex(1, "\"" #a "\" not found in \"%s\"", file); \
a = c_str_dup((a), -1); \
}

static int yflag;

static char *file;
static char *conf;
static char *dbdir;
static char *dbflag;
static char *tag;

static char *arch;
static char *fetch;
static char *root;
static char *url;
static char *uncompress;
static char *safeurl;

static usize columns = 80;

/* spawn routines */
static char *
getlines(ctype_fd fd) {
	static ctype_arr arr; /* "memory leak" */
	ctype_ioq ioq;
	ctype_status r;
	char buf[C_SMALLBIOSIZ];

	c_ioq_init(&ioq, fd, buf, sizeof(buf), &c_nix_fdread);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	while ((r = c_ioq_getln(&ioq, &arr)) > 0) ;
	c_arr_trunc(&arr, c_arr_bytes(&arr) - 1, sizeof(uchar));

	if (r < 0)
		c_err_die(1, "c_ioq_getln");

	return c_arr_data(&arr);
}

static char *
spawn_getlines(char **args) {
	ctype_fd fd;
	ctype_id id;
	char *s;

	if (!(id = c_exc_spawn1(*args, args, environ, &fd, 1)))
		c_err_die(1, "c_exc_spawn1 %s", *args);
	s = getlines(fd);
	c_nix_waitpid(id, nil, 0);
	c_nix_fdclose(fd);
	return (s && *s) ? s : nil;
}

static char **
split_lines(char *s) {
	static ctype_arr arr; /* "memory leak" */

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_cat(&arr, &s, 1, sizeof(char *)) < 0)
		c_err_die(1, "c_dyn_cat");
	for (; *s; ++s) {
		if (*s == '\n') {
			*s++ = 0;
			if (c_dyn_cat(&arr, &s, 1, sizeof(char *)) < 0)
				c_err_die(1, "c_dyn_cat");
		}
	}
	return c_arr_data(&arr);
}

/* helper routines */
static char *
getpath(char *s)
{
	static ctype_arr arr; /* "memory leak" */
	ctype_stat st;
	char *path;

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s/%s", dbdir, dbflag, s) < 0)
		c_err_die(1, "c_dyn_fmt");

	path = c_arr_data(&arr);
	if (c_nix_stat(&st, path) < 0) {
		if (errno != C_ENOENT)
			c_err_die(1, "c_nix_stat %s", path);
		c_err_diex(1, "package \"%s\" not found in \"%s\"", s, dbflag);
	}
	return path;
}

static void
chdir_tmpdir(char *s, usize n)
{
	ctype_fd fd;

	if ((fd = c_nix_mktemp(s, n, C_OTMPDIR)) < 0)
		c_err_die(1, "c_nix_mktemp %s", s);
	if (c_nix_fdchdir(fd) < 0)
		c_err_die(1, "c_nix_fdchdir");
	c_nix_fdclose(fd);
}

static int
yesno(void)
{
	int ans;
	char ch;

	c_ioq_get(ioq0, &ch, 1);
	ans = (ch | 32) != 'y';
	while (ch != '\n') c_ioq_get(ioq0, &ch, 1);
	return ans;
}

/* conf routines */
static char *
getkey(char *k)
{
	char *args[] = { "venus-conf", k, file, nil };
	return spawn_getlines(args);
}

static char *
gettag(char *k)
{
	char *args[] = { "venus-conf", "-t", k, file, nil };
	return spawn_getlines(args);
}

static char *
cut1(char *s)
{
	char *tmp = c_str_chr(s, -1, ' ');
	*tmp = 0;
	return s;
}

static char *
getwsum(char *s)
{
	char *args[] = { "venus-cksum", "-w", s, nil };
	return spawn_getlines(args);
}

static char *
getsum(char *s)
{
	char *args[] = { "venus-cksum", s, nil };
	return spawn_getlines(args);
}

static void
start(void)
{
	ctype_arr arr;
	usize len;
	char *home;

	if (!(c_sys_geteuid())) {
		conf = "/etc/venus.cfg";
		dbdir = "/var/pkg";
	} else {
		if (!(home = c_std_getenv("XDG_CONFIG_HOME"))) {
			if (!(home = c_std_getenv("HOME")))
				c_err_diex(1, "missing HOME env variable");
			c_mem_set(&arr, sizeof(arr), 0);
			if (c_dyn_fmt(&arr, "%s/.config", home) < 0)
				c_err_die(1, "c_dyn_fmt");
			c_dyn_shrink(&arr);
			home = c_arr_data(&arr); /* "memory leak" */
		}
		c_mem_set(&arr, sizeof(arr), 0);
		if (c_dyn_fmt(&arr, "%s/venus/config%c", home, 0) < 0)
			c_err_die(1, "c_dyn_fmt");
		len = c_arr_bytes(&arr);
		if (c_dyn_fmt(&arr, "%s/venus/db", home) < 0)
			c_err_die(1, "c_dyn_fmt");
		c_dyn_shrink(&arr);
		conf = c_arr_data(&arr); /* "memory leak" */
		dbdir = conf + len;
	}

	file = conf;
	STRALLOC(arch); /* "memory leak" */
	STRALLOC(fetch); /* "memory leak" */
	STRALLOC(root); /* "memory leak" */
	STRALLOC(url); /* "memory leak" */
	STRALLOC(safeurl); /* "memory leak" */
	STRALLOC(uncompress); /* "memory leak" */
}

/* do routines */
static char *
urlencode(char *url)
{
	static ctype_arr arr; /* "memory leak" */

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_ready(&arr, c_str_len(url, -1), sizeof(uchar)) < 0)
		c_err_die(1, "c_dyn_ready");
	while (*url) {
		switch (*url) {
		case '#':
			if (STRCAT(&arr, "%23") < 0)
				c_err_die(1, "c_dyn_cat");
			break;
		default:
			if (c_dyn_cat(&arr, url, 1, sizeof(uchar)) < 0)
				c_err_die(1, "c_dyn_cat");
		}
		++url;
	}
	return c_arr_data(&arr);
}

static void
dofetch(char *s, char *u)
{
	ctype_id id;
	char **av;

	if (!(av = c_exc_arglist(fetch, s, urlencode(u))))
		c_err_die(1, "c_std_vtoptr");
	if (!(id = c_exc_spawn0(*av, av, environ)))
		c_err_die(1, "c_exc_spawn0 %s", *av);
	c_std_free(av);
	c_nix_waitpid(id, nil, 0);

}

static void
douncompress(char *s)
{
	ctype_id id;
	ctype_fd fd;
	char *args[] = { "venus-ar", "-x", nil };
	char **av;

	if ((id = c_nix_fork()) < 0)
		c_err_die(1, "c_nix_fork");
	if (!id) {
		if (!(av = c_exc_arglist(uncompress, s)))
			c_err_die(1, "c_std_arglist");
		if (!c_exc_spawn1(*av, av, environ, &fd, 1))
			c_err_die(1, "c_exc_spawn1 %s", *av);
		c_std_free(av);
		c_nix_fdmove(C_FD0, fd);
		if (!(id = c_exc_spawn0(*args, args, environ)))
			c_err_die(1, "c_exc_spawn0 %s", *args);
		c_nix_waitpid(id, nil, 0);
		c_std_exit(0);
	}
	c_nix_waitpid(-1, nil, 0);
}

static ctype_status
dochecksum(char *s)
{
	static ctype_arr arr; /* "memory leak" */
	char *f1, *f2, *p;

	if (!(f1 = getwsum(s)))
		return c_err_warnx("failed to obtain sum from \"%s\"", s);
	f1 = cut1(f1);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/%s/cache/chksum", dbdir, dbflag) < 0)
		c_err_die(1, "c_dyn_fmt");

	file = c_arr_data(&arr);
	if (!(f2 = getkey((p = c_gen_basename(s)))))
		return c_err_warnx("\"%s\" not found in \"%s/cache/chksum\"",
		    p, dbflag);

	if (c_str_cmp(f2, -1, f1)) {
		c_err_warnx("checksum mismatch \"%s\"", s);
		c_nix_unlink(s);
		return 1;
	}
	return 0;
}

/* dep routines */
static int
xcheck(ctype_node *p, char *s)
{
	if (!p)
		return 1;
	p = p->next;
	do {
		if (!c_str_cmp(s, -1, p->p))
			return 0;
	} while ((p = p->next)->prev);
	return 1;
}

static void
xpush(ctype_node **p, char *s)
{
	if (xcheck(*p, s))
		if (c_adt_lpush(p, c_adt_lnew(s, c_str_len(s, -1) + 1)) < 0)
			c_err_die(1, "c_adt_lpush");
}

static int
check_pkg(char *s)
{
	static ctype_arr arr; /* "memory leak" */
	ctype_stat st;

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_fmt(&arr, "%s/local/%s", dbdir, s) < 0)
		c_err_die(1, "c_dyn_fmt");

	if (c_nix_stat(&st, c_arr_data(&arr)) < 0) {
		if (errno == C_ENOENT)
			return 0;
		c_err_die(1, "c_nix_stat %s", c_arr_data(&arr));
	}
	return 1;
}

static void
solve_deps(ctype_node **list, ctype_node *deps)
{
	ctype_node *n, *wp;
	char **args, *s;

	if (!deps)
		return;

	n = nil;
	wp = deps->next;
	do {
		if ((s = c_str_chr(wp->p, -1, '#'))) *s = 0;
		file = getpath(wp->p);
		if (!(s = gettag("rdeps"))) continue;
		args = split_lines(s);
		for (; *args; ++args) {
			/* TODO: check dep version */
			if (check_pkg(*args))
				continue;
			if (xcheck(*list, *args))
				xpush(&n, *args);
		}
	} while ((wp = wp->next)->prev);

	c_adt_ltpush(list, n);
	solve_deps(list, n);
}

/* pkg routines */
static void
nolocal(char *s)
{
	if (!c_mem_cmp("local", sizeof("local"), dbflag))
		c_err_die(1, "\"%s\" from \"local\" makes no sense", s);
}

static void
pkgcheck(char **argv)
{
	ctype_arr arr;
	usize n;
	char **args, *s, *lsum;
	ctype_status r;

	c_mem_set(&arr, sizeof(arr), 0);
	r = 0;
	for (; *argv; ++argv) {
		c_ioq_fmt(ioq1, "%s: check...\n", *argv);
		c_ioq_flush(ioq1);

		file = getpath(*argv);
		if (!(s = gettag("files"))) continue;
		args = split_lines(s);
		for (; *args; ++args) {
			/* assume no whitespaces on names */
			if (!(s = c_str_chr(*args, -1, ' '))) {
				r = c_err_warnx(
				    "\"%s\" in wrong format", *argv);
				goto next;
			}
			*s++ = 0;
			c_arr_trunc(&arr, 0, sizeof(uchar));
			if (c_dyn_fmt(&arr, "%s/%s%c", root, s, 0) < 0)
				c_err_die(1, "c_dyn_fmt");
			n = c_arr_bytes(&arr);
			if (c_dyn_fmt(&arr, "%s", *args) < 0)
				c_err_die(1, "c_dyn_fmt");
			if (!(lsum = getsum(c_arr_data(&arr)))) {
				r = c_err_warnx(
				   "failed to obtain sum from \"%s\"",
				   c_arr_data(&arr));
				continue;
			}
			lsum = cut1(lsum);
			if (c_str_cmp((char *)c_arr_data(&arr) + n, -1, lsum))
				r = c_err_warnx("checksum mismatch \"%s\"", s);
		}
next:
		;
	}
	c_dyn_free(&arr);
	c_std_exit(r);
}

static void
pkgdel(char **argv)
{
	char **args, *s;
	ctype_status r;

	if (c_nix_chdir(root) < 0)
		c_err_die(1, "c_nix_fdchdir %s", root);

	for (; *argv; ++argv) {
		c_ioq_fmt(ioq1, "%s: delete...\n", *argv);
		c_ioq_flush(ioq1);

		file = getpath(*argv);
		if (!(s = gettag("files"))) continue;
		args = split_lines(s);
		for (; *args; ++args) {
			/* assume no whitespaces on names */
			if (!(s = c_str_chr(*args, -1, ' '))) {
				r = c_err_warnx(
				    "\"%s\" in wrong format", *argv);
				goto next;

			}
			*s++ = 0;
			/* TODO: deal with errors in a better way */
			if ((c_nix_unlink(s) < 0) && errno != C_ENOENT)
				c_err_die(1, "c_nix_unlink %s/%s", root, s);
			c_nix_rmdir(c_gen_dirname(*args));
		}
		if ((c_nix_unlink(file) < 0) && errno != C_ENOENT)
			c_err_die(1, "c_nix_unlink %s", file);
next:
		;
	}
	c_std_exit(r);
}

static void
pkgexplode(char **argv)
{
	ctype_arr arr, tmpdir;
	ctype_dir dir;
	ctype_dent *dp;
	ctype_stat st;
	ctype_fd dest, src;
	char *args[] = { ".", nil };
	char *s;

	nolocal("explode");

	c_mem_set(&arr, sizeof(arr), 0);
	c_mem_set(&tmpdir, sizeof(tmpdir), 0);
	for (; *argv; ++argv) {
		c_ioq_fmt(ioq1, "%s: explode...\n", *argv);
		c_ioq_flush(ioq1);

		file = getpath(*argv);
		s = getkey("name");
		c_arr_trunc(&arr, 0, sizeof(uchar));
		if (c_dyn_fmt(&arr, "%s/%s/cache/%s", dbdir, dbflag, s) < 0)
			c_err_die(1, "c_dyn_fmt");
		s = getkey("version");
		if (c_dyn_fmt(&arr, "#%s.vlz%c", s, 0) < 0)
			c_err_die(1, "c_dyn_fmt");

		s = c_arr_data(&arr);
		if (c_nix_stat(&st, s) < 0) {
			if (errno != C_ENOENT)
				c_err_die(1, "c_nix_stat %s", s);
			c_err_diex(1, "file \"%s\" not found in \"%s/cache\"",
			    c_gen_basename(s), dbflag);
		}

		c_arr_trunc(&tmpdir, 0, sizeof(uchar));
		if (c_dyn_fmt(&tmpdir, "%s/VENUS-TMPDIR.XXXXXXXXX", root) < 0)
			c_err_die(1, "c_dyn_fmt");

		chdir_tmpdir(c_arr_data(&tmpdir), c_arr_bytes(&tmpdir));
		douncompress(c_arr_data(&arr));
		/* TODO: handle signals and maybe create a file to remember
		 * the operation in case of failure */
		if (c_dir_open(&dir, args, 0, nil) < 0)
			c_err_die(1, "c_dir_open %s", *args);
		while ((dp = c_dir_read(&dir))) {
			c_arr_trunc(&arr, 0, sizeof(uchar));
			if (c_dyn_fmt(&arr, "%s/%s", root, dp->path) < 0)
				c_err_die(1, "c_dyn_fmt");
			/* TODO: deal with errors in a better way */
			switch (dp->info) {
			case C_FSD:
				if (c_nix_mkdir(c_arr_data(&arr), 0755) < 0 &&
				    errno != C_EEXIST)
					c_err_die(1, "c_nix_mkdir %s", s);
				continue;
			case C_FSDP:
				c_nix_rmdir(dp->path);
				continue;
			case C_FSDNR:
			case C_FSNS:
			case C_FSERR:
				continue;
			default:
				if (c_nix_rename(c_arr_data(&arr),
				    dp->path) < 0)
					c_err_die(1, "c_nix_rename %s <- %s",
					    c_arr_data(&arr), dp->path);
			}
		}
		c_dir_close(&dir);
		c_nix_rmdir(c_arr_data(&tmpdir));

		c_arr_trunc(&arr, 0, sizeof(uchar));
		if (c_dyn_fmt(&arr, "%s/local/%s", dbdir, *argv) < 0)
			c_err_die(1, "c_dyn_fmt");
		if ((dest = c_nix_fdopen3(c_arr_data(&arr),
		    C_OCREATE|C_OTRUNC|C_OWRITE, 0644)) < 0)
			c_err_die(1, "c_nix_fdopen3 %s", c_arr_data(&arr));
		if ((src = c_nix_fdopen2(file, C_OREAD)) < 0)
			c_err_die(1, "c_nix_fdopen2 %s", file);
		if (c_nix_fdcat(dest, src) < 0)
			c_err_die(1, "c_nix_fdcat %s <- %s", dest, src);
		c_nix_fdclose(dest);
		c_nix_fdclose(src);
	}
	c_dyn_free(&arr);
	c_dyn_free(&tmpdir);
}

static void
pkgfetch(char **argv)
{
	ctype_arr arr;
	ctype_stat st;
	usize len, n;
	char *s;

	nolocal("fetch");

	c_mem_set(&arr, sizeof(arr), 0);
	for (; *argv; ++argv) {
		c_ioq_fmt(ioq1, "%s: fetch...\n", *argv);
		c_ioq_flush(ioq1);

		c_arr_trunc(&arr, 0, sizeof(uchar));
		if (c_dyn_fmt(&arr, "%s/%s/cache/", dbdir, dbflag) < 0)
			c_err_die(1, "c_dyn_fmt");
		len = c_arr_bytes(&arr);

		file = getpath(*argv);
		s = getkey("name");
		if (c_dyn_fmt(&arr, "%s", s) < 0)
			c_err_die(1, "c_dyn_fmt");
		s = getkey("version");
		if (c_dyn_fmt(&arr, "#%s.vlz%c", s, 0) < 0)
			c_err_die(1, "c_dyn_fmt");

		s = c_arr_data(&arr);
		if (c_nix_stat(&st, s) < 0) {
			if (errno != C_ENOENT)
				c_err_die(1, "c_nix_stat %s", s);
		} else if (!dochecksum(s)) {
			continue;
		}

		n = c_arr_bytes(&arr);
		if (c_dyn_fmt(&arr, "%s/%s/%s/%.*s",
		    url, dbflag, arch, (n - len) - 1, s + len) < 0)
			c_err_die(1, "c_dyn_fmt");

		s = c_arr_data(&arr);
		dofetch(s, s+n);
		/* TODO: deal with errors in a better way */
		if (dochecksum(s)) c_std_exit(1);
	}
	c_dyn_free(&arr);
}

static void
pkgadd(char **argv)
{
	ctype_node *deps, *list;
	usize len;
	char *args[2];


	nolocal("add");

	list = nil;
	for (; *argv; ++argv) xpush(&list, *argv);

	deps = nil;
	solve_deps(&deps, list);
	c_adt_ltpush(&list, deps);

	len = c_ioq_fmt(ioq1,
	    "The following packages are going to be installed:");
	deps = list->next;
	do {
		len += c_ioq_fmt(ioq1, " %s", deps->p);
		if (len > columns) {
			len = 0;
			c_ioq_put(ioq1, "\n");
		}
	} while ((deps = deps->next)->prev);
	c_ioq_put(ioq1, "\n");

	if (yflag) {
		c_ioq_flush(ioq1);
	} else {
		c_ioq_fmt(ioq1, "Do you want to continue? [Y/N] ");
		c_ioq_flush(ioq1);
		if (yesno()) c_err_diex(1, "Abort");
	}

	args[1] = nil;
	deps = list->next;
	do {
		args[0] = deps->p;
		pkgfetch(args);
	} while ((deps = deps->next)->prev);

	deps = list->next;
	do {
		args[0] = deps->p;
		pkgexplode(args);
	} while ((deps = deps->next)->prev);

	while (list)
		c_adt_lfree(c_adt_lpop(&list));
}

static void
pkginfo(char **argv)
{
	for (; *argv; ++argv) {
		file = getpath(*argv);
		c_ioq_fmt(ioq1, "Name: %s\n", getkey("name"));
		c_ioq_fmt(ioq1, "Version: %s\n", getkey("version"));
		c_ioq_fmt(ioq1, "License: %s\n", getkey("license"));
		c_ioq_fmt(ioq1, "Description: %s\n", getkey("description"));
		c_ioq_fmt(ioq1, "Size: %s\n", getkey("size"));
	}
}

static void
pkglist(char **argv)
{
	for (; *argv; ++argv) {
		file = getpath(*argv);
		c_ioq_fmt(ioq1, "%s\n", gettag(tag));
	}
}

static void
pkgupdate(char **argv)
{
	ctype_arr arr;
	usize len;
	char *s;

	(void)argv;

	c_ioq_fmt(ioq1, "populate remote...\n");
	c_ioq_flush(ioq1);

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/remote", dbdir) < 0)
		c_err_die(1, "c_dyn_fmt");

	s = c_arr_data(&arr);
	if (c_nix_chdir(s) < 0)
		c_err_die(1, "c_nix_chdir %s", s);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	c_arr_fmt(&arr, "cache/remote.vlz%c", 0);
	len = c_arr_bytes(&arr);

	if (c_dyn_fmt(&arr, "%s/remote/%s/remote.vlz", url, arch) < 0)
		c_err_die(1, "c_dyn_fmt");

	s = c_arr_data(&arr);
	dofetch(s, s+len);
	douncompress(s);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	c_arr_fmt(&arr, "cache/chksum%c", 0);
	len = c_arr_bytes(&arr);
	c_arr_fmt(&arr, "%s/remote/%s/chksum", url, arch);

	s = c_arr_data(&arr);
	dofetch(s, s+len);

	c_dyn_free(&arr);
}

static void
usage(void)
{
	c_ioq_fmt(ioq2,
	    "usage: %s [-LNR] -acdefi pkg [...]\n"
	    "       %s [-LNR] -l tag pkg [...]\n"
	    "       %s -u\n",
	    c_std_getprogname(), c_std_getprogname(), c_std_getprogname());
	c_std_exit(1);
}

ctype_status
main(int argc, char **argv)
{
	char *db, *tmp;
	void (*func)(char **);

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	func = nil;
	dbflag = nil;

	while (c_std_getopt(argmain, argc, argv, "LNRacdefil:uy")) {
		switch (argmain->opt) {
		case 'L':
			dbflag = "local";
			break;
		case 'N':
			dbflag = ".";
			break;
		case 'R':
			dbflag = "remote";
			break;
		case 'a':
			db = "remote";
			func = pkgadd;
			break;
		case 'c':
			db = "local";
			func = pkgcheck;
			break;
		case 'd':
			db = "local";
			func = pkgdel;
			break;
		case 'e':
			db = "remote";
			func = pkgexplode;
			break;
		case 'f':
			db = "remote";
			func = pkgfetch;
			break;
		case 'i':
			db = "local";
			func = pkginfo;
			break;
		case 'l':
			db = "local";
			tag = argmain->arg;
			func = pkglist;
			break;
		case 'u':
			db = "remote";
			func = pkgupdate;
			break;
		case 'y':
			yflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= argmain->idx;
	argv += argmain->idx;

	start();
	if (func == pkgupdate) {
		func(nil);
		c_std_exit(0);
	}

	if (!func || !argc)
		usage();

	if (!dbflag)
		dbflag = db;

	if ((tmp = c_std_getenv("COLUMNS")))
		columns = c_std_strtouvl(tmp, 0, 0, -1, nil, nil);

	func(argv);
	c_ioq_flush(ioq1);
	return 0;
}
