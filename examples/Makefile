.SUFFIXES: .c .o

     CC = gcc -g -Wall
TARGETS = motifButtonIvy gtkIvyButton testUnbind
   SRCS = motifButtonIvy.c gtkIvyButton.c testUnbind.c
   OBJS = $(SRCS:.c=.o)
PCRELIB = `pcre-config --libs`
EXTRALIB= -L../src
EXTRAINC=-I../src
GTKLIB  = `pkg-config gtk+-x11-2.0 --libs`
GTKINC  = `pkg-config gtk+-x11-2.0 --cflags`
MOTIFINC=
MOTIFLIB= -L/usr/X11R6/lib -lXm -lXt -lX11
#MOTIFINC= -I/sw/include
#MOTIFLIB= -L/sw/lib -L/usr/X11R6/lib -lXm -lXt -lX11


all: $(TARGETS)

testUnbind: testUnbind.o
	$(CC) -o $@ $< -livy $(PCRELIB) $(EXTRALIB)

gtkIvyButton.o: gtkIvyButton.c
	$(CC) -c $< $(GTKINC) $(EXTRAINC)

gtkIvyButton: gtkIvyButton.o
	$(CC) -o $@ $< $(GTKLIB) $(EXTRALIB) $(PCRELIB) -lglibivy

motifButtonIvy.o: motifButtonIvy.c
	$(CC) -c $< $(MOTIFINC) $(EXTRALIB)

motifButtonIvy: motifButtonIvy.o
	$(CC) -o $@ $< $(MOTIFLIB) -lxtivy -lpcre $(EXTRALIB)

clean:
	rm -fR $(OBJS) $(TARGETS) *~
