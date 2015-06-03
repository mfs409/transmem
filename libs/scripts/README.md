tm_stl::scripts
======

This folder stores scripts and supporting files for managing differences
between transmem code and the code in repositories on which the transmem
repository depends.

Contents
=====

libstdc++ update script
-----

This script consists of three files:
  - update_libstdc++.sh
  - makefile.libstdc++.src
  - makefile.libsupc++

The purpose of this script is to copy all of the various bits and parts that
are needed to extract a full libstdc++ source tree from a gcc installation
and build.

The main script file is update_libstdc++.sh.  You should run this from
*inside* of the folder where you wish to have a pristine stl build tree.  For
example:

    mkdir libstdc++_pristine
    cd libstdc++_pristine
    sh ../scripts/update_libstdc++.sh

The script **requires** you to set three variables, which are listed at the
top of the file.  They provide paths to the source tree of your gcc checkout,
the folder in which you built gcc, and the folder where gcc is installed.

After you've run the script, you should be able to build libstdc++ by typing
'make'.  This will build 32-bit and 64-bit .so files, in the obj32 and obj64
subfolders of libstdc++-v3/src/.  You probably want to use the '-j' flag.

Note that the way in which we created the primary makefiles is gross: we ran
a full gcc build on our target machine (CentOS, x86_64, gcc 4.9.3,
c/c++/multilib targets enabled), we recorded all of the output of the entire
build process, and then we extracted all of the commands that relate to
libstdc++.  We also copied a variety of files from multiple locations, in
order to get the versions that are consistent with the build platform.

If you are looking to work on another platform, you'll need to manually
create suitable makefiles, and you'll need to change the scripts to copy
different files than the ones that we copied.
