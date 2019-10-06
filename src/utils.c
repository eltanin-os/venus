#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define HDEC(x) ((x <= '9') ? x - '0' : (((uchar)x | 32) - 'a') + 10)

ctype_ioq *
ioqfd_new(ctype_fd fd, ctype_iofn fn)
{
	ctype_arr *ap;
	ctype_ioq *fp;
	uchar *buf;

	if (!(buf = c_std_alloc(IOQSIZ + C_BIOSIZ, sizeof(uchar)))) {
		c_sys_close(fd);
		return nil;
	}

	fp = (void *)buf;
	buf += sizeof(*fp);
	ap = (void *)buf;
	buf += sizeof(*ap);
	c_arr_init(ap, (void *)buf, C_BIOSIZ);
	c_ioq_init(fp, fd, ap, fn);

	return fp;
}

ctype_ioq *
ioq_new(char *file, uint opts, uint mode)
{
	ctype_fd fd;

	if ((fd = c_sys_open(file, opts, mode)) < 0)
		return nil;

	return ioqfd_new(fd, (opts & C_OWRITE) ? c_sys_write : c_sys_read);
}

ctype_arr *
getln(ctype_ioq *fp)
{
	static ctype_arr arr;

	c_arr_trunc(&arr, 0, sizeof(uchar));
	return c_ioq_getln(fp, &arr) > 0 ? &arr : nil;
}

/* var routines */
void
assign(char **sp, char *s)
{
	ctype_arr arr;
	usize n;

	if (*sp)
		return;

	n = c_str_len(s, C_USIZEMAX);
	(void)c_mem_set(&arr, sizeof(arr), 0);
	if ((c_dyn_ready(&arr, n + 1, sizeof(uchar))) < 0)
		c_err_die(1, "c_dyn_ready");

	(void)c_arr_cat(&arr, s, n, sizeof(uchar));
	*sp = c_arr_data(&arr);
}

char *
concat(char *p1, char *p2)
{
	static char buf[C_PATHMAX];
	ctype_arr arr;

	c_arr_init(&arr, buf, sizeof(buf));
	(void)c_arr_fmt(&arr, "%s/%s", p1, p2);
	return c_arr_data(&arr);
}

char *
sdup(char *s, uint n)
{
	static char buf[25];

	(void)c_mem_cpy(buf, n, s);
	buf[n] = 0;
	return buf;
}

char **
avmake2(char *s1, char *s2)
{
	return avmake3(s1, s2, nil);
}

char **
avmake3(char *s1, char *s2, char *s3)
{
	static char *av[4];

	av[0] = s1;
	av[1] = s2;
	av[2] = s3;
	av[3] = nil;
	return av;
}

int
check_sum(ctype_hst *h, char *s)
{
	int i;
	char buf[64];

	c_hsh_digest(h, c_hsh_whirlpool, buf);
	for (i = 0; i < 64; ++i) {
		if (((HDEC(s[0]) << 4) | HDEC(s[1])) != (uchar)buf[i])
			return -1;
		s += 2;
	}

	return 0;
}

/* dir routines */
int
makepath(char *path)
{
	char *s;

	s = path;

	for (;;) {
		if (!(s = c_str_chr(s, C_USIZEMAX, '/')))
			break;
		*s = 0;
		if (c_sys_mkdir(path, 0755) < 0 && errno != C_EEXIST)
			return c_err_warn("c_sys_mkdir %s", path);
		*s++ = '/';
	}

	return 0;
}

int
destroypath(char *path, usize n)
{
	char s[C_PATHMAX];
	char *p;

	c_mem_cpy(s, n, path);
	s[n] = 0;

	if (!(p = c_str_rchr(s, C_USIZEMAX, '/')))
		return 0;
	*p = 0;

	for (;;) {
		(void)c_sys_rmdir(s);
		if (!(p = c_str_rchr(s, C_USIZEMAX, '/')))
			break;
		*p = 0;
	}

	return 0;
}
