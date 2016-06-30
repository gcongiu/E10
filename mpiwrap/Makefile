.PHONY: build

INCLUDES=-I$(INSTALL_DIR)/include # include the path to json.h here
CFLAGS=-g -O2

default: build

build:
	g++ -static -o mpiwrap.o -c mpiwrap.cpp $(CFLAGS) $(INCLUDES) -Wall -ljson -lmpich
	ar r libmpiwrap.a mpiwrap.o
	g++ -fPIC -o mpiwrap.o -c mpiwrap.cpp $(CFLAGS) $(INCLUDES) -Wall -ljson -lmpich
	g++ -shared -o libmpiwrap.so mpiwrap.o
	
with_mpe:
	g++ -static -o mpiwrap.o -c mpiwrap.cpp $(CFLAGS) $(INCLUDES) -D_WITH_MPE_ -Wall -ljson -lmpich
	ar r libmpiwrap.a mpiwrap.o
	g++ -fPIC -o mpiwrap.o -c mpiwrap.cpp $(CFLAGS) $(INCLUDES) -D_WITH_MPE_ -Wall -ljson -lmpich
	g++ -shared -o libmpiwrap.so mpiwrap.o

with_darshan:
	g++ -static -o mpiwrap.o -c mpiwrap.cpp $(CFLAGS) $(INCLUDES) -D_WITH_DARSHAN_ -Wall -ljson -lmpich -ldl -ldarshan
	ar r libmpiwrap.a mpiwrap.o
	g++ -fPIC -o mpiwrap.o -c mpiwrap.cpp $(CFLAGS) $(INCLUDES) -D_WITH_DARSHAN_ -Wall -ljson -lmpich -ldl -ldarshan
	g++ -shared -o libmpiwrap.so mpiwrap.o

install:
	install libmpiwrap.a $(INSTALL_DIR)/lib
	install libmpiwrap.so $(INSTALL_DIR)/lib

clean:
	rm *.o *.a *.so

uninstall:
	rm $(INSTALL_DIR)/lib/libmpiwrap.*
