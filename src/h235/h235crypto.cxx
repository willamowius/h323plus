/*
 * h235crypto.cxx
 *
 * H.235 crypto engine class.
 *
 * H323Plus library
 *
 * Copyright (c) 2012 Jan Willamowius
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
 * The Initial Developer of the Original Code is Jan Willamowius.
 *
 * $Id$
 *
 */

#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

#include "h235/h235crypto.h"
#include <openssl/aes.h>
#include <openssl/rand.h>

#include "rtp.h"
#include "h235/h235caps.h"

#ifdef H323_H235_AES256
const char * OID_AES256 = "2.16.840.1.101.3.4.1.42";
#endif
const char * OID_AES192 = "2.16.840.1.101.3.4.1.22";
const char * OID_AES128 = "2.16.840.1.101.3.4.1.2";


H235CryptoEngine::H235CryptoEngine(const PString & algorithmOID)
{
	m_algorithmOID = algorithmOID;
}

H235CryptoEngine::H235CryptoEngine(const PString & algorithmOID, const PBYTEArray & key)
{
	m_algorithmOID = algorithmOID;
	SetKey(key);
}

H235CryptoEngine::~H235CryptoEngine()
{
}

void H235CryptoEngine::SetKey(PBYTEArray key)
{
	// TODO: H.235.6 clause 9.3.1.1 says iv should be initialized to seq# + timestamp of RTP packets
	unsigned char * iv;
	const EVP_CIPHER * cipher = NULL;

	if (m_algorithmOID == OID_AES128) {
		cipher = EVP_aes_128_cbc();
		iv = (unsigned char *)malloc(16);
		memset(iv, 0, 16);
	} else if (m_algorithmOID == OID_AES192) {
		cipher = EVP_aes_192_cbc();
		iv = (unsigned char *)malloc(24);
		memset(iv, 0, 32);
#ifdef H323_H235_AES256
	} else if (m_algorithmOID == OID_AES256) {
		cipher = EVP_aes_256_cbc();
		iv = (unsigned char *)malloc(32);
		memset(iv, 0, 32);
#endif
	} else {
		PTRACE(1, "Unsupported algorithm " << m_algorithmOID);
		return;
	}
  
	EVP_CIPHER_CTX_init(&m_encryptCtx);
	EVP_EncryptInit_ex(&m_encryptCtx, cipher, NULL, key.GetPointer(), iv);
	EVP_CIPHER_CTX_init(&m_decryptCtx);
	EVP_DecryptInit_ex(&m_decryptCtx, cipher, NULL, key.GetPointer(), iv);
	free(iv);
}

PBYTEArray H235CryptoEngine::Encrypt(const PBYTEArray & _data)
{
	PBYTEArray data = _data;	// TODO: avoid copy, done because GetPointer() breaks const

	/* max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE -1 bytes */
	int ciphertext_len = data.GetSize() + AES_BLOCK_SIZE;	// TODO: this assumes AES cyphers
	int final_len = 0;
    PBYTEArray ciphertext(ciphertext_len);
  
	/* allows reusing of context for multiple encryption cycles */
	EVP_EncryptInit_ex(&m_encryptCtx, NULL, NULL, NULL, NULL);

	/* update ciphertext, ciphertext_len is filled with the length of ciphertext generated,
	 *len is the size of plaintext in bytes */
	EVP_EncryptUpdate(&m_encryptCtx, ciphertext.GetPointer(), &ciphertext_len, data.GetPointer(), data.GetSize());

	/* update ciphertext with the final remaining bytes */
	EVP_EncryptFinal_ex(&m_encryptCtx, ciphertext.GetPointer() + ciphertext_len, &final_len);

	ciphertext.SetSize(ciphertext_len + final_len);
	return ciphertext;
}

PBYTEArray H235CryptoEngine::Decrypt(const PBYTEArray & _data)
{
	PBYTEArray data = _data;	// TODO: avoid copy, done because GetPointer() breaks const

	/* plaintext will always be equal to or lesser than length of ciphertext*/
	int plaintext_len = data.GetSize();
	int final_len = 0;
	PBYTEArray plaintext(plaintext_len);
  
	EVP_DecryptInit_ex(&m_decryptCtx, NULL, NULL, NULL, NULL);
	EVP_DecryptUpdate(&m_decryptCtx, plaintext.GetPointer(), &plaintext_len, data.GetPointer(), data.GetSize());
	EVP_DecryptFinal_ex(&m_decryptCtx, plaintext.GetPointer() + plaintext_len, &final_len);

	plaintext.SetSize(plaintext_len + final_len);
	return plaintext;
}

PBYTEArray H235CryptoEngine::GenerateRandomKey(const PString & algorithmOID)
{
	PBYTEArray key;

	if (algorithmOID == OID_AES128) {
		key.SetSize(16);
	} else if (m_algorithmOID == OID_AES192) {
		key.SetSize(24);
#ifdef H323_H235_AES256
	} else if (m_algorithmOID == OID_AES256) {
		key.SetSize(32);
#endif
	} else {
		PTRACE(1, "Unsupported algorithm " << algorithmOID);
		return key;
	}
	RAND_bytes(key.GetPointer(), key.GetSize());

	return key;
}

///////////////////////////////////////////////////////////////////////////////////

H235Session::H235Session(H235Capabilities * caps, const PString & oidAlgorithm)
: m_dh(*caps->GetDiffieHellMan()), m_context(oidAlgorithm), m_isInitialised(false)
{

}

H235Session::~H235Session()
{

}


void H235Session::EncodeMediaKey(PBYTEArray & key)
{
    PTRACE(4, "H235Key\tEncode plain media key: " << endl << hex << key); 

    PBYTEArray encrypted;
    // TODO: Encrypt Master Key with Diffie Hellman session key
    key = encrypted;

    PTRACE(4, "H235Key\tEncrypted key:" << endl << hex << key);
}

void H235Session::DecodeMediaKey(PBYTEArray & key)
{
    PTRACE(4, "H235Key\tH235v3 encrypted key received, size=" << key.GetSize() << endl << hex << key);

    PBYTEArray decrypted;
    // TODO: Decrypt Master Key with Diffie Hellman session key
    key = decrypted;

    PTRACE(4, "H235Key\tH235v3 key decrypted, size= " << key.GetSize() << endl << hex << key);
}

PBoolean H235Session::IsActive()
{
    return !IsInitialised();
}

PBoolean H235Session::IsInitialised() 
{ 
    return m_isInitialised; 
}

PBoolean H235Session::CreateSession()
{
    m_isInitialised = true;
    return true;
}

PBoolean H235Session::ReadFrame(DWORD & /*rtpTimestamp*/, RTP_DataFrame & frame)
{
    PBYTEArray buffer(frame.GetPayloadPtr(),frame.GetPayloadSize());
    buffer = m_context.Decrypt(buffer);
    frame.SetPayloadSize(buffer.GetSize());
    memcpy(frame.GetPayloadPtr(),buffer.GetPointer(), buffer.GetSize());
    buffer.SetSize(0);
    return true;
}

PBoolean H235Session::WriteFrame(RTP_DataFrame & frame)
{
    PBYTEArray buffer(frame.GetPayloadPtr(),frame.GetPayloadSize());
    buffer = m_context.Encrypt(buffer);
    frame.SetPayloadSize(buffer.GetSize());
    memcpy(frame.GetPayloadPtr(),buffer.GetPointer(), buffer.GetSize());
    buffer.SetSize(0);
    return true;
}

#endif

