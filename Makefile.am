a68g_SOURCES = \
./src/a68g/a68g.c \
./src/a68g/a68glib.c \
./src/a68g/apropos.c \
./src/a68g/bits.c \
./src/a68g/bool.c \
./src/a68g/bottom-up.c \
./src/a68g/brackets.c \
./src/a68g/char.c \
./src/a68g/compiler.c \
./src/a68g/conversion.c \
./src/a68g/curses.c \
./src/a68g/diagnostics.c \
./src/a68g/double.c \
./src/a68g/double-gamic.c \
./src/a68g/double-math.c \
./src/a68g/enquiries.c \
./src/a68g/environ.c \
./src/a68g/equivalence.c \
./src/a68g/extract.c \
./src/a68g/fft.c \
./src/a68g/format.c \
./src/a68g/genie.c \
./src/a68g/heap.c \
./src/a68g/io.c \
./src/a68g/keywords.c \
./src/a68g/laplace.c \
./src/a68g/listing.c \
./src/a68g/mem.c \
./src/a68g/modes.c \
./src/a68g/moid-size.c \
./src/a68g/moid-to-string.c \
./src/a68g/monitor.c \
./src/a68g/mp-bits.c \
./src/a68g/mp.c \
./src/a68g/mp-constant.c \
./src/a68g/mp-gamic.c \
./src/a68g/mp-gamma.c \
./src/a68g/mp-genie.c \
./src/a68g/mp-math.c \
./src/a68g/mp-mpfr.c \
./src/a68g/non-terminal.c \
./src/a68g/options.c \
./src/a68g/parallel.c \
./src/a68g/parser.c \
./src/a68g/physics.c \
./src/a68g/plotutils.c \
./src/a68g/postgresql.c \
./src/a68g/postulates.c \
./src/a68g/prelude-bits.c \
./src/a68g/prelude.c \
./src/a68g/prelude-gsl.c \
./src/a68g/prelude-mathlib.c \
./src/a68g/pretty.c \
./src/a68g/refinement.c \
./src/a68g/regex.c \
./src/a68g/rows.c \
./src/a68g/scanner.c \
./src/a68g/scope.c \
./src/a68g/script.c \
./src/a68g/single.c \
./src/a68g/single-gamic.c \
./src/a68g/single-gsl.c \
./src/a68g/single-math.c \
./src/a68g/single-mathlib.c \
./src/a68g/single-rnd.c \
./src/a68g/socket.c \
./src/a68g/sounds.c \
./src/a68g/taxes.c \
./src/a68g/top-down.c \
./src/a68g/torrix.c \
./src/a68g/transput.c \
./src/a68g/unix.c \
./src/a68g/victal.c \
./src/include/a68g-common.h \
./src/include/a68g-compiler.h \
./src/include/a68g-defines.h \
./src/include/a68g-diagnostics.h \
./src/include/a68g-double.h \
./src/include/a68g-enums.h \
./src/include/a68g-environ.h \
./src/include/a68g-frames.h \
./src/include/a68g-generic.h \
./src/include/a68g-genie.h \
./src/include/a68g.h \
./src/include/a68g-includes.h \
./src/include/a68g-level-3.h \
./src/include/a68g-lib.h \
./src/include/a68g-listing.h \
./src/include/a68g-masks.h \
./src/include/a68g-math.h \
./src/include/a68g-mp.h \
./src/include/a68g-nil.h \
./src/include/a68g-numbers.h \
./src/include/a68g-optimiser.h \
./src/include/a68g-options.h \
./src/include/a68g-parser.h \
./src/include/a68g-physics.h \
./src/include/a68g-platform.h \
./src/include/a68g-postulates.h \
./src/include/a68g-prelude-gsl.h \
./src/include/a68g-prelude.h \
./src/include/a68g-prelude-mathlib.h \
./src/include/a68g-stack.h \
./src/include/a68g-stddef.h \
./src/include/a68g-transput.h \
./src/include/a68g-types.h
includedir = /usr/local/include/algol68g
include_HEADERS = \
./a68g-config.h \
./src/include/a68g-common.h \
./src/include/a68g-compiler.h \
./src/include/a68g-defines.h \
./src/include/a68g-diagnostics.h \
./src/include/a68g-double.h \
./src/include/a68g-enums.h \
./src/include/a68g-environ.h \
./src/include/a68g-frames.h \
./src/include/a68g-generic.h \
./src/include/a68g-genie.h \
./src/include/a68g.h \
./src/include/a68g-includes.h \
./src/include/a68g-level-3.h \
./src/include/a68g-lib.h \
./src/include/a68g-listing.h \
./src/include/a68g-masks.h \
./src/include/a68g-math.h \
./src/include/a68g-mp.h \
./src/include/a68g-nil.h \
./src/include/a68g-numbers.h \
./src/include/a68g-optimiser.h \
./src/include/a68g-options.h \
./src/include/a68g-parser.h \
./src/include/a68g-physics.h \
./src/include/a68g-platform.h \
./src/include/a68g-postulates.h \
./src/include/a68g-prelude-gsl.h \
./src/include/a68g-prelude.h \
./src/include/a68g-prelude-mathlib.h \
./src/include/a68g-stack.h \
./src/include/a68g-stddef.h \
./src/include/a68g-transput.h \
./src/include/a68g-types.h
bin_PROGRAMS = a68g
a68g_CFLAGS = -DBINDIR='"$(bindir)"' -DINCLUDEDIR='"$(includedir)"'
a68g_CPPFLAGS = -I$(top_srcdir)/src/include
TESTS_ENVIRONMENT=./a68g
TESTS=\
	test-set/01-chaos.a68\
	test-set/02-decision.a68\
	test-set/03-digits.a68\
	test-set/04-end-of-time.a68\
	test-set/05-fft.a68\
	test-set/06-fibonacci-grammar.a68\
	test-set/07-formula-manipulation.a68\
	test-set/08-hamming.a68\
	test-set/09-hilbert.a68\
	test-set/10-lisp.a68\
	test-set/11-mandelbrot.a68\
	test-set/12-mastermind.a68\
	test-set/13-math.a68\
	test-set/14-mersenne.a68\
	test-set/15-procedures.a68\
	test-set/16-pseudo-switch.a68\
	test-set/17-qgammainc.a68\
	test-set/18-queens.a68\
	test-set/19-quicksort.a68\
	test-set/20-rationals.a68\
	test-set/21-tukey.a68\
	test-set/22-whetstones.a68
if EXPORT_DYNAMIC
a68g_LDFLAGS = -Wl,--export-dynamic
else
a68g_LDFLAGS =
endif

man_MANS = doc/a68g.1
docdir = @docdir@
doc_DATA = AUTHORS COPYING NEWS README LICENSE
EXTRA_DIST = $(man_MANS)\
	$(TESTS)\
	./ISSUES\
	./LICENSE\
	a68g-config.bsd.h\
	Makefile.bsd

