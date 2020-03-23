#
#	Ivy, C interface
#
#	Copyright (C) 1997-2002
#	Centre d'Études de la Navigation Aérienne
#
#	Makefile
#
#	Authors: François-Régis Colin <fcolin@cena.fr>
#		 Stéphane Chatty <chatty@cena.fr>
#		 Yannick Jestin <jestin@cena.fr>
#
#	Please refer to file version.h for the
#	copyright notice regarding this software
#


# change this in version.h too !!!!
MAJOR=3
MINOR=15

PERHAPS64 := $(shell getconf LONG_BIT | perl -ne "print /64/ ? '64' : '';")
LIB = /lib$(PERHAPS64)

# by default use fPIC on all systems
FPIC ?= -fPIC

ifndef PREFIX
	export PREFIX=/usr/local
endif

X11_PREFIX:= $(shell pkg-config --variable=prefix x11)
ifndef X11_PREFIX
	X11_PREFIX = /usr/X11R6
endif

# Export all variables (Petr Mejzlik)
export

XTINC = -I$(X11_PREFIX)/include
XTLIB = -L$(X11_PREFIX)$(LIB) -lXt -lX11 -lSM -lICE
HAVE_GLIB = $(shell pkg-config --exists glib-2.0 && echo 'yes')
GLIBINC = `pkg-config --cflags glib-2.0`
GLIBLIB = `pkg-config --libs glib-2.0`
GLUTINC = -I/usr/include -I.
GLUTLIB = -L. -lglut -lGLU -lGL -lX11 -lXmu
# use these if you want standard Tcl ( 8.3 on debian woody ... )

#TCLVERS = `perl -e '@_=sort map (m|/usr/lib/libtcl(\d\.\d)\.so|, glob ("/usr/lib/libtcl*")); print pop @_'`
TCLVERS:= $(shell perl -e '@_=sort map (m|/usr/lib/libtcl(\d\.\d)\.so|, glob ("/usr/lib/libtcl*")); print pop @_')
ifndef TCLVERS
TCLVERS:= $(shell perl -e '@_=sort map (m|/usr/lib64/libtcl(\d\.\d)\.so|, glob ("/usr/lib64/libtcl*")); print pop @_')
endif
ifndef TCLVERS
#TCLVERS=8.4
TCLVERS=
endif

TCLINCL = -I/usr/include/tcl$(TCLVERS)
TCLLIB = -ltcl$(TCLVERS)
#TCLINCL	= -I/usr/include/tcl8.4
#TCLLIB = -ltcl8.4
# use these if you want to use Aqua Tcl on macOSX
#TCLINC = -I/Library/Frameworks/Tcl.framework/Headers
#TCLLIB = -framework Tcl
PCREINC = $(shell pcre-config --cflags)
PCRELIB = $(shell pcre-config --libs)
CASESENSITIVE = "yes"
ifeq ($(CASESENSITIVE), "no")
	PCRE_OPT=PCRE_CASELESS
	REGCOMP_OPT=REG_ICASE
else
	PCRE_OPT=0
	REGCOMP_OPT=REG_EXTENDED
endif

#PCREOBJ = `pcre-config --prefix`/lib/libpcre.a
PCREOBJ =

ifeq ($(shell uname -s),SunOS)
	EXTRALIB=-lsocket -lnsl # for solaris
else
	EXTRALIB=
endif


TOOLS_DIR = ../tools
export CC=gcc
export CPP='g++'
LIBTOOL=ar q # linux and solaris
#LIBTOOL=libtool -static -o

#REGEXP = -DGNU_REGEXP	-DREGCOMP_OPT=$(REGCOMP_OPT) # deprecated !
REGEXP= -DUSE_PCRE_REGEX -DPCRE_OPT=$(PCRE_OPT)
# on activeTCL , set #define CHANNEL to null, and add ivyloop.o in the ivytcl target,
# see below
CHANNEL = -DTCL_CHANNEL_INTEGRATION

# DEBUG
CFLAGS_DEBUG = -g -Wall -Wshadow -DDEBUG $(FPIC)
# RELEASE
CFLAGS_RELEASE = -O2 -Wall -Wshadow $(FPIC)

ifdef DEBUG
	export CFLAGS += $(CFLAGS_DEBUG)
	LIB:= $(LIB)/debug
else
	export CFLAGS += $(CFLAGS_RELEASE)
endif



BUGGY_DEBIAN_OPTION = -DPCRECPP_EXP_DEFN="" -DPCRECPP_EXP_DECL=""
OMPCFLAGS = $(CFLAGS) -fopenmp -DOPENMP=1 -D_REENTRANT
OMPLIB = -lgomp -lpthread

OBJ = ivyloop.o timer.o ivysocket.o ivy.o ivybuffer.o ivyfifo.o ivybind.o intervalRegexp.o param.o
OMPOBJ = ivyloop.o timer.o ivysocket_omp.o ivy_omp.o ivybuffer.o ivyfifo.o ivybind.o intervalRegexp.o param.o
GOBJ = ivyloop.o timer.o ivysocket.o givy.o ivybuffer.o ivyfifo.o ivybind.o intervalRegexp.o param.o
XTOBJ = ivyxtloop.o ivysocket.o ivy.o ivybuffer.o ivyfifo.o ivybind.o intervalRegexp.o param.o
GLIBOBJ = ivyglibloop.o ivysocket.o ivy.o ivybuffer.o ivyfifo.o ivybind.o intervalRegexp.o param.o
GLUTOBJ = ivyglutloop.o ivysocket.o ivy.o ivybuffer.o ivyfifo.o ivybind.o intervalRegexp.o param.o
TCLOBJ = ivytcl.o  timer.o ivysocket.o givy.o ivybuffer.o ivyfifo.o ivybind.o intervalRegexp.o param.o
# WINDOWS add ivyloop.o if TCL_CHANNEL_INTEGRATION is not set

TARGETLIBS=libivy.so.$(MAJOR).$(MINOR) libgivy.so.$(MAJOR).$(MINOR) libxtivy.so.$(MAJOR).$(MINOR)  libtclivy.so.$(MAJOR).$(MINOR)
STATICTARGETLIBS=libivy.a libgivy.a libxtivy.a libtclivy.a

ifeq ($(HAVE_GLIB),yes)
		TARGETLIBS += libglibivy.so.$(MAJOR).$(MINOR)
		STATICTARGETLIBS += libglibivy.a
endif


# not yet need Modified Glut ivyglutprobe

.c.o:
	$(CC) $(CFLAGS) -c $*.c
.cpp.o:
	$(CPP) $(CFLAGS) $(BUGGY_DEBIAN_OPTION) -c $*.cpp

all: static-libs shared-libs pkgconf  tools
libomp: libivy_omp.a libivy_omp.so.$(MAJOR).$(MINOR)
omp: libomp
	@(cd $(TOOLS_DIR) && $(MAKE) omp)

static-libs:  $(STATICTARGETLIBS)
# not yet need Modified Glut libglutivy.a

shared-libs: $(TARGETLIBS)
# not yet need Modified Glut libglutivy.so.$(MAJOR).$(MINOR)


ivybind.o: ivybind.c
	$(CC) -c $(CFLAGS) $(REGEXP) $(PCREINC) ivybind.c


ivytcl.o: ivytcl.c
	$(CC) -c $(CFLAGS) $(TCLINCL) $(CHANNEL) ivytcl.c

givy.o: ivy.c
	$(CC) -c $(CFLAGS) -DDEBUG -o givy.o ivy.c

ivy_omp.o: ivy.c
	$(CC) -c $(OMPCFLAGS)  -o ivy_omp.o ivy.c

ivysocket_omp.o: ivysocket.c
	$(CC) -c $(OMPCFLAGS)  -o ivysocket_omp.o ivysocket.c

ivyglutloop.o: ivyglutloop.c ivyglutloop.h
	$(CC) -c $(CFLAGS) $(GLUTINC) ivyglutloop.c

ivyglibloop.o: ivyglibloop.c ivyglibloop.h
	$(CC) -c $(CFLAGS) $(GLIBINC) ivyglibloop.c


libivy.a: $(OBJ)
	rm -f $@
	$(LIBTOOL) $@ $(OBJ)

libivy_omp.a: $(OMPOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(OMPOBJ)

libgivy.a: $(GOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(GOBJ)

libxtivy.a: $(XTOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(XTOBJ)

libglibivy.a: $(GLIBOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(GLIBOBJ)

libglutivy.a: $(GLUTOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(GLUTOBJ)

libtclivy.a: $(TCLOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(TCLOBJ) $(PCREOBJ)
# TODO this PCREOBJ is a dirty hack

libivy.so.$(MAJOR).$(MINOR): $(OBJ)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,libivy.so.$(MAJOR) -o $@ $(OBJ) $(PCRELIB)
#	$(CC) -G -Wl,-h,libivy.so.$(MAJOR) -o $@ $(OBJ)		#solaris
#	libtool -dynamic -o $@ $(OBJ) $(PCRELIB) -lc

libivy_omp.so.$(MAJOR).$(MINOR): $(OMPOBJ)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,libivy.so.$(MAJOR) -o $@ $(OMPOBJ) $(PCRELIB) $(OMPLIB)
#	$(CC) -G -Wl,-h,libivy.so.$(MAJOR) -o $@ $(OBJ)		#solaris
#	libtool -dynamic -o $@ $(OBJ) $(PCRELIB) -lc

libgivy.so.$(MAJOR).$(MINOR): $(GOBJ)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,libgivy.so.$(MAJOR) -o $@ $(GOBJ) $(PCRELIB)
#	$(CC) -G -Wl,-h,libgivy.so.$(MAJOR) -o $@ $(GOBJ) #solaris
#	libtool -dynamic -o $@ $(GOBJ) $(PCRELIB) -lc

libxtivy.so.$(MAJOR).$(MINOR): $(XTOBJ)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,libxtivy.so.$(MAJOR) -o $@ $(XTOBJ) $(XTLIB) $(PCRELIB)
#	$(CC) -G -Wl,-h,libxtivy.so.$(MAJOR) -o $@ $(XTOBJ) $(XTLIB) #solaris
#	libtool -dynamic -o $@ $(XTOBJ) $(XTLIB) $(PCRELIB) -lc

libglibivy.so.$(MAJOR).$(MINOR): $(GLIBOBJ)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,libglibivy.so.$(MAJOR) -o $@ $(GLIBOBJ) $(GLIBLIB) $(PCRELIB)
#	$(CC) -G -Wl,-h,libglibivy.so.$(MAJOR) -o $@ $(GLIBOBJ) $(GLIBLIB) #solaris
#	libtool -dynamic -o $@ $(GLIBOBJ) $(GLIBLIB) $(PCRELIB) -lc

libglutivy.so.$(MAJOR).$(MINOR): $(GLIBOBJ)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,libglutivy.so.$(MAJOR) -o $@ $(GLUTOBJ) $(GLUTLIB) $(PCRELIB)
#	$(CC) -G -Wl,-h,libglutivy.so.$(MAJOR) -o $@ $(GLUTOBJ) $(GLUTLIB) #	solaris
#	libtool -dynamic -o $@ $(GLUTOBJ) $(GLUTLIB) $(PCRELIB) -lc

libtclivy.so.$(MAJOR).$(MINOR): $(TCLOBJ)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,libtclivy.so.$(MAJOR) -o $@ $(TCLOBJ) $(TCLLIB) $(PCREOBJ) $(PCRELIB)
#	$(CC) -G -Wl,-h,libtclivy.so.$(MAJOR) -o $@ $(TCLOBJ) $(TCLLIB) $(PCREOBJ) # solaris
#	libtool -dynamic -o $@ $(TCLOBJ) $(TCLLIB) $(PCREOBJ) $(PCRELIB) -lc

distclean: clean
clean:
	-rm -f $(TARGETS) $(TARGETLIBS)  *.o *.a *.so *.so.* *~ *.pc
	@(cd $(TOOLS_DIR) && $(MAKE) clean)


installlibs: static-libs shared-libs
	test -d $(DESTDIR)$(PREFIX)$(LIB)  || mkdir -p $(DESTDIR)$(PREFIX)$(LIB)
	-test -d $(DESTDIR)$(X11_PREFIX)$(LIB)  || mkdir -p $(DESTDIR)$(X11_PREFIX)$(LIB)

	install -m644 libivy.a						$(DESTDIR)$(PREFIX)$(LIB)
	install -m644 libgivy.a						$(DESTDIR)$(PREFIX)$(LIB)
	-install -m644 libxtivy.a					$(DESTDIR)$(X11_PREFIX)$(LIB)
	install -m644 libtclivy.a					$(DESTDIR)$(PREFIX)$(LIB)
	install -m644 libglibivy.a					$(DESTDIR)$(PREFIX)$(LIB)

	install -m644 libivy.so.$(MAJOR).$(MINOR)			$(DESTDIR)$(PREFIX)$(LIB)
	install -m644 libgivy.so.$(MAJOR).$(MINOR)			$(DESTDIR)$(PREFIX)$(LIB)
	-install -m644 libxtivy.so.$(MAJOR).$(MINOR)			$(DESTDIR)$(X11_PREFIX)$(LIB)
	install -m644 libtclivy.so.$(MAJOR).$(MINOR)			$(DESTDIR)$(PREFIX)$(LIB)
	install -m644 libglibivy.so.$(MAJOR).$(MINOR)			$(DESTDIR)$(PREFIX)$(LIB)

installlibs_omp: static-libs shared-libs
	test -d $(DESTDIR)$(PREFIX)$(LIB)  || mkdir -p $(DESTDIR)$(PREFIX)$(LIB)
	-test -d $(DESTDIR)$(X11_PREFIX)$(LIB)  || mkdir -p $(DESTDIR)$(X11_PREFIX)$(LIB)

	install -m644 libivy_omp.a					$(DESTDIR)$(PREFIX)$(LIB)
	install -m644 libivy_omp.so.$(MAJOR).$(MINOR)			$(DESTDIR)$(PREFIX)$(LIB)

includes:
	test -d $(DESTDIR)$(PREFIX)/include/Ivy || mkdir -p $(DESTDIR)$(PREFIX)/include/Ivy
	install -m644 ivy.h						$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivybind.h						$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivybuffer.h					$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivychannel.h					$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivydebug.h					$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivyglibloop.h					$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivyglutloop.h*					$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivyloop.h						$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivysocket.h					$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivytcl.h*						$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 ivyxtloop.h					$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 list.h						$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 timer.h						$(DESTDIR)$(PREFIX)/include/Ivy/
	install -m644 version.h						$(DESTDIR)$(PREFIX)/include/Ivy/

installliblinks: installlibs
	ln -fs $(PREFIX)$(LIB)/libivy.so.$(MAJOR).$(MINOR)		$(DESTDIR)$(PREFIX)$(LIB)/libivy.so
	ln -fs $(PREFIX)$(LIB)/libivy.so.$(MAJOR).$(MINOR)		$(DESTDIR)$(PREFIX)$(LIB)/libivy.so.$(MAJOR)
	ln -fs $(PREFIX)$(LIB)/libgivy.so.$(MAJOR).$(MINOR)		$(DESTDIR)$(PREFIX)$(LIB)/libgivy.so
	ln -fs $(PREFIX)$(LIB)/libgivy.so.$(MAJOR).$(MINOR)		$(DESTDIR)$(PREFIX)$(LIB)/libgivy.so.$(MAJOR)
	-ln -fs $(X11_PREFIX)$(LIB)/libxtivy.so.$(MAJOR).$(MINOR)	$(DESTDIR)$(X11_PREFIX)$(LIB)/libxtivy.so
	-ln -fs $(X11_PREFIX)$(LIB)/libxtivy.so.$(MAJOR).$(MINOR)	$(DESTDIR)$(X11_PREFIX)$(LIB)/libxtivy.so.$(MAJOR)
	ln -fs $(PREFIX)$(LIB)/libtclivy.so.$(MAJOR).$(MINOR)		$(DESTDIR)$(PREFIX)$(LIB)/libtclivy.so
	ln -fs $(PREFIX)$(LIB)/libtclivy.so.$(MAJOR).$(MINOR)		$(DESTDIR)$(PREFIX)$(LIB)/libtclivy.so.$(MAJOR)
	ln -fs $(PREFIX)$(LIB)/libivy_omp.so.$(MAJOR).$(MINOR)		$(DESTDIR)$(PREFIX)$(LIB)/libivy_omp.so

installbins:
	@(cd $(TOOLS_DIR) && $(MAKE) install)

installbins_omp:
	@(cd $(TOOLS_DIR) && $(MAKE) install_omp)



installdocs:
	test -d $(DESTDIR)$(PREFIX)/share/doc/ivy-c || mkdir -p $(DESTDIR)$(PREFIX)/share/doc/ivy-c
	for f in `find	../doc/*.html -type f -maxdepth 1`; do \
		install -m 644 $$f					$(DESTDIR)$(PREFIX)/share/doc/ivy-c; \
	done
	test -d $(DESTDIR)$(PREFIX)/man/man1 || mkdir -p $(DESTDIR)$(PREFIX)/man/man1
	for f in `find	../doc/*.1 -type f -maxdepth 1`; do \
		install -m 644 $$f					$(DESTDIR)$(PREFIX)/man/man1; \
	done
	test -d $(DESTDIR)$(PREFIX)/share/doc/ivy-c/examples || mkdir -p $(DESTDIR)$(PREFIX)/share/doc/ivy-c/examples
	install -m 644 version.h					$(DESTDIR)$(PREFIX)/share/doc/ivy-c/copyright
	install -m 644 ../examples/motifButtonIvy.c			$(DESTDIR)$(PREFIX)/share/doc/ivy-c/examples
	install -m 644 ../examples/testUnbind.c				$(DESTDIR)$(PREFIX)/share/doc/ivy-c/examples
	install -m 766 ../examples/Test.tcl				$(DESTDIR)$(PREFIX)/share/doc/ivy-c/examples
	install -m 766 ../examples/button.tk				$(DESTDIR)$(PREFIX)/share/doc/ivy-c/examples
	install -m 766 ../examples/unBind.tcl				$(DESTDIR)$(PREFIX)/share/doc/ivy-c/examples

install: installlibs installbins installliblinks installdocs includes installpkgconf
install_omp: installlibs_omp installbins_omp
tools: static-libs
	@(cd $(TOOLS_DIR) && $(MAKE))
tools_omp: libomp
	@(cd $(TOOLS_DIR) && $(MAKE) omp)
rpm::
	/usr/bin/rpmize

pkgconf:
	for f in *.pc.in ; do \
		sed -e 's,@PREFIX@,$(PREFIX),; s,@MAJOR@,$(MAJOR),; s,@MINOR@,$(MINOR),; s,@PCRELIB@,$(PCRELIB),; s,@EXTRALIB@,$(EXTRALIB),' $$f > $$(echo $$f | cut -f 1,2 -d .); \
	done

installpkgconf: pkgconf
	test -d $(DESTDIR)$(PREFIX)/lib/pkgconfig || mkdir -p $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 644 *.pc	$(DESTDIR)$(PREFIX)/lib/pkgconfig
