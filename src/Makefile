
CUDD_PATH = /YOUR/CUDD/PATH

DIR_PATH = /tmp/pla
TARGETS = main
OBJECTS = libpla.o \
	main.o \
	queue.o \
	PLAparser.o

LIBS = -I $(CUDD_PATH) -I $(CUDD_PATH)/util -I $(CUDD_PATH) -I $(CUDD_PATH)/include -static -L $(CUDD_PATH)/.libs/ -lcudd -lm
CFLAGS = -std=gnu99 -g -Wall
LDFLAGS 	= -L.

AR = ar
ARFLAGS = rvs

INCLUDES	= -I.
INCLUDE_FILES =	libpla.h \
	queue.h \
	utils.h \
	PLAparser.h \
	config.h

.PHONY: clean cleanall
.SUFFIXES: .c .h

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) 

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(TARGETS)

main: $(OBJECTS) lib.a $(INCLUDE_FILES)
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -O3 -o $@ $^ $(LIBS)
	#gcc -Wall -o main main.o PLAparser.o queue.o $(LIBS) -std=gnu99

lib.a: $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -f $(TARGETS)

cleanall:
	rm -f $(TARGETS)
	\rm -f *.o *~
	\rm -fr  $(DIR_PATH)
	\rm -fr ./temp
