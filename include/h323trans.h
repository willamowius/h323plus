/*
 * h323trans.h
 *
 * H.323 Transactor handler
 *
 * Open H323 Library
 *
 * Copyright (c) 2003 Equivalence Pty. Ltd.
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

#ifndef __OPAL_H323TRANS_H
#define __OPAL_H323TRANS_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include "transports.h"
#include "h235auth.h"

#include <ptclib/asner.h>


class H323TransactionPDU {
  public:
    H323TransactionPDU();
    H323TransactionPDU(const H235Authenticators & auth);

    virtual ~H323TransactionPDU() { }

    virtual PBoolean Read(H323Transport & transport);
    virtual PBoolean Write(H323Transport & transport);

    virtual PASN_Object & GetPDU() = 0;
    virtual PASN_Choice & GetChoice() = 0;
    virtual const PASN_Object & GetPDU() const = 0;
    virtual const PASN_Choice & GetChoice() const = 0;
    virtual unsigned GetSequenceNumber() const = 0;
    virtual unsigned GetRequestInProgressDelay() const = 0;
#if PTRACING
    virtual const char * GetProtocolName() const = 0;
#endif
    virtual H323TransactionPDU * ClonePDU() const = 0;
    virtual void DeletePDU() = 0;

    const H235Authenticators & GetAuthenticators() const { return authenticators; }
    void SetAuthenticators(
      const H235Authenticators & auth
    ) { authenticators = auth; }

    H235Authenticator::ValidationResult Validate(
      const PASN_Array & clearTokens,
      unsigned clearOptionalField,
      const PASN_Array & cryptoTokens,
      unsigned cryptoOptionalField
    ) const { return authenticators.ValidatePDU(*this, clearTokens, clearOptionalField, cryptoTokens, cryptoOptionalField, rawPDU); }

    void Prepare(
      PASN_Array & clearTokens,
      unsigned clearOptionalField,
      PASN_Array & cryptoTokens,
      unsigned cryptoOptionalField
    ) { authenticators.PreparePDU(*this, clearTokens, clearOptionalField, cryptoTokens, cryptoOptionalField); }

  protected:
    H235Authenticators authenticators;
    PPER_Stream        rawPDU;
};


///////////////////////////////////////////////////////////

class H323Transactor : public PObject
{
  PCLASSINFO(H323Transactor, PObject);
  public:
  /**@name Construction */
  //@{

    /**Create a new protocol handler.
     */
    H323Transactor(
      H323EndPoint & endpoint,   ///<  Endpoint gatekeeper is associated with.
      H323Transport * transport, ///<  Transport over which to communicate.
      WORD localPort,                     ///<  Local port to listen on
      WORD remotePort                     ///<  Remote port to connect on
    );
    H323Transactor(
      H323EndPoint & endpoint,   ///<  Endpoint gatekeeper is associated with.
      const H323TransportAddress & iface, ///<  Local interface over which to communicate.
      WORD localPort,                     ///<  Local port to listen on
      WORD remotePort                     ///<  Remote port to connect on
    );

    /**Destroy protocol handler.
     */
    ~H323Transactor();
  //@}

  /**@name Overrides from PObject */
  //@{
    /**Print the name of the gatekeeper.
      */
    void PrintOn(
      ostream & strm    ///<  Stream to print to.
    ) const;
  //@}

  /**@name new operations */
  //@{
    /**Set a new transport for use by the transactor.
      */
    PBoolean SetTransport(
      const H323TransportAddress & iface ///<  Local interface for transport
    );

    /**Return the list of addresses used for this peer element
      */
    H323TransportAddressArray GetInterfaceAddresses(
      PBoolean excludeLocalHost = TRUE,       ///<  Flag to exclude 127.0.0.1
      H323Transport * associatedTransport = NULL
                          ///<  Associated transport for precedence and translation
    );

    /**Start the channel processing transactions
      */
    virtual PBoolean StartChannel();

    /**Stop the channel processing transactions.
       Must be called in each descendants destructor.
      */
    virtual void StopChannel();

    /**Create the transaction PDU for reading.
      */
    virtual H323TransactionPDU * CreateTransactionPDU() const = 0;

    /**Handle and dispatch a transaction PDU
      */
    virtual PBoolean HandleTransaction(
      const PASN_Object & rawPDU
    ) = 0;

    /**Allow for modifications to PDU on send.
      */
    virtual void OnSendingPDU(
      PASN_Object & rawPDU
    ) = 0;

    /**Write PDU to transport after executing callback.
      */
    virtual PBoolean WritePDU(
      H323TransactionPDU & pdu
    );

    /**Write PDU to transport after executing callback.
      */
    virtual PBoolean WriteTo(
      H323TransactionPDU & pdu,
      const H323TransportAddressArray & addresses,
      PBoolean callback = TRUE
    );
  //@}

  /**@name Member variable access */
  //@{
    /**Get the gatekeepers associated endpoint.
      */
    H323EndPoint & GetEndPoint() const { return endpoint; }

    /**Get the gatekeepers transport channel.
      */
    H323Transport & GetTransport() const { return *transport; }

    /**Set flag to check all crypto tokens on responses.
      */
    void SetCheckResponseCryptoTokens(
      PBoolean value    ///<  New value for checking crypto tokens.
    ) { checkResponseCryptoTokens = value; }

    /**Get flag to check all crypto tokens on responses.
      */
    PBoolean GetCheckResponseCryptoTokens() { return checkResponseCryptoTokens; }
  //@}
	
    class Request : public PObject
    {
        PCLASSINFO(Request, PObject);
      public:
        Request(
          unsigned seqNum,
          H323TransactionPDU & pdu
        );
        Request(
          unsigned seqNum,
          H323TransactionPDU & pdu,
          const H323TransportAddressArray & addresses
        );

        PBoolean Poll(H323Transactor &);
        void CheckResponse(unsigned, const PASN_Choice *);
        void OnReceiveRIP(unsigned milliseconds);

        void SetUseAlternate(PBoolean isAlternate);

        // Inter-thread transfer variables
        unsigned rejectReason;
        void   * responseInfo;

        H323TransportAddressArray requestAddresses;

        unsigned             sequenceNumber;
        H323TransactionPDU & requestPDU;
        PTimeInterval        whenResponseExpected;
        PSyncPoint           responseHandled;
        PMutex               responseMutex;

        enum {
          AwaitingResponse,
          ConfirmReceived,
          RejectReceived,
          TryAlternate,
          BadCryptoTokens,
          RequestInProgress,
          NoResponseReceived
        } responseResult;

        PBoolean    useAlternate;
    };

  protected:
    void Construct();

    unsigned GetNextSequenceNumber();
    PBoolean SetUpCallSignalAddresses(
      H225_ArrayOf_TransportAddress & addresses
    );

    //Background thread handler.
    PDECLARE_NOTIFIER(PThread, H323Transactor, HandleTransactions);

    virtual PBoolean MakeRequest(
      Request & request
    );
    PBoolean CheckForResponse(
      unsigned,
      unsigned,
      const PASN_Choice * = NULL
    );
    PBoolean HandleRequestInProgress(
      const H323TransactionPDU & pdu,
      unsigned delay
    );
    PBoolean CheckCryptoTokens(
      const H323TransactionPDU & pdu,
      const PASN_Array & clearTokens,
      unsigned clearOptionalField,
      const PASN_Array & cryptoTokens,
      unsigned cryptoOptionalField
    );

    void AgeResponses();
    PBoolean SendCachedResponse(
      const H323TransactionPDU & pdu
    );

    class Response : public PString
    {
        PCLASSINFO(Response, PString);
      public:
        Response(const H323TransportAddress & addr, unsigned seqNum);
        ~Response();

        void SetPDU(const H323TransactionPDU & pdu);
        PBoolean SendCachedResponse(H323Transport & transport);

        PTime                lastUsedTime;
        PTimeInterval        retirementAge;
        H323TransactionPDU * replyPDU;
    };

    // Configuration variables
    H323EndPoint  & endpoint;
    WORD            defaultLocalPort;
    WORD            defaultRemotePort;
    H323Transport * transport;
    PBoolean            checkResponseCryptoTokens;

    unsigned  nextSequenceNumber;
    PMutex    nextSequenceNumberMutex;

    H323Dictionary<POrdinalKey, Request> requests;
    PMutex                            requestsMutex;
    Request                         * lastRequest;

    PMutex                pduWriteMutex;
    PSortedList<Response> responses;
};


////////////////////////////////////////////////////////////////////////////////////

class H323Transaction : public PObject
{
    PCLASSINFO(H323Transaction, PObject);
  public:
  /**@name Construction */
  //@{
    /**Create a new transaction handler.
     */
    H323Transaction(
      H323Transactor & transactor,
      const H323TransactionPDU & requestToCopy,
      H323TransactionPDU * confirm,
      H323TransactionPDU * reject
    );
    ~H323Transaction();
  //@}

    enum Response {
      Ignore = -2,
      Reject = -1,
      Confirm = 0
    };
    inline static Response InProgress(unsigned time) { return (Response)(time&0xffff); }

    virtual H323TransactionPDU * CreateRIP(
      unsigned sequenceNumber,
      unsigned delay
    ) const = 0;

    PBoolean HandlePDU();

    virtual PBoolean WritePDU(
      H323TransactionPDU & pdu
    );

    PBoolean CheckCryptoTokens(
      const H235Authenticators & authenticators
    );

#if PTRACING
    virtual const char * GetName() const = 0;
#endif
    virtual H235Authenticator::ValidationResult ValidatePDU() const = 0;
    virtual void SetRejectReason(
      unsigned reasonCode
    ) = 0;

    PBoolean IsFastResponseRequired() const { return fastResponseRequired && canSendRIP; }
    PBoolean CanSendRIP() const { return canSendRIP; }
    H323TransportAddress GetReplyAddress() const { return replyAddresses[0]; }
    const H323TransportAddressArray & GetReplyAddresses() const { return replyAddresses; }
    PBoolean IsBehindNAT() const { return isBehindNAT; }
    H323Transactor & GetTransactor() const { return transactor; }
    H235Authenticator::ValidationResult GetAuthenticatorResult() const { return authenticatorResult; }

  protected:
    virtual Response OnHandlePDU() = 0;
    PDECLARE_NOTIFIER(PThread, H323Transaction, SlowHandler);

    H323Transactor         & transactor;
    H323TransportAddressArray replyAddresses;
    PBoolean                     fastResponseRequired;
    H323TransactionPDU     * request;
    H323TransactionPDU     * confirm;
    H323TransactionPDU     * reject;

    H235Authenticators                  authenticators;
    H235Authenticator::ValidationResult authenticatorResult;
    PBoolean                                isBehindNAT;
    PBoolean                                canSendRIP;
};


///////////////////////////////////////////////////////////

class H323TransactionServer : public PObject
{
  PCLASSINFO(H323TransactionServer, PObject);
  public:
  /**@name Construction */
  //@{
    /**Create a new gatekeeper.
     */
    H323TransactionServer(
      H323EndPoint & endpoint
    );

    /**Destroy gatekeeper.
     */
    ~H323TransactionServer();
  //@}

    virtual WORD GetDefaultUdpPort() = 0;

  /**@name Access functions */
  //@{
    /**Get the owner endpoint.
     */
    H323EndPoint & GetOwnerEndPoint() const { return ownerEndPoint; }

  /**@name Protocol Handler Operations */
  //@{
    /**Add listeners to the transaction server.
       If a listener already exists on the interface specified in the list
       then it is ignored. If a listener does not yet exist a new one is
       created and if a listener is running that is not in the list then it
       is stopped and removed.

       If the array is empty then the string "*" is assumed which will listen
       on the standard UDP port on INADDR_ANY.

       Returns TRUE if at least one interface was successfully started.
      */
    PBoolean AddListeners(
      const H323TransportAddressArray & ifaces ///<  Interfaces to listen on.
    );

    /**Add a gatekeeper listener to this gatekeeper server given the
       transport address for the local interface.
      */
    PBoolean AddListener(
      const H323TransportAddress & interfaceName
    );

    /**Add a gatekeeper listener to this gatekeeper server given the transport.
       Note that the transport is then owned by the listener and will be
       deleted automatically when the listener is destroyed. Note also the
       transport is deleted if this function returns FALSE and no listener was
       created.
      */
    PBoolean AddListener(
      H323Transport * transport
    );

    /**Add a gatekeeper listener to this gatekeeper server.
       Note that the gatekeeper listener is then owned by the gatekeeper
       server and will be deleted automatically when the listener is removed.
       Note also the listener is deleted if this function returns FALSE and
       the listener was not used.
      */
    PBoolean AddListener(
      H323Transactor * listener
    );

    /**Create a new H323GatkeeperListener.
       The user woiuld not usually use this function as it is used internally
       by the server when new listeners are added by H323TransportAddress.

       However, a user may override this function to create objects that are
       user defined descendants of H323GatekeeperListener so the user can
       maintain extra information on a interface by interface basis.
      */
    virtual H323Transactor * CreateListener(
      H323Transport * transport  ///<  Transport for listener
    ) = 0;

    /**Remove a gatekeeper listener from this gatekeeper server.
       The gatekeeper listener is automatically deleted.
      */
    PBoolean RemoveListener(
      H323Transactor * listener
    );

    PBoolean SetUpCallSignalAddresses(H225_ArrayOf_TransportAddress & addresses);
  //@}

  protected:
    H323EndPoint & ownerEndPoint;

    PThread      * monitorThread;
    PSyncPoint     monitorExit;

    PMutex         mutex;
    H323LIST(ListenerList, H323Transactor);
    ListenerList listeners;
    PBoolean usingAllInterfaces;
};


#endif // __OPAL_H323TRANS_H


/////////////////////////////////////////////////////////////////////////////
