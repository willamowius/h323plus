/*
 * h2356.cxx
 *
 * H.235.6 Encryption class.
 *
 * h323plus library
 *
 * Copyright (c) 2011 Spranto Australia Pty Ltd.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the General Public License (the  "GNU License"), in which case the
 * provisions of GNU License are applicable instead of those
 * above. If you wish to allow use of your version of this file only
 * under the terms of the GNU License and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GNU License. If you do not delete
 * the provisions above, a recipient may use your version of this file
 * under either the MPL or the GNU License."
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte. Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 *
 */


#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

#include "h235/h2356.h"
#include "h235/h2351.h"
#include "h323con.h"
#include "ptclib/cypher.h"
#include <algorithm>

extern "C" {
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
};

////////////////////////////////////////////////////////////////////////////////////
// Diffie Hellman

H235_DiffieHellman::H235_DiffieHellman(const PConfig  & dhFile, const PString & section)
: dh(NULL), m_remKey(NULL), m_toSend(true), m_keySize(0), m_loadFromFile(false)
{
    if (Load(dhFile,section))
      m_keySize = BN_num_bytes(dh->pub_key);
}

H235_DiffieHellman::H235_DiffieHellman(const BYTE * pData, PINDEX pSize,
                                     const BYTE * gData, PINDEX gSize, 
                                     PBoolean send)
: m_remKey(NULL), m_toSend(send), m_keySize(pSize), m_loadFromFile(false)
{
  dh = DH_new();
  if (dh == NULL)
    return;

  dh->p = BN_bin2bn(pData, pSize, NULL);
  dh->g = BN_bin2bn(gData, gSize, NULL);
  if (dh->p != NULL && dh->g != NULL)
    return;

  DH_free(dh);
  dh = NULL;
}


H235_DiffieHellman::H235_DiffieHellman(const H235_DiffieHellman & diffie)
{
   dh = DHparams_dup(diffie);
}


H235_DiffieHellman & H235_DiffieHellman::operator=(const H235_DiffieHellman & diffie)
{
  if (dh != NULL)
    DH_free(dh);
   
   dh = DHparams_dup(diffie);

  return *this;
}

H235_DiffieHellman::~H235_DiffieHellman()
{
  if (dh != NULL)
    DH_free(dh);
}

PBoolean H235_DiffieHellman::CheckParams() const
{

    // TODO: FIX so it actually checks the whole DH 
    // including the strength of the DH Public/Private key pair - SH

 PWaitAndSignal m(vbMutex);

 int i;
 if (!DH_check(dh,&i))
 {
    switch (i) {
     case DH_CHECK_P_NOT_PRIME:
         PTRACE(4,"H235_DH\tCHECK: p value is not prime");
     case DH_CHECK_P_NOT_SAFE_PRIME:
         PTRACE(4,"H235_DH\tCHECK: p value is not a safe prime");
     case DH_UNABLE_TO_CHECK_GENERATOR:
         PTRACE(4,"H235_DH\tCHECK: unable to check the generator value");
     case DH_NOT_SUITABLE_GENERATOR:
         PTRACE(4,"H235_DH\tCHECK: the g value is not a generator");
    }
    return FALSE;
 }

  return TRUE;
}

void H235_DiffieHellman::Encode_P(PASN_BitString & p) const
{
    PWaitAndSignal m(vbMutex);

    if (!m_toSend)
        return;

    unsigned char * data= (unsigned char *)OPENSSL_malloc(BN_num_bytes(dh->p) + 20); // TODO: why + 20 ??
    if (data != NULL) {
        if (BN_bn2bin(dh->p, data) > 0)
            p.SetData(BN_num_bits(dh->p), data);
    }
    OPENSSL_free(data);
}

void H235_DiffieHellman::Decode_P(const PASN_BitString & p)
{
    PWaitAndSignal m(vbMutex);

    if (p.GetSize() == 0)
        return;

    const unsigned char *data = p.GetDataPointer();
    dh->p=BN_bin2bn(data,sizeof(data), NULL);
}

void H235_DiffieHellman::Encode_G(PASN_BitString & g) const
{
    PWaitAndSignal m(vbMutex);

    if (!m_toSend)
        return;

    int len_p = BN_num_bytes(dh->p);
    int len_g = BN_num_bytes(dh->g);
    int bits_p = BN_num_bits(dh->p);

    // G is padded out to the length of P
    unsigned char * data = (unsigned char *)OPENSSL_malloc(len_p + 20); // TODO: why + 20 ??
    memset(data, 0, len_p);
    if (data != NULL) {
         if (BN_bn2bin(dh->g, data + len_p - len_g) > 0)
             g.SetData(bits_p, data);
    }
    OPENSSL_free(data);
}

void H235_DiffieHellman::Decode_G(const PASN_BitString & g)
{
    PWaitAndSignal m(vbMutex);

    if (g.GetSize() == 0)
        return;

    // TODO: Valgrind says "invalid read of size 1", why ?
    dh->g = BN_bin2bn(g.GetDataPointer(), g.GetSize(), NULL);
}


void H235_DiffieHellman::Encode_HalfKey(PASN_BitString & hk) const
{
    PWaitAndSignal m(vbMutex);

    int len = BN_num_bytes(dh->pub_key);
    int bits_key = BN_num_bits(dh->pub_key);
    // TODO Verify that the halfkey is being packed properly - SH
    unsigned char * data = (unsigned char *)OPENSSL_malloc(len + 20); // TODO: why + 20 ??
    if (data != NULL){
        if (BN_bn2bin(dh->pub_key, data) > 0)
            hk.SetData(bits_key, data);
    }
    OPENSSL_free(data);
}

void H235_DiffieHellman::Decode_HalfKey(const PASN_BitString & hk)
{
    PWaitAndSignal m(vbMutex);

    const unsigned char *data = hk.GetDataPointer();
    dh->pub_key = BN_bin2bn(data, sizeof(data), NULL);   
}

void H235_DiffieHellman::SetRemoteKey(bignum_st * remKey)
{
    m_remKey = remKey;
}

PBoolean H235_DiffieHellman::GenerateHalfKey()
{
    PWaitAndSignal m(vbMutex);

    if (m_loadFromFile)
        return true;

    // TODO check if half key is generated correctly - SH - looks OK - JW
    if (!DH_generate_key(dh)) {
        char buf[256];
        ERR_error_string(ERR_get_error(), buf);
        PTRACE(4, "H235_DH\tERROR DH Halfkey " << buf);
        return FALSE;
    }

    return TRUE;
}

PBoolean H235_DiffieHellman::Load(const PConfig  & dhFile, const PString & section)
{
  if (dh != NULL) {
    DH_free(dh);
    dh = NULL;
  }

  dh = DH_new();
  if (dh == NULL)
    return false;

    PString str = PString();
    PBYTEArray data;

    PBoolean ok =true;
    if (dhFile.HasKey(section, "PRIME")) {
        str = dhFile.GetString(section, "PRIME", "");
        PBase64::Decode(str, data);
        dh->p = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    } else 
        ok = false;

    if (dhFile.HasKey(section, "GENERATOR")) {
        str = dhFile.GetString(section, "GENERATOR", "");
        PBase64::Decode(str, data);
        PBYTEArray temp(1);
        memcpy(temp.GetPointer(), data.GetPointer(), 1);
        memset(data.GetPointer(), 0, data.GetSize());
        memcpy(data.GetPointer() + data.GetSize()-1, temp.GetPointer(),1);
        dh->g = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    } else 
        ok = false;

    if (dhFile.HasKey(section, "PUBLIC")) {
        str = dhFile.GetString(section, "PUBLIC", "");
        PBase64::Decode(str, data);
        dh->pub_key = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    } else 
        ok = false;
  
    if (dhFile.HasKey(section, "PRIVATE")) {
        str = dhFile.GetString(section, "PRIVATE", "");
        PBase64::Decode(str, data);
        dh->priv_key = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    } else 
        ok = false;

    if (ok /*&& CheckParams()*/) 
       m_loadFromFile = true;
    else {
      DH_free(dh);
      dh = NULL;
    } 
    
    return m_loadFromFile;
}

PBoolean H235_DiffieHellman::LoadedFromFile() 
{
    return m_loadFromFile;
}

PBoolean H235_DiffieHellman::Save(const PFilePath & dhFile, const PString & oid)
{
  if (!dh && dh->pub_key)
    return false;

  PConfig config(dhFile, oid);
    PString str = PString();
    int len = BN_num_bytes(dh->pub_key);
    unsigned char * data = (unsigned char *)OPENSSL_malloc(len); 

    if (data != NULL && BN_bn2bin(dh->p, data) > 0) {
        str = PBase64::Encode(data, len, "");
        config.SetString("PRIME",str);
    }
    OPENSSL_free(data);

    data = (unsigned char *)OPENSSL_malloc(len); 
    if (data != NULL && BN_bn2bin(dh->g, data) > 0) {
        str = PBase64::Encode(data, len, "");
        config.SetString("GENERATOR",str);
    }
    OPENSSL_free(data);

    data = (unsigned char *)OPENSSL_malloc(len); 
    if (data != NULL && BN_bn2bin(dh->pub_key, data) > 0) {
        str = PBase64::Encode(data, len, "");
        config.SetString("PUBLIC",str);
    }
    OPENSSL_free(data);

    data = (unsigned char *)OPENSSL_malloc(len); 
    if (data != NULL && BN_bn2bin(dh->priv_key, data) > 0) {
        PString str = PBase64::Encode(data, len, "");
        config.SetString("PRIVATE",str);
    }
    OPENSSL_free(data);
   return true;
}

PBoolean H235_DiffieHellman::ComputeSessionKey(PBYTEArray & SessionKey)
{
    SessionKey.SetSize(0);
    if (!m_remKey) {
        PTRACE(2, "H235_DH\tERROR Generating Shared DH: No remote key!");
        return false;
    }

    int len = DH_size(dh);
    unsigned char * buf = (unsigned char *)OPENSSL_malloc(len);

    int out = DH_compute_key(buf, m_remKey, dh);
    if (out <= 0) {
        PTRACE(2,"H235_DH\tERROR Generating Shared DH!");
        OPENSSL_free(buf);
        return false;
    }

    SessionKey.SetSize(out);
    memcpy(SessionKey.GetPointer(), (void *)buf, out);

    OPENSSL_free(buf);

    return true;
}

bignum_st * H235_DiffieHellman::GetPublicKey() const
{
    return dh->pub_key;
}

int H235_DiffieHellman::GetKeyLength() const
{
    return m_keySize;
}

////////////////////////////////////////////////////////////////////////////////////
// Helper functions

template <class PAIR>
class deletepair { // PAIR::second_type is a pointer type
public:
    void operator()(const PAIR & p) { if (p.second) delete p.second; }
};

template <class M>
inline void DeleteObjectsInMap(const M & m)
{
    typedef typename M::value_type PAIR;
    std::for_each(m.begin(), m.end(), deletepair<PAIR>());
}

void LoadDiffieHellmanMap(std::map<PString, H235_DiffieHellman*> & dhmap, const PString & filePath = PString())
{
    PStringArray FilePaths;

    PINDEX k=0;
    if (!filePath.IsEmpty()) {
      PStringArray temp = filePath.Tokenise(';');
      for (k=0; k < temp.GetSize(); ++k) {
          PFilePath dhFile = PString(temp[k]);
          if (PFile::Exists(dhFile)) 
              FilePaths.AppendString(dhFile);
      }
    } else {
      char * env = ::getenv("H323_H235_DH");
      if (env != NULL) {
        const char * token = strtok(env, ";");
          while (token != NULL) {
            PFilePath dhFile = PString(token);
            if (PFile::Exists(dhFile)) 
              FilePaths.AppendString(dhFile);
            else
              PTRACE(1, "Error: DH key file not found: " << dhFile);
            token = strtok(NULL, ";");
          }
      }
    }
  
    int i=0;
    for (PINDEX k=0; k < FilePaths.GetSize(); ++k) {
        PConfig cfg(FilePaths[k],PString());
        PStringArray oidList(cfg.GetSections());
        for (PINDEX j=0; j < oidList.GetSize(); ++j) {  
            H235_DiffieHellman * dh = new H235_DiffieHellman(cfg,oidList[j]);
            if (dh->LoadedFromFile()) {
                dhmap.insert(pair<PString, H235_DiffieHellman*>(oidList[j], dh));
                i++;
            } else {
                PTRACE(1, "Error: Loading DH key file failed");
                delete dh;
            }
        }
    }
    if (i) {
        dhmap.insert(pair<PString, H235_DiffieHellman*>(OID_H235V3,NULL));
        return;
    }

    for (PINDEX i = 0; i < PARRAYSIZE(H235_DHParameters); ++i) {
        if (H235_DHParameters[i].sz > 0) {
           dhmap.insert(pair<PString, H235_DiffieHellman*>(H235_DHParameters[i].parameterOID,
                  new H235_DiffieHellman(H235_DHParameters[i].dh_p, H235_DHParameters[i].sz,
                                         H235_DHParameters[i].dh_g, H235_DHParameters[i].sz,
                                         H235_DHParameters[i].send)) );
        } else {
           dhmap.insert(pair<PString, H235_DiffieHellman*>(H235_DHParameters[i].parameterOID,NULL));
        }
    }

}

/////////////////////////////////////////////////////////////////////////////////////

#if PTLIB_VER >= 2110
H235SECURITY(Std6);
#else
static PFactory<H235Authenticator>::Worker<H2356_Authenticator> factoryH2356_Authenticator("H2356_Authenticator");
#endif

H2356_Authenticator::H2356_Authenticator()
: m_enabled(true), m_active(true), m_tokenState(e_clearNone)
{
  usage = MediaEncryption;
  m_algOIDs.SetSize(0);
  LoadDiffieHellmanMap(m_dhLocalMap, H235Authenticators::GetDHParameterFile());
  InitialiseSecurity(); // make sure m_algOIDs gets filled
}

H2356_Authenticator::~H2356_Authenticator()
{
    DeleteObjectsInMap(m_dhLocalMap);
    DeleteObjectsInMap(m_dhRemoteMap);
}

PStringArray H2356_Authenticator::GetAuthenticatorNames()
{
    return PStringArray("Std6");
}

#if PTLIB_VER >= 2110
PBoolean H2356_Authenticator::GetAuthenticationCapabilities(H235Authenticator::Capabilities * ids)
{
    for (PINDEX i = 0; i < PARRAYSIZE(H235_Encryptions); ++i) {
      H235Authenticator::Capability cap;
        cap.m_identifier = H235_Encryptions[i].algorithmOID;
        cap.m_cipher     = H235_Encryptions[i].sslDesc;
        cap.m_description= H235_Encryptions[i].desc;
        ids->capabilityList.push_back(cap);
    }
    return true;
}
#endif

PBoolean H2356_Authenticator::IsMatch(const PString & identifier) const 
{ 
    PStringArray ids;
    for (PINDEX i = 0; i < PARRAYSIZE(H235_DHParameters); ++i) {
        if (PString(H235_DHParameters[i].parameterOID) == identifier)
               return true;
    }
    return false; 
}


const char * H2356_Authenticator::GetName() const
{
    return "H.235.6";
}

PBoolean H2356_Authenticator::PrepareTokens(PASN_Array & clearTokens,
                                      PASN_Array & /*cryptoTokens*/)
{
  if (!IsActive() || (m_tokenState == e_clearDisable))
    return FALSE;

  H225_ArrayOf_ClearToken & tokens = (H225_ArrayOf_ClearToken &)clearTokens;

  std::map<PString, H235_DiffieHellman*>::iterator i = m_dhLocalMap.begin();
  while (i != m_dhLocalMap.end()) {
      int sz = tokens.GetSize();
      tokens.SetSize(sz+1);
      H235_ClearToken & clearToken = tokens[sz];
      clearToken.m_tokenOID = i->first;
      H235_DiffieHellman * m_dh = i->second;
      if (m_dh && m_dh->GenerateHalfKey()) {
#if 0  // For testing to generate a strong key pair - SH
        if (!m_dh->LoadedFromFile())
          m_dh->Save("test.pem",i->first);
#endif
          clearToken.IncludeOptionalField(H235_ClearToken::e_dhkey);
          H235_DHset & dh = clearToken.m_dhkey;
          m_dh->Encode_HalfKey(dh.m_halfkey);
          m_dh->Encode_P(dh.m_modSize);
          m_dh->Encode_G(dh.m_generator);
      }
      i++;
  }

  if (m_tokenState == e_clearNone) {
      m_tokenState = e_clearSent;
      return true;
  }

  if (m_tokenState == e_clearReceived) {
      m_tokenState = e_clearComplete;
      InitialiseSecurity();
  }

  return true;
}

H235Authenticator::ValidationResult H2356_Authenticator::ValidateTokens(const PASN_Array & clearTokens,
                                   const PASN_Array & /*cryptoTokens*/, const PBYTEArray & /*rawPDU*/)
{
   if (!IsActive() || (m_tokenState == e_clearDisable))
       return e_Disabled;

   const H225_ArrayOf_ClearToken & tokens = (const H225_ArrayOf_ClearToken &)clearTokens;
   if (tokens.GetSize() == 0) {
      DeleteObjectsInMap(m_dhLocalMap);
      m_tokenState = e_clearDisable;
      return e_Disabled; 
   }
  
  std::map<PString, H235_DiffieHellman*>::iterator it = m_dhLocalMap.begin();
  while (it != m_dhLocalMap.end()) {
      PBoolean found = false;
       for (PINDEX i = 0; i < tokens.GetSize(); ++i) {
          const H235_ClearToken & token = tokens[i];
          PString tokenOID = token.m_tokenOID.AsString();
          if (it->first == tokenOID && it->second != NULL ) {
              H235_DiffieHellman* m_dh = new H235_DiffieHellman(*it->second);
               const H235_DHset & dh = token.m_dhkey;
               m_dh->Decode_HalfKey(dh.m_halfkey);
               if (dh.m_modSize.GetSize() > 0) {
                   m_dh->Decode_P(dh.m_modSize);
                   m_dh->Decode_G(dh.m_generator);
               }
              m_dhRemoteMap.insert(pair<PString, H235_DiffieHellman*>(tokenOID,m_dh));
              found = true;
              break;
          }
       }
       if (!found) {
          delete it->second;
          m_dhLocalMap.erase(it++);
       } else
          it++;
  }

  if (m_dhLocalMap.size() == 0) {
      m_tokenState = e_clearDisable;
      return e_Disabled;
  }
  
  if (m_tokenState == e_clearNone) {
      m_tokenState = e_clearReceived;
      return e_OK;
  }

  if (m_tokenState == e_clearSent) {
      m_tokenState = e_clearComplete;
      InitialiseSecurity();
  }

   return e_OK;
}

PBoolean H2356_Authenticator::IsSecuredSignalPDU(unsigned signalPDU, PBoolean /*received*/) const
{
  switch (signalPDU) {
      case H225_H323_UU_PDU_h323_message_body::e_setup:
      case H225_H323_UU_PDU_h323_message_body::e_connect:
          return enabled;
      default :
         return false;
  }
}

PBoolean H2356_Authenticator::IsCapability(const H235_AuthenticationMechanism & /*mechansim*/, const PASN_ObjectId & /*algorithmOID*/)
{
    return false;
}

PBoolean H2356_Authenticator::SetCapability(H225_ArrayOf_AuthenticationMechanism & /*mechansim*/, H225_ArrayOf_PASN_ObjectId & /*algorithmOIDs*/)
{
    return false;
}

PBoolean H2356_Authenticator::IsActive() const
{
    return m_active;
}

void H2356_Authenticator::Disable()
{ 
    m_enabled = false;
    m_active = false;
}

void H2356_Authenticator::InitialiseSecurity()
{
  PString dhOID;
  int lastKeyLength = 0;
  std::map<PString, H235_DiffieHellman*>::iterator i = m_dhLocalMap.begin();
  while (i != m_dhLocalMap.end()) {
      if (i->second && i->second->GetKeyLength() > lastKeyLength) {
          dhOID = i->first;
          lastKeyLength = i->second->GetKeyLength();
      }
    i++;
  }

  if (dhOID.IsEmpty())
      return;

  m_algOIDs.SetSize(0);
  for (PINDEX i=0; i<PARRAYSIZE(H235_Algorithms); ++i) {
      if (PString(H235_Algorithms[i].DHparameters) == dhOID)
           m_algOIDs.AppendString(H235_Algorithms[i].algorithm);
  }

  std::map<PString, H235_DiffieHellman*>::iterator l = m_dhLocalMap.find(dhOID);
  std::map<PString, H235_DiffieHellman*>::iterator r = m_dhRemoteMap.find(dhOID);

  if (l == m_dhLocalMap.end() || r == m_dhRemoteMap.end())
      return;

  l->second->SetRemoteKey(r->second->GetPublicKey());
  if (connection && (m_algOIDs.GetSize() > 0)) {
      H235Capabilities * localCaps = (H235Capabilities *)connection->GetLocalCapabilitiesRef();
      localCaps->SetDHKeyPair(m_algOIDs,l->second,connection->IsH245Master());
  }
}

PBoolean H2356_Authenticator::GetMediaSessionInfo(PString & algorithmOID, PBYTEArray & sessionKey)
{
  InitialiseSecurity();
  PTRACE(0, "JW GetMediaSessionInfo #algos=" << m_algOIDs.GetSize() << " #localMap=" << m_dhLocalMap.size());

  if (m_algOIDs.GetSize() == 0) {
      PTRACE(1, "H235\tNo algorithms available");
      return false;
  }

  PString DhOID = GetDhOIDFromAlg(m_algOIDs[0]);
  std::map<PString, H235_DiffieHellman*>::const_iterator l = m_dhLocalMap.find(DhOID);
  if (l != m_dhLocalMap.end()) {
     algorithmOID = m_algOIDs[0];
     return l->second->ComputeSessionKey(sessionKey);
  }
  return false;
}

PBoolean H2356_Authenticator::GetAlgorithms(PStringList & algorithms) const
{
  algorithms = m_algOIDs;
  return (m_algOIDs.GetSize() > 0);
}

PBoolean H2356_Authenticator::GetAlgorithmDetails(const PString & algorithm, PString & sslName, PString & description)
{
  for (PINDEX i=0; i<PARRAYSIZE(H235_Encryptions); ++i) {
    if (PString(H235_Encryptions[i].algorithmOID) == algorithm) {
      sslName     = H235_Encryptions[i].sslDesc;
      description = H235_Encryptions[i].desc;
      return true;
    }
  }
  return false;
}

PString H2356_Authenticator::GetAlgFromOID(const PString & oid)
{
    if (oid.IsEmpty())
        return PString();

    for (PINDEX i = 0; i < PARRAYSIZE(H235_Encryptions); ++i) {
        if (PString(H235_Encryptions[i].algorithmOID) == oid)
            return H235_Encryptions[i].sslDesc;
    }
    return PString();
}

PString H2356_Authenticator::GetOIDFromAlg(const PString & sslName)
{
    if (sslName.IsEmpty())
        return PString();

    for (PINDEX i = 0; i < PARRAYSIZE(H235_Encryptions); ++i) {
        if (H235_Encryptions[i].sslDesc == sslName)
            return PString(H235_Encryptions[i].algorithmOID);
    }
    return PString();
}

PString H2356_Authenticator::GetDhOIDFromAlg(const PString & alg)
{
    if (alg.IsEmpty())
        return PString();

    for (PINDEX i = 0; i < PARRAYSIZE(H235_Algorithms); ++i) {
        if (PString(H235_Algorithms[i].algorithm) == alg)
            return H235_Algorithms[i].DHparameters;
    }
    return PString();
}

void H2356_Authenticator::ExportParameters(const PFilePath & path)
{
  std::map<PString, H235_DiffieHellman*>::iterator i = m_dhLocalMap.begin();
  while (i != m_dhLocalMap.end()) {
      if (i->second && i->second->GetKeyLength() > 0) {
          i->second->Save(path,i->first);
      }
    i++;
  }
}

#endif  // H323_H235

