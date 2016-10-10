CC ?= cc
AR ?= ar
RANLIB ?= ranlib
SILENT ?= @

CFLAGS_ALL = -MD -MP -Iinclude -include config.h $(CPPFLAGS) $(CFLAGS)
LDFLAGS_ALL = $(LDFLAGS)

LIB = lib
LIB_SRCS = $(shell find ./lib -name "*.c" -type f)
LIB_OBJS = $(LIB_SRCS:.c=.o)

LIBTAI = libtai
LIBTAI_SRCS = $(wildcard libtai/*.c)
LIBTAI_OBJS = $(LIBTAI_SRCS:%.c=%.o)

LIBS = $(LIB) $(LIBTAI)
STATIC_LIBS = $(LIBS:%=%.a)

TEST_SRCS = $(shell find ./test -name "*.c" -type f)
TESTS = $(TEST_SRCS:%.c=%)

PROGS = bin/halt bin/klogcat bin/syslogcat bin/svdir #bin/svrun

all:
$(LIB).a: $(LIB_OBJS)
$(LIBTAI).a: $(LIBTAI_OBJS)
$(PROGS) : % : %.o $(STATIC_LIBS)
$(TESTS) : % : %.o $(STATIC_LIBS)

$(STATIC_LIBS):
	@rm -f $@
	@printf "[AR]	%s\n" "$@"
	${SILENT}$(AR) rc $@ $^
	${SILENT}$(RANLIB) $@

$(PROGS) $(TESTS):
	@printf "[CCLD]	%s\n" "$@"
	${SILENT}$(CC) $(CFLAGS_ALL) $(LDFLAGS_ALL) -o $@ $^

%.o: %.c
	@printf "[CC]	%s\n" "$@"
	${SILENT}$(CC) $(CFLAGS_ALL) -o $@ -c $<

config.h: config.def.h
	cp $< $@

all: config.h $(PROGS)
runit: config.h $(RUNIT)
socklog: config.h $(SOCKLOG)
sockit: config.h $(SOCKIT)
test: config.h $(TESTS)

clean:
	@rm -f $(LIB_OBJS) $(LIBTAI_OBJS)

-include config.mk
-include $(shell find . -name "*.d" -type f)

.PHONY: all runit sockit clean
