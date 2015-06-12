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
