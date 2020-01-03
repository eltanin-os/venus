#include <tertium/cpu.h>
#include <tertium/std.h>

static char *nptr = nil;

static void
avmake(ctype_arr *arr, char *s)
{
	c_mem_set(arr, sizeof(arr), 0);
	if (c_dyn_cat(arr, &s, 1, sizeof(void *)) < 0)
		c_err_die(1, "c_dyn_cat");
	for (;;) {
		if (!(s = c_str_chr(s, C_USIZEMAX, ' ')))
			break;
		*s++ = 0;
		if (c_dyn_cat(arr, &s, 1, sizeof(void *)) < 0)
			c_err_die(1, "c_dyn_cat");
	}
}

char **
av3make(char *s1, char *s2, char *s3)
{
	ctype_arr arr;

	if (!s1)
		c_err_diex(1, "av3make: missing variable");

	c_mem_set(&arr, sizeof(arr), 0);
	avmake(&arr, s1);
	if (c_dyn_cat(&arr, &s2, 1, sizeof(void *)) < 0)
		c_err_die(1, "c_dyn_fmt");
	if (c_dyn_cat(&arr, &s3, 1, sizeof(void *)) < 0)
		c_err_die(1, "c_dyn_fmt");
	if (c_dyn_cat(&arr, &nptr, 1, sizeof(void *)) < 0)
		c_err_die(1, "c_dyn_fmt");
	return c_arr_data(&arr);
}

char **
av1make(char *s)
{
	ctype_arr arr;

	if (!s)
		c_err_diex(1, "av1make: missing variable");

	c_mem_set(&arr, sizeof(arr), 0);
	avmake(&arr, s);
	if (c_dyn_cat(&arr, &nptr, 1, sizeof(void *)) < 0)
		c_err_die(1, "c_dyn_fmt");
	return c_arr_data(&arr);
}

char *
estrdup(char *s)
{
	char *p;

	if (!(p = c_str_dup(s, C_USIZEMAX)))
		c_err_die(1, "c_str_dup");
	return p;
}

ctype_arr *
getln(ctype_ioq *p)
{
	static ctype_arr arr;
	size r;
	char *s;

	c_arr_trunc(&arr, 0, sizeof(uchar));
	if ((r = c_ioq_getln(p, &arr)) < 0)
		c_err_die(1, "c_ioq_getln");
	if (!r)
		return nil;
	s = c_arr_data(&arr);
	s[c_arr_bytes(&arr) - 1] = 0;
	return &arr;
}
