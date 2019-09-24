include config.mk

.SUFFIXES:
.SUFFIXES: .o .c

HDR=\
	src/common.h

# BIN
BIN=\
	src/venus-ar\
	src/venus-cksum\
	src/venus

BINSHARED=\
	src/ar.c\
	src/utils.c

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

$(BIN): $(BINSHAREDOBJ) $(@:=.o)
$(OBJ): $(HDR) config.mk

# SUFFIX RULES
.o:
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INC) -o $@ -c $<

# USER ACTIONS
clean:
	rm -f $(BIN) $(OBJ) $(LIB)

.PHONY:
	all install clean
