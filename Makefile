CC = clang
CFLAGS = -g -Weverything -Werror

TARGET = sdn

.PHONY: all clean

all: $(TARGET)

clean:
	-rm -rf $(TARGET) *.dSYM
