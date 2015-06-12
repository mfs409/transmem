/* Copyright (C) 2008-2015 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@redhat.com>.

   This file is part of the GNU Transactional Memory Library (libitm).

   Libitm is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   Libitm is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "libitm_i.h"

// [transmem] The entire retry functionality is not applicable when we only
//            have HTM and serial_irr.  The reason is that we're handling the
//            composition of those two modes via a spinlock, and hence there
//            is no need for mode switching.
//
//            Consequently, the only purpose of this file is to initialize a
//            dummy method group when the TM is first initialized.  It's not
//            even clear that we need a method group, but better safe than
//            sorry.

using namespace GTM;

// Gets notifications when the number of registered threads changes. This is
// used to initialize the method set choice and trigger straightforward choice
// adaption.
// This must be called only by serial threads.
void
GTM::gtm_thread::number_of_threads_changed(unsigned previous, unsigned now)
{
    // Flag so that we register once
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        GTM::abi_dispatch* disp = dispatch_htm();
        disp->get_method_group()->init();
    }
}
