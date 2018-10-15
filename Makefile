CFLAGS += -Wall -Werror -Wextra -Wformat=2 -Wpedantic -Wshadow -pedantic -std=c11 -g
LDLIBS += -lm

ls: ls.o print.o compare.o
	$(CC) $(CFLAGS) -o $@ $(LDLIBS) $> $^

clean:
	rm *.o
	rm ls

.PHONY: clean
