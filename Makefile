CFLAGS += -Wall -Werror -Wextra -Wformat=2 -Wpedantic -Wshadow -pedantic -std=c99 -g

ls: ls.c

clean:
	rm ls

.PHONY: clean
