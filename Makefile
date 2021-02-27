all: th_alloc.so test test1

CFLAGS=-Wall -Werror -g

th_alloc.so: th_alloc.c
	gcc $(CFLAGS) -fPIC -shared th_alloc.c -o th_alloc.so

test: test.c
	gcc $(CFLAGS) test.c -o test

test1: test1.c
	gcc $(CFLAGS) test1.c -o test1

update:
	git checkout master
	git pull https://github.com/comp530-f20/lab2.git master

clean:
	rm -f test th_alloc.so
