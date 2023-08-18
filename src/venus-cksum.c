#include <tertium/cpu.h>
#include <tertium/std.h>

#define WD C_HSH_BLAKE2B512DIG

static ctype_status
putfile(ctype_hst *h, ctype_hmd *md, char *s)
{
	ctype_status r;
	r = c_hsh_putfile(h, md, s);
	if (r < 0) return c_err_warnx("failed to hash file \"%s\"", s);
	return 0;
}

static ctype_status
putsymlink(ctype_hst *h, ctype_hmd *md, char *s)
{
	ctype_status r;
	char buf[C_LIM_PATHMAX];
	r = c_nix_readlink(buf, sizeof(buf), s);
	if (r < 0) return c_err_warnx("failed to read symbolic link \"%s\"", s);
	md->update(h, buf, r);
	return 0;
}

static ctype_status
hashfile(char *buf, ctype_hmd *md, char *s)
{
	ctype_hst h;
	ctype_stat st;
	ctype_status (*put)(ctype_hst *h, ctype_hmd *, char *);
	ctype_status r;

	r = c_nix_lstat(&st, s);
	if (r < 0) return c_err_warnx("failed to obtain file info \"%s\"", s);

	put = putfile;
	if (C_NIX_ISLNK(st.mode)) put = putsymlink;

	md->init(&h);
	if (put(&h, md, s)) return 1;
	md->end(&h, buf);
	return 0;
}

static void
usage(void)
{
	c_ioq_fmt(ioq2, "usage: %s file ...\n", c_std_getprogname());
	c_std_exit(1);
}

ctype_status
main(int argc, char **argv)
{
	ctype_status r;
	int i;
	char buf[WD];

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	if (c_std_noopt(argmain, *argv)) usage();
	argc -= argmain->idx;
	argv += argmain->idx;
	if (!argc) usage();

	for (; *argv; ++argv) {
		if (hashfile(buf, c_hsh_blake2b512, *argv)) {
			r = 1;
			continue;
		}
		c_ioq_fmt(ioq1, "%s:", *argv);
		for (i = 0; i < WD; ++i) c_ioq_fmt(ioq1, "%02x", (uchar)buf[i]);
		c_ioq_put(ioq1, "\n");
	}

	if (c_ioq_flush(ioq1) < 0) c_err_die(1, "failed to flush stdout");
	return r;
}
