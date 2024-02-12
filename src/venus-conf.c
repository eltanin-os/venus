#include <tertium/cpu.h>
#include <tertium/std.h>

static int lflag;

static ctype_kvtree stree;
static ctype_kvtree mtree;

static usize
graphspan(char *s, usize n)
{
	char *p;
	for (p = s; n && *p && c_utf8_isgraph(*p); --n, ++p) ;
	return p - s;
}

static int
machine(int state, char *s, usize n)
{
	static ctype_node *np;
	static ctype_arr key; /* "memory leak" */
	int tmp;
	char *p;

	if (s[0] == '#') return state;

	if (state == 0) {
		if (s[n - 1] == '{') {
			--n;
			if (!(graphspan(s, n) == n)) {
				c_err_diex(1, "bad formatted file");
			}
			c_arr_trunc(&key, 0, sizeof(uchar));
			if (c_dyn_fmt(&key, "%.*s", n, s) < 0) {
				c_err_diex(1, "no memory");
			}
			return 1;
		} else {
			if ((p = c_mem_chr(s, n, ':'))) {
				n = p - s;
				*p++ = 0;
			} else {
				p = "";
			}
			if (!(graphspan(s, n) == n)) {
				c_err_diex(1, "bad formatted file");
			}
			p = c_str_dup(p, -1);
			if (c_adt_kvadd(&stree, s, p) < 0) {
				c_err_diex(1, "no memory");
			}
			return 0;
		}
	} else {
		if (s[n - 1] == '}') {
			if (state > 1) {
				--state;
				if (lflag) return state;
			} else {
				p = c_arr_data(&key);
				if (c_adt_kvadd(&mtree, p, np) < 0) {
					c_err_diex(1, "no memory");
				}
				np = nil;
				return 0;
			}
		}
		tmp = (s[n - 1] == '{');
		if (lflag) {
			if (tmp) {
				s[--n] = 0;
			} else if ((p = c_str_chr(s, n, ':'))) {
				*p = 0;
			}
			if (state > 1) return state + tmp;
		}
		if (c_adt_lpush(&np, c_adt_lnew(s, n + 1)) < 0) {
			c_err_diex(1, "no memory");
		}
		return state + tmp;
	}
}

static void
initconf(ctype_fd fd)
{
	ctype_ioq ioq;
	ctype_arr arr;
	size r;
	int state;
	char buf[C_IOQ_BSIZ];
	char *s;

	state = 0;
	c_ioq_init(&ioq, fd, buf, sizeof(buf), &c_nix_fdread);
	c_mem_set(&arr, sizeof(arr), 0);
	while ((r = c_ioq_getln(&arr, &ioq)) > 0) {
		r = c_arr_bytes(&arr);
		c_arr_trunc(&arr, r - 1, sizeof(uchar));
		s = c_str_trim(c_arr_data(&arr), r, "\t ");
		state = machine(state, s, c_str_len(s, r));
		c_arr_trunc(&arr, 0, sizeof(uchar));
	}
	if (state) c_err_diex(1, "bad formatted file");
	if (r < 0) c_err_die(1, "failed to read file");
}

static char *
expand(char *k, char *value)
{
	ctype_arr arr;
	ctype_status r;
	char tmp;
	char *p, *s, *v;

	if (!(s = c_str_chr(value, -1, '$'))) return nil;

	c_mem_set(&arr, sizeof(arr), 0);
	r = c_dyn_fmt(&arr, "%s", value);
	if (r < 0) c_err_diex(1, "no memory");

	s = value = c_arr_data(&arr);
	for (;;) {
		if (!(s = c_str_chr(s, -1, '$'))) break;
		p = s + 1;
		if (*p == '$') {
			s += 2;
			if (!*s) break;
			continue;
		} else if (*p == '\0') {
			break;
		}
		for (; *p && c_utf8_isalnum(*p); ++p) ;
		if (p == s) { ++s; continue; }
		/* get key */
		if (!c_str_cmp(k, -1, s + 1)) { ++s; continue; }
		tmp = *p; *p = 0;
		v = c_adt_kvget(&stree, s + 1);
		*p = tmp;
		if (!v) { ++s; continue; }
		/* expand the variable */
		c_arr_trunc(&arr, s - value, sizeof(uchar));
		if (!(p = c_str_dup(p, -1))) c_err_diex(1, "no memory");
		r = c_dyn_fmt(&arr, "%s%s", v, p);
		if (r < 0) c_err_diex(1, "no memory");
		c_std_free(p);
		s = value = c_arr_data(&arr);
	}

	s = value = c_arr_data(&arr);
	for (; *s; ++s) if (!C_STR_CMP("$$", s)) c_str_cpy(s, -1, s + 1);
	c_arr_trunc(&arr, s - value, sizeof(uchar));

	c_dyn_shrink(&arr);
	return c_arr_data(&arr);
}

static ctype_status
skeys(char *k, void *v)
{
	char *nv;
	if (!v || !(nv = expand(k, v))) return 0;
	c_std_free(v);
	c_adt_kvadd(&stree, k, nv);
	return 0;
}

static ctype_status
mkeys(char *k, void *v)
{
	ctype_node *wp;

	if (!v) return 0;
	(void)k;

	wp = ((ctype_node *)v)->next;
	do {
		if (!(v = expand(k, wp->p))) continue;
		wp->p = v;
	} while ((wp = wp->next)->prev);
	return 0;
}

static void
printlist(ctype_node *p)
{
	ctype_node *wp;
	wp = p->next;
	do {
		c_ioq_fmt(ioq1, "%s\n", wp->p);
	} while ((wp = wp->next)->prev);
}

static void
freeobj(void *p)
{
	c_std_free(p);
}

static void
freelist(void *p)
{
	ctype_node *list;
	for (list = p; list;) c_adt_lfree(c_adt_lpop(&list));
}

static void
usage(void)
{
	c_ioq_fmt(ioq1, "usage: %s [-lt] key [file]\n", c_std_getprogname());
	c_std_exit(1);
}

ctype_status
main(int argc, char **argv)
{
	ctype_fd fd;
	int mode;
	char *s;

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	fd = 0;
	mode = 0;
	while (c_std_getopt(argmain, argc, argv, "lt")) {
		switch (argmain->opt) {
		case 'l':
			lflag = 1;
			break;
		case 't':
			mode = 1;
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
		fd = c_nix_fdopen2(argv[1], C_NIX_OREAD);
		if (fd < 0) c_err_die(1, "failed to open file \"%s\"", argv[1]);
		break;
	default:
		usage();
	}
	initconf(fd);
	c_adt_kvtraverse(&stree, skeys);
	c_adt_kvtraverse(&mtree, mkeys);
	if (mode) {
		if (!(s = c_adt_kvget(&mtree, *argv))) return 1;
		printlist((void *)s);
	} else {
		if (!(s = c_adt_kvget(&stree, *argv))) return 1;
		c_ioq_fmt(ioq1, "%s\n", s);
	}
	c_adt_kvfree(&stree, freeobj);
	c_adt_kvfree(&mtree, freelist);
	c_ioq_flush(ioq1);
	return 0;
}
