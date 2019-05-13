CDCMD=jsish -c
#CC=clang -fsanitize=address 

cups.so: cups.c
	$(CC)  `$(CDCMD) -cflags true cups.so` -lcups `pkg-config --cflags --libs GraphicsMagick`

cups.c: cups.jsc cupsinc.c
	$(CDCMD) $(CDOPTS)  cups.jsc

cupssh: cups.c
	$(CC)  `$(CDCMD) -cflags true cups` -lcups `pkg-config --cflags --libs GraphicsMagick`
	rm -f cupssh
	jsish -z zvfs cups

clean:
	rm -f cups.c cups.so cups cupssh

install: cups.so
	install cups.so /usr/local/lib/jsi/
