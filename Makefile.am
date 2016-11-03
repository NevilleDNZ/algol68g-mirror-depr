CYGWIN_RELEASE = 0
bin_PROGRAMS =a68g
a68g_SOURCES = source/a68g.c source/code.c source/environ.c source/genie.c source/gsl.c source/inet.c source/monitor.c source/mp.c source/plotutils.c source/postgresql.c source/pretty.c source/syntax.c
a68g_CFLAGS = -DBINDIR='"$(bindir)"' -DINCLUDEDIR='"$(includedir)"'
TESTS_ENVIRONMENT=./a68g
XFAIL_TESTS=
TESTS=

if EXPORT_DYNAMIC
a68g_LDFLAGS = -Wl,--export-dynamic
else
a68g_LDFLAGS =
endif

pkginclude_HEADERS = source/a68g.h source/a68g-config.h
man_MANS = doc/a68g.1
docdir = @docdir@
doc_DATA = AUTHORS COPYING NEWS README
EXTRA_DIST = $(man_MANS)\
	$(TESTS)\
	./ISSUES

cygwin:
	rm -fvr release/cygwin
	make install prefix=release/cygwin/usr
	cd release/cygwin/usr; mv -v doc share/; rm -fvr include
	cd release/cygwin/usr/share/doc; mv -v * $(PACKAGE_TARNAME)
	cd release/cygwin; tar -cf $(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(CYGWIN_RELEASE).tar usr
	cd release/cygwin; bzip2 $(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(CYGWIN_RELEASE).tar
	gunzip -c ../$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz | bzip2 -c > release/cygwin/$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(CYGWIN_RELEASE)-src.tar.bz2

