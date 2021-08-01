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

#include <h235auth.h>
#include "h235/h2356.h"
#include "h235/h235support.h"
#include "h323con.h"
#include <algorithm>


////////////////////////////////////////////////////////////////////////////////////
// Helper functions

template <class PAIR>
class deletepair { // PAIR::second_type is a pointer type
public:
    void operator()(const PAIR & p) { if (p.second) delete p.second; }
};

template <class M>
inline void DeleteObjectsInMap(M & m)
{
    typedef typename M::value_type PAIR;
    std::for_each(m.begin(), m.end(), deletepair<PAIR>());
	m.clear(); // delete pointers to deleted objects
}

PBoolean IsSupportedOID(const PString & oid, unsigned cipherlength)
{
    // Check key length
    for (PINDEX i = 0; i < PARRAYSIZE(H235_DHCustom); ++i) {
        if (H235_DHCustom[i].parameterOID == oid) {
            if (H235_DHCustom[i].sz <= (cipherlength * 8))
                return true;
            else
                return false;
        }
    }
    return false;
}

void LoadH235_DHMap(H235_DHMap & dhmap, H235_DHMap & dhcache, H235Authenticators::DH_DataList & customData, const PString & filePath = PString(), PINDEX cipherlength = P_MAX_INDEX, PINDEX maxTokenLength = 1024)
{
    if (!dhcache.empty()) {
        H235_DHMap::iterator i = dhcache.begin();
        while (i != dhcache.end()) {
            if (i->second)
                dhmap.insert(pair<PString, H235_DiffieHellman*>(i->first, (H235_DiffieHellman*)i->second->Clone()));
            else
                dhmap.insert(pair<PString, H235_DiffieHellman*>(i->first, (H235_DiffieHellman*)NULL));
            ++i;
        }
        return;
    }

    // Load from memory vendor supplied keys
    if (!customData.empty()) {
        H235Authenticators::DH_DataList::iterator r;
        for (r = customData.begin(); r != customData.end(); ++r) {
            if (IsSupportedOID(r->m_OID, cipherlength)) {
                dhmap.insert(pair<PString, H235_DiffieHellman*>(r->m_OID,
                    new H235_DiffieHellman(r->m_pData.GetPointer(), r->m_pData.GetSize(),
                        r->m_gData, r->m_gData.GetSize(), true)));
                PTRACE(6, "H2356\tMemory KeyPair " << r->m_OID << " loaded.");
            } else {
                PTRACE(6, "H2356\tMemory KeyPair " << r->m_OID << " ignored.");
            }
        }
    }

    // Load from vendor supplied filePath
    PStringArray FilePaths;
    if (!filePath.IsEmpty()) {
      PStringArray temp = filePath.Tokenise(';');
      for (PINDEX k = 0; k < temp.GetSize(); ++k) {
        PFilePath dhFile = PString(temp[k]);
        if (PFile::Exists(dhFile)) {
            FilePaths.AppendString(dhFile);
        }
      }
    }

    // Load default DH keypairs
    for (PINDEX k = 0; k < FilePaths.GetSize(); ++k) {
        PConfig cfg(FilePaths[k], PString());
        PStringArray oidList(cfg.GetSections());
        for (PINDEX j = 0; j < oidList.GetSize(); ++j) {
            if (IsSupportedOID(oidList[j], cipherlength)) {
                H235_DiffieHellman * dh = new H235_DiffieHellman(cfg, oidList[j]);
                if (dh->LoadedFromFile()) {
                    dhmap.insert(pair<PString, H235_DiffieHellman*>(oidList[j], dh));
                    PTRACE(6, "H2356\tFile KeyPair " << oidList[j] << " loaded.");
                } else {
                    PTRACE(1, "H2356\tError: Loading DH key file failed");
                    delete dh;
                }
            } else {
                PTRACE(6, "H2356\tFile KeyPair " << oidList[j] << " ignored.");
            }
        }
    }

    // if not loaded from File then create.
    for (PINDEX i = 0; i < PARRAYSIZE(H235_DHParameters); ++i) {
      if (dhmap.find(H235_DHParameters[i].parameterOID) == dhmap.end()) {
        if (H235_DHParameters[i].sz > 0 && H235_DHParameters[i].cipher <= (unsigned)cipherlength
            && (H235_DHParameters[i].dh_p == NULL || (H235_DHParameters[i].sz * 8) <= (unsigned)maxTokenLength)) {
           dhmap.insert(pair<PString, H235_DiffieHellman*>(H235_DHParameters[i].parameterOID,
                  new H235_DiffieHellman(H235_DHParameters[i].dh_p, H235_DHParameters[i].sz,
                                         H235_DHParameters[i].dh_g, H235_DHParameters[i].sz,
                                         H235_DHParameters[i].send)) );
           PTRACE(6, "H2356\tStd KeyPair " << H235_DHParameters[i].parameterOID << " loaded.");
        } else if (H235_DHParameters[i].cipher == 0) {
           dhmap.insert(pair<PString, H235_DiffieHellman*>(H235_DHParameters[i].parameterOID, (H235_DiffieHellman*)NULL));
           PTRACE(6, "H2356\tStd KeyPair " << H235_DHParameters[i].parameterOID << " loaded.");
        } else {
           PTRACE(6, "H2356\tStd KeyPair " << H235_DHParameters[i].parameterOID << " ignored.");
        }
      }
    }

}

/////////////////////////////////////////////////////////////////////////////////////

#if PTLIB_VER >= 2110
#ifdef H323_SSL
H235SECURITY(Std6);
#endif
#else
static PFactory<H235Authenticator>::Worker<H2356_Authenticator> factoryH2356_Authenticator("H2356_Authenticator");
#endif

H235_DHMap H2356_Authenticator::m_dhCachedMap;

H2356_Authenticator::H2356_Authenticator()
: m_tokenState(e_clearNone)
{
    usage = MediaEncryption;

    m_enabled = (H235Authenticators::GetEncryptionPolicy() > 0);
    m_active = m_enabled;

    m_algOIDs.SetSize(0);
    if (m_enabled) {
        LoadH235_DHMap(m_dhLocalMap, m_dhCachedMap, H235Authenticators::GetDHDataList(), H235Authenticators::GetDHParameterFile(), H235Authenticators::GetMaxCipherLength(), H235Authenticators::GetMaxTokenLength());
        InitialiseSecurity(); // make sure m_algOIDs gets filled
    }
}

H2356_Authenticator::~H2356_Authenticator()
{
    DeleteObjectsInMap(m_dhLocalMap);
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

void H2356_Authenticator::InitialiseCache(int cipherlength, unsigned maxTokenLength)
{
   LoadH235_DHMap(m_dhCachedMap, m_dhCachedMap, H235Authenticators::GetDHDataList(), H235Authenticators::GetDHParameterFile(), cipherlength, maxTokenLength);
}

void H2356_Authenticator::RemoveCache()
{
   DeleteObjectsInMap(m_dhCachedMap);
   m_dhCachedMap.clear();
}

PBoolean H2356_Authenticator::IsMatch(const PString & identifier) const
{
    PStringArray ids;
    for (PINDEX i = 0; i < PARRAYSIZE(H235_DHParameters); ++i) {
        if (PString(H235_DHParameters[i].parameterOID) == identifier)
            return true;
    }
    return false;
}


PObject * H2356_Authenticator::Clone() const
{
    H2356_Authenticator * auth = new H2356_Authenticator(*this);

    // We do NOT copy these fields in Clone()
    auth->lastRandomSequenceNumber = 0;
    auth->lastTimestamp = 0;

    return auth;
}

const char * H2356_Authenticator::GetName() const
{
    return "H.235.6";
}

PBoolean H2356_Authenticator::PrepareTokens(PASN_Array & clearTokens,
                                      PASN_Array & /*cryptoTokens*/,
                                      PINDEX max_keyLength)
{
    if (!IsActive() || (m_tokenState == e_clearDisable) || (max_keyLength==0))
        return FALSE;

    H225_ArrayOf_ClearToken & tokens = (H225_ArrayOf_ClearToken &)clearTokens;

    H235_DHMap::iterator i = m_dhLocalMap.begin();
    while (i != m_dhLocalMap.end()) {
        H235_DiffieHellman * dh = i->second;
        if (dh && (dh->GetKeyLength() > (max_keyLength/8))) {
            ++i; continue;
        }

        // Build the token
        int sz = tokens.GetSize();
        tokens.SetSize(sz+1);
        H235_ClearToken & clearToken = tokens[sz];
        clearToken.m_tokenOID = i->first;
        if (dh && dh->GenerateHalfKey()) {
            if (dh->GetKeySize() <= 256) {  // Key Size 2048 or smaller
                clearToken.IncludeOptionalField(H235_ClearToken::e_dhkey);
                H235_DHset & dhkey = clearToken.m_dhkey;
                dh->Encode_HalfKey(dhkey.m_halfkey);
                dh->Encode_P(dhkey.m_modSize);
                dh->Encode_G(dhkey.m_generator);
            } else { // Key Size > 2048
                clearToken.IncludeOptionalField(H235_ClearToken::e_dhkeyext);
                H235_DHsetExt & dhkey = clearToken.m_dhkeyext;
                dh->Encode_HalfKey(dhkey.m_halfkey);
                if (dh->Encode_P(dhkey.m_modSize))
                    dhkey.IncludeOptionalField(H235_DHsetExt::e_modSize);
                if (dh->Encode_G(dhkey.m_generator))
                    dhkey.IncludeOptionalField(H235_DHsetExt::e_generator);
            }
        }
        ++i;
    }

    if (m_tokenState == e_clearNone) {
        m_tokenState = e_clearSent;
        return true;
    }

    if (m_tokenState == e_clearReceived) {
        InitialiseSecurity();
        m_tokenState = e_clearComplete;
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

    PString selectOID;
    H235_DHMap::iterator it;
    for (PINDEX i = 0; i < tokens.GetSize(); ++i) {
        for (it = m_dhLocalMap.begin(); it != m_dhLocalMap.end(); ++it) {
            const H235_ClearToken & token = tokens[i];
            PString tokenOID = token.m_tokenOID.AsString();
            if (it->first == tokenOID) {
                if (it->second != NULL) {
                    if (token.HasOptionalField(H235_ClearToken::e_dhkey)) {  // For keysize up to and including 2048
                        const H235_DHset & dh = token.m_dhkey;
                        it->second->SetRemoteHalfKey(dh.m_halfkey);
                        if (!m_tokenState && dh.m_modSize.GetSize() > 0)  	// replace p and g if included and received first
                            it->second->SetDHReceived(dh.m_modSize,dh.m_generator);

                    } else if (token.HasOptionalField(H235_ClearToken::e_dhkeyext)) {  // For keysize greater then 2048
                        const H235_DHsetExt & dh = token.m_dhkeyext;
                        it->second->SetRemoteHalfKey(dh.m_halfkey);
                        if (!m_tokenState && dh.HasOptionalField(H235_DHsetExt::e_modSize) &&   // replace p and g if included if received first
                                             dh.HasOptionalField(H235_DHsetExt::e_generator))
                            it->second->SetDHReceived(dh.m_modSize,dh.m_generator);

                    } else {
                        PTRACE(2, "H2356\tERROR DH Parameters missing " << it->first << " skipping.");
                        continue;
                    }
                    selectOID = it->first;
                    PTRACE(4, "H2356\tSetting Encryption Algorithm for call " << selectOID);
                }
            }
            if (!selectOID) break;
        }
        if (!selectOID) break;
    }

    if (!selectOID) {
        // Remove unmatched authenticators
        it = m_dhLocalMap.begin();
        while (it != m_dhLocalMap.end()) {
            if (it->second && selectOID != it->first) {
                PTRACE(4, "H2356\tRemoving unmatched Encryption Algorithm " << it->first);
                delete it->second;
                m_dhLocalMap.erase(it++);
            } else
                ++it;
        }
    } else
        DeleteObjectsInMap(m_dhLocalMap);

    if (m_dhLocalMap.size() == 0) {
        PTRACE(4, "H2356\tNo matching Encryption Algorithms. Encryption Disabled!");
        m_tokenState = e_clearDisable;
        return e_Absent;
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

PBoolean H2356_Authenticator::IsSecuredPDU(unsigned /*rasPDU*/, PBoolean /*received*/) const
{
    return false;
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
  H235_DHMap::iterator i = m_dhLocalMap.begin();
  while (i != m_dhLocalMap.end()) {
      if (i->second && i->second->GetKeyLength() > lastKeyLength) {
          dhOID = i->first;
          lastKeyLength = i->second->GetKeyLength();
      }
    ++i;
  }

  if (dhOID.IsEmpty())
      return;

  m_algOIDs.SetSize(0);
  for (PINDEX i=0; i<PARRAYSIZE(H235_Algorithms); ++i) {
      if (PString(H235_Algorithms[i].DHparameters) == dhOID)
           m_algOIDs.AppendString(H235_Algorithms[i].algorithm);
  }

  H235_DHMap::iterator l = m_dhLocalMap.find(dhOID);

  if (connection && l != m_dhLocalMap.end()) {
      H235Capabilities * localCaps = (H235Capabilities *)connection->GetLocalCapabilitiesRef();
      localCaps->SetDHKeyPair(m_algOIDs,l->second,connection->IsH245Master());
  }
}

PBoolean H2356_Authenticator::GetMediaSessionInfo(PString & algorithmOID, PBYTEArray & sessionKey)
{

  if (m_algOIDs.GetSize() == 0) {
      PTRACE(1, "H235\tNo algorithms available");
      return false;
  }

  PString DhOID = GetDhOIDFromAlg(m_algOIDs[0]);
  H235_DHMap::const_iterator l = m_dhLocalMap.find(DhOID);
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
  H235_DHMap::iterator i = m_dhLocalMap.begin();
  while (i != m_dhLocalMap.end()) {
      if (i->second && i->second->GetKeyLength() > 0) {
          i->second->Save(path,i->first);
      }
    ++i;
  }
}

#else

  #ifdef _MSC_VER
    #pragma message("H.235 Media support DISABLED (missing OpenSSL)")
  #endif

#endif  // H323_H235

