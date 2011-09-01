/*
 * h235auth.h
 *
 * H.235 authorisation PDU's
 *
 * H323Plus Library
 *
 * Copyright (c) 1998-2001 Equivalence Pty. Ltd.
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
 * Contributor(s): Fürbass Franz <franz.fuerbass@infonova.at>
 *
 * $Id$
 *
 */

#ifndef __OPAL_H235AUTH_H
#define __OPAL_H235AUTH_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

class H323TransactionPDU;
class H225_CryptoH323Token;
class H225_ArrayOf_AuthenticationMechanism;
class H225_ArrayOf_PASN_ObjectId;
class H235_ClearToken;
class H235_AuthenticationMechanism;
class PASN_ObjectId;
class PASN_Sequence;
class PASN_Array;

class H323SignalPDU;
class H323Connection;
class PSSLCertificate;

#include "ptlib_extras.h"

/** This abtract class embodies an H.235 authentication mechanism.
    NOTE: descendants must have a Clone() function for correct operation.
*/
class H235Authenticator : public PObject
{
    PCLASSINFO(H235Authenticator, PObject);
  public:
    H235Authenticator();

    virtual void PrintOn(
      ostream & strm
    ) const;

    virtual const char * GetName() const = 0;

    virtual PBoolean PrepareTokens(
      PASN_Array & clearTokens,
      PASN_Array & cryptoTokens
    );

    virtual H235_ClearToken * CreateClearToken();
    virtual H225_CryptoH323Token * CreateCryptoToken();

    virtual PBoolean Finalise(
      PBYTEArray & rawPDU
    );

    enum ValidationResult {
      e_OK = 0,     ///< Security parameters and Msg are ok, no security attacks
      e_Absent,     ///< Security parameters are expected but absent
      e_Error,      ///< Security parameters are present but incorrect
      e_InvalidTime,///< Security parameters indicate peer has bad real time clock
      e_BadPassword,///< Security parameters indicate bad password in token
      e_ReplyAttack,///< Security parameters indicate an attack was made
      e_Disabled    ///< Security is disabled by local system
    };

    virtual ValidationResult ValidateTokens(
      const PASN_Array & clearTokens,
      const PASN_Array & cryptoTokens,
      const PBYTEArray & rawPDU
    );

    virtual ValidationResult ValidateClearToken(
      const H235_ClearToken & clearToken
    );

    virtual ValidationResult ValidateCryptoToken(
      const H225_CryptoH323Token & cryptoToken,
      const PBYTEArray & rawPDU
    );

    virtual PBoolean IsCapability(
      const H235_AuthenticationMechanism & mechansim,
      const PASN_ObjectId & algorithmOID
    ) = 0;

    virtual PBoolean SetCapability(
      H225_ArrayOf_AuthenticationMechanism & mechansims,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    ) = 0;

    virtual PBoolean UseGkAndEpIdentifiers() const;

    virtual PBoolean IsSecuredPDU(
      unsigned rasPDU,
      PBoolean received
    ) const;

    virtual PBoolean IsSecuredSignalPDU(
      unsigned signalPDU,
      PBoolean received
    ) const;

    virtual PBoolean IsActive() const;

    virtual void Enable(
      PBoolean enab = TRUE
    ) { enabled = enab; }
    virtual void Disable() { enabled = FALSE; }

    virtual const PString & GetRemoteId() const { return remoteId; }
    virtual void SetRemoteId(const PString & id) { remoteId = id; }

    virtual const PString & GetLocalId() const { return localId; }
    virtual void SetLocalId(const PString & id) { localId = id; }

    virtual const PString & GetPassword() const { return password; }
    virtual void SetPassword(const PString & pw) { password = pw; }

    virtual int GetTimestampGracePeriod() const { return timestampGracePeriod; }
    virtual void SetTimestampGracePeriod(int grace) { timestampGracePeriod = grace; }

    enum Application {
        GKAdmission,		///< To Be Used for GK Admission
        EPAuthentication,	///< To Be Used for EP Authentication
        LRQOnly,            ///< To Be Used for Location Request Authentication
        MediaEncryption,    ///< To Be Used for MediaEncryption
        AnyApplication,		///< To Be Used for Any Application
    };

    Application GetApplication() { return usage; }  // Get Authentication Application

    virtual void SetConnection(H323Connection * con);	// Set the connection for EPAuthentication

  protected:
    PBoolean AddCapability(
      unsigned mechanism,
      const PString & oid,
      H225_ArrayOf_AuthenticationMechanism & mechansims,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    PBoolean     enabled;

    PString  remoteId;      // ID of remote entity
    PString  localId;       // ID of local entity
    PString  password;      // shared secret

    unsigned sentRandomSequenceNumber;
    unsigned lastRandomSequenceNumber;
    unsigned lastTimestamp;
    int      timestampGracePeriod;

    Application usage;	       ///* Authenticator's Application 
    H323Connection * connection;   ///* CallToken of the Connection for EP Authentication
    PMutex mutex;
};


PDECLARE_LIST(H235Authenticators, H235Authenticator)
#ifdef DOC_PLUS_PLUS
{
#endif
  public:
// GKAdmission
    void PreparePDU(
      H323TransactionPDU & pdu,
      PASN_Array & clearTokens,
      unsigned clearOptionalField,
      PASN_Array & cryptoTokens,
      unsigned cryptoOptionalField
    ) const;

    H235Authenticator::ValidationResult ValidatePDU(
      const H323TransactionPDU & pdu,
      const PASN_Array & clearTokens,
      unsigned clearOptionalField,
      const PASN_Array & cryptoTokens,
      unsigned cryptoOptionalField,
      const PBYTEArray & rawPDU
    ) const;

// EPAuthentication
    void PrepareSignalPDU(
      unsigned code,
      PASN_Array & clearTokens,
      PASN_Array & cryptoTokens
    ) const;

    H235Authenticator::ValidationResult ValidateSignalPDU(
      unsigned code,
      const PASN_Array & clearTokens,
      const PASN_Array & cryptoTokens,
      const PBYTEArray & rawPDU
    ) const;

};

class H235AuthenticatorInfo : public PObject
{
    PCLASSINFO(H235AuthenticatorInfo, PObject);
public:
	H235AuthenticatorInfo(PString username,PString password,PBoolean ishashed);
	H235AuthenticatorInfo(PSSLCertificate * cert);
	PString UserName;
	PString Password;
	PBoolean isHashed;
	PSSLCertificate * Certificate;
};

PDECLARE_LIST(H235AuthenticatorList, H235AuthenticatorInfo)
#ifdef DOC_PLUS_PLUS
{
#endif
	PBoolean HasUserName(PString UserName) const;
	void LoadPassword(PString UserName, PString & pass) const;
	void Add(PString username, PString password, PBoolean isHashed = FALSE);
	PString PasswordEncrypt(const PString &clear) const;
	PString PasswordDecrypt(const PString &encrypt) const;
};

/** Dictionary of Addresses and Associated Security Info  */
H323DICTIONARY(H235AuthenticatorDict,PString,H235AuthenticatorInfo); 

/** This class embodies a simple MD5 based authentication.
    The users password is concatenated with the 4 byte timestamp and 4 byte
    random fields and an MD5 generated and sent/verified
*/
class H235AuthSimpleMD5 : public H235Authenticator
{
    PCLASSINFO(H235AuthSimpleMD5, H235Authenticator);
  public:
    H235AuthSimpleMD5();

    PObject * Clone() const;

    virtual const char * GetName() const;

    virtual H225_CryptoH323Token * CreateCryptoToken();

    virtual ValidationResult ValidateCryptoToken(
      const H225_CryptoH323Token & cryptoToken,
      const PBYTEArray & rawPDU
    );

    virtual PBoolean IsCapability(
      const H235_AuthenticationMechanism & mechansim,
      const PASN_ObjectId & algorithmOID
    );

    virtual PBoolean SetCapability(
      H225_ArrayOf_AuthenticationMechanism & mechansim,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    virtual PBoolean IsSecuredPDU(
      unsigned rasPDU,
      PBoolean received
    ) const;

    virtual PBoolean IsSecuredSignalPDU(
      unsigned rasPDU,
      PBoolean received
    ) const;
};


/** This class embodies a RADIUS compatible based authentication (aka Cisco
    Access Token or CAT).
    The users password is concatenated with the 4 byte timestamp and 1 byte
    random fields and an MD5 generated and sent/verified via the challenge
    field.
*/
class H235AuthCAT : public H235Authenticator
{
    PCLASSINFO(H235AuthCAT, H235Authenticator);
  public:
    H235AuthCAT();

    PObject * Clone() const;

    virtual const char * GetName() const;

    virtual H235_ClearToken * CreateClearToken();

    virtual ValidationResult ValidateClearToken(
      const H235_ClearToken & clearToken
    );

    virtual PBoolean IsCapability(
      const H235_AuthenticationMechanism & mechansim,
      const PASN_ObjectId & algorithmOID
    );

    virtual PBoolean SetCapability(
      H225_ArrayOf_AuthenticationMechanism & mechansim,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    virtual PBoolean IsSecuredPDU(
      unsigned rasPDU,
      PBoolean received
    ) const;
};


#if P_SSL

/** This class embodies the H.235 "base line" from H235.1.
*/

class H2351_Authenticator : public H235Authenticator
{
    PCLASSINFO(H2351_Authenticator, H235Authenticator);
  public:
    H2351_Authenticator();

    PObject * Clone() const;

    virtual const char * GetName() const;

    virtual H225_CryptoH323Token * CreateCryptoToken();

    virtual PBoolean Finalise(
      PBYTEArray & rawPDU
    );

    virtual ValidationResult ValidateCryptoToken(
      const H225_CryptoH323Token & cryptoToken,
      const PBYTEArray & rawPDU
    );

    virtual PBoolean IsCapability(
      const H235_AuthenticationMechanism & mechansim,
      const PASN_ObjectId & algorithmOID
    );

    virtual PBoolean SetCapability(
      H225_ArrayOf_AuthenticationMechanism & mechansim,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    virtual PBoolean IsSecuredPDU(
      unsigned rasPDU,
      PBoolean received
    ) const;

    virtual PBoolean IsSecuredSignalPDU(
      unsigned rasPDU,
      PBoolean received
    ) const;

    virtual PBoolean UseGkAndEpIdentifiers() const;
};

typedef H2351_Authenticator H235AuthProcedure1;  // Backwards interoperability

#endif


#endif //__OPAL_H235AUTH_H


/////////////////////////////////////////////////////////////////////////////
