/*
 * precompile.cxx
 *
 * Precompiled header generation file.
 *
 * Copyright (c) 1998-2000 Equivalence Pty. Ltd.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 */

#define P_DISABLE_FACTORY_INSTANCES

#include <ptlib.h>

#ifndef PTLIB_VERSION_CHECK
   #define PTLIB_VERSION_CHECK 1
      #if PTLIB_MAJOR <= 2 && PTLIB_MINOR < 6
         #error "You require PTLib v2.6.x or above to compile this version of H323plus")
      #endif
#endif


// End of File ///////////////////////////////////////////////////////////////
