bin_PROGRAMS =a68g
a68g_SOURCES = a68g.c code.c environ.c genie.c gsl.c inet.c monitor.c mp.c plotutils.c postgresql.c syntax.c edit.c
a68g_CFLAGS = -DBINDIR='"$(bindir)"' -DINCLUDEDIR='"$(includedir)"'
TESTS_ENVIRONMENT=./a68g
TESTS=check-01.a68 check-02.a68 check-03.a68 check-04.a68 check-05.a68 check-06.a68 check-07.a68 check-08.a68 check-09.a68

if EXPORT_DYNAMIC
a68g_LDFLAGS = -Wl,--export-dynamic
else
a68g_LDFLAGS =
endif

pkginclude_HEADERS = a68g.h a68g-config.h
man_MANS = a68g.1
docdir = ${prefix}/doc/@PACKAGE@-@VERSION@
doc_DATA = AUTHORS COPYING NEWS README 
EXTRA_DIST = $(man_MANS) a68g-config.win32.h check-01.a68 check-02.a68 check-03.a68 check-04.a68 check-05.a68 check-06.a68 check-07.a68 check-08.a68 check-09.a68

CYGWIN_RELEASE = 0

cygwin:
	rm -fr release/cygwin
	make install prefix=release/cygwin/usr
## adjust Cygwin binary tree
	cd release/cygwin/usr; mv doc share/; rm -fr include
	cd release/cygwin/usr/share/doc; mv * $(PACKAGE_TARNAME) # optionally strip version
## make Cygwin binary package
	cd release/cygwin; tar -cf $(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(CYGWIN_RELEASE).tar usr
	cd release/cygwin; bzip2 $(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(CYGWIN_RELEASE).tar
## make Cygwin source package
	gunzip -c ../$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz | bzip2 -c > release/cygwin/$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(CYGWIN_RELEASE)-src.tar.bz2
