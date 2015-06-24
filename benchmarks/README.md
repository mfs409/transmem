transmem::benchmarks
=====

This folder contains benchmark codes.

Contents
-----

### ubench

Simple data structure microbenchmarks to assess low-level characteristics of
TM implementations.

### stamp_c

This is the version of STAMP from the Ruan et al. TRANSACT 2014 paper.  It
updates the original STAMP to use the C++ TM interface.  However, it does not
use C++ in any real sense... the programs are still C programs.

### stamp_c++

This is an intermediate verison of STAMP in which we have moved from C to
C++.  This entailed, for example, using std::mt19937 instead of the STAMP
mt19937 implementation.

### stamp_c++_stl

This is a version of STAMP in which all ad-hoc containers are replaced with
their C++ STL equivalents.  This corresponds to the Kilgore et al. TRANSACT
2015 paper.

### parsec-3.0

This is a version of PARSEC in which the 8 benchmarks that use condition
variables have been ported to use TM and transaction-friendly condition
variables.
