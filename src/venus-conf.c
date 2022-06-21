#include <tertium/cpu.h>
#include <tertium/std.h>

struct conf {
	ctype_kvtree s;
	ctype_kvtree m;
};

static struct conf conf;

static int
machine(struct conf *c, int state, char *s, usize n)
{
	static ctype_node *np;
	static ctype_arr key; /* "memory leak" */
	char *p;
	ctype_status r;
	if (state == 0) {
		p = c_mem_chr(s, n, ':');
		if (p) {
			*p++ = 0;
			r = c_adt_kvadd(&c->s, s, c_str_dup(p, -1));
			if (r < 0) c_err_diex(1, "no memory");
			return 0;
		} else {
			r = s[n - 1] == '{';
			if (!r) c_err_diex(1, "bad formatted file");
			c_arr_trunc(&key, 0, sizeof(uchar));
			r = c_dyn_fmt(&key, "%.*s", n-1, s);
			if (r < 0) c_err_diex(1, "no memory");
			return 1;
		}
	} else {
		if (!(s[0] == '}' && s[1] == '\0')) {
			p = c_str_ltrim(s, n, "\t ");
			n -= p - s;
			r = c_adt_lpush(&np, c_adt_lnew(p, c_str_len(p, n)+1));
			if (r < 0) c_err_diex(1, "no memory");
			return 1;
		} else {
			r = c_adt_kvadd(&c->m, c_arr_data(&key), np);
			np = nil;
			return 0;
		}
	}
}

static void
initconf(struct conf *c, ctype_fd fd)
{
	ctype_ioq ioq;
	ctype_arr arr;
	size r;
	int state;
	char buf[C_IOQ_BSIZ];
	state = 0;
	c_ioq_init(&ioq, fd, buf, sizeof(buf), &c_nix_fdread);
	c_mem_set(&arr, sizeof(arr), 0);
	while ((r = c_ioq_getln(&ioq, &arr)) > 0) {
		c_arr_trunc(&arr, c_arr_bytes(&arr) - 1, sizeof(uchar));
		state = machine(c, state, c_arr_data(&arr), c_arr_bytes(&arr));
		c_arr_trunc(&arr, 0, sizeof(uchar));
	}
	if (r < 0) c_err_die(1, "failed to read file");
}

static char *
expand(char *value)
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
		tmp = *p; *p = 0;
		v = c_adt_kvget(&conf.s, s+1);
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
	for (; *s; ++s) if (*s == '$') c_str_cpy(s, -1, s+1);
	c_arr_trunc(&arr, s - value, sizeof(uchar));

	c_dyn_shrink(&arr);
	c_std_free(value);
	return c_arr_data(&arr);
}

static void
skeys(char *k, void *v)
{
	if (!v || !(v = expand(v))) return;
	c_adt_kvadd(&conf.s, k, v);
}

static void
mkeys(char *k, void *v)
{
	ctype_node *wp;

	if (!v) return;
	(void)k;

	wp = ((ctype_node *)v)->next;
	do {
		if (!(v = expand(wp->p))) continue;
		wp->p = v;
	} while ((wp = wp->next)->prev);
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
	c_ioq_fmt(ioq1, "usage: %s [-t] key [file]\n", c_std_getprogname());
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
	initconf(&conf, fd);
	c_adt_kvtraverse(&conf.s, skeys);
	c_adt_kvtraverse(&conf.m, mkeys);
	if (mode) {
		if ((s = c_adt_kvget(&conf.m, *argv))) printlist((void *)s);
	} else {
		s = c_adt_kvget(&conf.s, *argv);
		if (s) c_ioq_fmt(ioq1, "%s\n", s);
	}
	c_adt_kvfree(&conf.s, freeobj);
	c_adt_kvfree(&conf.m, freelist);
	c_ioq_flush(ioq1);
	return 0;
}
