transmem::libs
=====

This folder holds libraries that are used (or could be used) by multiple
transactional applications.

Contents
-----

The libstdc++_tm/ folder stores the source code for libstdc++, with
annotations to make the STL containers transaction-safe.

The scripts/ folder stores scripts to assist in maintaining the libstdc++
folder.

Open Issues
-----

The libstdc++_tm makefiles do not track dependencies correctly.  For the time
being, you will need to re-build the entire tree after any change.
