proyecto1: proyecto1a.o
	gcc proyecto1a.c -c
	gcc proyecto1a.o -o duplicados -lpthread -L. -lmd5
clean:
	rm -f proyecto1a.o duplicados
