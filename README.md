# sequtils
Utilities for biological sequences

Download, cd into the directory that you are in and type

 make

Then have a look at all the files that end in .1 These are man pages.
Try `man reduce.1`

## Required

* Compiler

Everything compiles for me with the pedantic option of gcc and clang.
It has only been tried under linux, but it makes no assumptions about the machine it is on.
It will not work under ancient gcc (<4.8) since it expects the regex library.

* Other programs

mafft
-----
We use [mafft](https://mafft.cbrc.jp/alignment/software/) for sequence alignments. It can be persuaded to print out its matrix of similarities between sequences. This matrix is essential for our program, reduce.
If you want to use it, you have to install mafft.

## Programs

There are a few, but for the moment, we concentrate on reduce, since it is references in some papers.

### Reduce

Read the man page for a full explanation (`reduce.1`). The idea is to get the most evenly possible distributed set of sequences. If you have very closely related sequences, you only want to keep one of them. Otherwise, you have more than one representative over a very short evolutionary distances.

We read a distance matrix (currently only from mafft) and put all the distances in a sorted list. We then walk down the list and from each pair of sequences, remove one of them. We keep going until you have a list of length *n*, the number you want to keep.

Which member of each pair you remove is arbitrary and usually makes no difference. Sometimes, however, there are specific sequences you want to keep. Imagine you are writing a paper about protein X. Your alignment has a bunch of sequences very close to protein X, so some will be removed. All of your figures and your text refer to protein X, so whenever you have the choice, you would rather the other protein be removed. Just put protein X into the list of sacred sequences (`-a` option).