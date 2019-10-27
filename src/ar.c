#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define MAGIC "3C47B1F3"

#define ROPTS C_OREAD
#define RMODE 0
#define WOPTS (C_OCREATE | C_OWRITE)
#define WMODE C_DEFFILEMODE

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

int
archive(char *file, char **av)
{
	ctype_fd fd;

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
	char buf[C_PATHMAX];

	if (!(fp = new_ioqfd(afd, c_sys_write)))
		c_err_die(1, "ioqfd_new");

	c_ioq_put(fp, MAGIC);

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
		c_ioq_fmt(fp, "%.*uo%.*uo%.*uo%.*s",
		    8, p->stp->mode, 8, p->len, p->len, p->path);

		switch (p->info) {
		case C_FSF:
			if (c_ioq_putfile(fp, p->path) < 0)
				c_err_diex(1, "c_ioq_putfile %s", p->path);
			continue;
		case C_FSSL:
		case C_FSSLN:
			if (c_sys_readlink(buf, sizeof(buf), p->path) < 0)
				c_err_die(1, "c_sys_readlink");
			c_ioq_nput(fp, buf, p->stp->size);
		}
	}

	c_ioq_flush(fp);

	c_dir_close(&dir);
	c_std_free(fp);
	c_sys_close(afd);
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
	struct strheader *p;
	struct header h;
	ctype_arr arr;
	ctype_ioq *fp;
	ctype_fd fd;
	size r;
	char *s;
	char buf[C_BIOSIZ], hbuf[40];

	if (!(fp = new_ioqfd(afd, c_sys_read)))
		c_err_die(1, "ioqfd_new");

	eioqgetall(fp, hbuf, sizeof(MAGIC) - 1);
	if (STRCMP(MAGIC, hbuf))
		c_err_diex(1, "unknown format");

	p = (void *)hbuf;
	for (;;) {
		if ((r = c_ioq_getall(fp, hbuf, sizeof(*p))) < 0)
			c_err_die(1, "ioq_getall");
		if (!r || r != sizeof(*p))
			break;

		s = sdup(p->mode, sizeof(p->mode));
		h.mode = estrtovl(s, 8, 0, 07777);
		s = sdup(p->namesize, sizeof(p->namesize));
		h.namesize = estrtovl(s, 8, 0, C_UINTMAX);
		s = sdup(p->size, sizeof(p->size));
		h.size = estrtovl(s, 8, 0, C_UVLONGMAX);

		c_arr_trunc(&arr, 0, sizeof(uchar));
		if (c_dyn_ready(&arr, h.namesize, sizeof(uchar)) < 0)
			c_err_die(1, "c_dyn_ready");

		s = c_arr_data(&arr);
		eioqgetall(fp, s, h.namesize);

		if (C_ISLNK(h.mode)) {
			eioqgetall(fp, buf, h.size);
			makepath(s);
			if (c_sys_symlink(buf, s))
				c_err_die(1, "c_sys_symlink %s %s", buf, s);
		} else {
			makepath(s);
			if ((fd = c_sys_open(s, WOPTS, WMODE)) < 0)
				c_err_die(1, "c_sys_open %s", s);
			while (h.size) {
				r = C_MIN(sizeof(buf), (usize)r);
				r = eioqget(fp, buf, r);
				if (c_sys_allrw(c_sys_write, fd, buf, r) < 0)
					c_err_die(1, "c_sys_allrw");
				h.size -= r;
			}
			if (c_sys_fchmod(fd, h.mode) < 0)
				c_err_die(1, "c_sys_fchmod");
			c_sys_close(fd);
		}
	}

	c_dyn_free(&arr);
	c_std_free(fp);
	c_sys_close(afd);
	return 0;
}
