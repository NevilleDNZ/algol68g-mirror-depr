# makefile generated on Sun Nov 16 12:11:54 EST 2008 by nevilled.
SHELL=/bin/sh
PACKAGE_NAME=algol68g-mk14.1
USR_BIN=/home/nevilled/prj/a68/tmp.rpmbuild/algol68g-mk14.1/BUILD/algol68g-mk14.1
USR_SRC=./source
USR_CHK=./regression-mk14.1
man_dir=/home/nevilled/prj/a68/tmp.rpmbuild/algol68g-mk14.1/BUILD/algol68g-mk14.1
all a68g::
	@cd $(USR_SRC); $(MAKE) CPP_FLAGS="$(CPP_FLAGS)" CFLAGS="$(CFLAGS)" LD_FLAGS="$(LD_FLAGS)"; mv a68g ..; cd ..
install:	all
	@install -m 755 -s a68g $(USR_BIN)
	@install -m 644 doc/man1/a68g.1 $(man_dir)
uninstall:
	@rm -f $(USR_BIN)/a68g $(man_dir)/a68g.1
depend:
	@cd $(USR_SRC); $(MAKE) depend; cd ..
regression:  all
	@cd $(USR_CHK); ./regression $(DIR); cd ..
regression-ref:  all
	@cd $(USR_CHK); ./regression-ref $(DIR); cd ..
clean:
	@cd $(USR_SRC); $(MAKE) clean; cd ..
	@for f in `echo core`; do rm -f $$f; done
distclean mostlyclean maintainer-clean: clean
	@rm -f makefile $(USR_SRC)/makefile
	@rm -f a68g .a68g.log .a68g.tmp .a68g.x .a68g.x.l *.tmp
	./configure -anonymous
	./configure -docs
dist:   distclean
	./configure -dist
TAGS info dvi:
	@echo "target not implemented"
