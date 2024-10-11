CC ?= gcc
override CFLAGS := $(filter-out -std=% -ansi -W%,$(CFLAGS)) \
			-std=c89 \
			-Wall -Wextra -pedantic \
			-Wmissing-prototypes -Wstrict-prototypes \
			-Wold-style-definition
TARGETS := $(patsubst %.c,%,$(wildcard *.c))
SRC = ../src

UNAME ?= $(shell uname)
ifeq ($(UNAME), FreeBSD)
  override LDFLAGS += -lkvm
endif

ifeq ($(shell echo "int main(void){return 0;}" | $(CC) -x c - -lrt -o /dev/null 2>&1),)
  override LDFLAGS += -lrt
endif

.PHONY: all clean

all: $(TARGETS)

busy: busy.c $(wildcard $(SRC)/util.*)
	$(CC) $(CFLAGS) $(filter-out %.h, $^) -lpthread $(LDFLAGS) -o $@

multi_process_busy: multi_process_busy.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

process_iterator_test: process_iterator_test.c $(filter-out $(SRC)/cpulimit.c, $(wildcard $(SRC)/*.c $(SRC)/*.h))
	$(CC) $(CFLAGS) $(filter-out $(SRC)/process_iterator_%.c %.h, $^) $(LDFLAGS) -o $@

clean:
	rm -f *~ $(TARGETS)
