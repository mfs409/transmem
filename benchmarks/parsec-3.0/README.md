transmem::benchmarks::parsec-3.0
=====

This folder provides a transactional version of the PARSEC 3.0 benchmark
suite.

Read This First
-----

PARSEC is a very large benchmark suite.  In particular, the tools, libs, and
inputs folders of a standard PARSEC download consume several gigabytes of
space.  These files and folders are not relevant to the task of
transactionalizing PARSEC, but are necessary for building and running PARSEC.

We do not include the aforementioned parts of PARSEC in this repository.  To
fetch them, use the fetch_nontm_parts.sh script.  It will pull the relevant
files from the PARSEC website, extract the key components, and place them
where they belong.

Building
-----

There are a total of 16 PARSEC benchmarks.  They are in three categories:

"Apps":
  * blackscholes
  * bodytrack
  * facesim
  * ferret
  * fluidanimate
  * freqmine
  * raytrace
  * swaptions
  * vips
  * x264

"Kernels":
  * canneal
  * dedup
  * streamcluster

"Net Apps":
  * netdedup
  * netferret
  * netstreamcluster

For our purposes, the two interesting build configurations are 'gcc-pthreads'
and 'gcc-tm'.

To build any of these benchmarks, use a command of the following form:

  bin/parsecmgmt -a build -p <<benchmark-name>> -c <<build-configuration>>

For example:

  bin/parsecmgmt -a build -p bodytrack -c gcc-pthreads

Note: to clean, you'll probably want to use the 'fullclean' and
'fulluninstall' actions.
