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
                   -Werror

# Target binaries and source directory
TARGETS := $(patsubst %.c, %, $(wildcard *.c))
SRC := ../src

# Detect operating system
UNAME ?= $(shell uname)

# Platform-specific linker flags
ifeq ($(UNAME), FreeBSD)
    override LDFLAGS += -lkvm
endif

ifeq ($(UNAME), Darwin)
    override LDFLAGS += -lproc
endif

# Check for librt availability
override LDFLAGS += $(shell \
    echo "int main(void){ return 0; }" | \
    $(CC) -x c -o /dev/null -lrt - 2>/dev/null && \
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
