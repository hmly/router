CC = gcc
CFLAGS= -Wall -ansi -pedantic -O3

all: router

router: router.c
	$(CC) $(CFLAGS) -o router router.c -lpthread

0:
	./router 0 9070 1 9071 4 9074

1:
	./router 1 9071 0 9070 2 9072 3 9073

2:
	./router 2 9072 1 9071 3 9073

3:
	./router 3 9073 1 9071 2 9072

4:
	./router 4 9074 0 9070 3 9073