#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

static char *slashv[2] = { "-", nil };

static void
usage(void)
{
	c_ioq_fmt(ioq2, "usage: %s [-w] [file ...]\n", c_std_getprogname());
	c_std_exit(1);
}

int
main(int argc, char **argv)
{
	ctype_hmd *hfn;
	ctype_hst hs;
	ctype_stat st;
	int i, rv;
	size r;
	char buf[C_PATHMAX];

	c_std_setprogname(argv[0]);
	hfn = c_hsh_fletcher32;

	C_ARGBEGIN {
	case 'w':
		hfn = c_hsh_whirlpool;
		break;
	default:
		usage();
	} C_ARGEND

	if (!argc)
		argv = slashv;

	rv = 0;

	for (; *argv; --argc, ++argv) {
		if (!c_sys_lstat(&st, *argv) && C_ISLNK(st.mode)) {
			if ((r = c_sys_readlink(buf, sizeof(buf), *argv)) < 0) {
				rv = c_err_warn("c_sys_readlink %s", *argv);
				continue;
			}
			c_hsh_all(&hs, hfn, buf, r);
		} else {
			if (C_ISDASH(*argv))
				*argv = "<stdin>";
			if (c_hsh_putfile(&hs, hfn, *argv) < 0) {
				rv = c_err_warn("c_hsh_putfile %s", *argv);
				continue;
			}
		}
		if (hfn == c_hsh_whirlpool) {
			c_hsh_digest(&hs, hfn, buf);
			c_ioq_fmt(ioq1, "%s ", *argv);
			for (i = 0; i < 64; ++i)
				c_ioq_fmt(ioq1, "%02x", (uchar)buf[i]);
			c_ioq_fmt(ioq1, " %o\n", hs.len);
			continue;
		}
		c_ioq_fmt(ioq1, "%s %o %o\n", *argv, c_hsh_state0(&hs), hs.len);
	}

	c_ioq_flush(ioq1);

	return rv;
}
