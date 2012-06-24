bin_PROGRAMS =a68g
a68g_SOURCES = source/a68g.c source/code.c source/environ.c source/genie.c source/gsl.c source/inet.c source/monitor.c source/mp.c source/plotutils.c source/postgresql.c source/pretty.c source/syntax.c source/edit.c
a68g_CFLAGS = -DBINDIR='"$(bindir)"' -DINCLUDEDIR='"$(includedir)"'
TESTS_ENVIRONMENT=./a68g
TESTS=check/check-01.a68 check/check-02.a68 check/check-03.a68 check/check-04.a68 check/check-05.a68 check/check-06.a68 check/check-07.a68 check/check-08.a68 check/check-09.a68 check/check-10.a68

if EXPORT_DYNAMIC
a68g_LDFLAGS = -Wl,--export-dynamic
else
a68g_LDFLAGS =
endif

pkginclude_HEADERS = source/a68g.h source/a68g-config.h
man_MANS = doc/a68g.1
docdir = ${prefix}/doc/@PACKAGE@-@VERSION@
doc_DATA = AUTHORS COPYING NEWS README 
EXTRA_DIST = $(man_MANS) source/a68g-config.win32.h check/check-01.a68 check/check-02.a68 check/check-03.a68 check/check-04.a68 check/check-05.a68 check/check-06.a68 check/check-07.a68 check/check-08.a68 check/check-09.a68 check/check-10.a68

