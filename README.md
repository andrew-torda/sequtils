# sequtils
Utilities for biological sequences

Download, cd into the directory that you are in and type

 make

Then have a look at all the files that end in .1 These are man pages.
Try

 man reduce.1

#Required

* Compiler

Everything compiles for me with the pedantic option of gcc and clang.
It has only been tried under linux, but it makes no assumptions about the machine it is on.
It will not work under ancient gcc (<4.8) since it expects the regex library.

* Other programs

mafft
-----
We use mafft for sequence alignments. It can be persuaded to print out its matrix of similarities between sequences. This matrix is essential for our program, reduce.
If you want to use it, you have to install mafft.

#Programs
--------
There are a few, but for the moment, we concentrate on reduce, since it is references in some papers.

Reduce
------
Read the man page for a full explanation. The idea is to get the most evenly possible distributed set of sequences.