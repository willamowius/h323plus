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
#include <ptlib/pluginmgr.h>
#include <list>

/** This abtract class embodies an H.235 authentication mechanism.
    NOTE: descendants must have a Clone() function for correct operation.
*/
#ifdef H323_H235
class H235_DiffieHellman;
#endif
class H235Authenticator : public PObject
{
    PCLASSINFO(H235Authenticator, PObject);
  public:
    H235Authenticator();

    virtual void PrintOn(
      ostream & strm
    ) const;

    static H235Authenticator * CreateAuthenticator(const PString & authname,        ///< Feature Name Expression
                                                PPluginManager * pluginMgr = NULL   ///< Plugin Manager
                                                 );
#if PTLIB_VER >= 2110

    struct Capability {
        const char * m_identifier;
        const char * m_cipher;
        const char * m_description;
    };

    typedef struct {
       std::list<Capability> capabilityList;
    } Capabilities;

    static H235Authenticator * CreateAuthenticatorByID(const PString & identifier
                                                 );

    static PBoolean GetAuthenticatorCapabilities(const PString & deviceName,
                                                Capabilities * caps,
                                                PPluginManager * pluginMgr = NULL);
#endif

#ifdef H323_H235

    virtual PBoolean GetMediaSessionInfo(PString & algorithmOID, PBYTEArray & sessionKey);
#endif

    virtual const char * GetName() const = 0;

    static PStringArray GetAuthenticatorList();

    virtual PBoolean IsMatch(const PString & /*identifier*/) const { return false; }

    virtual PBoolean PrepareTokens(
      PASN_Array & clearTokens,
      PASN_Array & cryptoTokens
    );

    virtual PBoolean PrepareTokens(
      PASN_Array & clearTokens,
      PASN_Array & cryptoTokens,
      PINDEX max_cipherSize
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
      e_Disabled,   ///< Security is disabled by local system
      e_Failed      ///< Security has Failed
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

    virtual PBoolean IsSecuredPDU(unsigned rasPDU, PBoolean received) const;

    virtual PBoolean IsSecuredSignalPDU(unsigned signalPDU, PBoolean received) const;

    virtual PBoolean IsSecuredSignalPDU(
      unsigned signalPDU,
      PBoolean received,
      PINDEX max_keyLength
    ) const;

    virtual PBoolean IsActive() const;

    virtual void Enable(PBoolean enab = true) { enabled = enab; }
    virtual void Disable() { enabled = false; }

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

    virtual PBoolean GetAlgorithms(PStringList & algorithms) const;  // Get the supported Algorithm OIDs

    virtual PBoolean GetAlgorithmDetails(const PString & algorithm,   ///< Algorithm OID
                                         PString & sslName,           ///< SSL Description
                                         PString & description        ///< Human Description
                                         );

    virtual void ExportParameters(const PFilePath & /*path*/) {}  // export Parameters to file


  protected:
    PBoolean AddCapability(
      unsigned mechanism,
      const PString & oid,
      H225_ArrayOf_AuthenticationMechanism & mechansims,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    PBoolean enabled;

    PString  remoteId;      // ID of remote entity
    PString  localId;       // ID of local entity
    PString  password;      // shared secret

    unsigned sentRandomSequenceNumber;
    //unsigned lastRandomSequenceNumber;
    int lastRandomSequenceNumber;	// using int now, because with unsigned 2 consecutive negative values within the same timestamp caused abort
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
      PASN_Array & cryptoTokens,
      PINDEX max_keyLength = P_MAX_INDEX
    ) const;

    H235Authenticator::ValidationResult ValidateSignalPDU(
      unsigned code,
      const PASN_Array & clearTokens,
      const PASN_Array & cryptoTokens,
      const PBYTEArray & rawPDU
    ) const;

#ifdef H323_H235
    struct DH_Data {
        DH_Data() 
         : m_gData(0) {};

        PString     m_OID;
        PBYTEArray  m_pData;
        PBYTEArray  m_gData;
    };
    typedef list<DH_Data> DH_DataList;

    PBoolean CreateAuthenticators(const PASN_Array & clearTokens, const PASN_Array & cryptoTokens);
    PBoolean CreateAuthenticators(H235Authenticator::Application usage);
    PBoolean CreateAuthenticator(const PString & name);
    PBoolean SupportsEncryption(PStringArray & list) const;
    PBoolean SupportsEncryption() const;
    PBoolean GetAlgorithms(PStringList & algorithms) const;
    PBoolean GetAlgorithmDetails(const PString & algorithm, PString & sslName, PString & description);
	PBoolean GetMediaSessionInfo(PString & algorithmOID, PBYTEArray & sessionKey);

    // Media Encryption Settings
    static void SetEncryptionPolicy(PINDEX policy);
    static PINDEX GetEncryptionPolicy();

    // maximum cipher length in bits
    static void SetMaxCipherLength(PINDEX cipher);
    static PINDEX GetMaxCipherLength();

    // maximum token length in bits
    static void SetMaxTokenLength(PINDEX len);
    static PINDEX GetMaxTokenLength();

    static PString & GetDHParameterFile();
    static void SetDHParameterFile(const PString & filePaths);

    static void LoadDHData(const PString & oid, const PBYTEArray & pData, const PBYTEArray & gData);
    static DH_DataList & GetDHDataList();

 protected:
    void CreateAuthenticatorsByID(const PStringArray & identifiers);

    static PINDEX m_encryptionPolicy;
    static PINDEX m_cipherLength;
    static PINDEX m_maxTokenLength;
    static PString m_dhFile;
    static DH_DataList m_dhData;
#endif

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

H323_DECLARELIST(H235AuthenticatorList, H235AuthenticatorInfo)
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


/* This class is a device independent Time class that is used with H.235 Authentication mechanisms
   It replaces the local time function to support remote gatekeeper synchronisation and ensures security
   is not effected if the user changes the local device time while the application is running. */

class H235AuthenticatorTime : public PObject
{
    PCLASSINFO(H235AuthenticatorTime, PObject);

  public:
    H235AuthenticatorTime();

    /* GetLocalTime : Returns the local calculated time */
    PUInt32b GetLocalTime();

    /* GetTime : Returns the calculated synchronised time */
    PUInt32b GetTime();

    /* GetTime : Adjust the time to synchronise with remote time */
    void SetAdjustedTime(time_t remoteTime);

  protected:
    time_t      m_localTimeStart;   ///< Local time at class instance.
    clock_t     m_localStartTick;   ///< Local Tick count at class instance.
    PInt64      m_adjustedTime;     ///< Adjusted time for remote synchronisation.

};


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

    static PStringArray GetAuthenticatorNames();
#if PTLIB_VER >= 2110
    static PBoolean GetAuthenticationCapabilities(Capabilities * ids);
#endif
    virtual PBoolean IsMatch(const PString & identifier) const;

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

////////////////////////////////////////////////////


#if PTLIB_VER >= 2110 && defined(P_SSL)
/// PFactory Loader
typedef H235AuthSimpleMD5 H235AuthenticatorMD5;
#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(MD5,H235Authenticator);
#endif
#endif


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

    static PStringArray GetAuthenticatorNames();
#if PTLIB_VER >= 2110
    static PBoolean GetAuthenticationCapabilities(Capabilities * ids);
#endif

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

    virtual PBoolean IsSecuredPDU(unsigned rasPDU, PBoolean received) const;
};

#if PTLIB_VER >= 2110 && defined(P_SSL)
// PFactory Loader
typedef H235AuthCAT H235AuthenticatorCAT;
#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(CAT,H235Authenticator);
#endif
#endif

//////////////////////////////////////////////////////////////////////////////

/** This class is used for Time Stamp Synchronisation between the gatekeeper
    and the endpoint
*/
class H235AuthenticatorTSS : public H235Authenticator
{
    PCLASSINFO(H235AuthenticatorTSS, H235Authenticator);
  public:
    H235AuthenticatorTSS();

    PObject * Clone() const;

    virtual const char * GetName() const;

    static PStringArray GetAuthenticatorNames();
#if PTLIB_VER >= 2110
    static PBoolean GetAuthenticationCapabilities(Capabilities * ids);
#endif

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

#if PTLIB_VER >= 2110 && defined(P_SSL)
#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(TSS,H235Authenticator);
#endif
#endif

//////////////////////////////////////////////////////////////////////////////
#if H323_SSL

/** This class embodies the H.235 "base line" from H235.1.
*/

class H2351_Authenticator : public H235Authenticator
{
    PCLASSINFO(H2351_Authenticator, H235Authenticator);
  public:
    H2351_Authenticator();

    PObject * Clone() const;

    virtual const char * GetName() const;

    static PStringArray GetAuthenticatorNames();
#if PTLIB_VER >= 2110
    static PBoolean GetAuthenticationCapabilities(Capabilities * ids);
#endif
    virtual PBoolean IsMatch(const PString & identifier) const;

    virtual H225_CryptoH323Token * CreateCryptoToken();

    virtual PBoolean Finalise(PBYTEArray & rawPDU);

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

    virtual PBoolean IsSecuredPDU(unsigned rasPDU, PBoolean received) const;

    virtual PBoolean IsSecuredSignalPDU(unsigned rasPDU, PBoolean received) const;

    virtual PBoolean UseGkAndEpIdentifiers() const;

    virtual void RequireGeneralID(bool value) { m_requireGeneralID = value; }
    virtual void FullQ931Checking(bool value) { m_fullQ931Checking = value; }

protected:
    PBoolean m_requireGeneralID;
    PBoolean m_fullQ931Checking;
};

typedef H2351_Authenticator H235AuthProcedure1;  // Backwards interoperability

//////////////////////////////////////////////////////////////////////////////

#if PTLIB_VER >= 2130

PCREATE_PLUGIN_DEVICE(H235Authenticator);
#define H235SECURITY_EX(name,extra) \
PCREATE_PLUGIN(name, H235Authenticator, H235Authenticator##name, PPlugin_H235Authenticator, \
  virtual PStringArray GetDeviceNames(P_INT_PTR /*userData*/) const { return H235Authenticator##name::GetAuthenticatorNames(); } \
  virtual bool  ValidateDeviceName(const PString & deviceName, int /*userData*/) const \
    { return (deviceName == H235Authenticator##name::GetAuthenticatorNames()[0]); } \
  virtual bool GetDeviceCapabilities(const PString & /*deviceName*/, void * caps) const \
    { return H235Authenticator##name::GetAuthenticationCapabilities((H235Authenticator::Capabilities *)caps); } \
extra)
#define H235SECURITY(name) H235SECURITY_EX(name, )

#else

template <class className> class H235PluginServiceDescriptor : public PDevicePluginServiceDescriptor
{
  public:
    virtual PObject *   CreateInstance(int /*userData*/) const { return new className; }
    virtual PStringArray GetDeviceNames(int /*userData*/) const {
               return className::GetAuthenticatorNames();
    }
    virtual bool  ValidateDeviceName(const PString & deviceName, int /*userData*/) const {
            return (deviceName == className::GetAuthenticatorNames()[0]);
    }
#if PTLIB_VER >= 2110
    virtual bool GetDeviceCapabilities(const PString & /*deviceName*/, void * capabilities) const {
        return className::GetAuthenticationCapabilities((H235Authenticator::Capabilities *)capabilities);
    }
#endif
};

#define H235SECURITY(name) \
static H235PluginServiceDescriptor<H235Authenticator##name> H235Authenticator##name##_descriptor; \
PCREATE_PLUGIN(name, H235Authenticator, &H235Authenticator##name##_descriptor);
#endif

#endif


#endif //__H323_H235AUTH_H


/////////////////////////////////////////////////////////////////////////////
