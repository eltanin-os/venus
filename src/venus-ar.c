#include <tertium/cpu.h>
#include <tertium/std.h>

#define MAGIC "3EA81233"

enum {
	ZERO,
	ARCHIVE,
	UNARCHIVE,
};

struct head {
	u32 mode;
	u32 namesize;
	u64 size;
};

static ctype_node *tmpfiles;

/* atexit routines */
static void
cleantrash(void)
{
	ctype_node *p;
	while ((p = c_adt_lpop(&tmpfiles))) {
		c_nix_unlink(p->p);
		c_adt_lfree(p);
	}
}

static void
trackfile(char *s, usize n)
{
	ctype_node *p;
	p = c_adt_lnew(s, n);
	if (c_adt_lpush(&tmpfiles, p) < 0) c_err_diex(1, "no memory");
}

/* archive routines */
static void
store32(ctype_ioq *p, u32 x)
{
	char tmp[4];
	c_ioq_nput(p, c_uint_32pack(tmp, x), sizeof(tmp));
}

static void
store64(ctype_ioq *p, u64 x)
{
	char tmp[8];
	c_ioq_nput(p, c_uint_64pack(tmp, x), sizeof(tmp));
}

static void
puthead(ctype_ioq *p, char *s, usize n, ctype_stat *stp)
{
	store32(p, stp->mode);
	store32(p, n);
	store64(p, stp->size);
	c_ioq_nput(p, s, n);
}

static void
putfile(ctype_ioq *p, char *s)
{
	ctype_status r;
	r = c_ioq_putfile(p, s);
	if (r < 0) c_err_die(1, "failed to copy file \"%s\"", s);
}

static void
putsymlink(ctype_ioq *p, char *s, usize n)
{
	static ctype_arr arr; /* "memory leak" */
	ctype_status r;
	c_arr_trunc(&arr, 0, sizeof(uchar));
	r = c_dyn_ready(&arr, n + 1, sizeof(uchar));
	if (r < 0) c_err_diex(1, "no memory");
	r = c_nix_readlink(c_arr_data(&arr), c_arr_total(&arr), s);
	if (r < 0) c_err_die(1, "faile to read symlink \"%s\"", s);
	c_ioq_nput(p, c_arr_data(&arr), n);
}

static char **
filelist(void)
{
	ctype_arr arr;
	ctype_status r;
	usize n;
	char **args;
	char *s;
	/* get lines */
	s = n = 0;
	c_mem_set(&arr, sizeof(arr), 0);
	while ((r = c_ioq_getln(ioq0, &arr)) > 0) ++n;
	if (r < 0) c_err_die(1, "failed to read stdin");
	c_dyn_shrink(&arr);
	s = c_arr_data(&arr);
	/* split lines */
	args = c_std_alloc(n + 1, sizeof(void *));
	args[n] = nil;
	while (n) {
		args[--n] = s;
		s = c_str_chr(s, -1, '\n');
		*s++ = 0;
	}
	return args;
}

static int
sort(void *va, void *vb)
{
	ctype_dent *a, *b;
	a = va;
	b = vb;
	return b->stp->size - a->stp->size;
}

static void
archivefd(ctype_ioq *p, char **argv)
{
	ctype_dir dir;
	ctype_dent *ep;
	ctype_status r;

	c_ioq_put(p, MAGIC);
	if (!argv) argv = filelist();
	r = c_dir_open(&dir, argv, 0, &sort);
	if (r < 0) c_err_die(1, "failed to read the given args");
	while ((ep = c_dir_read(&dir))) {
		switch (ep->info) {
		case C_DIR_FSD:
		case C_DIR_FSDC:
		case C_DIR_FSDOT:
		case C_DIR_FSDP:
		case C_DIR_FSINT:
			continue;
		case C_DIR_FSDEF:
		case C_DIR_FSDNR:
		case C_DIR_FSERR:
		case C_DIR_FSNS:
			continue;
		}
		puthead(p, ep->path, ep->len, ep->stp);
		switch (ep->info) {
		case C_DIR_FSF:
			putfile(p, ep->path);
			break;
		case C_DIR_FSSL:
		case C_DIR_FSSLN:
			putsymlink(p, ep->path, ep->stp->size);
			break;
		}
	}
	c_dir_close(&dir);
	if (c_ioq_flush(p) < 0) c_err_die(1, "failed to flush file buffer");
	c_std_free(*argv);
	c_std_free(argv);
}

static void
archive(char *file, char **argv)
{
	ctype_ioq ioq;
	ctype_fd fd;
	ctype_status r;
	char buf[C_IOQ_BSIZ];
	char tmp[] = "tmpvenus.XXXXXXXXX";

	if (!file) {
		archivefd(ioq1, argv);
		return;
	}

	fd = c_nix_mktemp5(tmp, sizeof(tmp), 0, 0, C_NIX_DEFFILEMODE);
	if (fd < 0) c_err_die(1, "failed to obtain temporary file");
	trackfile(tmp, sizeof(tmp));
	c_ioq_init(&ioq, fd, buf, sizeof(buf), &c_nix_fdwrite);

	archivefd(&ioq, argv);
	c_nix_fdclose(fd);

	r = c_nix_rename(file, tmp);
	if (r < 0) c_err_die(1, "failed to replace file \"%s\"", file);
}

/* unarchive routines */
static void
createreg(ctype_ioq *p, char *s, usize len, usize n, uint mode)
{
	ctype_fd fd;
	size r;

	fd = c_nix_mktemp(s, len);
	if (fd < 0) c_err_die(1, "failed to obtain temporary file");
	trackfile(s, len);

	while (n) {
		r = c_ioq_feed(p);
		if (r <= 0) c_err_diex(1, "incomplete or broken file");
		if (n < (usize)r) r = n;
		r = c_nix_allrw(c_nix_fdwrite, fd, c_ioq_peek(p), r);
		if (r < 0) c_err_die(1, "failed to extract file \"%s\"", s);
		n -= r;
		c_ioq_seek(p, r);
	}

	r = c_nix_fdchmod(fd, mode);
	if (r < 0) c_err_diex(1, "failed to set file mode \"%s\"", s);
	c_nix_fdclose(fd);
}

static void
getall(ctype_ioq *p, char *s, usize n)
{
	size r;
	r = c_ioq_get(p, s, n);
	if (r < 0) c_err_die(1, "failed to read file");
	if (r < (size)n) c_err_diex(1, "incomplete file");
}

static void
createsln(ctype_ioq *p, char *s, usize len, usize n)
{
	static ctype_arr arr; /* "memory leak" */
	ctype_status r;
	c_arr_trunc(&arr, 0, sizeof(uchar));
	r = c_dyn_ready(&arr, n, sizeof(uchar));
	if (r < 0) c_err_diex(1, "no memory");
	getall(p, c_arr_data(&arr), n);
	r = c_nix_mklntemp(s, len, c_arr_data(&arr));
	if (r < 0) c_err_die(1, "failed to obtain temporary file");
	trackfile(s, len);
}

static void
unarchivefd(ctype_ioq *p)
{
	ctype_arr arr, tmp;
	struct head h;
	ctype_status r;
	char buf[sizeof(h)];
	char *s, *d;

	getall(p, buf, sizeof(MAGIC) - 1);
	r = c_mem_cmp(MAGIC, sizeof(MAGIC) - 1, buf);
	if (r != 0) c_err_diex(1, "not a valid venus archive");

	c_mem_set(&arr, sizeof(arr), 0);
	c_mem_set(&tmp, sizeof(tmp), 0);
	for (;;) {
		if (c_ioq_feed(p) <= 0) break;
		/* header */
		getall(p, buf, sizeof(buf));
		s = buf;
		h.mode = c_uint_32unpack(s);
		s += sizeof(h.mode);
		h.namesize = c_uint_32unpack(s);
		s += sizeof(h.namesize);
		h.size = c_uint_32unpack(s);
		/* file name */
		c_arr_trunc(&arr, 0, sizeof(uchar));
		s = c_dyn_alloc(&arr, h.namesize, sizeof(uchar));
		if (!s) c_err_diex(1, "no memory");
		s = c_arr_data(&arr);
		getall(p, s, h.namesize);
		s[h.namesize] = 0;
		/* tmp file name */
		c_arr_trunc(&tmp, 0, sizeof(uchar));
		r = c_dyn_tofrom(&tmp, &arr);
		if (r < 0) c_err_diex(1, "no memory");
		d = c_gen_dirname(c_arr_data(&tmp));
		c_arr_trunc(&tmp, c_str_len(d, -1), sizeof(uchar));
		c_nix_mkpath(d, 0755, 0755);
		r = c_dyn_fmt(&tmp, "/tmpvenus.XXXXXXXXX");
		if (r < 0) c_err_diex(1, "no memory");
		/* data */
		d = c_arr_data(&tmp);
		switch (h.mode & C_NIX_IFMT) {
		case C_NIX_IFLNK:
			createsln(p, d, c_arr_bytes(&tmp), h.size);
			break;
		case C_NIX_IFREG:
			createreg(p, d, c_arr_bytes(&tmp), h.size, h.mode);
			break;
		default:
			break;
		}
		r = c_nix_rename(s, c_arr_data(&tmp));
		c_nix_unlink(c_arr_data(&tmp));
		if (r < 0) c_err_die(1, "failed to replace file \"%s\"", s);
	}
	c_dyn_free(&arr);
	c_dyn_free(&tmp);
}

static void
unarchive(char *file)
{
	ctype_ioq ioq;
	ctype_fd fd;
	char buf[C_IOQ_BSIZ];
	if (!file) {
		unarchivefd(ioq0);
		return;
	}
	fd = c_nix_fdopen2(file, C_NIX_OREAD);
	if (fd < 0) c_err_die(1, "failed to open file \"%s\"", file);
	c_ioq_init(&ioq, fd, buf, sizeof(buf), &c_nix_fdread);
	unarchivefd(&ioq);
	c_nix_fdclose(fd);
}

static void
usage(void)
{
	c_ioq_fmt(ioq2,
	    "usage: %s -c [-f file] [file ...]\n"
	    "       %s -x [-f file]\n",
	    c_std_getprogname(), c_std_getprogname());
	c_std_exit(1);
}

ctype_status
main(int argc, char **argv)
{
	int mode;
	char *file;

	c_std_setprogname(argv[0]);
	--argc, ++argv;

	mode = 0;
	file = nil;
	while (c_std_getopt(argmain, argc, argv, "cf:x")) {
		switch (argmain->opt) {
		case 'c':
			mode = ARCHIVE;
			break;
		case 'f':
			if (C_STD_ISDASH(argmain->arg)) argmain->arg = nil;
			file = argmain->arg;
			break;
		case 'x':
			mode = UNARCHIVE;
			break;
		default:
			usage();
		}
	}
	argc -= argmain->idx;
	argv += argmain->idx;

	switch (mode) {
	case ARCHIVE:
		if (!argc) argv = nil;
		archive(file, argv);
		break;
	case UNARCHIVE:
		if (argc) usage();
		c_std_atexit(cleantrash);
		unarchive(file);
		break;
	default:
		usage();
	}
	return 0;
}
