#include <tertium/cpu.h>
#include <tertium/std.h>

#define ALPHA "abcdefghijklmnopqrstuvxwyz"

struct cfg {
	ctype_arr arr;
	ctype_ioq ioq;
	ctype_fd fd;
	int tag;
	char buf[C_SMALLBIOSIZ];
};

static void
findstart(struct cfg *p)
{
	ctype_fd fd;

	fd = c_ioq_fileno(&p->ioq);
	c_nix_seek(fd, 0, C_SEEKSET);
	c_ioq_init(&p->ioq, fd, p->buf, sizeof(p->buf), &c_sys_read);
}

static char *
getline(struct cfg *p, ctype_status *r)
{
	char *s;

	c_arr_trunc(&p->arr, 0, sizeof(uchar));
	if ((*r = c_ioq_getln(&p->ioq, &p->arr)) < 0)
		return (void *)-1;
	if (!*r) {
		*r = -1;
		return nil;
	}
	s = c_arr_data(&p->arr);
	s[c_arr_bytes(&p->arr) - 1] = 0;
	return s;
}

void
cfginit(struct cfg *p, ctype_fd fd)
{
	c_ioq_init(&p->ioq, fd, p->buf, sizeof(p->buf), &c_sys_read);
	p->tag = 0;
	p->fd = fd;
}

char *
cfgfind(struct cfg *cfg, char *k)
{
	ctype_status r;
	char *p, *s;

	findstart(cfg);
	for (;;) {
		s = getline(cfg, &r);
		if (r < 0)
			return s;
		if (!(p = c_mem_chr(s, c_arr_bytes(&cfg->arr), ':')))
			continue;
		*p++ = 0;
		if (!(c_str_cmp(s, C_USIZEMAX, k)))
			return p;
	}
}

char *
cfgfindtag(struct cfg *cfg, char *k)
{
	ctype_status r;
	char *s;

	if (!cfg->tag) {
		findstart(cfg);
		do {
			s = getline(cfg, &r);
			if (r < 0)
				return s;
			if (!c_str_cmp(s, C_USIZEMAX, k))
				++cfg->tag;
		} while(!cfg->tag);
	}
	s = getline(cfg, &r);
	if (r < 0)
		return s;
	if (*s == '}') {
		cfg->tag = 0;
		return nil;
	}
	if (*s != '\t') {
		cfg->tag = 0;
		errno = C_EINVAL;
		return (void *)-1;
	}
	return ++s;
}

void
cfgclose(struct cfg *cfg)
{
	c_dyn_free(&cfg->arr);
	c_mem_set(cfg, sizeof(*cfg), 0);
}

static char *
getkey(struct cfg *cfg, char *k, int istag)
{
	struct cfg tmp;
	usize len;
	int ch;
	char *e, *p, *s;

	if ((s = (istag ? cfgfindtag : cfgfind)(cfg, k)) == (void *)-1)
		c_err_die(1, "getkey");
	if (!s)
		return nil;

	c_mem_set(&tmp, sizeof(tmp), 0);
	cfginit(&tmp, cfg->fd);
	while ((p = c_str_chr(s, c_arr_bytes(&cfg->arr), '$'))) {
		len = p - s;
		if (!(s = c_str_dup(s, c_arr_bytes(&cfg->arr))))
			c_err_die(1, "c_str_dup");
		p = s + len;
		*p++ = 0;
		for (e = p; *e && c_str_casechr(ALPHA, 26, *e); ++e) ;
		if (*e) {
			ch = *e;
			*e++ = 0;
			c_arr_trunc(&cfg->arr, 0, sizeof(uchar));
			if (c_dyn_fmt(&cfg->arr,
			    "%s%s%c%s", s, getkey(&tmp, p, 0), ch, e) < 0)
				c_err_die(1, "c_dyn_fmt");
		} else {
			c_arr_trunc(&cfg->arr, 0, sizeof(uchar));
			if (c_dyn_fmt(&cfg->arr,
			    "%s%s", s, getkey(&tmp, p, 0)) < 0)
				c_err_die(1, "c_dyn_fmt");
		}
		c_std_free(s);
		s = c_arr_data(&cfg->arr);
	}
	cfgclose(&tmp);
	return s;
}

static void
usage(void)
{
	c_ioq_fmt(ioq1, "usage: %s [-t] key file\n", c_std_getprogname());
	c_std_exit(1);
}

ctype_status
main(int argc, char **argv)
{
	ctype_arr arr;
	struct cfg cfg;
	ctype_fd fd;
	uint mode;
	char *k, *s;

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	mode = 0;

	while (c_std_getopt(argmain, argc, argv, "t")) {
		switch (argmain->opt) {
		case 't':
			mode = 1;
			break;
		default:
			usage();
		}
	}
	argc -= argmain->idx;
	argv += argmain->idx;

	if (argc - 2)
		usage();

	k = *argv++;

	if ((fd = c_nix_fdopen2(*argv, C_OREAD)) < 0)
		c_err_die(1, "c_nix_fdopen2 %s", *argv);

	c_mem_set(&cfg, sizeof(cfg), 0);
	cfginit(&cfg, fd);
	if (mode) {
		c_mem_set(&arr, sizeof(arr), 0);
		if (c_dyn_fmt(&arr, "%s{", k) < 0)
			c_err_die(1, "c_dyn_fmt");
		k = c_arr_data(&arr);
		if ((s = getkey(&cfg, k, 1))) {
			do {
				c_ioq_fmt(ioq1, "%s\n", s);
			} while ((s = getkey(&cfg, nil, 1)));
		}
	} else {
		if ((s = getkey(&cfg, k, 0)))
			c_ioq_fmt(ioq1, "%s\n", s);
	}
	cfgclose(&cfg);
	c_sys_close(fd);
	c_ioq_flush(ioq1);
	return 0;
}
