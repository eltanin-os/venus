#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define STRCAT(a, b) c_dyn_cat((a), (b), sizeof((b)) - 1, sizeof(uchar))

static char *
urlencode(char *url)
{
	ctype_arr arr;

	c_mem_set(&arr, sizeof(arr), 0);
	while (*url) {
		switch (*url) {
		case '#':
			if (STRCAT(&arr, "%23") < 0)
				c_err_die(1, "c_dyn_cat");
			break;
		default:
			if (c_dyn_cat(&arr, url, 1, sizeof(uchar)) < 0)
				c_err_die(1, "c_dyn_cat");
			break;
		}
		++url;
	}
	return c_arr_data(&arr);
}

void
dofetch(ctype_fd dirfd, char *url)
{
	char **av;
	char *s;

	s = estrdup(c_gen_basename(url));
	url = urlencode(url);

	switch (c_sys_fork()) {
	case -1:
		c_err_die(1, "c_sys_fork");
	case 0:
		efchdir(dirfd);
		av = av3make(fetch, s, url);
		c_exc_run(*av, av);
		c_err_die(1, "c_exc_run %s", *av);
	}
	c_sys_wait(nil);
	c_std_free(s);
	c_std_free(url);
}

void
douncompress(ctype_fd dirfd, ctype_fd fd)
{
	ctype_fd fds[2];
	char **av;

	if (c_sys_pipe(fds) < 0)
		c_err_die(1, "c_sys_pipe");

	switch (c_sys_fork()) {
	case -1:
		c_err_die(1, "c_sys_fork");
	case 0:
		c_sys_dup(fd, C_FD0);
		c_sys_dup(fds[1], C_FD1);
		c_sys_close(fds[0]);
		c_sys_close(fds[1]);
		av = av1make(uncompress);
		c_exc_run(*av, av);
		c_err_die(1, "c_exc_run %s", *av);
	default:
		c_sys_close(fds[1]);
		efchdir(dirfd);
		unarchivefd(fds[0]);
		c_sys_close(fds[0]);
	}
	c_sys_wait(nil);
}
