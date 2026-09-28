CC = gcc
CFLAGS = -Wall -Wextra -Werror

SRC_DIR = src
BIN_DIR = bin
STATIC = static_lib
DYNAMIC = dynamic_lib

SRCS = $(SRC_DIR)/*.c
OBJS = $(patsubst %.c, $(BIN_DIR)/%.o, $(SRCS))

STATIC_TARGET = $(STATIC)/libhtmlparser.a
DYNAMIC_TARGET = $(DYNAMIC)/libhtmlparser.so

.PHONY: all test clean

all: $(STATIC_TARGET) $(DYNAMIC_TARGET)

#TODO
test: all
	echo "To be done later"

$(STATIC_TARGET): $(OBJS) | $(STATIC)
	ar rcs $@ $^

$(DYNAMIC_TARGET): $(OBJS) | $(DYNAMIC)
	gcc --shared -o $@ $^

$(OBJS): $(SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(STATIC):
	mkdir -p $@

$(DYNAMIC):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BIN_DIR) $(STATIC) $(DYNAMIC)

