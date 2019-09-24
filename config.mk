PREFIX= /usr/local
MANDIR= $(PREFIX)/share/man

AR     = ar
CC     = cc
RANLIB = ranlib

CPPFLAGS =
CFLAGS   = -O0 -g -std=c99 -Wall -Wextra -Werror -pedantic
LDFLAGS  =
LDLIBS   = -ltertium
