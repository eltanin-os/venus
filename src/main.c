#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

int
main(int argc, char **argv)
{
	char *argv0;

	argv0 = c_gen_basename(sdup(*argv, c_str_len(*argv, C_USIZEMAX)));

	if (!c_str_cmp(argv0, C_USIZEMAX, "venus"))
		return venus_main(argc, argv);
	else if (!c_str_cmp(argv0, C_USIZEMAX, "venus-ar"))
		return ar_main(argc, argv);
	else if (!c_str_cmp(argv0, C_USIZEMAX, "venus-cksum"))
		return cksum_main(argc, argv);

	return 1;
}
