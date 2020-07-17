#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define MAGIC "3EA81233"
#define PATHSIZ(a) (((a) << 1) + 18)

struct header {
	u32int mode;
	u32int namesize;
	u64int size;
};

static void
store32(ctype_ioq *p, u32int x)
{
	char tmp[4];

	c_ioq_nput(p, c_uint_32pack(tmp, x), sizeof(x));
}

static void
store64(ctype_ioq *p, u64int x)
{
	char tmp[8];

	c_ioq_nput(p, c_uint_64pack(tmp, x), sizeof(x));
}

static void
putfile(ctype_ioq *ioq, ctype_arr *ap, char *s, usize len, ctype_stat *stp)
{
	store32(ioq, stp->mode);
	store32(ioq, len);
	store64(ioq, stp->size);
	c_ioq_nput(ioq, s, len);
	if (C_ISREG(stp->mode)) {
		if (c_ioq_putfile(ioq, s) < 0)
			c_err_diex(1, "c_ioq_putfile %s", s);
	} else if (C_ISLNK(stp->mode)) {
		c_arr_trunc(ap, 0, sizeof(uchar));
		if (c_dyn_ready(ap, stp->size + 1, sizeof(uchar)) < 0)
			c_err_die(1, "c_dyn_ready");
		if (c_sys_readlink(s, c_arr_data(ap), c_arr_total(ap)) < 0)
			c_err_die(1, "c_sys_readlink");
		c_ioq_nput(ioq, c_arr_data(ap), stp->size);
	}
}

ctype_status
archivefd(ctype_fd afd, char **av)
{
	ctype_stat st;
	ctype_dir dir;
	ctype_dent *p;
	ctype_ioq ioq;
	ctype_arr arr, line;
	usize len;
	char *s;
	char buf[C_BIOSIZ];

	c_ioq_init(&ioq, afd, buf, sizeof(buf), &c_sys_write);
	c_ioq_put(&ioq, MAGIC);
	c_mem_set(&arr, sizeof(arr), 0);
	if (!*av) {
		c_mem_set(&line, sizeof(line), 0);
		while (c_ioq_getln(ioq0, &line) > 0) {
			s = c_arr_data(&line);
			len = c_arr_bytes(&line);
			s[--len] = 0;
			if (c_sys_lstat(&st, s) < 0)
				c_err_die(1, "c_sys_lstat %s", s);
			putfile(&ioq, &arr, s, len, &st);
			c_arr_trunc(&line, 0, sizeof(uchar));
		}
		c_dyn_free(&line);
	} else {
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
			putfile(&ioq, &arr, p->path, p->len, p->stp);
		}
		c_dir_close(&dir);
	}
	c_dyn_free(&arr);
	c_ioq_flush(&ioq);
	return 0;
}

ctype_status
archive(char *file, char **av)
{
	ctype_status r;
	ctype_fd fd;

	if (!CSTRCMP("<stdin>", file))
		return archivefd(C_FD1, av);

	fd = eopen(file, WOPTS, WMODE);
	r = archivefd(fd, av);
	c_sys_close(fd);
	return r;
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

ctype_status
unarchivefd(ctype_fd afd)
{
	struct header h;
	ctype_arr arr, dest;
	ctype_ioq ioq;
	ctype_fd fd;
	size r;
	char *d, *s;
	char buf[C_BIOSIZ];
	char hb[16];

	c_ioq_init(&ioq, afd, buf, sizeof(buf), &c_sys_read);
	getall(&ioq, hb, sizeof(MAGIC) - 1);
	if (STRCMP(MAGIC, hb))
		c_err_diex(1, "unknown format");

	c_mem_set(&arr, sizeof(arr), 0);
	for (;;) {
		if (!c_ioq_feed(&ioq))
			break;

		getall(&ioq, hb, sizeof(hb));
		s = hb;
		h.mode = c_uint_32unpack(s);
		s += sizeof(h.mode);
		h.namesize = c_uint_32unpack(s);
		s += sizeof(h.namesize);
		h.size = c_uint_64unpack(s);

		c_arr_trunc(&arr, 0, sizeof(uchar));
		if (c_dyn_ready(&arr, h.namesize, sizeof(uchar)) < 0)
			c_err_die(1, "c_dyn_ready");

		s = c_arr_data(&arr);
		getall(&ioq, s, h.namesize);
		s[h.namesize] = 0;
		makepath(s);

		c_arr_trunc(&dest, 0, sizeof(uchar));
		if (c_dyn_ready(&dest, PATHSIZ(h.namesize), sizeof(uchar)) < 0)
			c_err_die(1, "c_dyn_ready");

		d = c_arr_data(&dest);
		if (c_mem_chr(s, h.namesize, '/')) {
			c_gen_dirname(c_mem_cpy(d, h.namesize + 1, s));
			c_arr_fmt(&dest, "%s/", d);
		}
		c_arr_fmt(&dest, "VENUS@XXXXXXXXX");
		r = c_arr_bytes(&dest);
		s = c_mem_cpy(d + r + 1, h.namesize + 1, s);
		if (C_ISLNK(h.mode)) {
			c_arr_trunc(&arr, 0, sizeof(uchar));
			if (c_dyn_ready(&arr, h.size, sizeof(uchar)) < 0)
				c_err_die(1, "c_dyn_ready");
			getall(&ioq, c_arr_data(&arr), h.size);
			*((char *)c_arr_data(&arr) + h.size) = 0;
			for (;;) {
				c_rand_name(d + (r - 9), 9);
				if (c_sys_symlink(c_arr_data(&arr), d) < 0) {
					if (errno == C_EEXIST)
						continue;
					c_err_die(1, "c_sys_symlink %s %s",
					    c_arr_data(&arr), d);
				}
				break;
			}
		} else {
			if ((fd = c_std_mktemp(d, r, 0)) < 0)
				c_err_die(1, "c_std_mktemp %s", d);
			while (h.size) {
				if ((r = c_ioq_feed(&ioq)) <= 0)
					c_err_diex(1, "incomplete file");
				r = C_MIN(r, (size)h.size);
				if (c_std_allrw(c_sys_write, fd,
				    c_ioq_peek(&ioq), r) < 0)
					c_err_die(1, "c_std_allrw");
				h.size -= r;
				c_ioq_seek(&ioq, r);
			}
			if (c_sys_fchmod(fd, h.mode) < 0)
				c_err_die(1, "c_sys_fchmod");
			c_sys_close(fd);
		}
		if (c_sys_rename(d, s) < 0)
			c_err_die(1, "c_sys_rename %s %s", d, s);
	}
	c_dyn_free(&arr);
	c_dyn_free(&dest);
	return 0;
}

ctype_status
unarchive(char *file)
{
	ctype_status r;
	ctype_fd fd;

	if (!CSTRCMP("<stdout>", file))
		return unarchivefd(C_FD0);

	fd = eopen(file, ROPTS, RMODE);
	r = unarchivefd(fd);
	c_sys_close(fd);
	return r;
}
