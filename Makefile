CC=gcc
CFLAGS=-c -Wall
LDFLAGS=
AWK=awk

# Add your source files here:
LIB_SOURCES=wrx_comp.c wrx_exec.c wrx_prnt.c wrx_free.c wrx_err.c
LIB_OBJECTS=$(LIB_SOURCES:.c=.o)
LIB=libwregex.a

ifeq ($(BUILD),debug)
# Debug
CFLAGS += -O0 -g
LDFLAGS +=
else
# Release mode
CFLAGS += -O2 -DNDEBUG
LDFLAGS += -s
endif

all: test wgrep docs

lib: $(LIB)

debug:
	make BUILD=debug

test : test.o $(LIB)
	$(CC) $(LDFLAGS) -o $@ $^

wgrep : wgrep.o $(LIB) 	
	$(CC) $(LDFLAGS) -o $@ $^

$(LIB): $(LIB_OBJECTS)
	ar rs $@ $^

.c.o:
	$(CC) $(CFLAGS) $< -o $@

wrx_comp.o : wregex.h wrxcfg.h
wrx_exec.o : wregex.h wrxcfg.h	
wrx_prnt.o : wregex.h wrxcfg.h
wrx_free.o : wregex.h
wrx_err.o : wrxcfg.h

test.o : wregex.h wrx_prnt.h
wgrep.o : wregex.h


docs: manual.html

manual.html: doc.awk wregex.h
	$(AWK) -f $^ > $@

.PHONY : clean wipe

clean: wipe
	-rm -f $(LIB)
	-rm -f test wgrep *.exe
	-rm -rf manual.html
	
wipe:
	-rm -f *.o