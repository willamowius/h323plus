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

extern "C" {
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
};

////////////////////////////////////////////////////////////////////////////////////

// Diffie Hellman


H235_DiffieHellman::H235_DiffieHellman()
: dh(NULL), m_remKey(NULL), m_toSend(true), m_keySize(0)
{
}

H235_DiffieHellman::H235_DiffieHellman(const BYTE * pData, PINDEX pSize,
                                     const BYTE * gData, PINDEX gSize, 
                                     PBoolean send)
: m_remKey(NULL), m_toSend(send), m_keySize(pSize)
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
   dh = NULL;
   dh = DHparams_dup(diffie.dh);
}


H235_DiffieHellman & H235_DiffieHellman::operator=(const H235_DiffieHellman & diffie)
{
  if (dh != NULL)
    DH_free(dh);
   
   dh = NULL;
   dh = DHparams_dup(diffie.dh);

  return *this;
}


H235_DiffieHellman::~H235_DiffieHellman()
{
  if (dh != NULL)
    DH_free(dh);
}

PBoolean H235_DiffieHellman::CreateParams()
{

   PWaitAndSignal m(vbMutex);

    dsa_st *dsaparams;  // Digital Signature Algorithm Structure

	int i;

	const char *seed[] = { ";-)  :-(  :-)  :-(  ",
				 ";-)  :-(  :-)  :-(  ",
				 "Random String no. 12",
				 ";-)  :-(  :-)  :-(  ",
				  "hackers have no mo", /* from jargon file */
		};
	   unsigned char seedbuf[20];

 vbMutex.Wait();
	RAND_bytes((unsigned char *) &i, sizeof i);
 vbMutex.Signal();

	// Make sure that i is non-negative
	if (i<0)
		 i = -i;

	if (i<0)
		 i= 0;

    if (i >= 0) {
    
		i %= sizeof seed / sizeof seed[0];

	  if (strlen(seed[i]) != 20) {
	     	 return FALSE;
	  }

 vbMutex.Wait();
		memcpy(seedbuf, seed[i], 20);
		dsaparams = DSA_generate_parameters(m_keySize, seedbuf, 20, NULL, NULL, 0, NULL);
 vbMutex.Signal();

	} else {
		 /* Random Parameters (may take awhile) You should never get here ever!!!*/
		dsaparams = DSA_generate_parameters(m_keySize, NULL, 0, NULL, NULL, 0, NULL);
	}
    
    if (dsaparams == NULL) {
		return FALSE;
    }

	dh = DH_new();
    dh = DSA_dup_DH(dsaparams);
    
	DSA_free(dsaparams);

    if (dh == NULL) {
		return FALSE;
    }

	return TRUE;
}

PBoolean H235_DiffieHellman::CheckParams()
{

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

void H235_DiffieHellman::Encode_P(PASN_BitString & p)
{
	PWaitAndSignal m(vbMutex);

    if (!m_toSend)
        return;

	unsigned char *data;
	int l,len,bits_p;

	len=BN_num_bytes(dh->p);
	bits_p=BN_num_bits(dh->p);
 

	data=(unsigned char *)OPENSSL_malloc(len+20);
	if (data != NULL) {
		l=BN_bn2bin(dh->p,data);
		p.SetData(bits_p,data);
	}

	OPENSSL_free(data);

}

void H235_DiffieHellman::Decode_P(const PASN_BitString & p)
{
	PWaitAndSignal m(vbMutex);

    if (p.GetSize() == 0)
        return;

	const unsigned char *data = p.GetDataPointer();
	dh->p=BN_bin2bn(data,sizeof(data),NULL);
}

void H235_DiffieHellman::Encode_G(PASN_BitString & g)
{
    PWaitAndSignal m(vbMutex);

    if (!m_toSend)
        return;

	unsigned char *data;
	int l,len_p,len_g,bits_p;

	len_p=BN_num_bytes(dh->p);
    len_g=BN_num_bytes(dh->g);

	bits_p=BN_num_bits(dh->p);

    // G is padded out to the length of P
	data=(unsigned char *)OPENSSL_malloc(len_p+20);
    memset(data,0,len_p);
	if (data != NULL) {
		 l=BN_bn2bin(dh->g,data+len_p-len_g);
		 g.SetData(bits_p,data);
	}

	OPENSSL_free(data);
}

void H235_DiffieHellman::Decode_G(const PASN_BitString & g)
{
	PWaitAndSignal m(vbMutex);

    if (g.GetSize() == 0)
        return;

    const unsigned char *data;
    if (g.GetSize() > 0) {
		data = g.GetDataPointer();
		dh->g=BN_bin2bn(data,sizeof(data),NULL);
    }
}


void H235_DiffieHellman::Encode_HalfKey(PASN_BitString & hk)
{
    PWaitAndSignal m(vbMutex);

	unsigned char *data;
	int l,len,bits_key;

	len=BN_num_bytes(dh->pub_key);
	bits_key=BN_num_bits(dh->pub_key);

	data=(unsigned char *)OPENSSL_malloc(len+20);
	if (data != NULL){
		l=BN_bn2bin(dh->pub_key,data);
		hk.SetData(bits_key,data);
	}

	OPENSSL_free(data);

}

void H235_DiffieHellman::Decode_HalfKey(const PASN_BitString & hk)
{
	PWaitAndSignal m(vbMutex);

	const unsigned char *data = hk.GetDataPointer();
	dh->pub_key = BN_bin2bn(data,sizeof(data),NULL);   
}

void H235_DiffieHellman::SetRemoteKey(bignum_st * remKey)
{
    m_remKey = remKey;
}

PBoolean H235_DiffieHellman::GenerateHalfKey()
{
    PWaitAndSignal m(vbMutex);

    if (!CheckParams())
	  return FALSE;

	if (!DH_generate_key(dh)) {
		PStringStream ErrStr;
		char buf[256];
		ERR_error_string(ERR_get_error(), buf);
        PTRACE(4,"H235_DH\tERROR DH Halfkey " << buf);
		return FALSE;
	}

    return TRUE;
}

PBoolean H235_DiffieHellman::ComputeSessionKey(PBYTEArray & SessionKey)
{

    if (!m_remKey)
        return false;

	int len, out;
	unsigned char *buf=NULL;

	len=DH_size(dh);
	buf=(unsigned char *)OPENSSL_malloc(len);

	out=DH_compute_key(buf, m_remKey, dh);

	if (out <= 0) {
		PTRACE(2,"H235_DH\tERROR Generating Shared DH!");
	    return false;
	}

    SessionKey.SetSize(out);
    memcpy(SessionKey.GetPointer(),(void *)buf,out);

	OPENSSL_free(buf);

    return true;
}

bignum_st * H235_DiffieHellman::GetPublicKey()
{
    return dh->pub_key;
}

int H235_DiffieHellman::GetKeyLength()
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

void LoadDiffieHellmanMap(std::map<PString, H235_DiffieHellman*> & dhmap)
{
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

H2356_Authenticator::H2356_Authenticator()
: m_enabled(true), m_active(true), m_tokenState(e_clearNone)
{
    usage = MediaEncryption;
    LoadDiffieHellmanMap(m_dhLocalMap);
}

H2356_Authenticator::~H2356_Authenticator()
{
    DeleteObjectsInMap(m_dhLocalMap);
    DeleteObjectsInMap(m_dhRemoteMap);
}

const char * H2356_Authenticator::GetName() const
{
    return "H.235.6 Encryption";
}

PBoolean H2356_Authenticator::PrepareTokens(PASN_Array & clearTokens,
                                      PASN_Array & /*cryptoTokens*/)
{
  if (!IsActive() || (m_tokenState == e_clearDisable))
    return FALSE;

  H225_ArrayOf_ClearToken & tokens = (H225_ArrayOf_ClearToken &)clearTokens;
  int sz = 0;

  std::map<PString, H235_DiffieHellman*>::iterator i = m_dhLocalMap.begin();
  while (i != m_dhLocalMap.end()) {
      sz = tokens.GetSize();
      tokens.SetSize(sz+1);
      H235_ClearToken & clearToken = tokens[sz];
      clearToken.m_tokenOID = i->first;
      H235_DiffieHellman * m_dh = i->second;
      if (m_dh && m_dh->GenerateHalfKey()) {
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
  
  PString dhOID         = PString();
  int     lastKeyLength = 0;
  std::map<PString, H235_DiffieHellman*>::iterator i = m_dhLocalMap.begin();
  while (i != m_dhLocalMap.end()) {
      if (i->second->GetKeyLength() > lastKeyLength) {
          dhOID = i->first;
          lastKeyLength = i->second->GetKeyLength();
      }
    i++;
  }

  if (dhOID.IsEmpty())
      return;

  std::map<PString, H235_DiffieHellman*>::iterator l = m_dhLocalMap.find(dhOID);
  std::map<PString, H235_DiffieHellman*>::iterator r = m_dhRemoteMap.find(dhOID);

  l->second->SetRemoteKey(r->second->GetPublicKey());

  PStringList algOIDs;
  algOIDs.SetSize(0);
  for (PINDEX i=0; i<PARRAYSIZE(H235_Algorithms); ++i) {
      if (PString(H235_Algorithms[i].DHparameters) == dhOID)
           algOIDs.AppendString(H235_Algorithms[i].algorithm);
  }

  if (connection && (algOIDs.GetSize() > 0)) {
      H235Capabilities * localCaps = (H235Capabilities *)connection->GetLocalCapabilitiesRef();
      localCaps->SetDHKeyPair(algOIDs,l->second,connection->IsH245Master());
  }
}


#endif  // H323_H235

