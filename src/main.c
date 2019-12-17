#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

int
main(int argc, char **argv)
{
	char *s;

	c_std_setprogname(argv[0]);
	s = c_gen_basename(c_std_getprogname());

	if (!CSTRCMP("venus", s))
		return venus_main(argc, argv);
	else if (!CSTRCMP("venus-ar", s))
		return ar_main(argc, argv);
	else if (!CSTRCMP("venus-cksum", s))
		return cksum_main(argc, argv);

	return 1;
}
