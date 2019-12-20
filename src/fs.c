#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

ctype_status
makedir(char *s, uint mode)
{
	ctype_stat st;

	if (c_sys_mkdir(s, mode) < 0) {
		if (errno == C_EEXIST) {
			if ((c_sys_stat(&st, s) < 0) || !C_ISDIR(st.mode)) {
				errno = C_ENOTDIR;
				return -1;
			}
		} else {
			return -1;
		}
	}

	return 0;
}

ctype_status
makepath(char *path)
{
	char *s;

	s = path + (*path == '/');

	for (;;) {
		if (!(s = c_str_chr(s, C_USIZEMAX, '/')))
			break;
		*s = 0;
		if (makedir(path, 0755) < 0)
			return c_err_warn("makedir %s", path);
		*s++ = '/';
	}

	if (makedir(path, 0755) < 0)
		return c_err_warn("makedir %s", path);

	return 0;
}

ctype_status
removepath(char *path)
{
	char *s;

	if (c_sys_unlink(path) < 0)
		return c_err_warn("c_sys_unlink %s", path);

	path = estrdup(path);
	if (!(s = c_str_rchr(path, C_USIZEMAX, '/')))
		return 0;
	*s = 0;

	for (;;) {
		c_sys_rmdir(path);
		if (!(s = c_str_rchr(path, C_USIZEMAX, '/')))
			break;
		*s = 0;
	}
	c_std_free(path);
	return 0;
}
