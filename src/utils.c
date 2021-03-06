#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define STRCAT(a, b) c_dyn_cat((a), (b), sizeof((b)) - 1, sizeof(uchar))

static char *
urlencode(char *url)
{
	ctype_arr arr;

	c_mem_set(&arr, sizeof(arr), 0);
	if (c_dyn_ready(&arr, c_str_len(url, C_USIZEMAX), sizeof(uchar)) < 0)
		c_err_die(1, "c_dyn_ready");
	while (*url) {
		switch (*url) {
		case '#':
			if (STRCAT(&arr, "%23") < 0)
				c_err_die(1, "c_dyn_cat");
			break;
		default:
			if (c_dyn_cat(&arr, url, 1, sizeof(uchar)) < 0)
				c_err_die(1, "c_dyn_cat");
		}
		++url;
	}
	return c_arr_data(&arr);
}

void
dofetch(char *url)
{
	ctype_id id;
	char **av;
	char *s;

	s = estrdup(c_gen_basename(url));
	url = urlencode(url);
	if (!(av = c_exc_arglist(fetch, s, url)))
		c_err_die(1, "c_std_vtoptr");
	if (!(id = c_exc_spawn0(*av, av, environ)))
		c_err_die(1, "c_exc_spawn0 %s", *av);
	c_std_free(av);
	c_std_free(s);
	c_std_free(url);
	c_sys_waitpid(id, nil, 0);
}

void
douncompress(char *file)
{
	ctype_fd fd;
	ctype_id id;
	char **av;

	if (!(av = c_exc_arglist(uncompress, file)))
		c_err_die(1, "c_std_arglist");
	if (!(id = c_exc_spawn1(*av, av, environ, &fd, 1)))
		c_err_die(1, "c_exc_spawn1 %s", *av);
	c_std_free(av);
	unarchivefd(fd);
	c_sys_waitpid(id, nil, 0);
	c_sys_close(fd);
}
