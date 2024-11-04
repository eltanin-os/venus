#include <tertium/cpu.h>
#include <tertium/std.h>

#define META(x) ((*(x) == '$') ? INFO_META : 0)
#define ISEXPLICIT(x) \
((x) == '_' || ((x) != '}' && c_utf8_isgraph((x))))
#define ISIMPLICIT(x) \
((x) == '_' || c_utf8_isalnum((x)))

/* flags */
enum {
	LFLAG = 1 << 0,
	KFLAG = 1 << 1,
	VFLAG = 1 << 2,
	XFLAG = 1 << 3,
};

/* modes */
enum {
	MOD_KEYVAL,
	MOD_ALL,
	MOD_BLOCK,
	MOD_SELF,
};

/* info */
enum {
	INFO_DEFFERING = 1 << 0,
	INFO_DEFFERED  = 1 << 1,
	INFO_KEYVAL    = 1 << 2,
	INFO_BLOCK     = 1 << 3,
	INFO_META      = 1 << 4,
	INFO_REF       = 1 << 5,
};

struct entry {
	ctype_node *data;
	uint info;
	char *ref;
};

struct ref {
	char **args;
	usize n;
	int free;
};

static ctype_kvtree tree;
static uint opts;

static ctype_node *deference(char *);

/* util routines */
static usize
graphspan(char *s, usize n)
{
	char *p;
	for (p = s; n && *p && c_utf8_isgraph(*p); --n, ++p) ;
	return p - s;
}

static usize
spacespan(char *s, usize n)
{
	char *p;
	for (p = s; n && *p && c_utf8_isspace(*p); --n, ++p) ;
	return p - s;
}

static int
explicitspan(char *s, usize n)
{
	char *p;
	for (p = s; n && *p && ISEXPLICIT(*p); --n, ++p) ;
	return p - s;
}

static int
implicitspan(char *s, usize n)
{
	char *p;
	for (p = s; n && *p && ISIMPLICIT(*p); --n, ++p) ;
	return p - s;
}

static ctype_status
isvariable(char *s, usize n)
{
	if (*s == '$') {
		++s;
		--n;
	}
	return !(graphspan(s, n) == n);
}

static ctype_status
checkprint(struct entry *e)
{
	if (opts & KFLAG) {
		if (!(e->data || (e->info & INFO_REF))) return 1;
	} else if (opts & VFLAG) {
		if (e->info & INFO_BLOCK) return 1;
		if (e->data || (e->info & INFO_REF)) return 1;
	}
	return 0;
}

/* build tree routines */
static void
epush(char *key, int type, void *data)
{
	struct entry *e;

	if (!(e = c_adt_kvget(&tree, key))) {
		if (!(e = c_std_calloc(1, sizeof(*e)))) c_err_die(1, nil);
	}

	if (type & INFO_META) {
		if (!e->ref) e->ref = c_str_dup(key, -1);
	}

	if (type & INFO_REF) {
		e->ref = data;
	} else if (type & INFO_BLOCK) {
		c_adt_lpush(&e->data, data);
	} else if ((type & INFO_KEYVAL) && data) {
		if (!(data = c_adt_lnew(data, c_str_len(data, -1) + 1))) {
			c_err_diex(1, "no memory");
		}
		while (e->data) c_adt_lfree(c_adt_lpop(&e->data));
		e->data = data;
	}
	e->info = (e->info & ~(INFO_BLOCK | INFO_KEYVAL)) | type;
	if (c_adt_kvadd(&tree, key, e) < 0) c_err_diex(1, "no memory");
}

static int
machine(int state, char *s, usize n, usize diff)
{
	static ctype_arr key;
	static ctype_node *np;
	static usize indent;
	int tmp;
	char *p;

	if (state == -1) {
		c_dyn_free(&key);
		return 0;
	}
	if (n == 0) return state;
	if (s[0] == '#' && !(opts & XFLAG)) return state;

	/* key | block */
	if (state == 0) {
		if (s[n - 1] == '{') {
			if (isvariable(s, n - 1)) {
				c_err_diex(1, "bad formatted file");
			}
			c_arr_trunc(&key, 0, sizeof(uchar));
			if (c_dyn_fmt(&key, "%.*s", n - 1, s) < 0) {
				c_err_diex(1, "no memory");
			}
			return 1;
		} else {
			if ((p = c_mem_chr(s, n, ':'))) {
				n = p - s;
				*p++ = 0;
				if (isvariable(s, n)) {
					c_err_diex(1, "bad formatted file");
				}
			}
			epush(s, INFO_KEYVAL | META(s), p);
			return 0;
		}
	}
	/* block */
	if (graphspan(s, n) != n) {
		tmp = 0;
		/* XXX: maybe use an heuristic, when "-x" is given,
		 * to deal with dangling '{}' */
		for (p = s; n && *p; ++p) {
			if (*p == '{') {
				++tmp;
			} else if (*p == '}') {
				--tmp;
			}
		}
	} else {
		if (s[0] == '}') {
			if (state > 1) {
				--state;
				if (opts & LFLAG) return state;
			} else {
				p = c_arr_data(&key);
				epush(p, INFO_BLOCK | META(p), np);
				np = nil;
				return 0;
			}
		}
		tmp = (s[n - 1] == '{');
		if (opts & LFLAG) {
			if (tmp) {
				s[--n] = 0;
			} else if ((p = c_str_chr(s, n, ':'))) {
				*p = 0;
			}
			if (state > 1) return state + tmp;
		}
	}
	if (!np) indent = diff;
	diff -= indent;
	if (c_adt_lpush(&np, c_adt_lnew(s - diff, (n + diff) + 1)) < 0) {
		c_err_diex(1, "no memory");
	}
	return state + tmp;
}

static void
initconf(ctype_fd fd)
{
	ctype_ioq ioq;
	ctype_arr arr;
	size diff, ret;
	int state;
	char buf[C_IOQ_BSIZ];
	char *s;

	state = 0;
	c_ioq_init(&ioq, fd, buf, sizeof(buf), &c_nix_fdread);
	c_mem_set(&arr, sizeof(arr), 0);
	while ((ret = c_ioq_getln(&arr, &ioq)) > 0) {
		ret = c_arr_bytes(&arr) - 1;
		c_arr_trunc(&arr, ret, sizeof(uchar));
		/* init */
		s = c_arr_data(&arr);
		diff = spacespan(s, ret);
		/* "parse" */
		state = machine(state, s + diff, ret - diff, diff);
		c_arr_trunc(&arr, 0, sizeof(uchar));
	}

	if (state) c_err_diex(1, "bad formatted file");
	if (ret < 0) c_err_die(1, "failed to read file");

	machine(-1, 0, 0, 0);
	c_dyn_free(&arr);
}

static ctype_status
initmeta(char *s, void *data)
{
	struct entry *e, *re;
	ctype_node *wp;
	(void)s;
	e = data;
	if (!(e->info & INFO_META)) return 0;
	if (!(re = c_adt_kvget(&tree, s + 1))) return 0;
	if (!(re->data)) return 0;
	wp = (re->data)->next;
	do {
		epush(wp->p, (e->info & ~INFO_META) | INFO_REF, e->ref);
	} while ((wp = wp->next)->prev);
	return 0;
}

/* expansion routines */
static ctype_status
expand(struct entry *e)
{
	ctype_node *np, *wp;
	if (e->info & INFO_DEFFERING) {
		return -1;
	} else if (e->info & INFO_DEFFERED) {
		return 0;
	} else if ((e->info & INFO_REF) && !e->data) {
		e->info |= INFO_DEFFERED;
		return 0;
	}
	e->info |= INFO_DEFFERING;
	wp = (e->data)->next;
	do {
		if (!(np = deference(wp->p))) continue;
		if (!wp->prev) {
			/* head */
			if (wp->next == wp) {
				e->data = np;
			} else {
				wp->next->prev = np;
				(e->data)->next = np->next;
				np->next = wp->next;
			}
		} else if (wp == e->data) {
			/* tail */
			(wp->prev)->next = np->next;
			(np->next)->prev = wp->prev;
			np->next = wp->next;
			e->data = np;
		} else {
			/* mid */
			(wp->prev)->next = np->next;
			(wp->next)->prev = np;
			(np->next)->prev = wp->prev;
			np->next = wp->next;
		}
		c_std_free(wp->p);
		c_std_free(wp);
		wp = np;
	} while ((wp = wp->next)->prev);
	e->info = (e->info & ~INFO_DEFFERING) | INFO_DEFFERED;
	return 0;
}

static void
combine(ctype_node **list, struct ref *refs, usize n)
{
	ctype_arr arr;
	usize *idxs;
	size i;

	if (!(idxs = c_std_calloc(n, sizeof(*idxs)))) c_err_die(1, nil);

	for (;;) {
		c_mem_set(&arr, sizeof(arr), 0);
		for (i = 0; (usize)i < n; ++i) {
			if (c_dyn_fmt(&arr, "%s", refs[i].args[idxs[i]]) < 0) {
				c_err_die(1, nil);
			}
		}
		c_dyn_shrink(&arr, sizeof(uchar));
		if (c_adt_lpush(list, c_adt_lnew(c_arr_data(&arr), 0)) < 0) {
			c_err_die(1, nil);
		}
		for (i = n - 1; i >= 0; --i) {
			++idxs[i];
			if (idxs[i] < refs[i].n) break;
			idxs[i] = 0;
			if (!i) {
				c_std_free(idxs);
				return;
			}
		}
	}
}

static void
refpush(ctype_arr *ap, char *s, usize n)
{
	struct ref r;
	if (!n) return;
	r.args = c_std_vtoptr(c_str_dup(s, n));
	r.n = 1;
	r.free = 1;
	if (c_dyn_cat(ap, &r, 1, sizeof(r)) < 0) c_err_diex(1, "no memory");
}

static ctype_status
reftree(ctype_arr *ap, char *s, usize n, int split)
{
	struct entry *e;
	struct ref r;
	ctype_arr arr;
	ctype_node *wp;
	usize i;
	int tmp;

	tmp = s[n];
	s[n] = 0;
	e = c_adt_kvget(&tree, s);
	s[n] = tmp;
	if (!e) return -1;
	expand(e);

	if (!split) {
		c_mem_set(&arr, sizeof(arr), 0);
		/* flat list */
		tmp = 1;
		wp = (e->data)->next;
		do {
			if (c_dyn_fmt(&arr,
			    "%s%s", tmp ? (--tmp, "") : " ", wp->p) < 0) {
				c_err_diex(1, "no memory");
			}
		} while ((wp = wp->next)->prev);
		c_dyn_shrink(&arr, sizeof(uchar));
		/* insert */
		r.args = c_std_vtoptr(c_arr_data(&arr));
		r.n = 1;
		r.free = 1;
		if (c_dyn_cat(ap, &r, 1, sizeof(r)) < 0) {
			c_err_diex(1, "no memory");
		}
		return 0;
	}

	/* count */
	r.n = 0;
	wp = (e->data)->next;
	do {
		++r.n;
	} while ((wp = wp->next)->prev);
	if (!(r.args = c_std_alloc(r.n + 1, sizeof(char *)))) {
		c_err_diex(1, "no memory");
	}
	/* insert */
	i = 0;
	wp = (e->data)->next;
	do {
		r.args[i++] = wp->p;
	} while ((wp = wp->next)->prev);
	r.args[i] = nil;
	r.free = 0;
	if (c_dyn_cat(ap, &r, 1, sizeof(r)) < 0) c_err_diex(1, "no memory");
	return 0;
}

static void
freeref(struct ref *r, usize n)
{
	usize i;
	for (i = 0; i < n; ++i) {
		if (r[i].free) c_std_free(r[i].args[0]);
		c_std_free(r[i].args);
	}
}

static ctype_node *
deference(char *line)
{
	ctype_arr set;
	ctype_node *list;
	usize diff;
	int split;
	char *end, *p, *s;

	s = line;
	end = c_mem_chr(s, -1, 0);
	if (!(c_str_chr(s, end - s, '$'))) return nil;

	c_mem_set(&set, sizeof(set), 0);
	for (; *s; ++s) {
		split = 1;
		if (!(s = c_str_chr(s, end - s, '$'))) break;
		p = s + 1;
		switch (*p) {
		case '$':
			c_str_cpy(s, end - s, s + 1);
			++s;
			break;
		case ':':
			if (p[1] != '{') continue;
			++p;
			split = 0;
			/* FALLTHROUGH */
		case '{':
			p += explicitspan(p, end - p);
			/* no further processing if there's no match */
			if (*p != '}') {
				s = nil;
				continue;
			}
			diff = 2 + !split;
			break;
		case '\0':
			continue;
		default:
			p += implicitspan(p, end - p);
			diff = 1;
		}
		if (!*s) break;
		refpush(&set, line, s - line);
		if (reftree(&set, s + diff, p - (s + diff), split) < 0) {
			c_arr_trunc(&set,
			    c_arr_len(&set, sizeof(struct ref)) - 1,
			    sizeof(struct ref));
			continue;
		}
		line = p + (diff > 1);
	}
	/* remaining raw string */
	if (line < end && *line) refpush(&set, line, end - line);
	/* to list */
	list = nil;
	combine(&list, c_arr_data(&set), c_arr_len(&set, sizeof(struct ref)));
	/* cleanup */
	freeref(c_arr_data(&set), c_arr_len(&set, sizeof(struct ref)));
	c_dyn_free(&set);
	return list;
}

/* tree interaction routines */
static ctype_status
eexpand(char *s, void *data)
{
	struct entry *e;
	(void)s;
	e = data;
	if (e->data) expand(data);
	return 0;
}

static ctype_status
edump(char *s, void *data)
{
	ctype_node *np;
	struct entry *e;
	e = data;
	if (checkprint(e)) return 0;
	if (!(np = deference(s))) {
		c_ioq_fmt(ioq1, "%s\n", s);
		return 0;
	}
	np = np->next;
	do {
		c_ioq_fmt(ioq1, "%s\n", np->p);
	} while ((np = np->next)->prev);
	while (np) c_adt_lfree(c_adt_lpop(&np));
	return 0;
}

static void
efree(void *p)
{
	struct entry *e;
	e = p;
	if (e->info & INFO_META) c_std_free(e->ref);
	while (e->data) c_adt_lfree(c_adt_lpop(&e->data));
	c_std_free(e);
}

static void
eprint(struct entry *e)
{
	struct entry *re;
	ctype_node *wp;
	expand(e);
	if (checkprint(e)) return;
	if (e->info & INFO_REF) {
		if (!(re = c_adt_kvget(&tree, e->ref))) return;
		wp = (re->data)->next;
		do {
			c_ioq_fmt(ioq1, "%s\n", wp->p);
		} while ((wp = wp->next)->prev);
		if (!e->data) return;
	}
	wp = (e->data)->next;
	do {
		c_ioq_fmt(ioq1, "%s\n", wp->p);
	} while ((wp = wp->next)->prev);
}

static void
usage(void)
{
	c_ioq_fmt(ioq1,
	    "usage: %s [-akltvx] key [file]\n",
	    c_std_getprogname());
	c_std_exit(1);
}

ctype_status
main(int argc, char **argv)
{
	struct entry *e;
	ctype_fd fd;
	int mode;

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	fd = 0;
	mode = MOD_KEYVAL;
	while (c_std_getopt(argmain, argc, argv, "akltvx")) {
		switch (argmain->opt) {
		case 'a':
			mode = MOD_ALL;
			break;
		case 'k':
			opts = (opts & ~VFLAG) | KFLAG;
			break;
		case 'l':
			opts |= LFLAG;
			break;
		case 't':
			mode = MOD_BLOCK;
			break;
		case 'v':
			opts = (opts & ~KFLAG) | VFLAG;
			break;
		case 'x':
			opts = (opts & ~(KFLAG | LFLAG | VFLAG)) | XFLAG;
			break;
		default:
			usage();
		}
	}
	argc -= argmain->idx;
	argv += argmain->idx;

	switch (argc) {
	case 1:
		fd = C_IOQ_FD0;
		break;
	case 2:
		if (C_STD_ISDASH(argv[1])) {
			fd = C_IOQ_FD0;
			break;
		}
		fd = c_nix_fdopen2(argv[1], C_NIX_OREAD);
		if (fd < 0) c_err_die(1, "failed to open file \"%s\"", argv[1]);
		break;
	default:
		usage();
	}
	initconf(fd);
	c_adt_kvtraverse(&tree, initmeta);
	c_adt_kvtraverse(&tree, eexpand);

	if (C_STD_ISDASH(*argv) && (mode == MOD_BLOCK || mode == MOD_ALL)) {
		mode = MOD_SELF;
	}
	switch (mode) {
	case MOD_KEYVAL:
		if (!(e = c_adt_kvget(&tree, *argv))) return 1;
		if (!(e->info & INFO_KEYVAL)) return 1;
		eprint(e);
		break;
	case MOD_BLOCK:
		if (!(e = c_adt_kvget(&tree, *argv))) return 1;
		if (!(e->info & INFO_BLOCK)) return 1;
		eprint(e);
		break;
	case MOD_ALL:
		if (!(e = c_adt_kvget(&tree, *argv))) return 1;
		eprint(e);
		break;
	case MOD_SELF:
		c_adt_kvtraverse(&tree, edump);
	}

	c_adt_kvfree(&tree, efree);
	c_ioq_flush(ioq1);
	return 0;
}
