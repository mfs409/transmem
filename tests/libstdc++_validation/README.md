transmem::tests::libstdc++_validation
=====

This folder holds single-threaded codes that we use to ensure that all
methods of our STL containers can be called safely from a transactional
context.

Status
-----

The list/ container currently builds and passes all tests.  Other containers
remain to be ported.

Open Issues
-----

There is a bug in GCC right now, where symbols for instantiations of
templates from transactional contexts can lead to multiply-defined symbols.
See common/common.mk for more information
