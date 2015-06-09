transmem::apps
=====

This folder serves two roles.  First, for small applications, it is the
official home for the source releases of those applications.  Secondly, it is
the virtual home for applications that are too big to be added to this
repository.  So, for example, memcached's source code is housed here.  But
the source code for large applications should be checked out as subfolders of
this repository.

Contents
-----

The following applications are part of this directory tree:

### Memcached

There are two versions of memcached provided here.  Both are based on
memcached 1.4.15.  The first version corresponds to the Ruan et al. ASPLOS
2014 paper.  It presents a transactional memcached that uses an ad-hoc
technique for condition synchronization.  In short, we use onCommit handlers
and semaphores.

The second version makes use of transaction-friendly condition variables from
libtmcondvar, to overcome the semaphore hack.  This is one step closer to a
memcached that a "real" programmer might write.  The main remaining issue is
all of the wrapped functions.

Warning
-----

There appears to be a bug in the latest GCC, which prevents this code from
compiling.  GCC 4.9.3 works fine.
