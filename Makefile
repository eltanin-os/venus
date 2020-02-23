include config.mk

.SUFFIXES:
.SUFFIXES: .o .c

HDR=\
	src/common.h

# BIN
BIN=\
	src/venus

BINSHARED=\
	src/ar.c\
	src/chksum.c\
	src/conf.c\
	src/fs.c\
	src/main.c\
	src/utils.c\
	src/venus-ar.c\
	src/venus-cksum.c\
	src/venus.c

# OBJ
BINSHAREDOBJ= $(BINSHARED:.c=.o)

# MAN
MAN1=\
	man/venus-ar.1\
	man/venus-cksum.1\
	man/venus.1

MAN5=\
	man/venus-ar.5\
	man/venus-conf.5\
	man/venus-pkgdesc.5

# ALL
OBJ=$(BIN:=.o) $(BINSHAREDOBJ)

# VAR RULES
all: $(BIN)

$(BIN): $(BINSHAREDOBJ)
$(OBJ): $(HDR) config.mk

# SUFFIX RULES
.o:
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INC) -o $@ -c $<

# USER ACTIONS
install-man: all
	install -dm 755 $(DESTDIR)/$(MANDIR)/man1
	install -dm 755 $(DESTDIR)/$(MANDIR)/man5
	install -cm 644 $(MAN1) $(DESTDIR)/$(MANDIR)/man1
	install -cm 644 $(MAN5) $(DESTDIR)/$(MANDIR)/man5

install: all install-man
	install -dm 755 $(DESTDIR)/$(PREFIX)/bin
	install -cm 755 $(BIN) $(DESTDIR)/$(PREFIX)/bin
	ln -s venus $(DESTDIR)/$(PREFIX)/bin/venus-ar
	ln -s venus $(DESTDIR)/$(PREFIX)/bin/venus-cksum

clean:
	rm -f $(BIN) $(OBJ) $(LIB)

.PHONY:
	all install install-man clean
