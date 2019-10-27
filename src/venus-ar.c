#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

enum {
	ZERO,
	ARCHIVE,
	UNARCHIVE,
};

static void
usage(void)
{
	c_ioq_fmt(ioq2,
	    "usage: %s -c [-f file] [file ...]\n"
	    "       %s -x [-f file]\n",
	    c_std_getprogname(), c_std_getprogname());
	c_std_exit(1);
}

int
ar_main(int argc, char **argv)
{
	int op;
	char *f, *file;

	c_std_setprogname(argv[0]);
	file = nil;
	op = ZERO;

	C_ARGBEGIN {
	case 'c':
		op = ARCHIVE;
		f = "<stdout>";
		break;
	case 'f':
		file = C_EARGF(usage());
		break;
	case 'x':
		op = UNARCHIVE;
		f = "<stdin>";
		break;
	default:
		usage();
	} C_ARGEND

	if (!op)
		usage();

	if (!file)
		file = f;

	if (op == ARCHIVE)
		archive(file, argv);
	else if (op == UNARCHIVE)
		unarchive(file);

	return 0;
}
