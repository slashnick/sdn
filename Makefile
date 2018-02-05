CC = clang
CFLAGS = -g -Weverything -Werror -Wno-missing-noreturn -Wno-padded -Wno-packed

SOURCES = sdn.c event.c event.h client.c client.h
OBJECTS = sdn.o event.o client.o
TARGET = sdn

.PHONY: all clean format test

all: $(TARGET)

$(TARGET): $(OBJECTS)

clean:
	-rm -rf $(TARGET) $(OBJECTS) *.dSYM

format:
	clang-format -i $(SOURCES)

test: $(TARGET)
	test/run.sh
