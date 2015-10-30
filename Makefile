# 8 oct 2015
# my notes:
#  * -fsanitize=address from clang seems to break valgrind
.POSIX:

EXE_NAME=clean_msa
OBJS = clean_msa.o distmat_rd.o fseq.o mgetline.o

# During debugging, one might want to put delays in and add delay.o to the list

#CXX=/usr/local/zbhtools/gcc/gcc-5.1.0/bin/g++
CXX=g++
CXXFLAGS=-std=c++11 -g -O3 -pg -pedantic -lpthread #-fsanitize=address #-fsanitize=thread -pie -fPIC -fPIE

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
	(cd /work/torda/people/inari/2jkf_oct15/merge;\
	valgrind --leak-check=full  \
        /home/torda/c/seq_work/clean_msa -a sacred -c first \
            merge_align.fa merge.fa.hat2 reduced_100.fa 100)

helgrind:
	make $(EXE_NAME)
	(cd /work/torda/people/inari/2jkf_oct15/merge;\
	valgrind --tool=helgrind \
           /home/torda/c/seq_work/clean_msa -a sacred -c first \
            merge_align.fa merge.fa.hat2 reduced_100.fa 100)
#	valgrind --tool=helgrind $(EXE_NAME) example/big.fa x

# DO NOT DELETE

clean_msa.o: distmat_rd.hh fseq.hh mgetline.hh t_queue.hh
delay.o: delay.hh
distmat_rd.o: distmat_rd.hh mgetline.hh
fseq.o: fseq.hh mgetline.hh
