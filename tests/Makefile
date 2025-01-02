# Compiler configuration
CC ?= gcc

# Compiler flags
override CFLAGS := $(filter-out -std=% -ansi -W%, $(CFLAGS)) \
                   -std=c89 \
                   -Wall \
                   -Wextra \
                   -pedantic \
                   -Wold-style-definition \
                   -Wmissing-prototypes \
                   -Wstrict-prototypes \
                   -Werror \
				   -lpthread

# If CHECK is set to 1, add stricter flags
ifeq ($(CHECK), 1)
override CFLAGS += -Wshadow \
                   -Wfloat-equal \
                   -Wformat=2 \
                   -Wcast-align \
                   -Wpointer-arith \
                   -Wnull-dereference \
                   -Wcast-qual \
                   -Wmissing-declarations \
                   -Wredundant-decls \
                   -Wstrict-aliasing=2 \
                   -Wswitch-enum \
                   -Wswitch-default \
                   -Wconversion \
                   -Wsign-conversion
endif

# Target binaries and source directory
TARGETS := $(patsubst %.c, %, $(wildcard *.c))
SRC := ../src

# Detect operating system
UNAME ?= $(shell uname)

# Platform-specific linker flags
override LDFLAGS += $(if $(findstring FreeBSD, $(UNAME)), -lkvm,) \
                    $(if $(findstring Darwin, $(UNAME)), -lproc,)

# Check for librt availability
override LDFLAGS += $(shell \
    echo "int main(void){ return 0; }" | \
    $(CC) -x c -o /dev/null -lrt - >/dev/null 2>&1 && \
    echo -lrt \
)

# Phony targets
.PHONY: all clean

# Default target
all: $(TARGETS)

# Target rules
busy: busy.c $(wildcard $(SRC)/util.*)
	$(CC) $(CFLAGS) $(filter-out %.h, $^) -lpthread $(LDFLAGS) -o $@

multi_process_busy: multi_process_busy.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

process_iterator_test: process_iterator_test.c \
                       $(filter-out $(SRC)/cpulimit.c, $(wildcard $(SRC)/*.c $(SRC)/*.h))
	$(CC) $(CFLAGS) $(filter-out $(SRC)/process_iterator_%.c %.h, $^) $(LDFLAGS) -o $@

# Clean target
clean:
	rm -f *~ $(TARGETS)
