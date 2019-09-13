PROG=et

default: 
	./mini-gcc --config minilib.conf -o et et.c

rebuild:
	make clean
	make

clean:
	$(shell rm $(PROG))

install: $(PROG)
	cp $(PROG) /usr/local/bin

