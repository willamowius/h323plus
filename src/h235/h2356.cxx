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
 * $Id $
 *
 *
 */


#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

#include "h235/h2356.h"
#include <h225.h>

extern "C" {
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
};



////////////////////////////////////////////////////////////////////////////////////

unsigned char DH1024_P[128] = {
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED, 
0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE6, 0x53, 0x81, 
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };


unsigned char DH1024_G[128] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

const char OID_H2351A[] = "0.0.8.235.0.1.5";
const char OID_H235V3[] = "0.0.8.235.0.3.24";
const char OID_DH1024[] = "0.0.8.235.0.3.43";	// AlgorithmOID

////////////////////////////////////////////////////////////////////////////////////

// Diffie Hellman


H235_DiffieHellman::H235_DiffieHellman()
: dh(NULL)
{
}

H235_DiffieHellman::H235_DiffieHellman(const BYTE * pData, PINDEX pSize,
                                     const BYTE * gData, PINDEX gSize)
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
		dsaparams = DSA_generate_parameters(ParamSize, seedbuf, 20, NULL, NULL, 0, NULL);
 vbMutex.Signal();

	} else {
		 /* Random Parameters (may take awhile) You should never get here ever!!!*/
		dsaparams = DSA_generate_parameters(ParamSize, NULL, 0, NULL, NULL, 0, NULL);
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

void H235_DiffieHellman::Decode_P(PASN_BitString & p)
{
	PWaitAndSignal m(vbMutex);

	const unsigned char *data = p.GetDataPointer();
	dh->p=BN_bin2bn(data,sizeof(data),NULL);
}

void H235_DiffieHellman::Encode_G(PASN_BitString & g)
{
    PWaitAndSignal m(vbMutex);

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

void H235_DiffieHellman::Decode_G(PASN_BitString & g)
{
	PWaitAndSignal m(vbMutex);

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

void H235_DiffieHellman::Decode_HalfKey(PASN_BitString & hk)
{
	PWaitAndSignal m(vbMutex);

	const unsigned char *data = hk.GetDataPointer();
	dh->pub_key = BN_bin2bn(data,sizeof(data),NULL);   
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

////////////////////////////////////////////////////////////////////////////////////

H2356_Authenticator::H2356_Authenticator()
: m_dh(NULL), m_enabled(true), m_active(true)
{
    usage = MediaEncryption;
}

H2356_Authenticator::~H2356_Authenticator()
{

}

const char * H2356_Authenticator::GetName() const
{
    return "H.235.6 Encryption";
}

PBoolean H2356_Authenticator::PrepareTokens(PASN_Array & clearTokens,
                                      PASN_Array & /*cryptoTokens*/)
{
  if (!IsActive())
    return FALSE;

  if (!m_dh) {
     m_dh = new H235_DiffieHellman(DH1024_P,128,DH1024_G,128);
     if (!m_dh->GenerateHalfKey())
         return false;
  }

  H225_ArrayOf_ClearToken & tokens = (H225_ArrayOf_ClearToken &)clearTokens;
  int sz = tokens.GetSize();
  tokens.SetSize(sz+3);

  // Procedure 1A (H.235v3)
  H235_ClearToken & clearToken1 = tokens[sz];
  clearToken1.m_tokenOID = OID_H2351A;
      clearToken1.IncludeOptionalField(H235_ClearToken::e_dhkey);
      H235_DHset & dh1 = clearToken1.m_dhkey;
           m_dh->Encode_HalfKey(dh1.m_halfkey);
           m_dh->Encode_P(dh1.m_modSize);
           m_dh->Encode_G(dh1.m_generator);

  // DH1024 (H.235v4)
  H235_ClearToken & clearToken2 = tokens[sz+1];
      clearToken2.m_tokenOID = OID_DH1024;
      clearToken2.IncludeOptionalField(H235_ClearToken::e_dhkey);
      H235_DHset & dh2 = clearToken2.m_dhkey;
           m_dh->Encode_HalfKey(dh2.m_halfkey);
           m_dh->Encode_P(dh2.m_modSize);
           m_dh->Encode_G(dh2.m_generator);

  // Backwards Compatibility with H.235v3
  tokens[sz+2].m_tokenOID = OID_H235V3;

  return TRUE;
}

H235Authenticator::ValidationResult H2356_Authenticator::ValidateTokens(const PASN_Array & clearTokens,
                                   const PASN_Array & /*cryptoTokens*/, const PBYTEArray & /*rawPDU*/)
{
   if (!IsActive())
       return e_Disabled;

   const H225_ArrayOf_ClearToken & tokens = (const H225_ArrayOf_ClearToken &)clearTokens;

   for (PINDEX i = 0; i < tokens.GetSize(); ++i) {
      const H235_ClearToken & token = tokens[i];

      if (token.m_tokenOID.AsString() == PString(OID_DH1024)) {
          break;
      }
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


#endif  // H323_H235

