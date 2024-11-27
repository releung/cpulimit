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

# Target binary
TARGET := cpulimit

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
all: $(TARGET)

# Build target
$(TARGET): $(wildcard *.c *.h)
	$(CC) $(CFLAGS) $(filter-out process_iterator_%.c %.h, $^) $(LDFLAGS) -o $@

# Clean target
clean:
	rm -f *~ $(TARGET)
