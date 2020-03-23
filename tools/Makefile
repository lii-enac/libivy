TARGETS = ivyprobe ivythroughput ivytestready ivyperf ivyglibprobe ivyxtprobe #ivyprobe_efence 
OMP_TARGET= ivyprobe_omp ivythroughput_omp 

PCRELIB = `pcre-config --libs`
EXTRALIB= -L../src
EXTRAINC=-I../src

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) $(EXTRAINC) -c $*.c
.cpp.o:
	$(CXX) $(CFLAGS) $(EXTRAINC) $(BUGGY_DEBIAN_OPTION) -c $*.cpp


all: $(TARGETS)
omp: $(OMP_TARGET)

ivyprobe: ivyprobe.o ../src/libivy.a
	$(CC)  $(CFLAGS) $(EXTRAINC) -o $@ ivyprobe.o  $(EXTRALIB) -livy $(PCRELIB)  

ivythroughput: ivythroughput.o ../src/libivy.a
	$(CXX)  $(CFLAGS) $(EXTRAINC) -o $@ ivythroughput.o $(EXTRALIB) -livy $(PCRELIB) -lpcrecpp  

ivytestready: ivytestready.o ../src/libivy.a
	$(CC)  $(CFLAGS) $(EXTRAINC) -o $@ ivytestready.o $(EXTRALIB) -livy $(PCRELIB) -lpcrecpp  

ivyprobe_efence: ivyprobe.o ../src/libivy.a
	$(CC)  $(CFLAGS) $(EXTRAINC) -o $@ ivyprobe.o  $(EXTRALIB) -livy $(PCRELIB) -lpcrecpp   -lefence



ivyprobe_omp: ivyprobe.o ../src/libivy_omp.a
	$(CC)  $(CFLAGS) $(EXTRAINC) -o $@ ivyprobe.o -L. -livy_omp $(PCRELIB) $(EXTRALIB) $(OMPLIB)

ivythroughput_omp: ivythroughput.o ../src/libivy_omp.a
	$(CXX)  $(CFLAGS) $(EXTRAINC) -o $@ ivythroughput.o -L. -livy_omp -lpcrecpp $(PCRELIB) $(EXTRALIB) $(OMPLIB)

ivyprobe.o : ivyprobe.c 
	$(CC)  $(CFLAGS) $(EXTRAINC) $(REGEXP) -c ivyprobe.c -o $@

ivyperf: ivyperf.o ../src/libivy.a
	$(CC)  $(CFLAGS) $(EXTRAINC) -o $@ ivyperf.o -L. -livy $(PCRELIB) $(EXTRALIB)

ivyxtprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(EXTRAINC) $(REGEXP) -DXTMAINLOOP -c ivyprobe.c -o $@ $(XTINC)

ivyxtprobe: ivyxtprobe.o ../src/libxtivy.a
	$(CC) -o $@ ivyxtprobe.o -L. $(EXTRALIB) $(XTLIB) -lxtivy  $(PCRELIB)

ivyglibprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(EXTRAINC) $(REGEXP) -DGLIBMAINLOOP -c ivyprobe.c -o ivyglibprobe.o $(GLIBINC)

ivyglibprobe: ivyglibprobe.o ../src/libglibivy.a
	$(CC) -o ivyglibprobe ivyglibprobe.o -L. $(GLIBLIB) -lglibivy $(PCRELIB) $(EXTRALIB)

ivyglutprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(EXTRAINC) $(REGEXP) -DGLUTMAINLOOP -c ivyprobe.c -o ivyglutprobe.o $(GLUTINC)

ivyglutprobe: ivyglutprobe.o ../src/libglutivy.a
	$(CC) -o ivyglutprobe ivyglutprobe.o -L. $(GLUTLIB) -lglutivy $(PCRELIB) $(EXTRALIB

installbins: $(TARGETS)
	test -d $(DESTDIR)$(PREFIX)/bin || mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m755 ivyprobe $(DESTDIR)$(PREFIX)/bin

installbins_omp:  $(OMP_TARGET)
	test -d $(DESTDIR)$(PREFIX)/bin || mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m755 ivyprobe_omp $(DESTDIR)$(PREFIX)/bin

install: installbins 
install_omp: installbins_omp


clean:
	rm -fR $(TARGETS) $(OMP_TARGET) *~ *.o
