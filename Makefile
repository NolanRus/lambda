CFLAGS = -Wall -Wextra -std=c11 -pedantic

a.out: main.o parser.o
	$(CC) $^ -o $@
