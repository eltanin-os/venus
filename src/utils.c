#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"
#include "config.h"

#define HDEC(x) ((x <= '9') ? x - '0' : (((uchar)x | 32) - 'a') + 10)

/* dynamic routines */
ctype_arr *
getln(ctype_ioq *fp)
{
	static ctype_arr arr;

	c_arr_trunc(&arr, 0, sizeof(uchar));
	return c_ioq_getln(fp, &arr) > 0 ? &arr : nil;
}

ctype_ioq *
new_ioqfd(ctype_fd fd, ctype_iofn func)
{
	ctype_ioq *fp;
	uchar *p;

	if (!(p = c_std_alloc(sizeof(*fp) + C_BIOSIZ, sizeof(uchar))))
		return nil;

	fp = (void *)p;
	p += sizeof(*fp);
	c_ioq_init(fp, fd, (void *)p, C_BIOSIZ, func);
	return fp;
}

/* fail routines */
void
efchdir(ctype_fd fd)
{
	if (c_sys_fchdir(fd) < 0)
		c_err_die(1, "c_sys_fchdir");
}

void
eioqgetall(ctype_ioq *p, char *b, usize n)
{
	size r;

	if ((r = c_ioq_getall(p, b, n)) < 0)
		c_err_die(1, "c_ioq_getall");
	if ((usize)r != n)
		c_err_diex(1, "incomplete file");
}

size
eioqget(ctype_ioq *p, char *b, usize n)
{
	size r;

	if ((r = c_ioq_get(p, b, n)) < 0)
		c_err_die(1, "c_ioq_get");
	if (!r)
		c_err_diex(1, "incomplete file");

	return r;
}

vlong
estrtovl(char *p, int b, vlong l, vlong h)
{
	vlong rv;
	int e;

	rv = c_std_strtovl(p, b, l, h, nil, &e);

	if (e < 0)
		c_err_die(1, "c_std_strtovl %s", p);

	return rv;
}

/* exec routines */
void
uncompress(ctype_fd dirfd, ctype_fd fd)
{
	ctype_fd fds[2];
	char **av;

	if (c_sys_pipe(fds) < 0)
		c_err_die(1, "c_sys_pipe");

	switch (c_sys_fork()) {
	case -1:
		c_err_die(1, "c_sys_fork");
	case 0:
		c_sys_dup(fd, 0);
		c_sys_dup(fds[1], 1);
		c_sys_close(fds[0]);
		c_sys_close(fds[1]);
		av = avmake3(inflate, nil, nil);
		c_exc_run(*av, av);
		c_err_die(1, "c_exc_run %s", inflate);
	}
	c_sys_close(fds[1]);
	efchdir(dirfd);
	unarchivefd(fds[0]);
	efchdir(fd_dot);
	c_sys_close(fds[0]);
	c_sys_wait(nil);
}

static char *
urlencode(char *url)
{
	static char buf[1024];
	usize i;

	i = 0;
	for (;;) {
		switch (*url) {
		case '\0':
			buf[i] = 0;
			return buf;
		case '#':
			c_mem_cpy(buf + i, 3, "%23");
			i += 3;
			break;
		default:
			buf[i] = *url;
			++i;
		}
		++url;
	}
}

void
dofetch(ctype_fd dirfd, char *url)
{
	char **av;
	char *f;

	f = c_gen_basename(sdup(url, URLMAX));
	url = urlencode(url);

	switch (c_sys_fork()) {
	case -1:
		c_err_die(1, "c_sys_fork");
	case 0:
		efchdir(dirfd);
		av = avmake3(fetch, f, url);
		c_exc_run(*av, av);
		c_err_die(1, "c_sys_run %s", fetch);
	}
	c_sys_wait(nil);
}

/* var routines */
void
assign(char **sp, char *s)
{
	usize n;

	if (!s)
		return;

	n = c_str_len(s, C_USIZEMAX);

	if (!(*sp = c_std_alloc(n, sizeof(uchar))))
		c_err_die(1, "c_std_alloc");

	c_mem_cpy(*sp, n, s);
}

char **
avmake3(char *s1, char *s2, char *s3)
{
	static char *av[8];
	int i;
	char *s;

	av[0] = s = s1;
	i = 1;
	for (;;) {
		if (!(s = c_str_chr(s, C_USIZEMAX, ' ')))
			break;
		*s++ = 0;
		av[i] = s;

		if (++i > 5)
			c_err_diex(1, "too much arguments");
	}

	av[i++] = s2;
	av[i++] = s3;
	av[i] = nil;

	return av;
}

char *
concat(char *p1, char *p2)
{
	static char buf[C_PATHMAX];
	ctype_arr arr;
	char *sep;

	c_arr_init(&arr, buf, sizeof(buf));
	sep = p1[c_str_len(p1, C_USIZEMAX) - 1] == '/' ? "" : "/";
	c_arr_fmt(&arr, "%s%s%s", p1, sep, p2);
	return c_arr_data(&arr);
}

char *
sdup(char *s, uint n)
{
	static char buf[C_PATHMAX];

	c_mem_cpy(buf, n, s);
	buf[n] = 0;
	return buf;
}

/* check routines */
static int
docheck_whirlpool(ctype_fd fd, char *s, u64int siz)
{
	ctype_hst hs;
	ctype_stat st;
	int i;
	char buf[64];

	if (c_sys_fstat(&st, fd) < 0)
		c_err_die(1, "c_sys_fstat");

	if (st.size != siz)
		return -1;

	if (c_hsh_putfd(&hs, c_hsh_whirlpool, fd, st.size) < 0)
		c_err_die(1, "c_hsh_putfd");

	c_hsh_digest(&hs, c_hsh_whirlpool, buf);
	for (i = 0; i < 64; ++i) {
		if (((HDEC(s[0]) << 4) | HDEC(s[1])) != (uchar)buf[i])
			return -1;
		s += 2;
	}

	return 0;
}

void
checksum_whirlpool(ctype_fd dirfd, char *s)
{
	ctype_arr *ap;
	ctype_fd fd;
	ctype_ioq *fp;
	u64int n;
	int check;
	char *sum, *siz;

	if (!(fp = new_ioqfd(dirfd, c_sys_read)))
		c_err_die(1, "new_ioqfd");

	check = 0;
	while ((ap = getln(fp))) {
		sum = c_arr_data(ap);
		n = c_arr_bytes(ap);
		sum[n - 1] = 0;
		if (!(sum = c_str_chr(sum, c_arr_bytes(ap), ' ')))
			c_err_diex(1, CHKSUMFILE ": wrong format");
		n -= sum - (char *)c_arr_data(ap);
		*sum++ = 0;
		if (c_str_cmp(c_arr_data(ap), n, s))
			continue;
		++check;
		if (!(siz = c_str_chr(sum, n, ' ')))
			c_err_diex(1, CHKSUMFILE ": wrong format");
		*siz++ = 0;
		if ((fd = c_sys_open(s, C_OREAD, 0)) < 0)
			c_err_die(1, "c_sys_open %s", s);
		n = estrtovl(siz, 8, 0, C_VLONGMAX);
		if (docheck_whirlpool(fd, sum, n) < 0)
			c_err_diex(1, "%s: checksum mismatch", s);
		break;
	}
	if (!check)
		c_err_die(1, "%s: have no checksum", s);
}

int
checksum_fletcher32(char *file, char *sum, u64int siz)
{
	ctype_fd fd;
	ctype_hst hs;
	ctype_stat st;
	size r;
	char buf[C_PATHMAX];

	if (c_sys_lstat(&st, file) < 0)
		c_err_die(1, "c_sys_stat %s", file);

	if (st.size != siz)
		return -1;

	if (C_ISLNK(st.mode)) {
		if ((r = c_sys_readlink(buf, sizeof(buf), file)) < 0)
			c_err_die(1, "c_sys_readlink %s", file);
		c_hsh_all(&hs, c_hsh_fletcher32, buf, r);
	} else {
		if ((fd = c_sys_open(file, C_OREAD, 0)) < 0)
			c_err_die(1, "c_sys_open %s", file);

		if (c_hsh_putfd(&hs, c_hsh_fletcher32, fd, st.size) < 0)
			c_err_die(1, "c_hsh_putfd %s", file);
	}

	if (estrtovl(sum, 8, 0, C_UINTMAX) != c_hsh_state0(&hs))
		return -1;

	return 0;
}

/* dir routines */
int
makepath(char *path)
{
	char *s;

	path = sdup(path, c_str_len(path, C_USIZEMAX));
	s = path + (*path == '/');

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
		c_sys_rmdir(s);
		if (!(p = c_str_rchr(s, C_USIZEMAX, '/')))
			break;
		*p = 0;
	}

	return 0;
}
