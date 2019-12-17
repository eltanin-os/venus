#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

static int
hlink(ctype_hst *p, ctype_hmd *md, char *s)
{
	ctype_arr arr;
	ctype_stat st;
	size r;

	if (c_sys_lstat(&st, s) < 0 || !C_ISLNK(st.mode))
		return 0;

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_ready(&arr, st.size, sizeof(uchar)) < 0)
		c_err_die(1, "c_dyn_ready");

	if ((r = c_sys_readlink(c_arr_data(&arr), st.size, s)) < 0)
		c_err_die(1, "c_sys_readlink %s", s);

	c_hsh_all(p, md, c_arr_data(&arr), r);
	c_dyn_free(&arr);
	return 1;
}

static void
usage(void)
{
	c_ioq_fmt(ioq2, "usage: %s [-w] [file ...]\n", c_std_getprogname());
	c_std_exit(1);
}

int
cksum_main(int argc, char **argv)
{
	ctype_hst hs;
	ctype_hmd *md;
	int i, r;
	char buf[C_HWHIRLPOOL_DIGEST];
	char tmp[2];

	md = c_hsh_fletcher32;

	C_ARGBEGIN {
	case 'w':
		md = c_hsh_whirlpool;
		break;
	default:
		usage();
	} C_ARGEND

	if (!argc) {
		tmp[0] = '-';
		tmp[1] = '\0';
		*argv = tmp;
	}

	r = 0;

	for (; *argv; ++argv) {
		if (hlink(&hs, md, *argv)) {
			;
		} else {
			if (C_ISDASH(*argv))
				*argv = "<stdin>";
			if (c_hsh_putfile(&hs, md, *argv)) {
				r = c_err_warn("c_hsh_putfile %s", *argv);
				continue;
			}
		}
		c_hsh_digest(&hs, md, buf);
		c_ioq_fmt(ioq1, "%s ", *argv);
		if (md == c_hsh_whirlpool) {
			for (i = 0; i < C_HWHIRLPOOL_DIGEST; ++i)
				c_ioq_fmt(ioq1, "%02x", (uchar)buf[i]);
		} else {
			c_ioq_fmt(ioq1, "%o", *(u32int *)(uintptr)buf);
		}
		c_ioq_fmt(ioq1, " %o", c_hsh_len(&hs));
	}

	c_ioq_flush(ioq1);
	return r;
}
