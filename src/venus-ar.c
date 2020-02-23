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

ctype_status
ar_main(int argc, char **argv)
{
	int op;
	char *file;

	c_std_setprogname(argv[0]);

	op = ZERO;
	file = nil;

	C_ARGBEGIN {
	case 'c':
		op = ARCHIVE;
		break;
	case 'f':
		file = C_EARGF(usage());
		break;
	case 'x':
		op = UNARCHIVE;
		break;
	default:
		usage();
	} C_ARGEND

	switch (op) {
	case ARCHIVE:
		if (!file)
			file = "<stdin>";

		return archive(file, argv);
	case UNARCHIVE:
		if (argc)
			usage();

		if (!file)
			file = "<stdout>";

		return unarchive(file);
	default:
		usage();
	}

	/* NOT REACHED */
	return 0;
}
