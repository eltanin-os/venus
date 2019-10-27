#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

int
main(int argc, char **argv)
{
	char *argv0;

	argv0 = c_gen_basename(sdup(*argv, c_str_len(*argv, C_USIZEMAX)));

	if (!STRCMP("venus", argv0))
		return venus_main(argc, argv);
	else if (!STRCMP("venus-ar", argv0))
		return ar_main(argc, argv);
	else if (!STRCMP("venus-cksum", argv0))
		return cksum_main(argc, argv);

	return 1;
}
