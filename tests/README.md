transmem::tests
=====

This folder holds source code for tests that we use internally.

Contents
-----

The main content of this folder, for now, is the libstdc++_validation folder.
This folder holds tests to ensure that we have coverage over entire STL data
structures.  It works by instantiating every method of the data structure
from within a transactional context.

Note that as the STL changes, this code is very hard to maintain.  The
Makefiles refer to a "trace" folder, which we use internally to keep track of
new methods that are added to the STL.
