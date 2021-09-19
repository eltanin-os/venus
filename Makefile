include config.mk

.SUFFIXES:
.SUFFIXES: .o .c

# BIN
BIN=\
	src/venus-ar\
	src/venus-cksum\
	src/venus-conf\
	src/venus

# MAN
MAN1=\
	man/venus-ar.1\
	man/venus-cksum.1\
	man/venus-conf.1\
	man/venus.1

MAN5=\
	man/venus-ar.5\
	man/venus-chksum.5\
	man/venus-pkgdesc.5\
	man/venus-cfg.5\
	man/venus-dbfile.5

# ALL
OBJ=$(BIN:=.o) $(BINSHAREDOBJ)

# VAR RULES
all: $(BIN)

$(OBJ): config.mk

# SUFFIX RULES
.o:
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INC) -o $@ -c $<

# USER ACTIONS
install-man:
	install -dm 755 $(DESTDIR)/$(MANDIR)/man1
	install -dm 755 $(DESTDIR)/$(MANDIR)/man5
	install -cm 644 $(MAN1) $(DESTDIR)/$(MANDIR)/man1
	install -cm 644 $(MAN5) $(DESTDIR)/$(MANDIR)/man5

install: all install-man
	install -dm 755 $(DESTDIR)/$(PREFIX)/bin
	install -cm 755 $(BIN) $(DESTDIR)/$(PREFIX)/bin

clean:
	rm -f $(BIN) $(OBJ) $(LIB)

.PHONY:
	all install install-man clean
