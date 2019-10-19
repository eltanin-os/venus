#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define MAGIC "3C47B1F3"

#define ROPTS C_OREAD
#define RMODE 0
#define WOPTS (C_OCREATE | C_OWRITE)
#define WMODE C_DEFFILEMODE

struct header {
	char mode[8];
	char namesize[8];
	char size[24];
};

static void
putoctal(ctype_ioq *fp, uint n, uvlong v)
{
	ctype_arr arr;
	char buf[24];

	(void)c_mem_set(buf, sizeof(buf), 0);
	c_arr_init(&arr, buf, sizeof(buf));
	(void)c_arr_fmt(&arr, "%.*uo", n, v);
	(void)c_ioq_nput(fp, c_arr_data(&arr), n);
}

int
archive(char *file, char **av)
{
	int fd;

	if (!STRCMP("<stdout>", file))
		return archivefd(C_FD1, av);

	if ((fd = c_sys_open(file, WOPTS, WMODE)) < 0)
		c_err_die(1, "c_sys_open %s", file);

	return archivefd(fd, av);
}

int
archivefd(int afd, char **av)
{
	ctype_dir dir;
	ctype_dent *p;
	ctype_ioq *fp;
	int fd;
	char buf[C_PATHMAX];

	if (!(fp = ioqfd_new(afd, c_sys_write)))
		c_err_die(1, "ioqfd_new");

	(void)c_ioq_put(fp, MAGIC);

	if (!av || !*av)
		return 0;

	if (c_dir_open(&dir, av, 0, nil) < 0)
		c_err_die(1, "c_dir_open");

	while ((p = c_dir_read(&dir))) {
		switch (p->info) {
		case C_FSD:
		case C_FSDP:
		case C_FSNS:
		case C_FSERR:
		case C_FSDEF:
			continue;
		}
		putoctal(fp, 8, p->stp->mode);
		putoctal(fp, 8, p->len);
		putoctal(fp, 24, p->stp->size);
		c_ioq_nput(fp, p->path, p->len);

		switch (p->info) {
		case C_FSF:
			if ((fd = c_sys_open(p->path, C_OREAD, 0)) < 0)
				c_err_die(1, "c_sys_open %s", p->path);
			if (c_ioq_putfd(fp, fd, p->stp->size) < 0)
				c_err_diex(1, "c_ioq_putfd %s", *av);
			c_sys_close(fd);
			continue;
		case C_FSSL:
		case C_FSSLN:
			if (c_sys_readlink(buf, sizeof(buf), p->path) < 0)
				c_err_die(1, "c_sys_readlink");
			c_ioq_nput(fp, buf, p->stp->size);
		}
	}

	(void)c_ioq_flush(fp);

	return 0;
}

int
unarchive(char *file)
{
	int fd;

	if (!STRCMP("<stdin>", file))
		return unarchivefd(C_FD0);

	if ((fd = c_sys_open(file, ROPTS, RMODE)) < 0)
		c_err_die(1, "c_sys_open %s", file);

	return unarchivefd(fd);
}

int
unarchivefd(int afd)
{
	struct header *p;
	ctype_ioq *fp;
	ctype_arr arr;
	size len, n;
	int fd;
	char hb[40];
	char buf[C_BIOSIZ];
	char *s;

	if (!(fp = ioqfd_new(afd, c_sys_read)))
		c_err_die(1, "ioqfd_new");

	(void)c_ioq_get(fp, hb, sizeof(MAGIC) - 1);
	if (STRCMP(MAGIC, hb))
		c_err_diex(1, "%s %s: unkown format", MAGIC, hb);

	(void)c_mem_set(&arr, sizeof(arr), 0);
	p = (void *)hb;
	for (;;) {
		s = hb;
		n = sizeof(hb);
		while (n) {
			len = c_ioq_get(fp, s, n);
			if (len <= 0) goto done;
			n -= len;
			s += len;
		}
		s = sdup(p->namesize, sizeof(p->namesize));
		n = c_std_strtovl(s, 8, 0, C_SIZEMAX, nil, nil);
		c_arr_trunc(&arr, 0, sizeof(uchar));
		if (c_dyn_ready(&arr, n, sizeof(uchar)) < 0)
			c_err_die(1, "c_dyn_ready");

		s = c_arr_data(&arr);
		s[n] = 0;
		while (n) {
			len = c_ioq_get(fp, s, n);
			if (len <= 0) goto done;
			n -= len;
			s += len;
		}

		s = sdup(p->mode, sizeof(p->mode));
		n = c_std_strtovl(s, 8, 0, C_SIZEMAX, nil, nil);
		if (C_ISLNK(n)) {
			s = sdup(p->size, sizeof(p->size));
			n = c_std_strtovl(s, 8, 0, C_SIZEMAX, nil, nil);
			s = buf;
			s[n] = 0;
			while (n) {
				len = c_ioq_get(fp, s, n);
				if (len <= 0) goto done;
				n -= len;
				s += len;
			}
			makepath(c_arr_data(&arr));
			if (c_sys_symlink(buf, c_arr_data(&arr)) < 0)
				c_err_die(1, "c_sym_link %s %s",
				    buf, c_arr_data(&arr));
		} else if (C_ISREG(n)) {
			s = c_arr_data(&arr);
			(void)makepath(s);
			if ((fd = c_sys_open(s, WOPTS, WMODE)) < 0)
				c_err_die(1, "c_sys_open %s", s);
			s = sdup(p->size, sizeof(p->size));
			n = c_std_strtovl(s, 8, 0, C_SIZEMAX, nil, nil);
			while (n) {
				len = c_ioq_get(fp, buf, C_MIN(C_BIOSIZ, n));
				if (len <= 0) goto done;
				if (c_sys_allrw(c_sys_write, fd, buf, len) < 0)
					c_err_die(1, "c_sys_allrw");
				n -= len;
			}
			s = sdup(p->mode, sizeof(p->mode));
			n = c_std_strtovl(s, 8, 0, C_SIZEMAX, nil, nil);
			if (c_sys_fchmod(fd, n) < 0)
				c_err_die(1, "c_sys_fchmod");
			(void)c_sys_close(fd);
		}
	}
done:
	c_dyn_free(&arr);
	(void)c_sys_close(fp->fd);
	c_std_free(fp);

	return 0;
}
