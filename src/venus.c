#include <tertium/cpu.h>
#include <tertium/std.h>

#define DUPKEY(a) \
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

/* spawn  routines */
static char *
getlines(ctype_fd fd)
{
	static ctype_arr arr;
	ctype_ioq ioq;
	ctype_status r;
	char buf[C_IOQ_BSIZ];

	c_ioq_init(&ioq, fd, buf, sizeof(buf), &c_nix_fdread);
	c_arr_trunc(&arr, 0, sizeof(uchar));
	while ((r = c_ioq_getln(&ioq, &arr)) > 0) ;
	c_arr_trunc(&arr, c_arr_bytes(&arr) - 1, sizeof(uchar));

	if (r < 0) c_err_diex(1, "failed to read pipe");
	return c_arr_data(&arr);
}

static void
waitchild(ctype_id id, char *prog)
{
	ctype_status r;
	switch (c_exc_wait(id, &r)) {
	case 1:
		if (!r) return;
		c_err_diex(1, "\"%s\" closed with exit code %d", prog, r);
	case 2:
		c_err_diex(1, "\"%s\" closed with signal %d", prog, r);
	}
}

static char *
spawn_getlines(char **args)
{
	ctype_id id;
	ctype_fd fd;
	char *s;

	id = c_exc_spawn1(*args, args, environ, &fd, 1);
	if (!id) c_err_diex(1, "failed to spawn \"%s\"", *args);
	s = getlines(fd);
	waitchild(id, *args);
	c_nix_fdclose(fd);
	return (s && *s) ? s : nil;
}

static char **
split(ctype_arr *ap, char *s)
{
	ctype_status r;

	c_arr_trunc(ap, 0, sizeof(uchar));
	r = c_dyn_cat(ap, &s, 1, sizeof(char *));
	if (r < 0) c_err_diex(1, "no memory");
	for (; *s; ++s) {
		if (*s != '\n') continue;
		*s++ = 0;
		r = c_dyn_cat(ap, &s, 1, sizeof(char *));
		if (r < 0) c_err_diex(1, "no memory");
	}
	s = nil;
	r = c_dyn_cat(ap, &s, 1, sizeof(char *));
	if (r < 0) c_err_diex(1, "no memory");
	return c_arr_data(ap);
}

static char **
ssplit(char *s)
{
	static ctype_arr arr; /* "memory leak" */
	c_arr_trunc(&arr, 0, sizeof(uchar));
	return split(&arr, s);
}

/* conf routines */
static char *
getkey(char *k)
{
	char *args[] = { "venus-conf", k, file, nil };
	return spawn_getlines(args);
}

static char *
getmkey(char *k)
{
	char *args[] = { "venus-conf", "-t", k, file, nil };
	return spawn_getlines(args);
}

static char *
getwsum(char *s)
{
	char *args[] = { "venus-cksum", "-w", s, nil };
	return spawn_getlines(args);
}

/* helper routines */
static char **
argslist(ctype_node *list)
{
	ctype_node *wp;
	usize i, n;
	char **args;

	n = 0;
	wp = list->next;
	do { ++n; } while ((wp = wp->next)->prev);

	args = c_std_alloc(n+1, sizeof(char *));
	if (!args) c_err_diex(1, "no memory");

	i = 0;
	wp = list->next;
	do {
		args[i++] = wp->p;
	} while ((wp = wp->next)->prev);
	args[i] = nil;
	return args;
}

static int
askconfirm(void)
{
	int ans;
	char ch;
	c_ioq_get(ioq0, &ch, 1);
	ans = (ch | 32) != 'y';
	while (ch != '\n') c_ioq_get(ioq0, &ch, 1);
	return ans;
}

static void
copyfile(char *target, char *s)
{
	ctype_fd destfd, fd;
	ctype_status r;
	char tmp[] = "tmpvenus.XXXXXXXXX";
	destfd = c_nix_mktemp(tmp, sizeof(tmp));
	if (destfd < 0) goto fail;
	fd = c_nix_fdopen2(s, C_NIX_OREAD);
	if (fd < 0) goto fail;
	r = c_nix_fdcat(destfd, fd);
	if (r < 0) goto fail;
	c_nix_fdclose(destfd);
	c_nix_fdclose(fd);
	r = c_nix_rename(target, tmp);
	if (r < 0) goto fail;
	return;
fail:
	c_nix_unlink(tmp);
	c_err_diex(1, "failed to copy \"%s\" to \"%s\"", target, s);
}

static int
exist(char *s)
{
	ctype_stat st;
	if (c_nix_stat(&st, s) < 0) return 0;
	return 1;
}

static char *
getpath(char *db, char *s)
{
	static ctype_arr arr;
	ctype_stat st;
	ctype_status r;

	c_arr_trunc(&arr, 0, sizeof(uchar));
	r = c_dyn_fmt(&arr, "%s/%s/%s", dbdir, db, s);
	if (r < 0) c_err_diex(1, "no memory");

	s = c_arr_data(&arr);
	if (c_nix_stat(&st, s) < 0) {
		if (errno == C_ERR_ENOENT) return nil;
		c_err_diex(1, "failed to obtain file info \"%s\"", s);
	}
	return s;
}

static char *
getpathx(char *s)
{
	char *path;
	if (!(path = getpath(dbflag, s)))
		c_err_diex(1, "package \"%s\" not found in \"%s\"", s, dbflag);
	return path;
}

static void
remove(char *s)
{
	if ((c_nix_unlink(s) < 0) && errno != C_ERR_ENOENT)
		c_err_die(1, "failed to remove file \"%s\"", s);
}

/* do routines */
static char *
urlencode(char *url)
{
	static ctype_arr arr; /* "memory leak" */
	ctype_status r;

	c_arr_trunc(&arr, 0, sizeof(uchar));
	r = c_dyn_ready(&arr, c_str_len(url, -1), sizeof(uchar));
	if (r < 0) c_err_diex(1, "no memory");

	for (; *url; ++url) {
		switch (*url) {
		case '#':
			r = c_dyn_cat(&arr, "%23", 3, sizeof(uchar));
			if (r < 0) c_err_diex(1, "no memory");
			break;
		default:
			r = c_dyn_cat(&arr, url, 1, sizeof(uchar));
			if (r < 0) c_err_diex(1, "no memory");
		}
	}
	return c_arr_data(&arr);
}

static void
dofetch(char *target, char *url)
{
	ctype_id id;
	char **args;

	args = c_exc_arglist(fetch, target, urlencode(url));
	if (!args) c_err_diex(1, "no memory");
	id = c_exc_spawn0(*args, args, environ);
	waitchild(id, *args);
	c_std_free(args);
}

static void
douncompress(char *s)
{
	ctype_id id;
	ctype_fd fd, svfd;
	char *prog[] = { "venus-ar", "-x", nil };
	char **args;
	/* uncompress */
	args = c_exc_arglist(uncompress, s);
	if (!args) c_err_diex(1, "no memory");
	id = c_exc_spawn1(*args, args, environ, &fd, 1);
	if (!id) c_err_die(1, nil);
	c_std_free(args);
	/* unarchive */
	svfd = 0;
	c_nix_fdcopy(svfd, C_IOQ_FD0);
	c_nix_fdcopy(C_IOQ_FD0, fd);
	id = c_exc_spawn0(*prog, prog, environ);
	if (!id) c_err_die(1, nil);
	c_nix_fdcopy(C_IOQ_FD0, svfd);
	c_nix_waitpid(-1, nil, 0);
}

static ctype_status
dochecksum(char *s)
{
	static ctype_arr arr; /* "memory leak" */
	ctype_status r;
	usize n;
	char *local, *p, *remote;

	local = getwsum(s);
	if (!local) return c_err_warnx("failed to obtain sum from \"%s\"", s);
	local = c_str_chr(local, -1, ':');
	if (!local) c_err_diex(1, "bad formatted sum");

	c_arr_trunc(&arr, 0, sizeof(uchar));
	r = c_dyn_fmt(&arr, "%s%c", local + 1, '\0');
	if (r < 0) c_err_diex(1, "no memory");
	n = c_arr_bytes(&arr);

	r = c_dyn_fmt(&arr, "%s/%s/cache/chksum", dbdir, dbflag);
	if (r < 0) c_err_diex(1, "no memory");
	file = (local = c_arr_data(&arr)) + n;

	if (!(remote = getkey((p = c_gen_basename(s)))))
		return c_err_warnx(
		    "package \"%s\" not found in \"%s\" chksum", p, dbflag);

	if (c_str_cmp(remote, -1, local)) {
		remove(s);
		return 0;
	}
	return 1;
}

/* deps routines */
static void
xpush(ctype_node **p, ctype_kvtree *t, char *s)
{
	ctype_status r;
	if ((r = c_adt_kvadd(t, s, nil)) == 1) return;
	if (r < 0) c_err_diex(1, "no memory");
	r = c_adt_lpush(p, c_adt_lnew(s, c_str_len(s, -1) + 1));
	if (r < 0) c_err_diex(1, "no memory");
}

static void
resolvedeps(ctype_node **list, ctype_kvtree *t, ctype_node *deps)
{
	ctype_node *n, *wp;
	char **args, *s;

	if (!deps) return;

	n = nil;
	wp = deps->next;
	do {
		if ((s = c_str_chr(wp->p, -1, '#'))) *s = 0;
		file = getpathx(wp->p);
		if (!(s = getmkey("rdeps"))) continue;
		args = ssplit(s);
		for (; *args; ++args) {
			if (getpath("local", *args)) continue;
			xpush(&n, t, *args);
		}
	} while ((wp = wp->next)->prev);

	c_adt_ltpush(list, n);
	resolvedeps(list, t, n);
}

/* pkg routines */
static void
pkgdel(char **argv)
{
	ctype_status r;
	char **args, *s;

	r = c_nix_chdir(root);
	if (r < 0) c_err_die(1, "failed to change dir to \"%s\"", root);

	for (; *argv; ++argv) {
		c_ioq_fmt(ioq2, "%s: delete...\n", *argv);
		c_ioq_flush(ioq2);

		file = getpathx(*argv);
		if (!(s = getmkey("files"))) continue;
		args = ssplit(s);
		for (; *args; ++args) {
			if ((s = c_str_chr(*args, -1, ':'))) *s = 0;
			remove(*args);
			c_nix_rmpath(c_gen_dirname(*args));
		}
		remove(file);
	}
}

static void
pkgfetch(char **argv)
{
	ctype_arr arr;
	usize len, n;
	ctype_status r;
	char *s;

	c_mem_set(&arr, sizeof(arr), 0);
	for (; *argv; ++argv) {
		c_ioq_fmt(ioq2, "%s: fetch...\n", *argv);
		c_ioq_flush(ioq2);

		file = getpathx(*argv);
		c_arr_trunc(&arr, 0, sizeof(uchar));
		r = c_dyn_fmt(&arr, "%s/%s/cache/", dbdir, dbflag);
		if (r < 0) c_err_diex(1, "no memory");
		len = c_arr_bytes(&arr);

		s = getkey("name");
		r = c_dyn_fmt(&arr, "%s", s);
		if (r < 0) c_err_diex(1, "no memory");
		s = getkey("version");
		r = c_dyn_fmt(&arr, "#%s.vlz%c", s, '\0');
		if (r < 0) c_err_diex(1, "no memory");

		s = c_arr_data(&arr);
		if (exist(s) && dochecksum(s)) continue;

		n = c_arr_bytes(&arr);
		r = c_dyn_fmt(&arr, "%s/%s/%s/%.*s",
		    url, dbflag, arch, (n - len) - 1, s + len);
		if (r < 0) c_err_diex(1, "no memory");
		s = c_arr_data(&arr);
		dofetch(s, s+n);
		if (!dochecksum(s)) c_err_diex(1, "file corrupted \"%s\"", s);
	}
	c_dyn_free(&arr);
}

static void
pkgexplode(char **argv)
{
	ctype_arr arr;
	ctype_status r;
	char *s;

	r = c_nix_chdir(root);
	if (r < 0) c_err_die(1, "failed to change dir to \"%s\"", root);

	c_mem_set(&arr, sizeof(arr), 0);
	for (; *argv; ++argv) {
		c_ioq_fmt(ioq2, "%s: explode...\n", *argv);
		c_ioq_flush(ioq2);

		file = getpathx(*argv);
		c_arr_trunc(&arr, 0, sizeof(uchar));
		r = c_dyn_fmt(&arr, "%s/%s/cache/", dbdir, dbflag);
		if (r < 0) c_err_diex(1, "no memory");

		s = getkey("name");
		r = c_dyn_fmt(&arr, "%s", s);
		if (r < 0) c_err_diex(1, "no memory");
		s = getkey("version");
		r = c_dyn_fmt(&arr, "#%s.vlz", s);
		if (r < 0) c_err_diex(1, "no memory");

		s = c_arr_data(&arr);
		if (!exist(s))
			c_err_diex(1,
			    "package \"%s\" not found in \"%s\" cache",
			    *argv, dbflag);

		douncompress(s);

		c_arr_trunc(&arr, 0, sizeof(uchar));
		r = c_dyn_fmt(&arr, "%s/local/%s", dbdir, *argv);
		if (r < 0) c_err_diex(1, "no memory");
		copyfile(c_arr_data(&arr), file);
	}
	c_dyn_free(&arr);
}

static void
pkgadd(char **argv)
{
	ctype_kvtree t;
	ctype_node *deps, *list;
	usize n;

	c_mem_set(&t, sizeof(t), 0);
	list = nil;
	for (; *argv; ++argv) xpush(&list, &t, *argv);

	deps = nil;
	resolvedeps(&deps, &t, list);
	c_adt_kvfree(&t, nil);
	c_adt_ltpush(&list, deps);

	n = c_ioq_fmt(ioq2,
	    "The following packages are going to be installed:");
	deps = list->next;
	do {
		n += c_ioq_fmt(ioq2, " %s", deps->p);
		if (n > columns) {
			n = 0;
			c_ioq_put(ioq2, "\n");
		}
	} while ((deps = deps->next)->prev);
	c_ioq_put(ioq2, "\n");

	if (yflag) {
		c_ioq_flush(ioq2);
	} else {
		c_ioq_fmt(ioq2, "Do you want to continue? [Y/N] ");
		c_ioq_flush(ioq2);
		if (askconfirm()) c_err_diex(1, "Abort");
	}

	argv = argslist(list);
	pkgfetch(argv);
	pkgexplode(argv);
	while (list) c_adt_lfree(c_adt_lpop(&list));
	c_std_free(argv);
}

static void
pkginfo(char **argv)
{
	for (; *argv; ++argv) {
		file = getpathx(*argv);
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
		file = getpathx(*argv);
		c_ioq_fmt(ioq1, "%s\n", getmkey(tag));
	}
}

static void
pkgupdate(char **argv)
{
	ctype_arr arr;
	ctype_status r;
	usize len;
	char *s;

	(void)argv;

	c_ioq_fmt(ioq2, "populate remote...\n");
	c_ioq_flush(ioq2);

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_fmt(&arr, "%s/remote", dbdir) < 0) c_err_diex(1, "no memory");
	s = c_arr_data(&arr);

	r = c_nix_chdir(s);
	if (r < 0) c_err_die(1, "failed to change dir to \"%s\"", s);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	r = c_dyn_fmt(&arr, "cache/remote.vlz%c", '\0');
	if (r < 0) c_err_diex(1, "no memory");
	len = c_arr_bytes(&arr);
	r = c_dyn_fmt(&arr, "%s/remote/%s/remote.vlz", url, arch);
	if (r < 0) c_err_diex(1, "no memory");
	s = c_arr_data(&arr);

	dofetch(s, s+len);
	douncompress(s);

	c_arr_trunc(&arr, 0, sizeof(uchar));
	c_arr_fmt(&arr, "cache/chksum%c", '\0');
	len = c_arr_bytes(&arr);
	c_arr_fmt(&arr, "%s/remote/%s/chksum", url, arch);
	s = c_arr_data(&arr);

	dofetch(s, s+len);

	c_dyn_free(&arr);
}

/* main routines */
static void
start(void)
{
	ctype_arr arr;
	usize len;
	ctype_status r;
	char *home;

	if (!(c_sys_geteuid())) {
		conf = "/etc/venus.cfg";
		root = "/var/pkg";
	} else {
		if (!(home = c_std_getenv("XDG_CONFIG_HOME"))) {
			home = c_std_getenv("HOME");
			if (!home) c_err_diex(1, "missing HOME variable");
			c_mem_set(&arr, sizeof(arr), 0);
			r = c_dyn_fmt(&arr, "%s/.config", home);
			if (r < 0) c_err_diex(1, "no memory");
			c_dyn_shrink(&arr);
			home = c_arr_data(&arr);
		}
		c_mem_set(&arr, sizeof(arr), 0);
		r = c_dyn_fmt(&arr, "%s/venus/config%c", home, '\0');
		if (r < 0) c_err_diex(1, "no memory");
		len = c_arr_bytes(&arr);
		r = c_dyn_fmt(&arr, "%s/venus/db", home);
		if (r < 0) c_err_diex(1, "no memory");
		c_dyn_shrink(&arr);
		conf = c_arr_data(&arr); /* "memory leak" */
		dbdir = conf + len;
		c_std_free(home);
	}
	file = conf;
	/* "memory leak" */
	DUPKEY(arch);
	DUPKEY(fetch);
	DUPKEY(root);
	DUPKEY(url);
	DUPKEY(safeurl);
	DUPKEY(uncompress);
}

static void
usage(void)
{
	c_ioq_fmt(ioq2,
	    "usage: %s [-LNR] -adefi pkg [...]\n"
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

	db = nil;
	dbflag = nil;
	func = nil;
	while (c_std_getopt(argmain, argc, argv, "LNRadefil:uy")) {
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

	if (!func || !argc) usage();

	tmp = c_std_getenv("COLUMNS");
	if (tmp) columns = c_std_strtouvl(tmp, 0, 0, -1, nil, nil);

	if (!dbflag) dbflag = db;
	func(argv);
	c_ioq_flush(ioq1);
	return 0;
}
