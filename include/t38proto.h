/*
 * t38proto.h
 *
 * T.38 protocol handler
 *
 * Open Phone Abstraction Library
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
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
 * $ Id $
 *
 */

#ifndef __OPAL_T38PROTO_H
#define __OPAL_T38PROTO_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


class H323Transport;
class T38_IFPPacket;
class PASN_OctetString;

#include "ptlib_extras.h"

///////////////////////////////////////////////////////////////////////////////

/**This class handles the processing of the T.38 protocol.
  */
class OpalT38Protocol : public PObject
{
    PCLASSINFO(OpalT38Protocol, PObject);
  public:
  /**@name Construction */
  //@{
    /**Create a new protocol handler.
     */
    OpalT38Protocol();

    /**Destroy the protocol handler.
     */
    ~OpalT38Protocol();
  //@}

  /**@name Operations */
  //@{
    /**This is called to clean up any threads on connection termination.
     */
    virtual void CleanUpOnTermination();

    /**Handle the origination of a T.38 connection.
       An application would normally override this. The default just sends
       "heartbeat" T.30 no signal indicator.
      */
    virtual PBoolean Originate();

    /**Write packet to the T.38 connection.
      */
    virtual PBoolean WritePacket(
      const T38_IFPPacket & pdu
    );

    /**Write T.30 indicator packet to the T.38 connection.
      */
    virtual PBoolean WriteIndicator(
      unsigned indicator
    );

    /**Write data packet to the T.38 connection.
      */
    virtual PBoolean WriteMultipleData(
      unsigned mode,
      PINDEX count,
      unsigned * type,
      const PBYTEArray * data
    );

    /**Write data packet to the T.38 connection.
      */
    virtual PBoolean WriteData(
      unsigned mode,
      unsigned type,
      const PBYTEArray & data
    );

    /**Handle the origination of a T.38 connection.
      */
    virtual PBoolean Answer();

    /**Handle incoming T.38 packet.

       If returns FALSE, then the reading loop should be terminated.
      */
    virtual PBoolean HandlePacket(
      const T38_IFPPacket & pdu
    );

    /**Handle lost T.38 packets.

       If returns FALSE, then the reading loop should be terminated.
      */
    virtual PBoolean HandlePacketLost(
      unsigned nLost
    );

    /**Handle incoming T.38 indicator packet.
       If returns FALSE, then the reading loop should be terminated.
      */
    virtual PBoolean OnIndicator(
      unsigned indicator
    );

    /**Handle incoming T.38 CNG indicator.
       If returns FALSE, then the reading loop should be terminated.
      */
    virtual PBoolean OnCNG();

    /**Handle incoming T.38 CED indicator.
       If returns FALSE, then the reading loop should be terminated.
      */
    virtual PBoolean OnCED();

    /**Handle incoming T.38 V.21 preamble indicator.
       If returns FALSE, then the reading loop should be terminated.
      */
    virtual PBoolean OnPreamble();

    /**Handle incoming T.38 data mode training indicator.
       If returns FALSE, then the reading loop should be terminated.
      */
    virtual PBoolean OnTraining(
      unsigned indicator
    );

    /**Handle incoming T.38 data packet.

       If returns FALSE, then the reading loop should be terminated.
      */
    virtual PBoolean OnData(
      unsigned mode,
      unsigned type,
      const PBYTEArray & data
    );
  //@}

    H323Transport * GetTransport() const { return transport; }
    void SetTransport(
      H323Transport * transport,
      PBoolean autoDelete = TRUE
    );

  protected:
    PBoolean HandleRawIFP(
      const PASN_OctetString & pdu
    );

    H323Transport * transport;
    PBoolean            autoDeleteTransport;

    PBoolean     corrigendumASN;
    unsigned indicatorRedundancy;
    unsigned lowSpeedRedundancy;
    unsigned highSpeedRedundancy;

    int               lastSentSequenceNumber;
    H323List<PBYTEArray> redundantIFPs;
};


#endif // __OPAL_T38PROTO_H


/////////////////////////////////////////////////////////////////////////////
