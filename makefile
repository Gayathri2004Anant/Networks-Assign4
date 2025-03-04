lib:
	gcc -c ksocket.c -o ksocket.o
	ar rcs libksocket.a ksocket.o

server:
	gcc -o server initksocket.c ksocket.c 
	./server

user1:
	gcc -L. user1.c -lksocket -o user1
	./user1

user2:
	gcc -L. user2.c -lksocket -o user2
	./user2

funrun:
	gcc -L. fun.c -lksocket -o fun
	./fun

clean:
	rm -f *.o *.a server user1 user2 socket


