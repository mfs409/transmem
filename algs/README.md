transmem::algs
====

This folder holds versions of the gcc libitm source code.  All of these
implementations differ from the stock GCC libitm in that they can be built
without an entire GCC source checkout.  In addition, some of these
implementations add additional TM algorithms or extend the API.

Contents
-----

### libitm_x86_linux

This folder has a copy of the latest libitm from the gcc source tree.  The
only differences are that some files have been removed, since they are not
needed for x86/linux, and there is now a Makefile that produces 32-bit and
64-bit versions of the library without requiring a full gcc source tree /
build tree.

### libitm_tsx

This folder has a libitm implementation that only supports Intel TSX or
serial mode with no undo logs.  It can't handle transaction_cancel, but it is
streamlined.  It also has support for oncommit handlers, so that HTM can be
used with tmcondvars.

### libitm_eager

This folder has a libitm implementation that only supports three algorithms:
ml_wt (i.e., GCC's eager, privatization-safe version of TinySTM), serial (one
transaction at a time, self-abort possible), and serialirr (serial +
irrevocable: no self-abort).

The implementation does not have support for closed nesting, or any sort of
HTM support.  It does still use the GCC mechanism for mode switching (a
readers/writer lock).  This is a good baseline for implementing pure-software
TM algorithms, since it doesn't have a lot of the complexity of GCC's TM, but
it does present a fully general, and generally scalable, STM.

### libitm_lazy

This folder has a libitm implementation that is similar to libitm_eager,
except that it uses commit-time locking and redo logs (i.e., lazy TM).

### libitm_norec

An implementation of the lazy, livelock-free NOrec algorithm.  This is not
expected to scale on multi-chip machines, nor is it expected to do well with
frequent small writer transactions.  However, it is a very low-overhead STM
algorithm, and one that is useful for building hybrids.
