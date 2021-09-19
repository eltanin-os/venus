#include <tertium/cpu.h>
#include <tertium/std.h>

static char *slash[] = { "-", nil };

static void
usage(void)
{
	c_ioq_fmt(ioq2, "usage: %s [-w] file ...\n", c_std_getprogname());
	c_std_exit(1);
}

ctype_status
main(int argc, char **argv)
{
	ctype_hst hs;
	ctype_hmd *md;
	ctype_status r;
	int i, len;
	char buf[C_HWHIRLPOOL_DIGEST];

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	md = c_hsh_fletcher32;
	len = C_H32GEN_DIGEST;

	while (c_std_getopt(argmain, argc, argv, "w")) {
		switch (argmain->opt) {
		case 'w':
			md = c_hsh_whirlpool;
			len = C_HWHIRLPOOL_DIGEST;
			break;
		default:
			usage();
		}
	}
	argc -= argmain->idx;
	argv += argmain->idx;

	if (!argc)
		argv = slash;

	r = 0;
	for (; *argv; ++argv) {
		if (C_ISDASH(*argv))
			*argv = "<stdin>";
		md->init(&hs);
		if (c_hsh_putfile(&hs, md, *argv) < 0) {
			r = c_err_warn("c_hsh_putfile %s", *argv);
			continue;
		}
		md->end(&hs, buf);
		for (i = 0; i < len; ++i)
			c_ioq_fmt(ioq1, "%02x", (uchar)buf[i]);
		c_ioq_fmt(ioq1, " %s", *argv);
		c_ioq_put(ioq1, "\n");
	}
	c_ioq_flush(ioq1);
	return r;
}
