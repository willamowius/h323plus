/*
 * h235auth.cxx
 *
 * H.235 security PDU's
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
 * Contributor(s): __________________________________
 *
 * $Id$
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "h235auth.h"
#endif

#if defined(_WIN32) && (_MSC_VER > 1300)
  #pragma warning(disable:4244) // warning about possible loss of data
#endif

#include "h235auth.h"

#include "h323pdu.h"
#include <ptclib/random.h>
#include <ptclib/cypher.h>


#define new PNEW

/////////////////////////////////////////////////////////////////////////////

static const char H235AuthenticatorPluginBaseClass[] = "H235Authenticator";

#if PTLIB_VER >= 2110 && PTLIB_VER < 2130

template <> H235Authenticator * PDevicePluginFactory<H235Authenticator>::Worker::Create(const PDefaultPFactoryKey & type) const
{
  return H235Authenticator::CreateAuthenticator(type);
}

typedef PDevicePluginAdapter<H235Authenticator> PDevicePluginH235;
PFACTORY_CREATE(PFactory<PDevicePluginAdapterBase>, PDevicePluginH235, H235AuthenticatorPluginBaseClass, true);

#endif

///////////////////////////////////////////////////////////////////////////////

H235Authenticator::H235Authenticator()
{
  enabled = TRUE;
  sentRandomSequenceNumber = PRandom::Number()&INT_MAX;
  lastRandomSequenceNumber = 0;
  lastTimestamp = 0;
  timestampGracePeriod = 2*60*60+10; // 2 hours 10 seconds to allow for DST adjustments
  usage = GKAdmission;               // Set Default Application to GKAdmission
  connection = NULL;
}


PStringArray H235Authenticator::GetAuthenticatorList()
{
#if PTLIB_VER >= 2130
    PPluginManager * plugMgr = &PPluginManager::GetPluginManager();
    return plugMgr->GetPluginsProviding(H235AuthenticatorPluginBaseClass, false);
#else
    PStringArray authList;
      PFactory<H235Authenticator>::KeyList_T keyList = PFactory<H235Authenticator>::GetKeyList();
      PFactory<H235Authenticator>::KeyList_T::const_iterator r;
      for (r = keyList.begin(); r != keyList.end(); ++r) {
           authList.AppendString(*r);
      }
    return authList;
#endif
}

H235Authenticator * H235Authenticator::CreateAuthenticator(const PString & authname, PPluginManager * pluginMgr)
{
  if (pluginMgr == NULL)
    pluginMgr = &PPluginManager::GetPluginManager();

#if PTLIB_VER >= 2130
  return (H235Authenticator *)pluginMgr->CreatePlugin(authname, H235AuthenticatorPluginBaseClass);
#else
  return (H235Authenticator *)pluginMgr->CreatePluginsDeviceByName(authname, H235AuthenticatorPluginBaseClass);
#endif
}

#if PTLIB_VER >= 2110

PBoolean H235Authenticator::GetAuthenticatorCapabilities(const PString & deviceName, H235Authenticator::Capabilities * caps, PPluginManager * pluginMgr)
{
  if (pluginMgr == NULL)
    pluginMgr = &PPluginManager::GetPluginManager();
  
  return pluginMgr->GetPluginsDeviceCapabilities(H235AuthenticatorPluginBaseClass,"",deviceName, caps);
}

H235Authenticator * H235Authenticator::CreateAuthenticatorByID(const PString & identifier)
{
  PBoolean found=false;
  PStringArray list = GetAuthenticatorList();
  for (PINDEX i=0; i < list.GetSize(); ++i) {
      Capabilities caps;
      if (GetAuthenticatorCapabilities(list[i], &caps)) {
	      for (std::list<Capability>::const_iterator r = caps.capabilityList.begin(); r != caps.capabilityList.end(); ++r)
              if (PString(r->m_identifier) == identifier) {
                  found = true;
                  break;
              }
      }
      if (found) {
         return CreateAuthenticator(list[i]);
      }
  }
  return NULL;
}
#endif

#ifdef H323_H235
PBoolean H235Authenticator::GetMediaSessionInfo(PString & /* algorithmOID */, PBYTEArray & /* sessionKey */)
{
    return false;
}
#endif

void H235Authenticator::PrintOn(ostream & strm) const
{
  PWaitAndSignal m(mutex);

  strm << GetName() << '<';
  if (IsActive())
    strm << "active";
  else if (!enabled)
    strm << "disabled";
  else if (password.IsEmpty())
    strm << "no-pwd";
  else
    strm << "inactive";
  strm << '>';
}


PBoolean H235Authenticator::PrepareTokens(PASN_Array & clearTokens,
                                      PASN_Array & cryptoTokens)
{
  PWaitAndSignal m(mutex);

  if (!IsActive())
    return FALSE;

  H235_ClearToken * clearToken = CreateClearToken();
  if (clearToken != NULL) {
    // Check if already have a token of this type and overwrite it
    for (PINDEX i = 0; i < clearTokens.GetSize(); i++) {
      H235_ClearToken & oldToken = (H235_ClearToken &)clearTokens[i];
      if (clearToken->m_tokenOID == oldToken.m_tokenOID) {
        oldToken = *clearToken;
        delete clearToken;
        clearToken = NULL;
        break;
      }
    }

    if (clearToken != NULL)
      clearTokens.Append(clearToken);
  }

  H225_CryptoH323Token * cryptoToken = CreateCryptoToken();
  if (cryptoToken != NULL)
    cryptoTokens.Append(cryptoToken);

  return TRUE;
}

PBoolean H235Authenticator::PrepareTokens(PASN_Array & clearTokens, PASN_Array & cryptoTokens, PINDEX /*max_cipherSize*/)
{
    return PrepareTokens(clearTokens, cryptoTokens);
}


H235_ClearToken * H235Authenticator::CreateClearToken()
{
  return NULL;
}


H225_CryptoH323Token * H235Authenticator::CreateCryptoToken()
{
  return NULL;
}


PBoolean H235Authenticator::Finalise(PBYTEArray & /*rawPDU*/)
{
  return TRUE;
}


H235Authenticator::ValidationResult H235Authenticator::ValidateTokens(
                                        const PASN_Array & clearTokens,
                                        const PASN_Array & cryptoTokens,
                                        const PBYTEArray & rawPDU)
{
  PWaitAndSignal m(mutex);

  if (!IsActive())
    return e_Disabled;

  PINDEX i;
  for (i = 0; i < clearTokens.GetSize(); i++) {
    ValidationResult s = ValidateClearToken((H235_ClearToken &)clearTokens[i]);
    if (s != e_Absent)
      return s;
  }

  for (i = 0; i < cryptoTokens.GetSize(); i++) {
    ValidationResult s = ValidateCryptoToken((H225_CryptoH323Token &)cryptoTokens[i], rawPDU);
    if (s != e_Absent)
      return s;
  }

  return e_Absent;
}


H235Authenticator::ValidationResult H235Authenticator::ValidateClearToken(
                                                 const H235_ClearToken & /*clearToken*/)
{
  return e_Absent;
}


H235Authenticator::ValidationResult H235Authenticator::ValidateCryptoToken(
                                            const H225_CryptoH323Token & /*cryptoToken*/,
                                            const PBYTEArray & /*rawPDU*/)
{
  return e_Absent;
}


PBoolean H235Authenticator::UseGkAndEpIdentifiers() const
{
  return FALSE;
}


PBoolean H235Authenticator::IsSecuredPDU(unsigned, PBoolean) const
{
  return TRUE;
}

PBoolean H235Authenticator::IsSecuredSignalPDU(unsigned, PBoolean) const
{
  return FALSE;
}

PBoolean H235Authenticator::IsSecuredSignalPDU(unsigned signalPDU, PBoolean received, PINDEX /*max_keyLength*/) const
{
  return IsSecuredSignalPDU(signalPDU, received);
}

PBoolean H235Authenticator::IsActive() const
{
  return enabled && !password;
}


PBoolean H235Authenticator::AddCapability(unsigned mechanism,
                                      const PString & oid,
                                      H225_ArrayOf_AuthenticationMechanism & mechanisms,
                                      H225_ArrayOf_PASN_ObjectId & algorithmOIDs)
{
  PWaitAndSignal m(mutex);

  if (!IsActive()) {
    PTRACE(2, "RAS\tAuthenticator " << *this
            << " not active during SetCapability negotiation");
    return FALSE;
  }

  PINDEX i;
  PINDEX size = mechanisms.GetSize();
  for (i = 0; i < size; i++) {
    if (mechanisms[i].GetTag() == mechanism)
      break;
  }
  if (i >= size) {
    mechanisms.SetSize(size+1);
    mechanisms[size].SetTag(mechanism);
  }

  size = algorithmOIDs.GetSize();
  for (i = 0; i < size; i++) {
    if (algorithmOIDs[i] == oid)
      break;
  }
  if (i >= size) {
    algorithmOIDs.SetSize(size+1);
    algorithmOIDs[size] = oid;
  }

  return TRUE;
}

void H235Authenticator::SetConnection(H323Connection * con)
{
    connection = con;
}

PBoolean H235Authenticator::GetAlgorithms(PStringList & /*algorithms*/) const
{
    return false;
}

PBoolean H235Authenticator::GetAlgorithmDetails(const PString & /*algorithm*/, 
                                    PString & /*sslName*/, PString & /*description*/)
{
    return false;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef H323_H235
PINDEX   H235Authenticators::m_encryptionPolicy = 0;   // Default Encryption is disabled
PINDEX   H235Authenticators::m_cipherLength = 128;     // Ciphers above 128 must be expressly enabled.  
PINDEX   H235Authenticators::m_maxTokenLength = 1024;  // Tokens longer than 1024 bits must be expressly enabled.  
PString  H235Authenticators::m_dhFile=PString();
#endif

void H235Authenticators::PreparePDU(H323TransactionPDU & pdu,
                                    PASN_Array & clearTokens,
                                    unsigned clearOptionalField,
                                    PASN_Array & cryptoTokens,
                                    unsigned cryptoOptionalField) const
{
  // Clean out any crypto tokens in case this is a retry message
  // and we are regenerating the tokens due to possible timestamp
  // issues. We don't do this for clear tokens which may be used by
  // other endpoints and should be passed through unchanged.
  cryptoTokens.RemoveAll();

  for (PINDEX i = 0; i < GetSize(); i++) {
    H235Authenticator & authenticator = (*this)[i];
    if (authenticator.IsSecuredPDU(pdu.GetChoice().GetTag(), FALSE) &&
        authenticator.PrepareTokens(clearTokens, cryptoTokens)) {
      PTRACE(4, "H235RAS\tPrepared PDU with authenticator " << authenticator);
    }
  }

  PASN_Sequence & subPDU = (PASN_Sequence &)pdu.GetChoice().GetObject();
  if (clearTokens.GetSize() > 0)
    subPDU.IncludeOptionalField(clearOptionalField);

  if (cryptoTokens.GetSize() > 0)
    subPDU.IncludeOptionalField(cryptoOptionalField);
}


H235Authenticator::ValidationResult
       H235Authenticators::ValidatePDU(const H323TransactionPDU & pdu,
                                       const PASN_Array & clearTokens,
                                       unsigned clearOptionalField,
                                       const PASN_Array & cryptoTokens,
                                       unsigned cryptoOptionalField,
                                       const PBYTEArray & rawPDU) const
{
  PBoolean noneActive = TRUE;
  PINDEX i;
  for (i = 0; i < GetSize(); i++) {
    H235Authenticator & authenticator = (*this)[i];
    if (authenticator.IsActive() && authenticator.IsSecuredPDU(pdu.GetChoice().GetTag(), TRUE)) {
      noneActive = FALSE;
      break;
    }
  }

  if (noneActive)
    return H235Authenticator::e_OK;

  //do not accept non secure RAS Messages
  const PASN_Sequence & subPDU = (const PASN_Sequence &)pdu.GetChoice().GetObject();
  if (!subPDU.HasOptionalField(clearOptionalField) &&
      !subPDU.HasOptionalField(cryptoOptionalField)) {
    PTRACE(2, "H235RAS\tReceived unsecured RAS message (no crypto tokens),"
              " need one of:\n" << setfill(',') << *this << setfill(' '));
    return H235Authenticator::e_Absent;
  }

  for (i = 0; i < GetSize(); i++) {
    H235Authenticator & authenticator = (*this)[i];
    if (authenticator.IsSecuredPDU(pdu.GetChoice().GetTag(), TRUE)) {
      H235Authenticator::ValidationResult result = authenticator.ValidateTokens(clearTokens, cryptoTokens, rawPDU);
      switch (result) {
        case H235Authenticator::e_OK :
          PTRACE(4, "H235RAS\tAuthenticator " << authenticator << " succeeded");
          return H235Authenticator::e_OK;

        case H235Authenticator::e_Absent :
          PTRACE(4, "H235RAS\tAuthenticator " << authenticator << " absent from PDU");
          authenticator.Disable();
          break;

        case H235Authenticator::e_Disabled :
          PTRACE(4, "H235RAS\tAuthenticator " << authenticator << " disabled");
          break;

        default : // Various other failure modes
          PTRACE(4, "H235RAS\tAuthenticator " << authenticator << " failed: " << (int)result);
          return result;
      }
    }
  }

  return H235Authenticator::e_Absent;
}


void H235Authenticators::PrepareSignalPDU(unsigned code,
                                    PASN_Array & clearTokens,
                                    PASN_Array & cryptoTokens,
                                    PINDEX max_keyLength
                                    ) const
{
  // Clean out any crypto tokens in case this is a retry message
  // and we are regenerating the tokens due to possible timestamp
  // issues. We don't do this for clear tokens which may be used by
  // other endpoints and should be passed through unchanged.
  cryptoTokens.RemoveAll();

  PINDEX keyLength = (max_keyLength > m_maxTokenLength) ? m_maxTokenLength : max_keyLength;

  for (PINDEX i = 0; i < GetSize(); i++) {
    H235Authenticator & authenticator = (*this)[i];
     if (authenticator.IsSecuredSignalPDU(code, FALSE) &&
            authenticator.PrepareTokens(clearTokens, cryptoTokens, keyLength)) {
            PTRACE(4, "H235EP\tPrepared SignalPDU with authenticator " << authenticator);
       }
  }
}


H235Authenticator::ValidationResult
       H235Authenticators::ValidateSignalPDU(unsigned code,
                                            const PASN_Array & clearTokens,
                                            const PASN_Array & cryptoTokens,
                                            const PBYTEArray & rawPDU) const
{

  H235Authenticator::ValidationResult finalresult = H235Authenticator::e_Absent;
 
  for (PINDEX i = 0; i < GetSize(); i++) {
    H235Authenticator & authenticator = (*this)[i];
    if (authenticator.IsSecuredSignalPDU(code, TRUE)) {

        H235Authenticator::ValidationResult result = authenticator.ValidateTokens(clearTokens, cryptoTokens, rawPDU);
          switch (result) {
            case H235Authenticator::e_OK :
              PTRACE(4, "H235EP\tAuthenticator " << authenticator << " succeeded");
              finalresult = result;
              break;

            case H235Authenticator::e_Absent :
              PTRACE(4, "H235EP\tAuthenticator " << authenticator << " absent from PDU");
              authenticator.Disable();
#ifdef H323_H235
              if (authenticator.GetApplication() == H235Authenticator::MediaEncryption)
                  return (H235Authenticators::GetEncryptionPolicy() < 2) ? H235Authenticator::e_Absent : H235Authenticator::e_Failed;
#endif
              break;

            case H235Authenticator::e_Disabled :
              PTRACE(4, "H235EP\tAuthenticator " << authenticator << " disabled");
              break;

            default : // Various other failure modes
              PTRACE(4, "H235EP\tAuthenticator " << authenticator << " failed: " << (int)result);
                if (finalresult != H235Authenticator::e_OK) 
                                    finalresult = result;
              break;
          }    

    } else {
        authenticator.Disable();
    }

  }

  return finalresult;
}

#ifdef H323_H235
PStringArray GetIdentifiers(const PASN_Array & clearTokens, const PASN_Array & cryptoTokens)
{
 PStringArray ids;

  for (PINDEX i = 0; i < clearTokens.GetSize(); i++) {
    ids.AppendString(((const H235_ClearToken &)clearTokens[i]).m_tokenOID.AsString());
  }

  for (PINDEX i = 0; i < cryptoTokens.GetSize(); i++) {
      if ((cryptoTokens.GetTag() == H225_CryptoH323Token::e_cryptoEPPwdHash) ||
         (cryptoTokens.GetTag() == H225_CryptoH323Token::e_cryptoGKPwdHash) ||
         (cryptoTokens.GetTag() == H225_CryptoH323Token::e_cryptoEPPwdEncr) ||
         (cryptoTokens.GetTag() == H225_CryptoH323Token::e_cryptoGKPwdEncr) ||
         (cryptoTokens.GetTag() == H225_CryptoH323Token::e_cryptoEPCert) ||
         (cryptoTokens.GetTag() == H225_CryptoH323Token::e_cryptoGKCert) ||
         (cryptoTokens.GetTag() == H225_CryptoH323Token::e_cryptoFastStart)) {
            PTRACE(5,"H235\tReceived unsupported Token: " << cryptoTokens[i]);
      } else if (cryptoTokens.GetTag() == H225_CryptoH323Token::e_nestedcryptoToken) {
         const H235_CryptoToken & nestedCryptoToken = (const H235_CryptoToken &)cryptoTokens[i];
         if (nestedCryptoToken.GetTag() != H235_CryptoToken::e_cryptoHashedToken) {
            PTRACE(5,"H235\tReceived unsupported Nested Token: " << cryptoTokens[i]);
            break;
         }
         const H235_CryptoToken_cryptoHashedToken & cryptoHashedToken = nestedCryptoToken;
         ids.AppendString(cryptoHashedToken.m_tokenOID.AsString());    
      }
  }
  return ids;
}

PBoolean H235Authenticators::CreateAuthenticators(H235Authenticator::Application usage)
{
  PFactory<H235Authenticator>::KeyList_T keyList = PFactory<H235Authenticator>::GetKeyList();
  PFactory<H235Authenticator>::KeyList_T::const_iterator r;
  for (r = keyList.begin(); r != keyList.end(); ++r) {
    H235Authenticator * Auth = PFactory<H235Authenticator>::CreateInstance(*r);
    if (Auth && (Auth->GetApplication() == usage ||
                 Auth->GetApplication() == H235Authenticator::AnyApplication)) 
           this->Append(Auth);
    else
           delete Auth;
  }
  return true;
}

#if PTLIB_VER >= 2110
PBoolean H235Authenticators::CreateAuthenticators(const PASN_Array & clearTokens, const PASN_Array & cryptoTokens)
{
    if (clearTokens.GetSize() == 0 && cryptoTokens.GetSize() == 0)
        return false;
    
    PStringArray identifiers = GetIdentifiers(clearTokens, cryptoTokens);
    for (PINDEX i = 0; i < identifiers.GetSize(); ++i) {
       PBoolean found = false;
       for (PINDEX j = 0; j < this->GetSize(); ++j) {
           H235Authenticator & auth = (*this)[j];
           if (auth.IsMatch(identifiers[i]))  {
               found = true;
               break; 
           }
       }
       if (!found) {
         H235Authenticator * auth = H235Authenticator::CreateAuthenticatorByID(identifiers[i]);
         if (auth)
           this->Append(auth);
       }
    }
    return true;
}
#endif

PBoolean H235Authenticators::CreateAuthenticator(const PString & name)
{
   H235Authenticator * newAuth = H235Authenticator::CreateAuthenticator(name);

   if (!newAuth) 
     return false;

   this->Append(newAuth);
   return true;
}

PBoolean H235Authenticators::SupportsEncryption(PStringArray & list) const
{
   PBoolean found = false;
   for (PINDEX j=0; j< this->GetSize(); ++j) {
       H235Authenticator & auth = (*this)[j];
       if (auth.GetApplication() == H235Authenticator::MediaEncryption)  {
           list.AppendString(auth.GetName());
           found = true;
       }
   }
   return found;
}

PBoolean H235Authenticators::SupportsEncryption() const
{
    PStringArray list;
    return SupportsEncryption(list);
}

PBoolean H235Authenticators::GetAlgorithms(PStringList & algorithms) const
{
   PBoolean found = false;
   for (PINDEX j=0; j< this->GetSize(); ++j) {
       H235Authenticator & auth = (*this)[j];
       if (auth.GetApplication() == H235Authenticator::MediaEncryption)  {
           PStringList algs;
           if (auth.GetAlgorithms(algs)) {
              for (PINDEX i=0; i<algs.GetSize(); ++i)
                     algorithms.AppendString(algs[i]);
              found = true;
           }
       }
   }
   return found; 
}

PBoolean H235Authenticators::GetAlgorithmDetails(const PString & algorithm, PString & sslName, PString & description)
{
   for (PINDEX j=0; j< this->GetSize(); ++j) {
       H235Authenticator & auth = (*this)[j];
       if (auth.GetApplication() == H235Authenticator::MediaEncryption)  {
           if (auth.GetAlgorithmDetails(algorithm, sslName, description)) 
              return true;
       }
   }
   return false;
}

PBoolean H235Authenticators::GetMediaSessionInfo(PString & algorithmOID, PBYTEArray & sessionKey)
{
   for (PINDEX j=0; j< this->GetSize(); ++j) {
       H235Authenticator & auth = (*this)[j];
       if (auth.GetApplication() == H235Authenticator::MediaEncryption)  {
           return auth.GetMediaSessionInfo(algorithmOID, sessionKey);
       }
   }
   return NULL;
}

PString & H235Authenticators::GetDHParameterFile()
{
    return m_dhFile;
}

void H235Authenticators::SetDHParameterFile(const PString & filePaths)
{
    m_dhFile = filePaths;
}

PINDEX H235Authenticators::GetMaxCipherLength()
{
    return m_cipherLength;
}
    
void H235Authenticators::SetMaxCipherLength(PINDEX cipher)
{
    m_cipherLength = cipher;
}

PINDEX H235Authenticators::GetMaxTokenLength()
{
    return m_maxTokenLength;
}
    
void H235Authenticators::SetMaxTokenLength(PINDEX len)
{
    m_maxTokenLength = len;
}

void H235Authenticators::SetEncryptionPolicy(PINDEX policy)
{
    m_encryptionPolicy = policy;
}

PINDEX H235Authenticators::GetEncryptionPolicy()
{
    return m_encryptionPolicy;
}

#endif

///////////////////////////////////////////////////////////////////////////////

PBoolean H235AuthenticatorList::HasUserName(PString UserName) const
{
    for (PINDEX i = 0; i < GetSize(); i++) {
        H235AuthenticatorInfo & info = (*this)[i];
            if (UserName == info.UserName)
                return TRUE;
    }
    return FALSE;
}

void H235AuthenticatorList::LoadPassword(PString UserName, PString & pass) const
{
    for (PINDEX i = 0; i < GetSize(); i++) {
        H235AuthenticatorInfo & info = (*this)[i];
        if (UserName == info.UserName) {
            if (info.isHashed) {
                pass = PasswordDecrypt(info.Password);
            } else 
                pass = info.Password;
        }
    }
}

void H235AuthenticatorList::Add(PString username, PString password, PBoolean isHashed)
{
  H235AuthenticatorInfo * info = new H235AuthenticatorInfo(username,password,isHashed);

  (*this).Append(info);

}

#define AuthenticatorListHashKey  "H235Authenticator"

PString H235AuthenticatorList::PasswordEncrypt(const PString &clear) const
{

    int keyFilled = 0;

    const PString key = AuthenticatorListHashKey;
    
    PTEACypher::Key thekey;
    memset(&thekey, keyFilled, sizeof(PTEACypher::Key));
    memcpy(&thekey, (const char *)key, min(sizeof(PTEACypher::Key), size_t(key.GetLength())));
    PTEACypher cypher(thekey);
    return cypher.Encode(clear);    
}

PString H235AuthenticatorList::PasswordDecrypt(const PString &encrypt) const
{

    int keyFilled = 0;

    const PString key = AuthenticatorListHashKey;

    PTEACypher::Key thekey;
    memset(&thekey, keyFilled, sizeof(PTEACypher::Key));
    memcpy(&thekey, (const char *)key, min(sizeof(PTEACypher::Key), size_t(key.GetLength())));
    PTEACypher cypher(thekey);
    return cypher.Decode(encrypt);    
}

///////////////////////////////////////////////////////////////////////////////
H235AuthenticatorInfo::H235AuthenticatorInfo(PString username,PString password,PBoolean ishashed)
    : UserName(username), Password(password), isHashed(ishashed), Certificate(NULL)
{
}

H235AuthenticatorInfo::H235AuthenticatorInfo(PSSLCertificate * cert)
    : isHashed(false), Certificate(cert)
{
}
///////////////////////////////////////////////////////////////////////////////

#if PTLIB_VER >= 2110
#ifdef H323_SSL
H235SECURITY(MD5);
#endif
#else
static PFactory<H235Authenticator>::Worker<H235AuthSimpleMD5> factoryH235AuthSimpleMD5("SimpleMD5");
#endif

static const char OID_MD5[] = "1.2.840.113549.2.5";

H235AuthSimpleMD5::H235AuthSimpleMD5()
{
    usage = AnyApplication; // Can be used either for GKAdmission or EPAuthenticstion
}


PObject * H235AuthSimpleMD5::Clone() const
{
  return new H235AuthSimpleMD5(*this);
}


const char * H235AuthSimpleMD5::GetName() const
{
  return "MD5";
}

PStringArray H235AuthSimpleMD5::GetAuthenticatorNames()
{
    return PStringArray("MD5");
}

#if PTLIB_VER >= 2110
PBoolean H235AuthSimpleMD5::GetAuthenticationCapabilities(H235Authenticator::Capabilities * ids)
{
      H235Authenticator::Capability cap;
        cap.m_identifier = OID_MD5;
        cap.m_cipher     = "MD5";
        cap.m_description= "md5";
       ids->capabilityList.push_back(cap);
    
    return true;
}
#endif

PBoolean H235AuthSimpleMD5::IsMatch(const PString & identifier) const 
{ 
    return (identifier == PString(OID_MD5)); 
}

static PWCharArray GetUCS2plusNULL(const PString & str)
{
  PWCharArray ucs2 = str.AsUCS2();
  PINDEX len = ucs2.GetSize();
  if (len > 0 && ucs2[len-1] != 0)
    ucs2.SetSize(len+1);
  return ucs2;
}


H225_CryptoH323Token * H235AuthSimpleMD5::CreateCryptoToken()
{
  if (!IsActive())
    return NULL;

  if (localId.IsEmpty()) {
    PTRACE(2, "H235RAS\tH235AuthSimpleMD5 requires local ID for encoding.");
    return NULL;
  }

  // Cisco compatible hash calculation
  H235_ClearToken clearToken;

  // fill the PwdCertToken to calculate the hash
  clearToken.m_tokenOID = "0.0";

  clearToken.IncludeOptionalField(H235_ClearToken::e_generalID);
    // use SetValueRaw() starting with PTLib 2.7.x to preserve the trailing null byte
#if PTLIB_VER < 270
  clearToken.m_generalID = GetUCS2plusNULL(localId);
#else
  clearToken.m_generalID.SetValueRaw(GetUCS2plusNULL(localId));
#endif

  clearToken.IncludeOptionalField(H235_ClearToken::e_password);
#if PTLIB_VER < 270
  clearToken.m_password = GetUCS2plusNULL(password);
#else
  clearToken.m_password.SetValueRaw(GetUCS2plusNULL(password));
#endif

  clearToken.IncludeOptionalField(H235_ClearToken::e_timeStamp);
  clearToken.m_timeStamp = (int)time(NULL);

  // Encode it into PER
  PPER_Stream strm;
  clearToken.Encode(strm);
  strm.CompleteEncoding();

  // Generate an MD5 of the clear tokens PER encoding.
  PMessageDigest5 stomach;
  stomach.Process(strm.GetPointer(), strm.GetSize());
  PMessageDigest5::Code digest;
  stomach.Complete(digest);

  // Create the H.225 crypto token
  H225_CryptoH323Token * cryptoToken = new H225_CryptoH323Token;
  cryptoToken->SetTag(H225_CryptoH323Token::e_cryptoEPPwdHash);
  H225_CryptoH323Token_cryptoEPPwdHash & cryptoEPPwdHash = *cryptoToken;

  // Set the token data that actually goes over the wire
  H323SetAliasAddress(localId, cryptoEPPwdHash.m_alias);

  cryptoEPPwdHash.m_timeStamp = clearToken.m_timeStamp;
  cryptoEPPwdHash.m_token.m_algorithmOID = OID_MD5;
  cryptoEPPwdHash.m_token.m_hash.SetData(sizeof(digest)*8, (const BYTE *)&digest);

  return cryptoToken;
}


H235Authenticator::ValidationResult H235AuthSimpleMD5::ValidateCryptoToken(
                                             const H225_CryptoH323Token & cryptoToken,
                                             const PBYTEArray &)
{
  if (!IsActive())
    return e_Disabled;

  // verify the token is of correct type
  if (cryptoToken.GetTag() != H225_CryptoH323Token::e_cryptoEPPwdHash)
    return e_Absent;

  const H225_CryptoH323Token_cryptoEPPwdHash & cryptoEPPwdHash = cryptoToken;

  PString alias = H323GetAliasAddressString(cryptoEPPwdHash.m_alias);

  // If has connection then EP Authenticator so CallBack to Check SenderID and Set Password
  if (connection != NULL) { 
     if (!connection->OnCallAuthentication(alias,password)) {
        PTRACE(1, "H235EP\tH235AuthSimpleMD5 Authentication Fail UserName \"" << alias 
               << "\", not Authorised. \"");
        return e_BadPassword;
     }
  } else {
     if (!remoteId && alias != remoteId) {
        PTRACE(1, "H235RAS\tH235AuthSimpleMD5 alias is \"" << alias
           << "\", should be \"" << remoteId << '"');
         return e_Error;
     }
  }

  // Build the clear token
  H235_ClearToken clearToken;
  clearToken.m_tokenOID = "0.0";

  clearToken.IncludeOptionalField(H235_ClearToken::e_generalID);
  // use SetValueRaw() starting with PTLib 2.7.x to preserve the trailing null byte
#if PTLIB_VER < 270
  clearToken.m_generalID = GetUCS2plusNULL(alias);
#else
  clearToken.m_generalID.SetValueRaw(GetUCS2plusNULL(alias));
#endif

  clearToken.IncludeOptionalField(H235_ClearToken::e_password);
#if PTLIB_VER < 270
  clearToken.m_password = GetUCS2plusNULL(password);
#else
  clearToken.m_password.SetValueRaw(GetUCS2plusNULL(password));
#endif

  clearToken.IncludeOptionalField(H235_ClearToken::e_timeStamp);
  clearToken.m_timeStamp = cryptoEPPwdHash.m_timeStamp;

  // Encode it into PER
  PPER_Stream strm;
  clearToken.Encode(strm);
  strm.CompleteEncoding();

  // Generate an MD5 of the clear tokens PER encoding.
  PMessageDigest5 stomach;
  stomach.Process(strm.GetPointer(), strm.GetSize());
  PMessageDigest5::Code digest;
  stomach.Complete(digest);

  if (cryptoEPPwdHash.m_token.m_hash.GetSize() == sizeof(digest)*8 &&
      memcmp(cryptoEPPwdHash.m_token.m_hash.GetDataPointer(), &digest, sizeof(digest)) == 0)
    return e_OK;

  PTRACE(1, "H235RAS\tH235AuthSimpleMD5 digest does not match.");
  return e_BadPassword;
}


PBoolean H235AuthSimpleMD5::IsCapability(const H235_AuthenticationMechanism & mechanism,
                                     const PASN_ObjectId & algorithmOID)
{
  return mechanism.GetTag() == H235_AuthenticationMechanism::e_pwdHash &&
         algorithmOID.AsString() == OID_MD5;
}


PBoolean H235AuthSimpleMD5::SetCapability(H225_ArrayOf_AuthenticationMechanism & mechanisms,
                                      H225_ArrayOf_PASN_ObjectId & algorithmOIDs)
{
  return AddCapability(H235_AuthenticationMechanism::e_pwdHash, OID_MD5, mechanisms, algorithmOIDs);
}


PBoolean H235AuthSimpleMD5::IsSecuredPDU(unsigned rasPDU, PBoolean received) const
{
  switch (rasPDU) {
    case H225_RasMessage::e_registrationRequest :
/*  case H225_RasMessage::e_unregistrationRequest :
    case H225_RasMessage::e_admissionRequest :
    case H225_RasMessage::e_disengageRequest :
    case H225_RasMessage::e_bandwidthRequest :
    case H225_RasMessage::e_infoRequestResponse : */
      return received ? !remoteId.IsEmpty() : !localId.IsEmpty();

    default :
      return FALSE;
  }
}

PBoolean H235AuthSimpleMD5::IsSecuredSignalPDU(unsigned signalPDU, PBoolean received) const
{
  switch (signalPDU) {
    case H225_H323_UU_PDU_h323_message_body::e_setup:       
      return received ? !remoteId.IsEmpty() : !localId.IsEmpty();

    default :
      return FALSE;
  }
}


///////////////////////////////////////////////////////////////////////////////

#if PTLIB_VER >= 2110
#ifdef H323_SSL
H235SECURITY(CAT);
#endif
#else
static PFactory<H235Authenticator>::Worker<H235AuthCAT> factoryH235AuthCAT("SimpleCAT");
#endif

static const char OID_CAT[] = "1.2.840.113548.10.1.2.1";

H235AuthCAT::H235AuthCAT()
{
    usage = GKAdmission;         /// To be used for GKAdmission
}


PObject * H235AuthCAT::Clone() const
{
  return new H235AuthCAT(*this);
}


const char * H235AuthCAT::GetName() const
{
  return "CAT";
}

PStringArray H235AuthCAT::GetAuthenticatorNames()
{
    return PStringArray("CAT");
}


#if PTLIB_VER >= 2110
PBoolean H235AuthCAT::GetAuthenticationCapabilities(H235Authenticator::Capabilities * ids)
{
      H235Authenticator::Capability cap;
        cap.m_identifier = OID_CAT;
        cap.m_cipher     = "CAT";
        cap.m_description= "cat";
       ids->capabilityList.push_back(cap);
    
    return true;
}
#endif


H235_ClearToken * H235AuthCAT::CreateClearToken()
{
  if (!IsActive())
    return NULL;

  if (localId.IsEmpty()) {
    PTRACE(2, "H235RAS\tH235AuthCAT requires local ID for encoding.");
    return NULL;
  }

  H235_ClearToken * clearToken = new H235_ClearToken;

  // Cisco compatible hash calculation
  clearToken->m_tokenOID = OID_CAT;

  clearToken->IncludeOptionalField(H235_ClearToken::e_generalID);
  // use SetValueRaw() starting with PTLib 2.7.x to preserve the trailing null byte
#if PTLIB_VER < 270
  clearToken->m_generalID = GetUCS2plusNULL(localId);
#else
  clearToken->m_generalID.SetValueRaw(GetUCS2plusNULL(localId));
#endif

  clearToken->IncludeOptionalField(H235_ClearToken::e_timeStamp);
  clearToken->m_timeStamp = (int)time(NULL);
  PUInt32b timeStamp = (DWORD)clearToken->m_timeStamp;

  clearToken->IncludeOptionalField(H235_ClearToken::e_random);
  BYTE random = (BYTE)++sentRandomSequenceNumber;
  clearToken->m_random = (unsigned)random;

  // Generate an MD5 of the clear tokens PER encoding.
  PMessageDigest5 stomach;
  stomach.Process(&random, 1);
  stomach.Process(password);
  stomach.Process(&timeStamp, 4);
  PMessageDigest5::Code digest;
  stomach.Complete(digest);

  clearToken->IncludeOptionalField(H235_ClearToken::e_challenge);
  clearToken->m_challenge.SetValue((const BYTE *)&digest, sizeof(digest));

  return clearToken;
}


H235Authenticator::ValidationResult
        H235AuthCAT::ValidateClearToken(const H235_ClearToken & clearToken)
{
  if (!IsActive())
    return e_Disabled;

  if (clearToken.m_tokenOID != OID_CAT)
    return e_Absent;

  if (!clearToken.HasOptionalField(H235_ClearToken::e_generalID) ||
      !clearToken.HasOptionalField(H235_ClearToken::e_timeStamp) ||
      !clearToken.HasOptionalField(H235_ClearToken::e_random) ||
      !clearToken.HasOptionalField(H235_ClearToken::e_challenge)) {
    PTRACE(2, "H235RAS\tCAT requires generalID, timeStamp, random and challenge fields");
    return e_Error;
  }

  //first verify the timestamp
  PTime now;
  int deltaTime = now.GetTimeInSeconds() - clearToken.m_timeStamp;
  if (PABS(deltaTime) > timestampGracePeriod) {
    PTRACE(1, "H235RAS\tInvalid timestamp ABS(" << now.GetTimeInSeconds() << '-' 
           << (int)clearToken.m_timeStamp << ") > " << timestampGracePeriod);
    //the time has elapsed
    return e_InvalidTime;
  }

  //verify the randomnumber
  if (lastTimestamp == clearToken.m_timeStamp &&
      lastRandomSequenceNumber == clearToken.m_random) {
    //a message with this timespamp and the same random number was already verified
    PTRACE(1, "H235RAS\tConsecutive messages with the same random and timestamp");
    return e_ReplyAttack;
  }

  if (!remoteId && clearToken.m_generalID.GetValue() != remoteId) {
    PTRACE(1, "H235RAS\tGeneral ID is \"" << clearToken.m_generalID.GetValue()
           << "\", should be \"" << remoteId << '"');
    return e_Error;
  }

  int randomInt = clearToken.m_random;
  if (randomInt < -127 || randomInt > 255) {
    PTRACE(2, "H235RAS\tCAT requires single byte random field, got " << randomInt);
    return e_Error;
  }

  PUInt32b timeStamp = (DWORD)clearToken.m_timeStamp;
  BYTE randomByte = (BYTE)randomInt;

  // Generate an MD5 of the clear tokens PER encoding.
  PMessageDigest5 stomach;
  stomach.Process(&randomByte, 1);
  stomach.Process(password);
  stomach.Process(&timeStamp, 4);
  PMessageDigest5::Code digest;
  stomach.Complete(digest);

  if (clearToken.m_challenge.GetValue().GetSize() != sizeof(digest)) {
    PTRACE(2, "H235RAS\tCAT requires 16 byte challenge field");
    return e_Error;
  }

  if (memcmp((const BYTE *)clearToken.m_challenge.GetValue(), &digest, sizeof(digest)) == 0) {

    // save the values for the next call
    lastRandomSequenceNumber = clearToken.m_random;
    lastTimestamp = clearToken.m_timeStamp;
  
    return e_OK;
  }

  PTRACE(2, "H235RAS\tCAT hash does not match");
  return e_BadPassword;
}


PBoolean H235AuthCAT::IsCapability(const H235_AuthenticationMechanism & mechanism,
                                     const PASN_ObjectId & algorithmOID)
{
  if (mechanism.GetTag() != H235_AuthenticationMechanism::e_authenticationBES ||
         algorithmOID.AsString() != OID_CAT)
    return FALSE;

  const H235_AuthenticationBES & bes = mechanism;
  return bes.GetTag() == H235_AuthenticationBES::e_radius;
}


PBoolean H235AuthCAT::SetCapability(H225_ArrayOf_AuthenticationMechanism & mechanisms,
                                H225_ArrayOf_PASN_ObjectId & algorithmOIDs)
{
  if (!AddCapability(H235_AuthenticationMechanism::e_authenticationBES, OID_CAT,
                     mechanisms, algorithmOIDs))
    return FALSE;

  H235_AuthenticationBES & bes = mechanisms[mechanisms.GetSize()-1];
  bes.SetTag(H235_AuthenticationBES::e_radius);
  return TRUE;
}


PBoolean H235AuthCAT::IsSecuredPDU(unsigned rasPDU, PBoolean received) const
{
  switch (rasPDU) {
    case H225_RasMessage::e_registrationRequest :
    case H225_RasMessage::e_admissionRequest :
      return received ? !remoteId.IsEmpty() : !localId.IsEmpty();

    default :
      return FALSE;
  }
}

// need to load into factory here otherwise doesn't load...
#if PTLIB_VER < 2110 && defined(H323_SSL)
static PFactory<H235Authenticator>::Worker<H2351_Authenticator> factoryH2351_Authenticator("H2351_Authenticator");
#endif

/////////////////////////////////////////////////////////////////////////////
