/*
 * peclient.h
 *
 * H.323 Annex G Peer Element client protocol handler
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
 * $Id$
 *
 */

#ifndef __OPAL_PECLIENT_H
#define __OPAL_PECLIENT_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include "h323annexg.h"
#include "h323ep.h"
#include "h501.h"

#include <ptlib/safecoll.h>


class H323PeerElement;


////////////////////////////////////////////////////////////////

class H501Transaction : public H323Transaction
{
    PCLASSINFO(H501Transaction, H323Transaction);
  public:
    H501Transaction(
      H323PeerElement & pe,
      const H501PDU & pdu,
      PBoolean hasReject
    );

    virtual H323TransactionPDU * CreateRIP(
      unsigned sequenceNumber,
      unsigned delay
    ) const;

    virtual H235Authenticator::ValidationResult ValidatePDU() const;

    H501_MessageCommonInfo & requestCommon;
    H501_MessageCommonInfo & confirmCommon;

  protected:
    H323PeerElement & peerElement;
};


////////////////////////////////////////////////////////////////

class H501ServiceRequest : public H501Transaction
{
    PCLASSINFO(H501ServiceRequest, H501Transaction);
  public:
    H501ServiceRequest(
      H323PeerElement & pe,
      const H501PDU & pdu
    );

#if PTRACING
    virtual const char * GetName() const;
#endif
    virtual void SetRejectReason(
      unsigned reasonCode
    );

    H501_ServiceRequest & srq;
    H501_ServiceConfirmation & scf;
    H501_ServiceRejection  & srj;

  protected:
    virtual Response OnHandlePDU();
};


////////////////////////////////////////////////////////////////

class H501DescriptorUpdate : public H501Transaction
{
    PCLASSINFO(H501DescriptorUpdate, H501Transaction);
  public:
    H501DescriptorUpdate(
      H323PeerElement & pe,
      const H501PDU & pdu
    );

#if PTRACING
    virtual const char * GetName() const;
#endif
    virtual void SetRejectReason(
      unsigned reasonCode
    );

    H501_DescriptorUpdate & du;
    H501_DescriptorUpdateAck & ack;

  protected:
    virtual Response OnHandlePDU();
};


////////////////////////////////////////////////////////////////

class H501AccessRequest : public H501Transaction
{
    PCLASSINFO(H501AccessRequest, H501Transaction);
  public:
    H501AccessRequest(
      H323PeerElement & pe,
      const H501PDU & pdu
    );

#if PTRACING
    virtual const char * GetName() const;
#endif
    virtual void SetRejectReason(
      unsigned reasonCode
    );

    H501_AccessRequest & arq;
    H501_AccessConfirmation & acf;
    H501_AccessRejection  & arj;

  protected:
    virtual Response OnHandlePDU();
};


////////////////////////////////////////////////////////////////

class H323PeerElementDescriptor : public PSafeObject
{
  PCLASSINFO(H323PeerElementDescriptor, PSafeObject);
  public:
    H323PeerElementDescriptor(const OpalGloballyUniqueID & _descriptorID)
      : descriptorID(_descriptorID), state(Dirty), creator(0)
    { }

    Comparison Compare(const PObject & obj) const;

    enum Options {
      Protocol_H323            = 0x0001,
      Protocol_Voice           = 0x0002,
      Protocol_Max             = 0x0002,
      Option_WildCard          = 0x0004,
      Option_SendAccessRequest = 0x0008,
      Option_NotAvailable      = 0x0010,
      Option_PrioritySet       = 0x0020,
      Option_PriorityMask      = 0x1fc0,
    };

    enum {
      HighestPriority          = 0,
      DefaultPriority          = 80,
      LowestPriority           = 127
    };

    static inline unsigned SetPriorityOption(unsigned pri)     { return Option_PrioritySet | ((pri & 0x7f) << 6); }
    static inline unsigned GetPriorityOption(unsigned options) { return (options & Option_PrioritySet) ? ((options >> 6) & 0x7f) : DefaultPriority; }

    void CopyTo(H501_Descriptor & descriptor);
    static PBoolean CopyToAddressTemplate(H501_AddressTemplate & addressTemplates, 
                                   const H225_EndpointType & ep, 
                           const H225_ArrayOf_AliasAddress & aliases, 
                           const H225_ArrayOf_AliasAddress & transportAddress, 
                                                    unsigned options = H323PeerElementDescriptor::Protocol_H323);

    static void SetProtocolList(H501_ArrayOf_SupportedProtocols & h501Protocols, unsigned protocols);
    static unsigned GetProtocolList(const H501_ArrayOf_SupportedProtocols & h501Protocols);

    OpalGloballyUniqueID descriptorID;

    PBoolean ContainsNonexistent();

    enum States {
      Clean,
      Dirty,
      Deleted
    } state;

    H501_ArrayOf_AddressTemplate addressTemplates;
    PString gatekeeperID;
    PTime lastChanged;
    POrdinalKey creator;
};


////////////////////////////////////////////////////////////////

class H323PeerElementServiceRelationship : public PSafeObject
{
    PCLASSINFO(H323PeerElementServiceRelationship, PSafeObject);
  public:
    H323PeerElementServiceRelationship()
      : ordinal(0)
      { }

    H323PeerElementServiceRelationship(const OpalGloballyUniqueID & _serviceID)
      : serviceID(_serviceID), ordinal(0)
      { }

    Comparison Compare(const PObject & obj) const
      { return serviceID.Compare(((H323PeerElementServiceRelationship&)obj).serviceID); }

    OpalGloballyUniqueID serviceID;
    POrdinalKey ordinal;
    H323TransportAddress peer;
    PString name;
    PTime createdTime;
    PTime lastUpdateTime;
    PTime expireTime;
};


////////////////////////////////////////////////////////////////

/**This class embodies the an H.323 AnnexG server/client
  */
class H323PeerElement : public H323_AnnexG
{
    PCLASSINFO(H323PeerElement, H323_AnnexG);
  public:
  /**@name Construction */
  //@{
    /**Create a new client.
     */
    H323PeerElement(
      H323EndPoint & endpoint,  ///<  Endpoint gatekeeper is associated with.
      H323Transport * transport = NULL
    );
    H323PeerElement(
      H323EndPoint & endpoint,  ///<  Endpoint gatekeeper is associated with.
      const H323TransportAddress & addr
    );

    /**Destroy client.
     */
    ~H323PeerElement();
  //@}

    enum Error {
      Confirmed,
      Rejected,
      NoResponse,
      NoServiceRelationship,
      ServiceRelationshipReestablished
    };

    enum {
      LocalServiceRelationshipOrdinal  = 0,
      NoServiceRelationshipOrdinal     = 1,
      RemoteServiceRelationshipOrdinal = 2
    };

  /**@name Overrides from PObject */
  //@{
    /**Print the name of the peer element.
      */
    void PrintOn(
      ostream & strm    ///<  Stream to print to.
    ) const;
  //@}

    PSafePtr<H323PeerElementDescriptor> GetFirstDescriptor(
      PSafetyMode mode = PSafeReference
    ) { return PSafePtr<H323PeerElementDescriptor>(descriptors, mode); }

    PSafePtr<H323PeerElementServiceRelationship> GetFirstLocalServiceRelationship(
      PSafetyMode mode = PSafeReference
    ) { return PSafePtr<H323PeerElementServiceRelationship>(localServiceRelationships, mode); }

    PSafePtr<H323PeerElementServiceRelationship> GetFirstRemoteServiceRelationship(
      PSafetyMode mode = PSafeReference
    ) { return PSafePtr<H323PeerElementServiceRelationship>(remoteServiceRelationships, mode); }

    void SetLocalName(const PString & name);
    PString GetLocalName() const;

    void SetDomainName(const PString & name);
    PString GetDomainName() const;

    /*********************************************************
      functions to establish and break service relationships
      */

    /**Send a service request
     */
    PBoolean SetOnlyServiceRelationship(const PString & peer, PBoolean keepTrying = TRUE);
    PBoolean AddServiceRelationship(const H323TransportAddress & peer, PBoolean keepTrying = TRUE);
    PBoolean AddServiceRelationship(const H323TransportAddress & peer, OpalGloballyUniqueID & serviceID, PBoolean keepTrying = TRUE);
    PBoolean RemoveServiceRelationship(const OpalGloballyUniqueID & serviceID, int reason = H501_ServiceReleaseReason::e_terminated);
    PBoolean RemoveServiceRelationship(const H323TransportAddress & peer, int reason = H501_ServiceReleaseReason::e_terminated);
    PBoolean RemoveAllServiceRelationships();

    Error ServiceRequestByAddr(const H323TransportAddress & peer);
    Error ServiceRequestByAddr(const H323TransportAddress & peer, OpalGloballyUniqueID & serviceID);
    Error ServiceRequestByID(OpalGloballyUniqueID & serviceID);

    /**Send a service release
     */
    PBoolean ServiceRelease(const OpalGloballyUniqueID & serviceID, unsigned reason);

    /*********************************************************
      functions to manipulate the local descriptor table
     */

    PBoolean AddDescriptor(
      const OpalGloballyUniqueID & descriptorID,
      const PStringArray & aliases, 
      const H323TransportAddressArray & transportAddrs, 
      unsigned options = H323PeerElementDescriptor::Protocol_H323, 
      PBoolean now = FALSE
    );

    PBoolean AddDescriptor(
      const OpalGloballyUniqueID & descriptorID,
      const H225_ArrayOf_AliasAddress & aliases, 
      const H323TransportAddressArray & transportAddrs, 
      unsigned options = H323PeerElementDescriptor::Protocol_H323, 
      PBoolean now = FALSE
    );

    PBoolean AddDescriptor(
      const OpalGloballyUniqueID & descriptorID,
      const H225_ArrayOf_AliasAddress & aliases, 
      const H225_ArrayOf_AliasAddress & transportAddr, 
      unsigned options = H323PeerElementDescriptor::Protocol_H323, 
      PBoolean now = FALSE
    );

    PBoolean AddDescriptor(
      const OpalGloballyUniqueID & descriptorID,
      const POrdinalKey & creator,
      const H225_ArrayOf_AliasAddress & alias, 
      const H225_ArrayOf_AliasAddress & transportAddresses,
      unsigned options = H323PeerElementDescriptor::Protocol_H323,
      PBoolean now = FALSE
    );

    PBoolean AddDescriptor(
      const OpalGloballyUniqueID & descriptorID,
      const POrdinalKey & creator,
      const H501_ArrayOf_AddressTemplate & addressTemplates,
      const PTime & updateTime,
      PBoolean now = FALSE
    );

    /**Remove a descriptor from the local table
     */
    PBoolean DeleteDescriptor(const PString & alias, PBoolean now = FALSE);
    PBoolean DeleteDescriptor(const H225_AliasAddress & alias, PBoolean now = FALSE);
    PBoolean DeleteDescriptor(const OpalGloballyUniqueID & descriptorID, PBoolean now = FALSE);

    /** Request access to an alias
    */
    PBoolean AccessRequest(
      const PString & searchAlias,
      PStringArray & destAliases, 
      H323TransportAddress & transportAddress,
      unsigned options = H323PeerElementDescriptor::Protocol_H323
    );

    PBoolean AccessRequest(
      const PString & searchAlias,
      H225_ArrayOf_AliasAddress & destAliases,
      H323TransportAddress & transportAddress,
      unsigned options = H323PeerElementDescriptor::Protocol_H323
    );

    PBoolean AccessRequest(
      const H225_AliasAddress & searchAlias,
      H225_ArrayOf_AliasAddress & destAliases,
      H323TransportAddress & transportAddress,
      unsigned options = H323PeerElementDescriptor::Protocol_H323
    );

    PBoolean AccessRequest(
      const H225_AliasAddress & alias,
      H225_ArrayOf_AliasAddress & destAliases,
      H225_AliasAddress & transportAddress,
      unsigned options = H323PeerElementDescriptor::Protocol_H323
    );

    /*********************************************************
      functions to send send descriptors to another peer element
      */
    PBoolean UpdateDescriptor(H323PeerElementDescriptor * descriptor);
    PBoolean UpdateDescriptor(H323PeerElementDescriptor * descriptor, H501_UpdateInformation_updateType::Choices updateType);


    Error SendUpdateDescriptorByID(const OpalGloballyUniqueID & serviceID, 
                                    H323PeerElementDescriptor * descriptor, 
                     H501_UpdateInformation_updateType::Choices updateType);

    Error SendUpdateDescriptorByAddr(const H323TransportAddress & peer, 
                                      H323PeerElementDescriptor * descriptor, 
                       H501_UpdateInformation_updateType::Choices updateType);

    Error SendAccessRequestByID(const OpalGloballyUniqueID & peerID, 
                                                   H501PDU & request, 
                                                   H501PDU & confirmPDU);

    Error SendAccessRequestByAddr(const H323TransportAddress & peerAddr, 
                                                     H501PDU & request, 
                                                     H501PDU & confirmPDU);


    /*********************************************************
      low level request functions
      */

    PBoolean MakeRequest(H323_AnnexG::Request & request);

    virtual void OnAddServiceRelationship(const H323TransportAddress &) { }
    virtual void OnRemoveServiceRelationship(const H323TransportAddress &) { }

    virtual void OnNewDescriptor(const H323PeerElementDescriptor &) { }
    virtual void OnUpdateDescriptor(const H323PeerElementDescriptor &) { }
    virtual void OnRemoveDescriptor(const H323PeerElementDescriptor &) { }

    virtual H323Transaction::Response OnServiceRequest(H501ServiceRequest & info);
    virtual H323Transaction::Response OnDescriptorUpdate(H501DescriptorUpdate & info);
    virtual H323Transaction::Response OnAccessRequest(H501AccessRequest & info);

    PBoolean OnReceiveServiceRequest(const H501PDU & pdu, const H501_ServiceRequest & pduBody);
    PBoolean OnReceiveServiceConfirmation(const H501PDU & pdu, const H501_ServiceConfirmation & pduBody);

    PBoolean OnReceiveDescriptorUpdate(const H501PDU & pdu, const H501_DescriptorUpdate & pduBody);
    PBoolean OnReceiveDescriptorUpdateACK(const H501PDU & pdu, const H501_DescriptorUpdateAck & pduBody);

    PBoolean OnReceiveAccessRequest(const H501PDU & pdu, const H501_AccessRequest & pduBody);
    PBoolean OnReceiveAccessConfirmation (const H501PDU & pdu, const H501_AccessConfirmation & pduBody);
    PBoolean OnReceiveAccessRejection(const H501PDU & pdu,     const H501_AccessRejection & pduBody);

    class AliasKey : public H225_AliasAddress
    {
      public:
        AliasKey(const H225_AliasAddress & _alias, const OpalGloballyUniqueID & _id, PINDEX _pos, PBoolean _wild = FALSE)
          : H225_AliasAddress(_alias), id(_id), pos(_pos), wild(_wild)
        { }

        OpalGloballyUniqueID id;
        PINDEX pos;
        PBoolean wild;
    };

  protected:
    void Construct();

    Error SendUpdateDescriptor(              H501PDU & pdu,  
                          const H323TransportAddress & peer, 
                           H323PeerElementDescriptor * descriptor,
            H501_UpdateInformation_updateType::Choices updateType);

    PBoolean OnRemoteServiceRelationshipDisappeared(OpalGloballyUniqueID & serviceID, const H323TransportAddress & peer);
    void InternalRemoveServiceRelationship(const H323TransportAddress & peer);
    H323Transaction::Response HandleServiceRequest(H501ServiceRequest & info);

    virtual H323PeerElementDescriptor          * CreateDescriptor(const OpalGloballyUniqueID & descriptorID);
    virtual H323PeerElementServiceRelationship * CreateServiceRelationship();
    virtual AliasKey                           * CreateAliasKey(const H225_AliasAddress & alias, const OpalGloballyUniqueID & id, PINDEX pos, PBoolean wild = FALSE);

    void RemoveDescriptorInformation(const H501_ArrayOf_AddressTemplate & addressTemplates);

    PDECLARE_NOTIFIER(PThread, H323PeerElement, MonitorMain);
    PDECLARE_NOTIFIER(PThread, H323PeerElement, UpdateAllDescriptors);
    PDECLARE_NOTIFIER(PTimer, H323PeerElement, TickleMonitor);

    PMutex localNameMutex;
    PString localIdentifier;
    PString domainName;

    PSemaphore requestMutex;
    PThread  * monitor;
    PBoolean       monitorStop;
    PSyncPoint monitorTickle;

    PMutex basePeerOrdinalMutex;
    PINDEX basePeerOrdinal;

    // structures needed to maintain local service relationships (for which we receive a ServiceRequest)
    PSafeSortedList<H323PeerElementServiceRelationship> localServiceRelationships;
    PMutex localPeerListMutex;
    POrdinalSet localServiceOrdinals;

    // structures needed to maintain remote service relationships (for which we send a ServiceRequest)
    PMutex remotePeerListMutex;
    PSafeSortedList<H323PeerElementServiceRelationship> remoteServiceRelationships;
    PStringToString remotePeerAddrToServiceID;
    H323DICTIONARY(StringToOrdinalKey, PString, POrdinalKey);
    StringToOrdinalKey remotePeerAddrToOrdinalKey;

    PSafeSortedList<H323PeerElementDescriptor> descriptors;

    PSORTED_LIST(AliasKeyList, H225_AliasAddress);

    PMutex aliasMutex;
    AliasKeyList transportAddressToDescriptorID;
    AliasKeyList specificAliasToDescriptorID;
    AliasKeyList wildcardAliasToDescriptorID;
};


#endif // __OPAL_PECLIENT_H


/////////////////////////////////////////////////////////////////////////////
