#include <tertium/cpu.h>
#include <tertium/std.h>

static ctype_status
fletcher32(char **argv)
{
	ctype_hst h;
	ctype_status r;
	char buf[C_HSH_H32DIG];
	r = 0;
	for (; *argv; ++argv) {
		if (C_STD_ISDASH(*argv)) *argv = "<stdin>";
		c_hsh_fletcher32->init(&h);
		if (c_hsh_putfile(&h, c_hsh_fletcher32, *argv) < 0) {
			r = c_err_warn("failed to hash file \"%s\"", *argv);
			continue;
		}
		c_hsh_fletcher32->end(&h, buf);
		c_ioq_fmt(ioq1, "%s:%x\n", *argv, c_uint_32unpack(buf));
	}
	return r;
}

static ctype_status
whirlpool(char **argv)
{
	ctype_hst h;
	int i;
	ctype_status r;
	char buf[C_HSH_WHIRLPOOLDIG];
	r = 0;
	for (; *argv; ++argv) {
		if (C_STD_ISDASH(*argv)) *argv = "<stdin>";
		c_hsh_whirlpool->init(&h);
		if (c_hsh_putfile(&h, c_hsh_whirlpool, *argv) < 0) {
			r = c_err_warn("failed to hash file \"%s\"", *argv);
			continue;
		}
		c_hsh_whirlpool->end(&h, buf);
		c_ioq_fmt(ioq1, "%s:", *argv);
		for (i = 0; i < (int)sizeof(buf); ++i)
			c_ioq_fmt(ioq1, "%02x", (uchar)buf[i]);
		c_ioq_put(ioq1, "\n");
	}
	return r;
}

static void
usage(void)
{
	c_ioq_fmt(ioq2, "usage: %s [-w] file ...\n", c_std_getprogname());
	c_std_exit(1);
}

ctype_status
main(int argc, char **argv)
{
	ctype_status r;
	ctype_status (*func)(char **);
	char *args[] = { "-", nil };

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	func = fletcher32;
	while (c_std_getopt(argmain, argc, argv, "w")) {
		switch (argmain->opt) {
		case 'w':
			func = whirlpool;
			break;
		default:
			usage();
		}
	}
	argc -= argmain->idx;
	argv += argmain->idx;
	if (!argc) argv = args;
	r = func(argv);
	if (c_ioq_flush(ioq1) < 0) c_err_die(1, "failed to flush stdout");
	return r;
}
