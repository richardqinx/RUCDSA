CC ?= gcc
CFLAGS ?= -std=gnu11 -Wall -Wextra -Werror -O2
LDLIBS ?= -lm

TARGET := test_runner

.PHONY: all run clean

all: $(TARGET)

$(TARGET): test.c ../container_of.h ../list.h ../vec.h ../polynomial.h
	$(CC) $(CFLAGS) -I.. test.c $(LDLIBS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
