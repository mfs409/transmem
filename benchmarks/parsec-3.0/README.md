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
