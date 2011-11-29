CC = gcc
LIBS = -lgmp -lgc -ledit -ltermcap -lpthread
PROG = FACT
CFLAGS = -c -g3 -I$(INCLUDE_DIR)
LDFLAGS = -rdynamic 
INCLUDE_DIR = ./includes 
INSTALL_DIR = /usr/bin/
SRCS =	FACT_alloc.c FACT_shell.c FACT_basm.c FACT_vm.c  FACT_mpc.c    \
	FACT_num.c FACT_scope.c FACT_error.c FACT_BIFs.c FACT_debug.c   \
	FACT_signals.c FACT_lexer.c FACT_var.c FACT_parser.c FACT_comp.c \
	FACT_file.c FACT_strs.c FACT_main.c FACT_threads.c

OBJS = $(SRCS:.c=.o)

all: $(SRCS) $(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

install:
	rm -f $(INSTALL_DIR)/$(PROG)
	cp $(PROG) $(INSTALL_DIR)

clean:
	rm *.o
	rm $(PROG)