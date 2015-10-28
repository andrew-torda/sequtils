# 8 oct 2015
# my notes:
#  * -fsanitize=address from clang seems to break valgrind
.POSIX:

EXE_NAME=clean_msa
OBJS = clean_msa.o delay.o distmat_rd.o fseq.o

#CXX=/usr/local/zbhtools/gcc/gcc-5.1.0/bin/g++
CXX=g++
CXXFLAGS=-std=c++11 -g -pedantic -lpthread #-fsanitize=thread -pie -fPIC -fPIE

#CXX=clang++
#CXXFLAGS=-g -Weverything -pedantic -std=c++11 -lpthread 

CFLAGS=$(CXXFLAGS)
$(EXE_NAME): $(OBJS)
	$(CXX) -o $(EXE_NAME) $(CFLAGS) $(OBJS)


clean:
	rm -f $(OBJS) $(EXE_NAME) *~ core TAGS *.bak

depend:
	makedepend -Y *.cc *.hh

tags:
	etags *.cc *.hh

valgrind:
	make $(EXE_NAME)
	valgrind --leak-check=full  $(EXE_NAME) -v example/test.fa example/shorter.fa.hat2 x 3

helgrind:
	make $(EXE_NAME)
	valgrind --tool=helgrind $(EXE_NAME) example/big.fa x

# DO NOT DELETE

clean_msa.o: fseq.hh t_queue.hh distmat_rd.hh
delay.o: delay.hh
distmat_rd.o: distmat_rd.hh
fseq.o: fseq.hh
