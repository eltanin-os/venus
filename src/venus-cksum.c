#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

static char *tmpav[] = { "-", nil };

static ctype_status
putlink(ctype_hst *h, ctype_hmd *md, char *s, size n)
{
	static ctype_arr arr;
	size r;

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if (c_dyn_ready(&arr, n, sizeof(uchar)) < 0)
		c_err_die(1, "c_dyn_ready");

	if ((r = c_sys_readlink(s, c_arr_data(&arr), n)) < 0)
		return c_err_warn("c_sys_readlink %s", s);

	c_hsh_all(h, md, c_arr_data(&arr), n);
	return 0;
}

static void
usage(void)
{
	c_ioq_fmt(ioq2, "usage: %s [-w] [file ...]\n", c_std_getprogname());
	c_std_exit(1);
}

ctype_status
cksum_main(int argc, char **argv)
{
	ctype_hst h;
	ctype_hmd *md;
	ctype_stat st;
	int i, r;
	char buf[C_HWHIRLPOOL_DIGEST];

	c_std_setprogname(argv[0]);

	md = c_hsh_fletcher32;

	C_ARGBEGIN {
	case 'w':
		md = c_hsh_whirlpool;
		break;
	default:
		usage();
	} C_ARGEND

	if (!argc)
		argv = tmpav;

	r = 0;
	for (; *argv; ++argv) {
		md->init(&h);
		if (C_ISDASH(*argv)) {
			*argv = "<stdin>";
			if (c_hsh_putfile(&h, md, *argv) < 0) {
				r = c_err_warnx("c_hsh_putfile %s", *argv);
				continue;
			}
		} else {
			if (c_sys_lstat(&st, *argv) < 0) {
				r = c_err_warnx("c_sys_lstat %s", *argv);
				continue;
			}
			if (C_ISLNK(st.mode)) {
				if (putlink(&h, md, *argv, st.size + 1) < 0)
					continue;
			} else if (c_hsh_putfile(&h, md, *argv) < 0) {
				r = c_err_warnx("c_hsh_putfile %s", *argv);
				continue;
			}
		}
		putsize(&h, md);
		md->end(&h);
		c_hsh_digest(&h, md, buf);
		c_ioq_fmt(ioq1, "%s ", *argv);
		if (md == c_hsh_fletcher32) {
			c_ioq_fmt(ioq1, "%x", c_uint_32unpack(buf));
		} else {
			for (i = 0; i < C_HWHIRLPOOL_DIGEST; ++i)
				c_ioq_fmt(ioq1, "%02x", (uchar)buf[i]);
		}
		c_ioq_put(ioq1, "\n");
	}
	c_ioq_flush(ioq1);
	return r;
}
