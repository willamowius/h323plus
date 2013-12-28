/*
 * x224.h
 *
 * X.224 protocol handler
 *
 * Open H323 Library
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
 *
 * $Id$
 *
 */

#ifndef __OPAL_X224_H
#define __OPAL_X224_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <ptlib/sockets.h>



///////////////////////////////////////////////////////////////////////////////

/**This class embodies X.224 Class Zero Protocol Data Unit.
  */
class X224 : public PObject
{
  PCLASSINFO(X224, PObject)

  public:
    enum Codes {
      ConnectRequest = 0xe0,
      ConnectConfirm = 0xd0,
      DataPDU = 0xf0
    };

    X224();

    void BuildConnectRequest();
    void BuildConnectConfirm();
    void BuildData(const PBYTEArray & data);

    void PrintOn(ostream & strm) const;
    PBoolean Decode(const PBYTEArray & rawData);
    PBoolean Encode(PBYTEArray & rawData) const;

    int GetCode() const { return header[0]; }
    const PBYTEArray & GetData() const { return data; }

  private:
    PBYTEArray header;
    PBYTEArray data;
};


#endif // __OPAL_X224_H


/////////////////////////////////////////////////////////////////////////////
