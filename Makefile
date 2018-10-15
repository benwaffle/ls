CFLAGS += -Wall -Werror -Wextra -Wformat=2 -Wpedantic -Wshadow -pedantic -std=c11 -g
LDLIBS += -lm

ls: ls.c

clean:
	rm ls

.PHONY: clean
