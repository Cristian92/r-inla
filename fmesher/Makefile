SHELL   = /bin/bash

# USER OPTIONS

# the full path to where to install the program/library, headerfiles and docs.
PREFIX = /usr/local

INSTALL = install
RSYNC = rsync
SED = sed

# The path to the external libraries: metis, taucs, lapack, blas and zlib, often the same as $(PREFIX)
LEXTPREFIX = $(PREFIX)

################################################################################
#
# no changes usually needed below this point
#
################################################################################

CP    = cp
MV    = mv
RM    = rm -f
RMR   = rm -rf
RMDIR = rmdir

GITCOMMIT:= $(shell git log --pretty=format:"%H - %cd" | head -n1)
FLAGS=-O3 -mfpmath=sse -msse2 -funroll-loops -ftracer -pipe
FMESHER = fmesher
FMESHER_TEST = fmesher_test
all: $(FMESHER)


$(FMESHER) : src/$(FMESHER)
	cp src/$(FMESHER) .

src/$(FMESHER) :
	make -C src CC=$(CC) CXX=$(CXX) FLAGS="$(FLAGS)" $(FMESHER)

debug :
##	make clean
	CXX="\"$(CXX) -DDEBUG\"" make $(FMESHER)

$(FMESHER_TEST) : src/$(FMESHER_TEST)
	cp src/$(FMESHER_TEST) .

src/$(FMESHER_TEST) :
	make -C src CC=$(CC) CXX=$(CXX) FLAGS="$(FLAGS)" $(FMESHER_TEST)

doc:
	$(SED) 's/^\(PROJECT_NUMBER[^=]*=\)\(.*\)/\1 $(shell date)/' \
	  < Doxyfile \
	  | doxygen -
	-make -C doc/latex pdf
	$(MV) doc/latex/refman.pdf doc/fmesher-refman.pdf
	$(RMR) doc/latex/

install: $(FMESHER)
	 @umask 022;\
	 mode=644;\
	 for dir in  " " bin doc doc/fmesher doc/fmesher/examples; do \
	   test -d $(PREFIX)/$$dir || mkdir $(PREFIX)/$$dir; \
	   echo "Created" $$dir; \
	 done; \
	 $(INSTALL) -m755 $(FMESHER) $(PREFIX)/bin/$(FMESHER);\
	 test -d doc && \
	   $(RSYNC) -auv --no-p --no-o --no-g --chmod=ugo=rwX --delete doc/ $(PREFIX)/doc/fmesher/; \
	 $(RSYNC) -auv --no-p --no-o --no-g --chmod=ugo=rwX --delete examples/ $(PREFIX)/doc/fmesher/examples/; 

clean:
	make -C src clean
	-$(RM) $(FMESHER) $(FMESHER_TEST)
	-$(RM) examples/koala.out.*
clean-doc:
	-$(RMR) doc/
	-$(RM) example-doxygen-*.txt
clean-deps:
	make -C src clean-deps
depend:
	make -C src depend

gitstamp: dummytarget
	[ -f $@ ] || touch $@
	echo '$(GITCOMMIT)' | cmp -s $@ - || echo '$(GITCOMMIT)' > $@

dummytarget: ;

.PHONY: depend clean clean-doc clean-deps uninstall install tags doc \
	dummytarget \
	src/$(FMESHER) src/$(FMESHER_TEST) \
	debug
