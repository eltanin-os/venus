#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

static void
findstart(struct cfg *p)
{
	ctype_fd fd;

	fd = c_ioq_fileno(&p->ioq);
	c_sys_seek(fd, 0, C_SEEKSET);
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
	c_arr_init(&p->arr, nil, 0);
	c_ioq_init(&p->ioq, fd, p->buf, sizeof(p->buf), &c_sys_read);
	p->tag = 0;
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
