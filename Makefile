TARGET = flow_divert-memleak

all: $(TARGET)

CFLAGS = -O2 -Wall -Werror -Wpedantic

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f -- $(TARGET)
