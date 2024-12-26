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

# Target binary
TARGET := cpulimit

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
all: $(TARGET)

# Build target
$(TARGET): $(wildcard *.c *.h)
	$(CC) $(CFLAGS) $(filter-out process_iterator_%.c %.h, $^) $(LDFLAGS) -o $@

# Clean target
clean:
	rm -f *~ $(TARGET)
