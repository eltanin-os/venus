#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define HDEC(x) (((x) <= '9') ? x - '0' : (((uchar)x | 32) - 'a') + 10)

static ctype_status
wcheck(ctype_fd fd, char *s, u64int n)
{
	ctype_hst hs;
	ctype_stat st;
	int i;
	char buf[C_HWHIRLPOOL_DIGEST];

	if (c_sys_fstat(&st, fd) < 0)
		c_err_die(1, "c_sys_fstat");

	if ((u64int)st.size != n)
		return -1;

	if (c_hsh_putfd(&hs, c_hsh_whirlpool, fd, st.size) < 0)
		c_err_die(1, "c_hsh_putfd");

	c_hsh_digest(&hs, c_hsh_whirlpool, buf);
	for (i = 0; i < C_HWHIRLPOOL_DIGEST; ++i) {
		if (((HDEC(s[0]) << 4) | HDEC(s[1])) != (uchar)buf[i])
			return -1;
		s += 2;
	}
	return 0;
}

void
chksum_whirlpool(ctype_fd dirfd, ctype_fd etcfd, char *file)
{
	ctype_ioq ioq;
	ctype_arr *ap;
	ctype_fd fd;
	u64int n;
	int check;
	char *p, *s;
	char buf[C_BIOSIZ];

	efchdir(dirfd);
	c_ioq_init(&ioq, etcfd, buf, sizeof(buf), &c_sys_read);
	c_sys_seek(etcfd, 0, C_SEEKSET);
	check = 0;
	while ((ap = getln(&ioq))) {
		n = c_arr_bytes(ap);
		if (!(s = c_str_chr(c_arr_data(ap), n, ' ')))
			c_err_diex(1, CHKSUMFILE ": wrong format");
		n -= s - (char *)c_arr_data(ap);
		*s++ = 0;
		if (!c_str_cmp(c_arr_data(ap), n, file)) {
			++check;
			break;
		}
	}
	if (!check)
		c_err_diex(1, "%s: have no checksum", file);
	if (!(p = c_str_chr(s, n, ' ')))
		c_err_diex(1, CHKSUMFILE ": wrong format");
	*p++ = 0;
	n = estrtovl(p, 8, 0, C_VLONGMAX);
	if ((fd = c_sys_open(file, ROPTS, RMODE)) < 0)
		c_err_die(1, "c_sys_open %s", file);
	if (wcheck(fd, s, n) < 0)
		c_err_diex(1, "%s: checksum mismatch", file);
	c_sys_close(fd);
}
