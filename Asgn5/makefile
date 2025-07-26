library: mysocket.o
	ar -rcs libmsocket.a mysocket.o

mysocket.o: mysocket.h mysocket.c
	gcc -c mysocket.c

clean:
	rm -rf mysocket.o

distclean:
	rm -rf mysocket.o libmsocket.a