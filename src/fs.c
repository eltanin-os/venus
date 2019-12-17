#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

ctype_status
makepath(char *path)
{
	char *s;

	path = estrdup(path);
	s = path + (*path == '/');

	for (;;) {
		if (!(s = c_str_chr(s, C_USIZEMAX, '/')))
			break;
		*s = 0;
		if ((c_sys_mkdir(path, 0755) < 0) &&
		    errno != C_EEXIST)
			return c_err_warn("c_sys_mkdir %s", path);
		*s++ = '/';
	}
	c_std_free(path);
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
