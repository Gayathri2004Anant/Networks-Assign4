lib:
	gcc -c ksocket.c -o ksocket.o
	ar rcs libksocket.a ksocket.o

server:
	gcc initksocket.c -L. -lksocket -o server
	./server

user1:
	gcc -L. user1.c -lksocket -o user1
	./user1

user2:
	gcc -L. user2.c -lksocket -o user2
	./user2

user3:
	gcc -L. user3.c -lksocket -o user3
	./user3

user4:
	gcc -L. user4.c -lksocket -o user4
	./user2

funrun:
	gcc -L. fun.c -lksocket -o fun
	./fun

clean:
	rm -f *.o *.a server user1 user2 socket

all:
	make clean
	make lib
	make server


