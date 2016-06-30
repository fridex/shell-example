all: clean proj3

.PHONY: clean

proj3:
	gcc -Wall -std=gnu99 proj3.c pidlist.c parse.c -pthread -pedantic -o proj3

clean:
	rm -f proj3

