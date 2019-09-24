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
	char uid[8];
	char gid[8];
	char namesize[8];
	char size[24];
};

static void
putoctal(ctype_ioq *fp, uint n, uint v)
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

	if (!(fp = ioqfd_new(afd, c_sys_write)))
		c_err_die(1, "ioqfd_new");

	(void)c_ioq_put(fp, MAGIC);

	if (c_dir_open(&dir, av, 0, nil) < 0)
		c_err_die(1, "c_dir_open");

	while ((p = c_dir_read(&dir))) {
		switch (p->info) {
		case C_FSD:
		case C_FSDP:
			continue;
		case C_FSNS:
		case C_FSERR:
			continue;
		}

		if ((fd = c_sys_open(p->path, C_OREAD, 0)) < 0)
			c_err_die(1, "c_sys_open %s", p->path);

		putoctal(fp, 8, p->stp->mode);
		putoctal(fp, 8, p->stp->uid);
		putoctal(fp, 8, p->stp->gid);
		putoctal(fp, 8, p->len);
		putoctal(fp, 24, p->stp->size);
		c_ioq_nput(fp, p->path, p->len);
		if (c_ioq_putfd(fp, fd, p->stp->size) < 0)
			c_err_diex(1, "c_ioq_putfd %s", *av);
	}

	(void)c_ioq_flush(fp);

	return 0;
}

int
unarchive(char *file)
{
	int fd;

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
	char hb[56];
	char *s;

	if (!(fp = ioqfd_new(afd, c_sys_read)))
		c_err_die(1, "ioqfd_new");

	(void)c_ioq_get(fp, hb, sizeof(MAGIC) - 1);
	if (!STRCMP(MAGIC, hb))
		c_err_diex(1, "unkown format");

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_ready(&arr, C_BIOSIZ, sizeof(uchar)) < 0)
		c_err_die(1, "c_dyn_ready");

	p = (void *)hb;
	for (;;) {
		if ((n = c_ioq_get(fp, hb, sizeof(hb))) < (size)sizeof(hb)) {
			if (n <= 0)
				break;
			while (n < (size)sizeof(hb))
				n += c_ioq_get(fp, hb + n, sizeof(hb) - n);
		}
		s = sdup(p->namesize, sizeof(p->namesize));
		n = c_std_strtovl(s, 8, 0, C_UVLONGMAX, nil, nil);

		if (c_dyn_ready(&arr, n, sizeof(uchar)) < 0)
			c_err_die(1, "c_dyn_ready");
		s = c_arr_data(&arr);
		n = c_ioq_get(fp, s, n);
		s[n] = 0;

		(void)makepath(s);
		if ((fd = c_sys_open(s, WOPTS, WMODE)) < 0)
			c_err_die(1, "c_sys_open %s", s);
		s = sdup(p->size, sizeof(p->size));
		n = c_std_strtovl(s, 8, 0, C_UVLONGMAX, nil, nil);
		while (n) {
			len = c_ioq_get(fp, c_arr_data(&arr),
			    C_MIN(C_BIOSIZ, n));
			if (c_sys_allrw(c_sys_write, fd,
			    c_arr_data(&arr), len) < 0)
				c_err_die(1, "c_sys_allrw");
			n -= len;
		}
	}

	return 0;
}
