CFLAGS = -Wall -Wextra -std=c11 -pedantic -Werror

a.out: main.o parser.o
	$(CC) $^ -o $@
