#include <tertium/cpu.h>
#include <tertium/std.h>

#define ISTAG(x) \
(((uchar *)(uintptr)x)[-1])
#define ISPRINT(x) \
(!(flags & LFLAG) || ISTAG(x))
#define META(x) \
((*(x) == '$') ? INFO_META : 0)
#define ISEXPLICIT(x) \
((x) == '_' || ((x) != '}' && c_utf8_isgraph((x))))
#define ISIMPLICIT(x) \
((x) == '_' || c_utf8_isalnum((x)))
#define WL(a, b, c) \
{ a = (b)->next; do { c; } while((a = a->next)->prev); }

/* flags */
enum {
	OPT_LIST = 1 << 0,
	LFLAG = 1 << 1,
	KFLAG = 1 << 2,
	QFLAG = 1 << 3,
	VFLAG = 1 << 4,
	XFLAG = 1 << 5,
	XXFLAG = 1 << 6,
};

/* modes */
enum {
	MOD_KEYVAL = 1 << 0,
	MOD_ALL = 1 << 1,
	MOD_BLOCK = 1 << 2,
	MOD_SELF = 1 << 3,
};

/* info */
enum {
	INFO_DEFFERING = 1 << 0,
	INFO_DEFFERED  = 1 << 1,
	INFO_KEY       = 1 << 2,
	INFO_VALUE     = 1 << 3,
	INFO_BLOCK     = 1 << 4,
	INFO_META      = 1 << 5,
	INFO_REF       = 1 << 6,
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

static ctype_node *elist;
static ctype_kvtree tree;
static uint flags;

static void edump(char *, void *);
static ctype_node *deference(char *, int);

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
	return graphspan(s, n) == n;
}

static ctype_status
blockprint(struct entry *e)
{
	if (flags & KFLAG) {
		if (!(e->info & (INFO_REF | INFO_KEY | INFO_BLOCK))) return 1;
	} else if (flags & VFLAG) {
		if (e->info & (INFO_REF | INFO_KEY | INFO_BLOCK)) return 1;
	}
	return 0;
}

/* fail routines */
static void *
alloc(usize m, usize n)
{
	void *p;
	if (!(p = c_std_calloc(m, n))) c_err_diex(1, "no memory");
	return p;
}

static void
dyncat(ctype_arr *p, void *v, usize m, usize n)
{
	if (c_dyn_cat(p, v, m, n) < 0) c_err_diex(1, "no memory");
}

static void
dynfmt(ctype_arr *p, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (c_dyn_vfmt(p, fmt, ap) < 0) c_err_diex(1, "no memory");
	va_end(ap);
}

static void *
lnew(void *v, usize n)
{
	if (!(v = c_adt_lnew(v, n)) && n) c_err_diex(1, "no memory");
	return v;
}

/* entry routines */
static void *
entnew(char *s, usize n, int state)
{
	ctype_node *p;

	p = alloc(1, sizeof(*p));
	p->next = p;
	p->prev = nil;

	p->p = alloc(n + 2, sizeof(uchar));
	*(uchar *)(uintptr)p->p++ = (state > 1) ? 0 : 1;
	c_mem_cpy(p->p, s, n);
	return p;
}

static void *
entdup(char *s, usize n)
{
	char *p;
	n = c_str_len(s, n);
	p = alloc(n + 2, sizeof(uchar));
	p[n] = 0;
	*p++ = 1;
	c_mem_cpy(p, s, n);
	return p;
}

static void
entfree(void *p)
{
	c_std_free_(&ISTAG(p));
}

/* build tree routines */
static void
epush(char *key, int type, void *data)
{
	struct entry *e;
	char *s;

	if (!*key) return;

	if (!(e = c_adt_kvget(&tree, key))) {
		e = alloc(1, sizeof(*e));
		if (flags & OPT_LIST) {
			c_adt_ltpush(&elist,
			    lnew(key, c_str_len(key, -1) + 1));
		}
	}

	if (type & INFO_META) {
		if (!e->ref) e->ref = c_str_dup(key, -1);
	}

	if (type & INFO_REF) {
		e->ref = data;
	} else if (type & INFO_BLOCK) {
		c_adt_lpush(&e->data, data);
	} else if (type & INFO_KEY) {
		while (e->data) c_adt_lfree(c_adt_lpop(&e->data), c_std_free_);
		if (data && *(char *)(uintptr)data) {
			e->data = entnew(data, c_str_len(data, -1) + 1, 0);
		} else {
			e->data = 0;
		}
	}
	e->info = (e->info & ~(INFO_BLOCK | INFO_KEY | INFO_VALUE)) | type;
	if (c_adt_kvadd(&tree, key, e) < 0) c_err_diex(1, "no memory");
}

static int
machine(int state, char *s, usize n, usize diff)
{
	static ctype_arr key;
	static ctype_node *np;
	static usize indent;
	int type, tmp;
	char *p;

	if (state == -1) {
		c_dyn_free(&key);
		return 0;
	}
	if (n == 0) return state;
	if (s[0] == '#' && !(flags & XFLAG) && state <= 1) return state;

	/* key | block */
	if (state == 0) {
		if (s[n - 1] == '{') {
			--n;
			if (!isvariable(s + (*s == '$'), n - (*s == '$'))) {
				c_err_diex(1, "bad formatted file");
			}
			c_arr_trunc(&key, 0, sizeof(uchar));
			dynfmt(&key, "%.*s", n, s);
			return 1;
		} else {
			if ((p = c_mem_chr(s, n, ':'))) {
				n = p - s;
				if (isvariable(s, n)) {
					*p++ = 0;
					epush(s, INFO_KEY | META(s), p);
					return 0;
				}
			}
			epush(s, INFO_VALUE, nil);
			return 0;
		}
	}
	/* block */
	type = tmp = 0;
	if (graphspan(s, n) != n) {
		/* XXX: (-x) maybe use an heuristic for dangling '{}' */
		for (p = s; n && *p; ++p) tmp += (*p == '{') - (*p == '}');
	} else {
		if (s[0] == '}') {
			type = 1;
			if (state > 1) {
				--state;
			} else {
				p = c_arr_data(&key);
				epush(p, INFO_BLOCK | META(p), np);
				np = nil;
				return 0;
			}
		} else {
			tmp = (s[n - 1] == '{');
			if ((state <= 1) && (flags & LFLAG)) {
				if (tmp) {
					s[--n] = 0;
					type = -1;
				} else if ((p = c_str_chr(s, n, ':'))) {
					*p = 0;
				}
			}
		}
	}

	if (!np) indent = diff;
	diff -= indent;

	state += tmp;
	c_adt_lpush(&np, entnew(s - diff, (n + diff), state + type));
	return state;
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
	struct entry *e, *ee;
	ctype_node *wp;

	e = data;
	if (!(e->info & INFO_META)) return 0;

	if (!(ee = c_adt_kvget(&tree, s + 1)) || !ee->data) return 0;
	WL(wp, ee->data, if (ISPRINT(wp->p))
	    epush(wp->p, (e->info & ~INFO_META) | INFO_REF, e->ref));
	return 0;
}

/* expansion routines */
static ctype_status
expand(char *s, struct entry *e)
{
	struct entry *ee;
	ctype_node *np, *wp;

	if (e->info & INFO_DEFFERING) {
		return -1;
	} else if (e->info & INFO_DEFFERED) {
		return 0;
	}

	if (e->info & INFO_REF) {
		if ((ee = c_adt_kvget(&tree, e->ref))) expand(e->ref, ee);
		e->info |= INFO_DEFFERED;
	}

	if (e->data) {
		e->info |= INFO_DEFFERING;
		wp = (e->data)->next;
		do {
			if (!(np = deference(wp->p, ISTAG(wp->p)))) continue;
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
			entfree(wp->p);
			c_std_free(wp);
			wp = np;
		} while ((wp = wp->next)->prev);
		e->info = (e->info & ~INFO_DEFFERING) | INFO_DEFFERED;
	}

	if (flags & OPT_LIST) edump(s, e);
	return 0;
}

static void
combine(ctype_node **list, struct ref *refs, usize n, int print)
{
	ctype_arr arr;
	usize *indexes;
	size i;
	char *s;

	if (!n) return;
	indexes = alloc(n, sizeof(*indexes));

	for (;;) {
		c_mem_set(&arr, sizeof(arr), 0);
		/* print tag */
		dynfmt(&arr, "%c", print);
		/* list */
		for (i = 0; (usize)i < n; ++i) {
			if (*(s = refs[i].args[indexes[i]])) {
				dynfmt(&arr, "%s", s);
				if (!ISTAG(s)) {
					s = c_arr_get(&arr, 0, sizeof(uchar));
					*s = 0;
					break;
				}
			}
		}
		c_dyn_shrink(&arr, sizeof(uchar));
		c_adt_lpush(list,
		    lnew((uchar *)(uintptr)c_arr_data(&arr) + 1, 0));
		for (i = n - 1; i >= 0; --i) {
			++indexes[i];
			if (indexes[i] < refs[i].n) break;
			indexes[i] = 0;
			if (!i) {
				c_std_free(indexes);
				return;
			}
		}
	}
}

static void
refpush(ctype_arr *ap, char *s)
{
	struct ref r;
	r.args = c_std_ptrlist(s);
	r.n = 1;
	r.free = 1;
	dyncat(ap, &r, 1, sizeof(r));
}

static void
refepush(ctype_arr *ap, struct entry *e)
{
	struct ref r;
	ctype_node *wp;
	usize i;
	/* count */
	r.n = 0;
	WL(wp, e->data, r.n++);
	r.args = alloc(r.n + 1, sizeof(char *));
	/* insert */
	i = 0;
	WL(wp, e->data, r.args[i++] = wp->p);
	r.free = 0;
	dyncat(ap, &r, 1, sizeof(r));
}

static ctype_status
refbuild(ctype_arr *ap, char *s, usize n, int split)
{
	struct entry *e;
	ctype_arr arr;
	ctype_node *wp;
	int tmp;

	tmp = s[n];
	s[n] = 0;
	e = c_adt_kvget(&tree, s);
	if (!e || expand(s, e) < 0) {
		s[n] = tmp;
		return -1;
	}
	s[n] = tmp;
	if (!e->data) return 0;

	if (!split) {
		c_mem_set(&arr, sizeof(arr), 0);
		/* print tag */
		dynfmt(&arr, "%c", 1);
		/* flat list */
		tmp = 1;
		WL(wp, e->data, if (ISTAG(wp->p) && *(char *)(uintptr)wp->p)
		    dynfmt(&arr, "%s%s", tmp ? (--tmp, "") : " ", wp->p));
		c_dyn_shrink(&arr, sizeof(uchar));
		/* insert */
		refpush(ap, (char *)(uintptr)c_arr_data(&arr) + 1);
		return 0;
	}
	refepush(ap, e);
	return 0;
}

static void
refclean(struct ref *r, usize n)
{
	usize i;
	for (i = 0; i < n; ++i) {
		if (r[i].free) entfree(r[i].args[0]);
		c_std_free(r[i].args);
	}
}

static ctype_node *
deference(char *line, int print)
{
	ctype_arr set;
	ctype_node *list;
	usize diff;
	int split;
	char *end, *p, *s;

	if (flags & XXFLAG) return nil;

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
			continue;
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

		if (s - line) refpush(&set, entdup(line, s - line));
		if (refbuild(&set, s + diff, p - (s + diff), split) < 0) {
			c_arr_trunc(&set,
			    c_arr_len(&set, sizeof(struct ref)) - 1,
			    sizeof(struct ref));
			continue;
		}
		line = p + (diff > 1);
	}
	/* remaining raw string */
	if (line < end && *line) refpush(&set, entdup(line, end - line));
	/* to list */
	list = nil;
	combine(&list,
	    c_arr_data(&set), c_arr_len(&set, sizeof(struct ref)), print);
	/* cleanup */
	refclean(c_arr_data(&set), c_arr_len(&set, sizeof(struct ref)));
	c_dyn_free(&set);
	return list;
}

/* tree interaction routines */
static void
efree(void *p)
{
	struct entry *e;
	e = p;
	if (e->info & INFO_META) c_std_free(e->ref);
	while (e->data) c_adt_lfree(c_adt_lpop(&e->data), entfree);
	c_std_free(e);
}

static void
eprint(struct entry *e)
{
	struct entry *ee;
	ctype_node *wp;

	if (blockprint(e)) return;
	expand(nil, e);

	if (e->info & INFO_REF) {
		if (!(ee = c_adt_kvget(&tree, e->ref))) return;
		WL(wp, ee->data, c_ioq_fmt(ioq1, "%s\n", wp->p));
	}
	if (!e->data) return;
	WL(wp, e->data, if (ISPRINT(wp->p)) c_ioq_fmt(ioq1, "%s\n", wp->p));
}

static void
_edump(char *s, struct entry *e)
{
	if ((flags & XFLAG) && (e->info & INFO_REF)) return;
	if ((flags & LFLAG) || (e->info & INFO_VALUE)) {
		if (!blockprint(e)) c_ioq_fmt(ioq1, "%s\n", s);
		return;
	}
	if (e->info & INFO_KEY) {
		c_ioq_fmt(ioq1, "%s:", s);
		eprint(e);
		c_ioq_put(ioq1, "\n");
	} else {
		c_ioq_fmt(ioq1, "%s{\n", s);
		eprint(e);
		c_ioq_put(ioq1, "}\n");
	}
}

static void
edump(char *s, void *data)
{
	ctype_node *n;
	struct entry *e;

	e = data;
	if (blockprint(e)) return;

	if (!(n = deference(s, 1))) {
		_edump(s, e);
		return;
	}
	WL(n, n, if (ISPRINT(n->p)) _edump(n->p, e));
	while (n) c_adt_lfree(c_adt_lpop(&n), c_std_free_);
}

static void
usage(void)
{
	c_ioq_fmt(ioq2,
	    "usage: %s [-aklqtvx] key [file]\n",
	    c_std_getprogname());
	c_std_exit(1);
}

ctype_status
main(int argc, char **argv)
{
	struct entry *e;
	ctype_fd fd;
	uint mode;

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	mode = MOD_KEYVAL;

	while (c_std_getopt(argmain, argc, argv, "aklqtvx")) {
		switch (argmain->opt) {
		case 'a':
			mode = MOD_ALL;
			break;
		case 'k':
			flags = (flags & ~VFLAG) | KFLAG;
			break;
		case 'l':
			flags |= LFLAG;
			break;
		case 'q':
			flags |= QFLAG;
			break;
		case 't':
			mode = MOD_BLOCK;
			break;
		case 'v':
			flags = (flags & ~KFLAG) | VFLAG;
			break;
		case 'x':
			flags |= (flags & XFLAG) ? XXFLAG : XFLAG;
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

	if (C_STD_ISDASH(*argv) && (mode & (MOD_ALL | MOD_BLOCK))) {
		flags |= OPT_LIST;
		mode = MOD_SELF;
	}

	initconf(fd);
	c_adt_kvtraverse(&tree, initmeta);

	if (mode & (MOD_KEYVAL | MOD_BLOCK | MOD_ALL)) {
		if (!(e = c_adt_kvget(&tree, *argv))) return 1;
	}

	if (flags & QFLAG) {
		if (blockprint(e)) return 1;
		return 0;
	}

	switch (mode) {
	case MOD_KEYVAL:
		if (e->info & INFO_VALUE) {
			if (flags & KFLAG) return 1;
			c_ioq_fmt(ioq1, "%s\n", *argv);
		} else {
			if (flags & VFLAG || !(e->info & INFO_KEY)) return 1;
			eprint(e);
		}
		break;
	case MOD_BLOCK:
		if (!(e->info & INFO_BLOCK)) return 1;
		eprint(e);
		break;
	case MOD_ALL:
		if (e->info & INFO_VALUE) {
			if (flags & KFLAG) return 1;
			c_ioq_fmt(ioq1, "%s\n", *argv);
		} else {
			if (blockprint(e)) return 1;
			eprint(e);
		}
		break;
	case MOD_SELF:
		while (elist) {
			expand(elist->p, c_adt_kvget(&tree, elist->p));
			c_adt_lfree(c_adt_lpop(&elist), c_std_free_);
		}
	}
	c_adt_kvfree(&tree, efree);
	c_ioq_flush(ioq1);
	return 0;
}
