# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99

# Build target
TARGET = byteforge

# Object files needed
OBJS = main.o parser.o jit.o

# Default rule
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleanup generated files
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
