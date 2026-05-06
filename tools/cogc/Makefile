CC     = gcc
# tools/cogc/ listed first so our globals.h stub shadows src/globals.h.cog
CFLAGS = -I. -I../../src -I../../3rdparty \
         -DPLATFORM_POSIX -DJKM_AI -DLINUX -DSTDSOUND_NULL \
         -m32 -g

SRCS = main.c \
       cogc_verbs.c \
       cogc_libc.c \
       ../../src/Cog/sithCogParse.c \
       ../../src/Cog/y.tab.c \
       ../../src/Cog/lex.yy.c \
       ../../src/General/stdHashTable.c \
       ../../src/General/stdLinklist.c \
       ../../src/General/stdString.c \
       ../../src/General/stdConffile.c \
       ../../src/Win95/std.c \
       ../../src/stdPlatform.c \
       ../../src/external/fcaseopen/fcaseopen.c

OBJS = $(SRCS:.c=.o)

cogc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) cogc
