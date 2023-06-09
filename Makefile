 # 8 oct 2015
# my notes:
#  * -fsanitize=address from clang seems to break valgrind
.POSIX:

OPT=-g -O2

LDFLAGS=-pthread
PARA_LIB = -lpthread

CXX=g++
CXXFLAGS=-Wall -Wextra -Wunused -Wuninitialized -std=c++11 -pedantic  -Wno-unknown-pragmas $(OPT) -pthread ## -fsanitize=thread  -pie -fPIE #  -fsanitize=address 
LDFLAGS=$(CXXFLAGS) 

# possibly useful -L/home/torda/junk/gperftools-master/.libs -lprofiler

#CLPATH=/work/torda/no_backup/clang+llvm-3.8.1-x86_64-opensuse13.2
#CXX=$(CLPATH)/bin/clang++
#CXXFLAGS=$(OPT)  -Weverything -pedantic -Wno-exit-time-destructors -Wno-c++98-compat  -Wno-format-n#onliteral -std=c++11 -stdlib=libc++ 
#LDFLAGS=$(CXXFLAGS) -rpath $(CLPATH)/lib -stdlib=libc++ -nodefaultlibs -lc++ -lc++abi -lm -lc -lgcc_s -lgcc -lpthread


ALL_EXE = clean_seqs reduce findpath seqfrag_e split_seq
# These can be compiled to free-standing executables, depending on some #defines,
# but this is only for testing.
TEST_EXE =  seq_index sym_mat check_white_start_end

all: $(ALL_EXE)

check_white_start_end: filt_string.cc
	$(CXX) -o check_white_end -D check_white_start_end $(LDFLAGS) $<

seqfrag_e:
	cd seqfrag; make CXX=$(CXX) "CXXFLAGS_PASSED=$(CXXFLAGS)" "LDFLAGS_PASSED=$(LDFLAGS)" seqfrag

REDUCE_OBJS = reduce.o bust.o distmat_rd.o fseq.o fseq_prop.o mgetline.o plot_dist_reduce.o prog_bug.o
reduce:$(REDUCE_OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(REDUCE_OBJS)

FINDPATḦ_OBJS = bust.o findpath.o distmat_rd.o filt_string.o fseq.o \
	mgetline.o pathprint.o prog_bug.o seq_index.o delay.o
findpath: $(FINDPATḦ_OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(FINDPATḦ_OBJS)

# This is not an interesting executable. It is just for testing the functions
# in seq_index.cc.
SEQ_INDEX_OBJS = fseq.o getopt.o seq_index.o mgetline.o prog_bug.o
seq_index:$(SEQ_INDEX_OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(SEQ_INDEX_OBJS)

SPLIT_SEQ_OBJS = bust.o split_seq.o fseq.o mgetline.o prog_bug.o
split_seq:$(SPLIT_SEQ_OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(SPLIT_SEQ_OBJS)

SYM_MAT_OBJS = sym_mat.o
sym_mat.o: sym_mat.c
	$(CXX) -c sym_mat.c $(LDFLAGS) -Dcheck_me
sym_mat:$(SŸM_MAT_OBJS)
	$(CXX) -o $@ $(LDFLAGS) -Dcheck_me $(SYM_MAT_OBJS)

FILT_OBJS = filt_string.o fseq.o mgetline.o prog_bug.o
filt_string: $(FILT_OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(PARA_LIB)  $(FILT_OBJS)

TQOBJS=tqtest.o delay.o
tqtest: $(TQOBJS)
	$(CXX) -o $@  $(LDFLAGS) $(TQOBJS)

CLEAN_SEQS_OBJS=bust.o clean_seqs.o fseq.o mgetline.o prog_bug.o
clean_seqs: $(CLEAN_SEQS_OBJS)
	$(CXX) -o $@  $(LDFLAGS) $(PARA_LIB) $(CLEAN_SEQS_OBJS)

clean:
	rm -f *.o $(ALL_EXE) *~ core TAGS *.bak
	cd seqfrag; make clean

depend:
	$(CXX)  -MM $(CXXFLAGS) *.cc *.hh > deps
#       makedepend -Y -m *.cc *.hh

tags:
	etags *.cc *.hh

valgrind: $(EXE_NAME)
	(cd /work/torda/people/inari/2jkf_oct15/merge;\
	valgrind --leak-check=full  \
        /home/torda/c/seq_utils/reduce -a sacred \
        -f -v -c first \
            merge_align.fa merge.fa.hat2 reduced_100.fa 100)

helgrind: $(EXE_NAME)
	(cd /work/torda/people/inari/2jkf_oct15/merge;\
	valgrind --tool=helgrind \
           /home/torda/c/seq_utils/reduce -a sacred -c first \
            merge_align.fa merge.fa.hat2 reduced_100.fa 100)
#	valgrind --tool=helgrind $(EXE_NAME) example/big.fa x

# DO NOT DELETE
bust.o: bust.cc bust.hh
check_rmv.o: check_rmv.cc
clean_seqs.o: clean_seqs.cc regex_prob.hh bust.hh fseq.hh mgetline.hh \
 t_queue.hh t_queue.tcc
delay.o: delay.cc delay.hh
distmat_rd.o: distmat_rd.cc bust.hh distmat_rd.hh mgetline.hh prog_bug.hh
filt_string.o: filt_string.cc filt_string.hh fseq.hh
findpath.o: findpath.cc bust.hh distmat_rd.hh filt_string.hh fseq.hh \
 graphmisc.hh mgetline.hh pathprint.hh prog_bug.hh seq_index.hh
fseq.o: fseq.cc regex_prob.hh fseq.hh mgetline.hh
fseq_prop.o: fseq_prop.cc fseq_prop.hh fseq.hh
getline.o: getline.cc
mgetline.o: mgetline.cc mgetline.hh prog_bug.hh
pathprint.o: pathprint.cc bust.hh distmat_rd.hh filt_string.hh fseq.hh \
 graphmisc.hh pathprint.hh seq_index.hh
prog_bug.o: prog_bug.cc prog_bug.hh
reduce.o: reduce.cc bust.hh distmat_rd.hh fseq.hh fseq_prop.hh \
 mgetline.hh t_queue.hh t_queue.tcc
seq_index.o: seq_index.cc bust.hh filt_string.hh fseq.hh mgetline.hh \
 seq_index.hh
sym_mat.o: sym_mat.cc sym_mat.hh
tqtest.o: tqtest.cc t_queue.hh t_queue.tcc delay.hh
bust2.o: bust2.hh
bust.o: bust.hh
delay.o: delay.hh
distmat_rd.o: distmat_rd.hh
filt_string.o: filt_string.hh
fseq.o: fseq.hh
fseq_prop.o: fseq_prop.hh
graphmisc.o: graphmisc.hh
mgetline.o: mgetline.hh
pathprint.o: pathprint.hh
prog_bug.o: prog_bug.hh
regex_prob.o: regex_prob.hh
seq_index.o: seq_index.hh
sym_mat.o: sym_mat.hh
t_queue.o: t_queue.hh t_queue.tcc t_queue.hh
