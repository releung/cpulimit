CC ?= gcc
override CFLAGS := $(filter-out -std=% -ansi -W%,$(CFLAGS)) \
			-std=c89 \
			-Wall -Wextra -pedantic \
			-Wmissing-prototypes -Wstrict-prototypes \
			-Wold-style-definition
TARGETS = busy multi_process_busy process_iterator_test
SRC = ../src
SYSLIBS ?= -lpthread $(SRC)/util.c
LIBS := $(SRC)/list.c $(SRC)/process_iterator.c $(SRC)/process_group.c \
		$(SRC)/process_table.c $(SRC)/util.c 

UNAME ?= $(shell uname)
ifeq ($(UNAME), FreeBSD)
  override LDFLAGS += -lkvm
endif

ifeq ($(shell echo "int main(void){return 0;}" | $(CC) -x c - -lrt -o /dev/null 2>&1),)
  override LDFLAGS += -lrt
endif

.PHONY: all clean

all: $(TARGETS)

busy: busy.c
	$(CC) $(CFLAGS) $^ $(SYSLIBS) $(LDFLAGS) -o $@

multi_process_busy: multi_process_busy.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

process_iterator_test: process_iterator_test.c $(LIBS)
	$(CC) $(CFLAGS) -I$(SRC) $^ $(LDFLAGS) -o $@

clean:
	rm -f *~ $(TARGETS)
