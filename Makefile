CFLAGS = -g -Wall -Wextra -std=c11 -pedantic -Werror

a.out: main.o parser.o
	$(CC) $^ -o $@

clean:
	$(RM) a.out *.o
