CFLAGS += -Wall -Werror -Wextra -Wformat=2 -Wpedantic -Wshadow -pedantic -std=c99 -g
LDLIBS += -lm

ls: ls.c

clean:
	rm ls

.PHONY: clean
