.PHONY: main.o build

default: main.o build

main.o: main.c 
	gcc -o $@ -c $< $(CFLAGS)

build:
	gcc -fPIC -o matrix_mult.o -c matrix_mult.c $(CFLAGS)
	gcc -shared -o libmatrix_mult.so matrix_mult.o $(CFLAGS)
	gcc -o main main.o -L. -lmatrix_mult $(CFLAGS)

install:
	cp matrix_mult.h $(INSTALL_DIR)/include
	cp libmatrix_mult.so $(INSTALL_DIR)/lib

clean:
	rm -f *.o *.so main
