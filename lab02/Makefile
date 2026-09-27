CC ?= gcc
CFLAGS ?= -std=gnu11 -Wall -Wextra -O2
LDLIBS ?= -lm

TARGET := calculator
SOURCES := main.c ui.c tui.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean test

all: $(TARGET)

test:
	$(MAKE) -C tests run

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)
