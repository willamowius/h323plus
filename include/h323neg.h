/*
 * h323neg.h
 *
 * H.323 protocol handler
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
 * Portions of this code were written with the assisance of funding from
 * Vovida Networks, Inc. http://www.vovida.com.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef __OPAL_H323NEG_H
#define __OPAL_H323NEG_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include "h323pdu.h"
#include "channels.h"



///////////////////////////////////////////////////////////////////////////////

/**Base class for doing H245 negotiations
 */
class H245Negotiator : public PObject
{
  PCLASSINFO(H245Negotiator, PObject);

  public:
    H245Negotiator(H323EndPoint & endpoint, H323Connection & connection);

  protected:
    PDECLARE_NOTIFIER(PTimer, H245Negotiator, HandleTimeout);

    H323EndPoint   & endpoint;
    H323Connection & connection;
    PTimer           replyTimer;
    PMutex           mutex;
};


/**Determine the master and slave on a H245 connection as per H245 section 8.2
 */
class H245NegMasterSlaveDetermination : public H245Negotiator
{
  PCLASSINFO(H245NegMasterSlaveDetermination, H245Negotiator);

  public:
    H245NegMasterSlaveDetermination(H323EndPoint & endpoint, H323Connection & connection);

    PBoolean Start(PBoolean renegotiate);
    void Stop();
    PBoolean HandleIncoming(const H245_MasterSlaveDetermination & pdu);
    PBoolean HandleAck(const H245_MasterSlaveDeterminationAck & pdu);
    PBoolean HandleReject(const H245_MasterSlaveDeterminationReject & pdu);
    PBoolean HandleRelease(const H245_MasterSlaveDeterminationRelease & pdu);
    void HandleTimeout(PTimer &, INT);

    PBoolean IsMaster() const     { return status == e_DeterminedMaster; }
    PBoolean IsDetermined() const { return state == e_Idle && status != e_Indeterminate; }

    void SetTryToBecomeSlave(bool val = true) { tryToBecomSlave = val; }

  protected:
    PBoolean Restart();

    enum States {
      e_Idle, e_Outgoing, e_Incoming,
      e_NumStates
    } state;
#if PTRACING
    static const char * const StateNames[e_NumStates];
    friend ostream & operator<<(ostream & o, States s) { return o << StateNames[s]; }
#endif

    DWORD    determinationNumber;
    unsigned retryCount;
    bool     tryToBecomSlave;

    enum MasterSlaveStatus {
      e_Indeterminate, e_DeterminedMaster, e_DeterminedSlave,
      e_NumStatuses
    } status;
#if PTRACING
    static const char * const StatusNames[e_NumStatuses];
    friend ostream & operator<<(ostream & o , MasterSlaveStatus s) { return o << StatusNames[s]; }
#endif
};


/**Exchange capabilities on a H245 connection as per H245 section 8.3
 */
class H245NegTerminalCapabilitySet : public H245Negotiator
{
  PCLASSINFO(H245NegTerminalCapabilitySet, H245Negotiator);

  public:
    H245NegTerminalCapabilitySet(H323EndPoint & endpoint, H323Connection & connection);

    PBoolean Start(PBoolean renegotiate, PBoolean empty = FALSE);
    void Stop();
    PBoolean HandleIncoming(const H245_TerminalCapabilitySet & pdu);
    PBoolean HandleAck(const H245_TerminalCapabilitySetAck & pdu);
    PBoolean HandleReject(const H245_TerminalCapabilitySetReject & pdu);
    PBoolean HandleRelease(const H245_TerminalCapabilitySetRelease & pdu);
    void HandleTimeout(PTimer &, INT);

    PBoolean HasSentCapabilities() const { return state == e_Sent; }
    PBoolean HasReceivedCapabilities() const { return receivedCapabilites; }

  protected:
    enum States {
      e_Idle, e_InProgress, e_Sent,
      e_NumStates
    } state;
#if PTRACING
    static const char * const StateNames[e_NumStates];
    friend ostream & operator<<(ostream & o, States s) { return o << StateNames[s]; }
#endif

    unsigned inSequenceNumber;
    unsigned outSequenceNumber;

    PBoolean receivedCapabilites;
};


/**Logical Channel signalling on a H245 connection as per H245 section 8.4
 */
class H245NegLogicalChannel : public H245Negotiator
{
  PCLASSINFO(H245NegLogicalChannel, H245Negotiator);

  public:
    H245NegLogicalChannel(H323EndPoint & endpoint,
                          H323Connection & connection,
                          const H323ChannelNumber & channelNumber);
    H245NegLogicalChannel(H323EndPoint & endpoint,
                          H323Connection & connection,
                          H323Channel & channel);
    ~H245NegLogicalChannel();

    virtual PBoolean Open(
      const H323Capability & capability,
      unsigned sessionID,
      unsigned replacementFor = 0,
	  unsigned roleLabel = 0
    );
    virtual PBoolean Close();
    virtual PBoolean HandleOpen(const H245_OpenLogicalChannel & pdu);
    virtual PBoolean HandleOpenAck(const H245_OpenLogicalChannelAck & pdu);
    virtual PBoolean HandleOpenConfirm(const H245_OpenLogicalChannelConfirm & pdu);
    virtual PBoolean HandleReject(const H245_OpenLogicalChannelReject & pdu);
    virtual PBoolean HandleClose(const H245_CloseLogicalChannel & pdu);
    virtual PBoolean HandleCloseAck(const H245_CloseLogicalChannelAck & pdu);
    virtual PBoolean HandleRequestClose(const H245_RequestChannelClose & pdu);
    virtual PBoolean HandleRequestCloseAck(const H245_RequestChannelCloseAck & pdu);
    virtual PBoolean HandleRequestCloseReject(const H245_RequestChannelCloseReject & pdu);
    virtual PBoolean HandleRequestCloseRelease(const H245_RequestChannelCloseRelease & pdu);
    virtual void HandleTimeout(PTimer &, INT);

    H323Channel * GetChannel();


  protected:
    virtual PBoolean OpenWhileLocked(
      const H323Capability & capability,
      unsigned sessionID,
      unsigned replacementFor = 0,
	  unsigned roleLabel = 0
    );
    virtual PBoolean CloseWhileLocked();
    virtual void Release();


    H323Channel * channel;

    H323ChannelNumber channelNumber;

    enum States {
      e_Released,
      e_AwaitingEstablishment,
      e_Established,
      e_AwaitingRelease,
      e_AwaitingConfirmation,
      e_AwaitingResponse,
      e_NumStates
    } state;
#if PTRACING
    static const char * const StateNames[e_NumStates];
    friend ostream & operator<<(ostream & o, States s) { return o << StateNames[s]; }
#endif


  friend class H245NegLogicalChannels;
};


H323DICTIONARY(H245LogicalChannelDict, H323ChannelNumber, H245NegLogicalChannel);

/**Dictionary of all Logical Channels
 */
class H245NegLogicalChannels : public H245Negotiator
{
  PCLASSINFO(H245NegLogicalChannels, H245Negotiator);

  public:
    H245NegLogicalChannels(H323EndPoint & endpoint, H323Connection & connection);

    virtual void Add(H323Channel & channel);

    virtual PBoolean Open(
      const H323Capability & capability,
      unsigned sessionID,
      unsigned replacementFor = 0
    );

    virtual PBoolean Open(
      const H323Capability & capability,
      unsigned sessionID,
	  H323ChannelNumber & channelnumber,
      unsigned replacementFor = 0,
	  unsigned roleLabel = 0
    );

    virtual PBoolean Close(unsigned channelNumber, PBoolean fromRemote);
    virtual PBoolean HandleOpen(const H245_OpenLogicalChannel & pdu);
    virtual PBoolean HandleOpenAck(const H245_OpenLogicalChannelAck & pdu);
    virtual PBoolean HandleOpenConfirm(const H245_OpenLogicalChannelConfirm & pdu);
    virtual PBoolean HandleReject(const H245_OpenLogicalChannelReject & pdu);
    virtual PBoolean HandleClose(const H245_CloseLogicalChannel & pdu);
    virtual PBoolean HandleCloseAck(const H245_CloseLogicalChannelAck & pdu);
    virtual PBoolean HandleRequestClose(const H245_RequestChannelClose & pdu);
    virtual PBoolean HandleRequestCloseAck(const H245_RequestChannelCloseAck & pdu);
    virtual PBoolean HandleRequestCloseReject(const H245_RequestChannelCloseReject & pdu);
    virtual PBoolean HandleRequestCloseRelease(const H245_RequestChannelCloseRelease & pdu);

    H323ChannelNumber GetNextChannelNumber();
    H323ChannelNumber GetLastChannelNumber();
    PINDEX GetSize() const { return channels.GetSize(); }
    H323Channel * GetChannelAt(PINDEX i);
    H323Channel * FindChannel(unsigned channelNumber, PBoolean fromRemote);
    H245NegLogicalChannel & GetNegLogicalChannelAt(PINDEX i);
    H245NegLogicalChannel * FindNegLogicalChannel(unsigned channelNumber, PBoolean fromRemote);
    H323Channel * FindChannelBySession(unsigned rtpSessionId, PBoolean fromRemote);
    void RemoveAll();

  protected:
    H323ChannelNumber      lastChannelNumber;
    H245LogicalChannelDict channels;
};


/**Request mode change as per H245 section 8.9
 */
class H245NegRequestMode : public H245Negotiator
{
  PCLASSINFO(H245NegRequestMode, H245Negotiator);

  public:
    H245NegRequestMode(H323EndPoint & endpoint, H323Connection & connection);

    virtual PBoolean StartRequest(const PString & newModes);
    virtual PBoolean StartRequest(const H245_ArrayOf_ModeDescription & newModes);
    virtual PBoolean HandleRequest(const H245_RequestMode & pdu);
    virtual PBoolean HandleAck(const H245_RequestModeAck & pdu);
    virtual PBoolean HandleReject(const H245_RequestModeReject & pdu);
    virtual PBoolean HandleRelease(const H245_RequestModeRelease & pdu);
    virtual void HandleTimeout(PTimer &, INT);

  protected:
    PBoolean awaitingResponse;
    unsigned inSequenceNumber;
    unsigned outSequenceNumber;
};


/**Request mode change as per H245 section 8.9
 */
class H245NegRoundTripDelay : public H245Negotiator
{
  PCLASSINFO(H245NegRoundTripDelay, H245Negotiator);

  public:
    H245NegRoundTripDelay(H323EndPoint & endpoint, H323Connection & connection);

    PBoolean StartRequest();
    PBoolean HandleRequest(const H245_RoundTripDelayRequest & pdu);
    PBoolean HandleResponse(const H245_RoundTripDelayResponse & pdu);
    void HandleTimeout(PTimer &, INT);

    PTimeInterval GetRoundTripDelay() const { return roundTripTime; }
    PBoolean IsRemoteOffline() const { return retryCount == 0; }

  protected:
    PBoolean          awaitingResponse;
    unsigned      sequenceNumber;
    PTimeInterval tripStartTime;
    PTimeInterval roundTripTime;
    unsigned      retryCount;
};


#endif // __OPAL_H323NEG_H


/////////////////////////////////////////////////////////////////////////////
