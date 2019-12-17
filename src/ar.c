#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define MAGIC "3C47B1F3"

struct strheader {
	char mode[8];
	char namesize[8];
	char size[24];
};

struct header {
	u32int mode;
	u32int namesize;
	u64int size;
};

static void
putoctal(ctype_ioq *p, u64int x, int n)
{
	char buf[24];

	c_mem_set(buf, sizeof(buf), 0);
	n -= c_ioq_fmt(p, "%.*lluo", n, x);
	c_ioq_nput(p, buf, n);
}

ctype_status
archivefd(ctype_fd afd, char **av)
{
	ctype_arr arr;
	ctype_dir dir;
	ctype_dent *p;
	ctype_ioq ioq;
	char buf[C_BIOSIZ];

	c_ioq_init(&ioq, afd, buf, sizeof(buf), &c_sys_write);
	c_ioq_put(&ioq, MAGIC);
	if (!*av)
		return 0;

	if (c_dir_open(&dir, av, 0, nil) < 0)
		c_err_die(1, "c_dir_open");

	c_mem_set(&arr, sizeof(arr), 0);
	while ((p = c_dir_read(&dir))) {
		switch (p->info) {
		case C_FSD:
		case C_FSDP:
		case C_FSNS:
		case C_FSERR:
		case C_FSDEF:
			continue;
		}
		putoctal(&ioq, p->stp->mode, 8);
		putoctal(&ioq, p->len, 8);
		putoctal(&ioq, p->stp->size, 24);
		c_ioq_nput(&ioq, p->path, p->len);
		switch (p->info) {
		case C_FSF:
			if (c_ioq_putfile(&ioq, p->path) < 0)
				c_err_diex(1, "c_ioq_putfile %s", p->path);
			break;
		case C_FSSL:
		case C_FSSLN:
			c_arr_trunc(&arr, 0, sizeof(uchar));
			if (c_dyn_ready(&arr, p->stp->size, sizeof(uchar)) < 0)
				c_err_die(1, "c_dyn_ready");
			if (c_sys_readlink(c_arr_data(&arr),
			    p->stp->size, p->path) < 0)
				c_err_die(1, "c_sys_readlink");
			c_ioq_nput(&ioq, c_arr_data(&arr), p->stp->size);
		}
	}
	c_dir_close(&dir);
	c_dyn_free(&arr);
	c_ioq_flush(&ioq);
	c_sys_close(afd);
	return 0;
}

ctype_status
archive(char *file, char **av)
{
	ctype_fd fd;

	if (!CSTRCMP("<stdin>", file))
		return archivefd(C_FD1, av);

	if ((fd = c_sys_open(file, WOPTS, WMODE)) < 0)
		c_err_die(1, "c_sys_open %s", file);

	return archivefd(fd, av);
}

static void
getall(ctype_ioq *p, char *s, usize n)
{
	size r;

	if ((r = c_ioq_getall(p, s, n)) < 0)
		c_err_die(1, "c_ioq_getall");
	if ((usize)r != n)
		c_err_diex(1, "file incomplete");
}

static char *
sdup(char *s, int n)
{
	static char buf[25];

	c_mem_cpy(buf, n, s);
	buf[n] = 0;
	return buf;
}

ctype_status
unarchivefd(ctype_fd afd)
{
	struct strheader *p;
	struct header h;
	ctype_arr arr;
	ctype_ioq ioq;
	ctype_fd fd;
	size r;
	char *s;
	char buf[C_BIOSIZ];
	char hbuf[40];

	c_ioq_init(&ioq, afd, buf, sizeof(buf), &c_sys_read);
	getall(&ioq, hbuf, sizeof(MAGIC) - 1);
	if (STRCMP(MAGIC, hbuf))
		c_err_diex(1, "unknown format");

	c_mem_set(&arr, sizeof(arr), 0);
	p = (void *)hbuf;
	for (;;) {
		if (!c_ioq_feed(&ioq))
			break;

		getall(&ioq, hbuf, sizeof(hbuf));
		s = sdup(p->mode, sizeof(p->mode));
		h.mode = estrtovl(s, 8, 0, 0777777);
		s = sdup(p->namesize, sizeof(p->namesize));
		h.namesize = estrtovl(s, 8, 0, C_UINTMAX);
		s = sdup(p->size, sizeof(p->size));
		h.size = estrtovl(s, 8, 0, C_VLONGMAX);

		c_arr_trunc(&arr, 0, sizeof(uchar));
		if (c_dyn_ready(&arr, h.namesize, sizeof(uchar)) < 0)
			c_err_die(1, "c_dyn_ready");

		s = c_arr_data(&arr);
		getall(&ioq, s, h.namesize);
		s[h.namesize] = 0;
		makepath(s);
		if (C_ISLNK(h.mode)) {
			s = estrdup(s);
			c_arr_trunc(&arr, 0, sizeof(uchar));
			if (c_dyn_ready(&arr, h.size, sizeof(uchar)) < 0)
				c_err_die(1, "c_dyn_ready");
			c_sys_unlink(s);
			getall(&ioq, c_arr_data(&arr), h.size);
			if (c_sys_symlink(c_arr_data(&arr), s))
				c_err_die(1, "c_sys_symlink %s %s",
				    c_arr_data(&arr), s);
			c_std_free(s);
		} else {
			if ((fd = c_sys_open(s, WOPTS, WMODE)) < 0)
				c_err_die(1, "c_sys_open %s", s);
			while (h.size) {
				if ((r = c_ioq_feed(&ioq)) <= 0)
					c_err_diex(1, "incomplete file");
				r = C_MIN(r, (size)h.size);
				s = c_ioq_peek(&ioq);
				if (c_std_allrw(c_sys_write, fd, s, r) < 0)
					c_err_die(1, "c_std_allrw");
				h.size -= r;
				c_ioq_seek(&ioq, r);
			}
			if (c_sys_fchmod(fd, h.mode) < 0)
				c_err_die(1, "c_sys_fchmod");
			c_sys_close(fd);
		}
	}
	c_dyn_free(&arr);
	c_sys_close(afd);
	return 0;
}

ctype_status
unarchive(char *file)
{
	ctype_fd fd;

	if (!CSTRCMP("<stdout>", file))
		return unarchivefd(C_FD0);

	if ((fd = c_sys_open(file, ROPTS, RMODE)) < 0)
		c_err_die(1, "c_sys_open %s", file);

	return unarchivefd(fd);
}
