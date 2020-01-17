.POSIX:
# 9 Feb 2016

LIBDIR=..

INCS=-I$(LIBDIR)

CXXFLAGS=$(CXXFLAGS_PASSED) $(INCS)
LDFLAGS=$(LDFLAGS_PASSED)

SEQFRAG_OBJS=seqfrag.o ../bust.o ../fseq.o ../mgetline.o  ../prog_bug.o
all:
	cd ..; make all

seqfrag: $(SEQFRAG_OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(SEQFRAG_OBJS)

clean:
	rm -f *.o $(ALL_EXE) *~ core TAGS *.bak

depend:
	makedepend -Y *.cc *.hh

tags:
	etags *.cc *.hh
