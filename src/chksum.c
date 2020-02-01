#include <tertium/cpu.h>
#include <tertium/std.h>

#include "common.h"

#define HDEC(x) (((x) <= '9') ? x - '0' : (((uchar)x | 32) - 'a') + 10)

void
putsize(ctype_hst *h, ctype_hmd *md)
{
	char buf[8];

	c_uint_64pack(buf, c_hsh_len(h));
	md->update(h, buf, sizeof(buf));
}

static ctype_status
wcheck(char *file, char *s)
{
	ctype_hst hs;
	int i;
	char buf[C_HWHIRLPOOL_DIGEST];

	c_hsh_whirlpool->init(&hs);
	if (c_hsh_putfile(&hs, c_hsh_whirlpool, file) < 0)
		c_err_die(1, "c_hsh_putfile %s", file);
	putsize(&hs, c_hsh_whirlpool);
	c_hsh_whirlpool->end(&hs);

	c_hsh_digest(&hs, c_hsh_whirlpool, buf);
	for (i = 0; i < C_HWHIRLPOOL_DIGEST; ++i) {
		if (((HDEC(s[0]) << 4) | HDEC(s[1])) != (uchar)buf[i])
			return -1;
		s += 2;
	}
	return 0;
}

void
chksumetc(ctype_fd etcfd, char *file)
{
	ctype_ioq ioq;
	ctype_arr *ap;
	usize n;
	char *s;
	char buf[C_BIOSIZ];

	c_sys_seek(etcfd, 0, C_SEEKSET);
	c_ioq_init(&ioq, etcfd, buf, sizeof(buf), &c_sys_read);
	s = nil;
	while ((ap = getln(&ioq))) {
		n = c_arr_bytes(ap);
		if (!(s = c_mem_chr(c_arr_data(ap), n, ' ')))
			c_err_diex(1, CHKSUMFILE ": wrong format");
		n -= s - (char *)c_arr_data(ap);
		*s++ = 0;
		if (!c_str_cmp(c_arr_data(ap), n, file))
			break;
		s = nil;
	}
	if (!s)
		c_err_diex(1, "%s: have no checksum", file);
	if (wcheck(file, s) < 0)
		c_err_diex(1, "%s: checksum mismatch", file);
}

ctype_status
chksumfile(char *file, char *chksum)
{
	ctype_hst hs;
	u32int h1, h2;
	char buf[C_H32GEN_DIGEST];

	c_hsh_fletcher32->init(&hs);
	if (c_hsh_putfile(&hs, c_hsh_fletcher32, file) < 0)
		c_err_die(1, "c_hsh_putfile %s", file);
	putsize(&hs, c_hsh_fletcher32);
	c_hsh_fletcher32->end(&hs);

	c_hsh_digest(&hs, c_hsh_fletcher32, buf);
	h1 = c_uint_32unpack(buf);
	h2 = estrtovl(chksum, 16, 0, C_UINT32MAX);
	return h1 == h2 ? 0 : -1;
}
