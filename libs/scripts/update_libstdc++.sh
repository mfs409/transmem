#
# Set the path to a gcc source tree checkout, build folder, and install
# folder.  We assume that if you are maintaining the project, you've got all
# of these
#
# This is the only part of the file that should ever be modified
#
GCC_SRC=/scratch/spear/gcc_latest/
GCC_BLD=/scratch/spear/gcc_latest_build/
GCC_INST=/scratch/gcc_latest/

#
# Figure out where this script is
#
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

#
# Wipe out the stuff in this folder
#
rm -rf include/ libgcc/ libiberty/ libstdc++-v3/ Makefile

#
# Make the directories we require
#
mkdir include
mkdir libgcc
mkdir libiberty
mkdir libstdc++-v3

#
# Fetch the files for the supporting directories (include, libgcc, libiberty)
#

files="ansidecl.h demangle.h libiberty.h"
for file in $files
do
    cp $GCC_SRC/include/$file include/
done

files="unwind-pe.h"
for file in $files
do
    cp $GCC_SRC/libgcc/$file libgcc
done

files="cp-demangle.c cp-demangle.h"
for file in $files
do
    cp $GCC_SRC/libiberty/$file libiberty
done

#
# Fetch the files for libstdc++-v3.  Don't forget to remove the Makefile
# stuff.
#
mkdir libstdc++-v3/libsupc++
cp $GCC_SRC/libstdc++-v3/libsupc++/* libstdc++-v3/libsupc++/
rm libstdc++-v3/libsupc++/Makefile.in libstdc++-v3/libsupc++/Makefile.am

mkdir libstdc++-v3/src
folders="c++11 c++98 filesystem shared"
for folder in $folders
do
    mkdir libstdc++-v3/src/$folder
    cp $GCC_SRC/libstdc++-v3/src/$folder/* libstdc++-v3/src/$folder/
done
folders="c++11 c++98 filesystem"
for folder in $folders
do
    rm libstdc++-v3/src/$folder/Makefile.in libstdc++-v3/src/$folder/Makefile.am
done

mkdir libstdc++-v3/include
cp -R $GCC_INST/include/c++/6.0.0/* libstdc++-v3/include

# these are links in the original build
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/collate_members.cc libstdc++-v3/src/c++98/collate_members.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/collate_members.cc  libstdc++-v3/src/c++98/collate_members_cow.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/messages_members.cc libstdc++-v3/src/c++98/messages_members.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/messages_members.cc libstdc++-v3/src/c++98/messages_members_cow.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/monetary_members.cc libstdc++-v3/src/c++98/monetary_members.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/monetary_members.cc libstdc++-v3/src/c++98/monetary_members_cow.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/numeric_members.cc libstdc++-v3/src/c++98/numeric_members.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/numeric_members.cc libstdc++-v3/src/c++98/numeric_members_cow.cc
cp $GCC_SRC/libstdc++-v3/config/cpu/generic/atomicity_builtins/atomicity.h libstdc++-v3/src/c++98/atomicity.cc
cp $GCC_SRC/libstdc++-v3/config/io/basic_file_stdio.cc libstdc++-v3/src/c++98/basic_file.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/c_locale.cc libstdc++-v3/src/c++98/c++locale.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/codecvt_members.cc libstdc++-v3/src/c++98/codecvt_members.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/time_members.cc libstdc++-v3/src/c++98/time_members.cc
cp $GCC_SRC/libstdc++-v3/config/os/gnu-linux/ctype_configure_char.cc libstdc++-v3/src/c++11/ctype_configure_char.cc
cp $GCC_SRC/libstdc++-v3/config/locale/gnu/ctype_members.cc libstdc++-v3/src/c++11/ctype_members.cc

#
# one-off files
#
cp $GCC_BLD/x86_64-unknown-linux-gnu/libstdc++-v3/include/gstdint.h libstdc++-v3/include

#
# Copy the template for building the .so files
#
cp $GCC_SRC/libstdc++-v3/config/abi/pre/gnu.ver $GCC_SRC/libstdc++-v3/config/abi/pre/float128.ver  libstdc++-v3

#
# Set up the config.h stuff in the libstdc++-v3 folder
#
cp $GCC_BLD/x86_64-unknown-linux-gnu/libstdc++-v3/config.h libstdc++-v3/config_64.h
cp $GCC_BLD/x86_64-unknown-linux-gnu/32/libstdc++-v3/config.h libstdc++-v3/config_32.h
cat > libstdc++-v3/config.h <<'EOF'
#ifdef __x86_64__
#include "config_64.h"
#else
#include "config_32.h"
#endif
EOF

#
# Pull in the platform-specific bits/ stuff, but be careful about c++config.h
#
cp $GCC_BLD/x86_64-unknown-linux-gnu/libstdc++-v3/include/x86_64-unknown-linux-gnu/bits/*.h libstdc++-v3/include/x86_64-unknown-linux-gnu/bits
cp $GCC_BLD/x86_64-unknown-linux-gnu/libstdc++-v3/include/x86_64-unknown-linux-gnu/bits/c++config.h libstdc++-v3/include/x86_64-unknown-linux-gnu/bits/c++config_64.h
cp $GCC_BLD/x86_64-unknown-linux-gnu/32/libstdc++-v3/include/x86_64-unknown-linux-gnu/bits/c++config.h libstdc++-v3/include/x86_64-unknown-linux-gnu/bits/c++config_32.h
cat > libstdc++-v3/include/x86_64-unknown-linux-gnu/bits/c++config.h <<'EOF'
#ifdef __x86_64__
#include "c++config_64.h"
#else
#include "c++config_32.h"
#endif
EOF

#
# Avoid a name collision
#
mv libstdc++-v3/src/c++11/codecvt.cc libstdc++-v3/src/c++11/codecvt11.cc

#
# Set up the Makefiles... some we auto-generate, some we copy...
#
cat > Makefile <<'EOF'
all:
	@cd libstdc++-v3 && $(MAKE)

clean:
	@cd libstdc++-v3 && $(MAKE) clean
EOF

cat > libstdc++-v3/Makefile <<'EOF'
all:
	@cd libsupc++ && $(MAKE)
	@cd src && $(MAKE)

clean:
	@cd libsupc++ && $(MAKE) clean
	@cd src && $(MAKE) clean
EOF

cp $DIR/makefile.libsupc++ libstdc++-v3/libsupc++/Makefile

cp $DIR/makefile.libstdc++.src libstdc++-v3/src/Makefile


#
# The rest cannot be automated
#
cat <<EOF
Script complete

If there were no errors, you ought to have a complete libstdc++ folder in
which you can type 'make' to produce a *non-transactional* libstdc++.so.

If the gcc source tree has moved forward by much since the last time you did
this, it's likely that you'll have to update the Makefiles.  To do that,
you'll need to dissect the output of a successful build, see everything that
is built in the libstdc++ subfolder, and then update the Makefiles in
libstdc++-v3/libsupc++ and libstdc++-v3/src accordingly.

Also, note that this script does *not* update the libstdc++_tm folder
completely.  If you ran this script from that folder, here are the main steps
you'll need to take:

  - Restore the transaction_safe annotations in your .cc and .h files, and
    restore any other hacks/workarounds (e.g., transaction_pure,
    transaction_wrap, ad-hoc wrapped functions)

  - Restore the -fgnu-tm flags in the Makefiles.

  - update libstdc++-v3/gnu.ver to add all transaction clones to the .so.
    You can do this by appending this:
        STL_TM {
            _ZGT*;
        }
EOF
