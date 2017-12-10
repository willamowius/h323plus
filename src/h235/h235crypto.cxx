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

#include "h323con.h"
#include "h235/h235crypto.h"
#include "h235/h235caps.h"
#include "h235/h235support.h"
#include <openssl/rand.h>

#include "rtp.h"

#ifdef H323_H235_AES256
#if _WIN32
#pragma message("AES256 Encryption Enabled. Software may be subject to US export restrictions. http://www.bis.doc.gov/encryption/")
#else
#warning("AES256 Encryption Enabled. Software may be subject to US export restrictions. http://www.bis.doc.gov/encryption/")
#endif
#endif


// the IV sequence is always 6 bytes long (2 bytes seq number + 4 bytes timestamp)
const unsigned int IV_SEQUENCE_LEN = 6;


// ciphertext stealing code based on a OpenSSL patch by An-Cheng Huang
// Note: This ciphertext stealing implementation doesn't seem to always produce
// compatible results, avoid when encrypting:

int EVP_EncryptUpdate_cts(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
                      const unsigned char *in, int inl)
{
    int bl = ctx->cipher->block_size;
    OPENSSL_assert(bl <= (int)sizeof(ctx->buf));
    *outl = 0;

    if ((ctx->buf_len + inl) <= bl) {
        /* new plaintext is no more than 1 block */
        /* copy the in data into the buffer and return */
        memcpy(&(ctx->buf[ctx->buf_len]), in, inl);
        ctx->buf_len += inl;
        *outl = 0;
        return 1;
    }

    /* more than 1 block of new plaintext available */
    /* encrypt the previous plaintext, if any */
    if (ctx->final_used) {
        if (!(ctx->cipher->do_cipher(ctx, out, ctx->final, bl))) {
          return 0;
        }
        out += bl;
        *outl += bl;
        ctx->final_used = 0;
    }

    /* we already know ctx->buf_len + inl must be > bl */
    memcpy(&(ctx->buf[ctx->buf_len]), in, (bl - ctx->buf_len));
    in += (bl - ctx->buf_len);
    inl -= (bl - ctx->buf_len);
    ctx->buf_len = bl;

    if (inl <= bl) {
        memcpy(ctx->final, ctx->buf, bl);
        ctx->final_used = 1;
        memcpy(ctx->buf, in, inl);
        ctx->buf_len = inl;
        return 1;
    } else {
        if (!(ctx->cipher->do_cipher(ctx, out, ctx->buf, bl))) {
            return 0;
        }
        out += bl;
        *outl += bl;
        ctx->buf_len = 0;

        int leftover = inl & ctx->block_mask;
        if (leftover) {
            inl -= (bl + leftover);
            memcpy(ctx->buf, &(in[(inl + bl)]), leftover);
            ctx->buf_len = leftover;
        } else {
            inl -= (2 * bl);
             memcpy(ctx->buf, &(in[(inl + bl)]), bl);
             ctx->buf_len = bl;
        }
        memcpy(ctx->final, &(in[inl]), bl);
        ctx->final_used = 1;
        if (!(ctx->cipher->do_cipher(ctx, out, in, inl))) {
            return 0;
        }
        out += inl;
        *outl += inl;
    }

    return 1;
}

#if PTLIB_VER >= 2130
int EVP_EncryptFinal_ctsA(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
#else
int EVP_EncryptFinal_cts(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
#endif
{
    unsigned char tmp[EVP_MAX_BLOCK_LENGTH];
    int bl = ctx->cipher->block_size;
    int leftover = 0;
    *outl = 0;

    if (!ctx->final_used) {
        PTRACE(1, "H235\tCTS Error: expecting previous ciphertext");
        return 0;
    }
    if (ctx->buf_len == 0) {
        PTRACE(1, "H235\tCTS Error: expecting previous plaintext");
        return 0;
    }

    /* handle leftover bytes */
    leftover = ctx->buf_len;

    switch (EVP_CIPHER_CTX_mode(ctx)) {
        case EVP_CIPH_ECB_MODE: {
            /* encrypt => C_{n} plus C' */
            if (!(ctx->cipher->do_cipher(ctx, tmp, ctx->final, bl))) {
                 return 0;
            }

            /* P_n plus C' */
            memcpy(&(ctx->buf[leftover]), &(tmp[leftover]), (bl - leftover));
            /* encrypt => C_{n-1} */
            if (!(ctx->cipher->do_cipher(ctx, out, ctx->buf, bl))) {
                return 0;
            }

            memcpy((out + bl), tmp, leftover);
            *outl += (bl + leftover);
            return 1;
        }
        case EVP_CIPH_CBC_MODE: {
            /* encrypt => C_{n} plus C' */
            if (!(ctx->cipher->do_cipher(ctx, tmp, ctx->final, bl))) {
                return 0;
            }

            /* P_n plus 0s */
            memset(&(ctx->buf[leftover]), 0, (bl - leftover));

            /* note that in cbc encryption, plaintext will be xor'ed with the previous
             * ciphertext, which is what we want.
             */
            /* encrypt => C_{n-1} */
            if (!(ctx->cipher->do_cipher(ctx, out, ctx->buf, bl))) {
                return 0;
            }

            memcpy((out + bl), tmp, leftover);
            *outl += (bl + leftover);
            return 1;
        }
        default:
            PTRACE(1, "H235\tCTS Error: unsupported mode");
            return 0;
    }
}

int EVP_DecryptUpdate_cts(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
                      const unsigned char *in, int inl)
{
    return EVP_EncryptUpdate_cts(ctx, out, outl, in, inl);
}

int EVP_DecryptFinal_cts(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    unsigned char tmp[EVP_MAX_BLOCK_LENGTH];
    int bl = ctx->cipher->block_size;
    int leftover = 0;
    *outl = 0;

    if (!ctx->final_used) {
        PTRACE(1, "H235\tCTS Error: expecting previous ciphertext");
        return 0;
    }
    if (ctx->buf_len == 0) {
        PTRACE(1, "H235\tCTS Error: expecting previous ciphertext");
        return 0;
    }

    /* handle leftover bytes */
    leftover = ctx->buf_len;

    switch (EVP_CIPHER_CTX_mode(ctx)) {
        case EVP_CIPH_ECB_MODE: {
            /* decrypt => P_n plus C' */
            if (!(ctx->cipher->do_cipher(ctx, tmp, ctx->final, bl))) {
                return 0;
            }

            /* C_n plus C' */
            memcpy(&(ctx->buf[leftover]), &(tmp[leftover]), (bl - leftover));
            /* decrypt => P_{n-1} */
            if (!(ctx->cipher->do_cipher(ctx, out, ctx->buf, bl))) {
                return 0;
            }

            memcpy((out + bl), tmp, leftover);
            *outl += (bl + leftover);
            return 1;
        }
        case EVP_CIPH_CBC_MODE: {
            int i = 0;
            unsigned char C_n_minus_2[EVP_MAX_BLOCK_LENGTH];

            memcpy(C_n_minus_2, ctx->iv, bl);

            /* C_n plus 0s in ctx->buf */
            memset(&(ctx->buf[leftover]), 0, (bl - leftover));

            /* ctx->final is C_{n-1} */
            /* decrypt => (P_n plus C')'' */
            if (!(ctx->cipher->do_cipher(ctx, tmp, ctx->final, bl))) {
                return 0;
            }
            /* XOR'ed with C_{n-2} => (P_n plus C')' */
            for (i = 0; i < bl; i++) {
                tmp[i] = tmp[i] ^ C_n_minus_2[i];
            }
            /* XOR'ed with (C_n plus 0s) => P_n plus C' */
            for (i = 0; i < bl; i++) {
                tmp[i] = tmp[i] ^ ctx->buf[i];
            }

            /* C_n plus C' in ctx->buf */
            memcpy(&(ctx->buf[leftover]), &(tmp[leftover]), (bl - leftover));
            /* decrypt => P_{n-1}'' */
            if (!(ctx->cipher->do_cipher(ctx, out, ctx->buf, bl))) {
                return 0;
            }
            /* XOR'ed with C_{n-1} => P_{n-1}' */
            for (i = 0; i < bl; i++) {
                out[i] = out[i] ^ ctx->final[i];
            }
            /* XOR'ed with C_{n-2} => P_{n-1} */
            for (i = 0; i < bl; i++) {
                out[i] = out[i] ^ C_n_minus_2[i];
            }

            memcpy((out + bl), tmp, leftover);
            *outl += (bl + leftover);
            return 1;
        }
        default:
            PTRACE(1, "H235\tCTS Error: unsupported mode");
            return 0;
    }
}

int EVP_DecryptFinal_relaxed(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    int n = 0;
    unsigned int b = 0;
    *outl = 0;

    b=ctx->cipher->block_size;
    if (ctx->flags & EVP_CIPH_NO_PADDING) {
        if(ctx->buf_len) {
            PTRACE(1, "H235\tDecrypt error: data not a multiple of block length");
            return 0;
        }
        *outl = 0;
        return 1;
    }
    if (b > 1) {
        if (ctx->buf_len || !ctx->final_used) {
            PTRACE(1, "H235\tDecrypt error: wrong final block length");
            return(0);
        }
        OPENSSL_assert(b <= sizeof ctx->final);
        n=ctx->final[b-1];
        if (n == 0 || n > (int)b) {
            PTRACE(1, "H235\tDecrypt error: bad decrypt");
            return(0);
        }
        // Polycom endpoints (eg. m100 and PVX) don't fill the padding propperly, so we have to disable this check
/*
        for (int i=0; i<n; i++) {
            if (ctx->final[--b] != n) {
                PTRACE(1, "H235\tDecrypt error: incorrect padding");
                return(0);
            }
        }
*/
        n=ctx->cipher->block_size-n;
        for (int i=0; i<n; i++)
            out[i]=ctx->final[i];
        *outl=n;
    }
    else
        *outl=0;
    return(1);
}

H235CryptoEngine::H235CryptoEngine(const PString & algorithmOID)
:  m_algorithmOID(algorithmOID), m_operationCnt(0), m_initialised(false),
   m_enc_blockSize(0), m_enc_ivLength(0), m_dec_blockSize(0), m_dec_ivLength(0)
{
}

H235CryptoEngine::H235CryptoEngine(const PString & algorithmOID, const PBYTEArray & key)
:  m_algorithmOID(algorithmOID), m_operationCnt(0), m_initialised(false),
   m_enc_blockSize(0), m_enc_ivLength(0), m_dec_blockSize(0), m_dec_ivLength(0)
{
    SetKey(key);
}

H235CryptoEngine::~H235CryptoEngine()
{
    if (m_initialised) {
        EVP_CIPHER_CTX_cleanup(&m_encryptCtx);
        EVP_CIPHER_CTX_cleanup(&m_decryptCtx);
    }
}

void H235CryptoEngine::SetKey(PBYTEArray key)
{
    const EVP_CIPHER * cipher = NULL;

    if (m_algorithmOID == ID_AES128) {
        cipher = EVP_aes_128_cbc();
#ifdef H323_H235_AES256
    } else if (m_algorithmOID == ID_AES192) {
        cipher = EVP_aes_192_cbc();
    } else if (m_algorithmOID == ID_AES256) {
        cipher = EVP_aes_256_cbc();
#endif
    } else {
        PTRACE(1, "H235\tUnsupported algorithm " << m_algorithmOID);
        return;
    }

    EVP_CIPHER_CTX_init(&m_encryptCtx);
    EVP_EncryptInit_ex(&m_encryptCtx, cipher, NULL, key.GetPointer(), NULL);
    m_enc_blockSize =  EVP_CIPHER_CTX_block_size(&m_encryptCtx);
    m_enc_ivLength = EVP_CIPHER_CTX_iv_length(&m_encryptCtx);

    EVP_CIPHER_CTX_init(&m_decryptCtx);
    EVP_DecryptInit_ex(&m_decryptCtx, cipher, NULL, key.GetPointer(), NULL);
    m_dec_blockSize =  EVP_CIPHER_CTX_block_size(&m_decryptCtx);
    m_dec_ivLength = EVP_CIPHER_CTX_iv_length(&m_decryptCtx);

    m_operationCnt = 0;
    m_initialised = true;
}

void H235CryptoEngine::SetIV(unsigned char * iv, unsigned char * ivSequence, unsigned ivLen)
{
    // fill iv by repeating ivSequence until block size is reached
    if (ivSequence) {
        for (unsigned i = 0; i < (ivLen / IV_SEQUENCE_LEN); i++) {
            memcpy(iv + (i * IV_SEQUENCE_LEN), ivSequence, IV_SEQUENCE_LEN);
        }
        // copy partial ivSequence at end
        if (ivLen % IV_SEQUENCE_LEN > 0) {
            memcpy(iv + ivLen - (ivLen % IV_SEQUENCE_LEN), ivSequence, ivLen % IV_SEQUENCE_LEN);
        }
    } else {
        memset(iv, 0, ivLen);
    }
}

PBYTEArray H235CryptoEngine::Encrypt(const PBYTEArray & _data, unsigned char * ivSequence, bool & rtpPadding)
{
    if (!m_initialised)
        return PBYTEArray();

    PBYTEArray & data = *(PRemoveConst(PBYTEArray, &_data));
    unsigned char iv[EVP_MAX_IV_LENGTH];

    // max ciphertext len for a n bytes of plaintext is n + BLOCK_SIZE -1 bytes
    int ciphertext_len = data.GetSize() + EVP_CIPHER_CTX_block_size(&m_encryptCtx);
    int final_len = 0;
    PBYTEArray ciphertext(ciphertext_len);

    SetIV(iv, ivSequence, EVP_CIPHER_CTX_iv_length(&m_encryptCtx));
    EVP_EncryptInit_ex(&m_encryptCtx, NULL, NULL, NULL, iv);

    // always use padding, because our ciphertext stealing implementation
    // doesn't seem to produce compatible results
    //rtpPadding = (data.GetSize() < EVP_CIPHER_CTX_block_size(&m_encryptCtx));  // not interoperable!
    rtpPadding = (data.GetSize() % EVP_CIPHER_CTX_block_size(&m_encryptCtx) > 0);
    EVP_CIPHER_CTX_set_padding(&m_encryptCtx, rtpPadding ? 1 : 0);

    if (!rtpPadding && (data.GetSize() % EVP_CIPHER_CTX_block_size(&m_encryptCtx) > 0)) {
        // use cyphertext stealing
        if (!EVP_EncryptUpdate_cts(&m_encryptCtx, ciphertext.GetPointer(), &ciphertext_len, data.GetPointer(), data.GetSize())) {
            PTRACE(1, "H235\tEVP_EncryptUpdate_cts() failed");
        }
#if PTLIB_VER >= 2130
        if (!EVP_EncryptFinal_ctsA(&m_encryptCtx, ciphertext.GetPointer() + ciphertext_len, &final_len)) {
#else
        if (!EVP_EncryptFinal_cts(&m_encryptCtx, ciphertext.GetPointer() + ciphertext_len, &final_len)) {
#endif
            PTRACE(1, "H235\tEVP_EncryptFinal_cts() failed");
        }
    } else {
        /* update ciphertext, ciphertext_len is filled with the length of ciphertext generated,
         *len is the size of plaintext in bytes */
        if (!EVP_EncryptUpdate(&m_encryptCtx, ciphertext.GetPointer(), &ciphertext_len, data.GetPointer(), data.GetSize())) {
            PTRACE(1, "H235\tEVP_EncryptUpdate() failed");
        }

        // update ciphertext with the final remaining bytes, if any use RTP padding
        if (!EVP_EncryptFinal_ex(&m_encryptCtx, ciphertext.GetPointer() + ciphertext_len, &final_len)) {
            PTRACE(1, "H235\tEVP_EncryptFinal_ex() failed");
        }
    }

    ciphertext.SetSize(ciphertext_len + final_len);
    m_operationCnt++;
    return ciphertext;
}

PINDEX H235CryptoEngine::EncryptInPlace(const BYTE * inData, PINDEX inLength, BYTE * outData, unsigned char * ivSequence, bool & rtpPadding)
{
    if (!m_initialised) {
        PTRACE(1, "H235\tERROR: Encryption not initialised!!");
        memset(outData,0,inLength);
        return inLength;
    }

    // max ciphertext len for a n bytes of plaintext is n + BLOCK_SIZE -1 bytes
    int outSize = 0;
    int inSize =  inLength + m_enc_blockSize;

    SetIV(m_iv, ivSequence, m_enc_ivLength);
    EVP_EncryptInit_ex(&m_encryptCtx, NULL, NULL, NULL, m_iv);

    rtpPadding = (inLength % m_enc_blockSize > 0);
    EVP_CIPHER_CTX_set_padding(&m_encryptCtx, rtpPadding ? 1 : 0);

    if (!rtpPadding && (inLength % m_enc_blockSize > 0)) {
        // use cyphertext stealing
        if (!EVP_EncryptUpdate_cts(&m_encryptCtx, outData, &inSize, inData, inLength)) {
            PTRACE(1, "H235\tEVP_EncryptUpdate_cts() failed");
        }
#if PTLIB_VER >= 2130
        if (!EVP_EncryptFinal_ctsA(&m_encryptCtx, outData + inSize, &outSize)) {
#else
        if (!EVP_EncryptFinal_cts(&m_encryptCtx, outData + inSize, &outSize)) {
#endif
            PTRACE(1, "H235\tEVP_EncryptFinal_cts() failed");
        }
    } else {
        /* update ciphertext, ciphertext_len is filled with the length of ciphertext generated,
         *len is the size of plaintext in bytes */
        if (!EVP_EncryptUpdate(&m_encryptCtx, outData, &inSize, inData, inLength)) {
            PTRACE(1, "H235\tEVP_EncryptUpdate() failed");
        }

        // update ciphertext with the final remaining bytes, if any use RTP padding
        if (!EVP_EncryptFinal_ex(&m_encryptCtx, outData + inSize, &outSize)) {
            PTRACE(1, "H235\tEVP_EncryptFinal_ex() failed");
        }
    }
    return inSize + outSize;
}

PBYTEArray H235CryptoEngine::Decrypt(const PBYTEArray & _data, unsigned char * ivSequence, bool & rtpPadding)
{
    if (!m_initialised)
        return PBYTEArray();

    PBYTEArray & data = *(PRemoveConst(PBYTEArray, &_data));
    unsigned char iv[EVP_MAX_IV_LENGTH];

    /* plaintext will always be equal to or lesser than length of ciphertext*/
    int plaintext_len = data.GetSize();
    int final_len = 0;
    PBYTEArray plaintext(plaintext_len);

    SetIV(iv, ivSequence, EVP_CIPHER_CTX_iv_length(&m_decryptCtx));
    EVP_DecryptInit_ex(&m_decryptCtx, NULL, NULL, NULL, iv);

    EVP_CIPHER_CTX_set_padding(&m_decryptCtx, rtpPadding ? 1 : 0);

    if (!rtpPadding && data.GetSize() % EVP_CIPHER_CTX_block_size(&m_decryptCtx) > 0) {
        // use cyphertext stealing
        if (!EVP_DecryptUpdate_cts(&m_decryptCtx, plaintext.GetPointer(), &plaintext_len, data.GetPointer(), data.GetSize())) {
            PTRACE(1, "H235\tEVP_DecryptUpdate_cts() failed");
        }
        if(!EVP_DecryptFinal_cts(&m_decryptCtx, plaintext.GetPointer() + plaintext_len, &final_len)) {
            PTRACE(1, "H235\tEVP_DecryptFinal_cts() failed");
        }
    } else {
        if (!EVP_DecryptUpdate(&m_decryptCtx, plaintext.GetPointer(), &plaintext_len, data.GetPointer(), data.GetSize())) {
            PTRACE(1, "H235\tEVP_DecryptUpdate() failed");
        }
        if (!EVP_DecryptFinal_relaxed(&m_decryptCtx, plaintext.GetPointer() + plaintext_len, &final_len)) {
            PTRACE(1, "H235\tEVP_DecryptFinal_ex() failed - incorrect padding ?");
        }
    }

	rtpPadding = false;	// we return the real length of the decrypted data without padding
    plaintext.SetSize(plaintext_len + final_len);
    m_operationCnt++;
    return plaintext;
}

PINDEX H235CryptoEngine::DecryptInPlace(const BYTE * inData, PINDEX inLength, BYTE * outData, unsigned char * ivSequence, bool & rtpPadding)
{

    /* plaintext will always be equal to or lesser than length of ciphertext*/
    int outSize = 0;
    int inSize =  inLength;

    SetIV(m_iv, ivSequence, m_dec_ivLength);
    EVP_DecryptInit_ex(&m_decryptCtx, NULL, NULL, NULL, m_iv);

    EVP_CIPHER_CTX_set_padding(&m_decryptCtx, rtpPadding ? 1 : 0);

    if (!rtpPadding && inLength % m_dec_blockSize > 0) {
        // use cyphertext stealing
        if (!EVP_DecryptUpdate_cts(&m_decryptCtx, outData, &inSize, inData, inLength)) {
            PTRACE(1, "H235\tEVP_DecryptUpdate_cts() failed");
			return 0;	// no usable payload
        }
        if(!EVP_DecryptFinal_cts(&m_decryptCtx, outData + inSize, &outSize)) {
            PTRACE(1, "H235\tEVP_DecryptFinal_cts() failed");
			return 0;	// no usable payload
        }
    } else {
        if (!EVP_DecryptUpdate(&m_decryptCtx, outData, &inSize, inData, inLength)) {
            PTRACE(1, "H235\tEVP_DecryptUpdate() failed");
			return 0;	// no usable payload
        }
        if (!EVP_DecryptFinal_relaxed(&m_decryptCtx, outData + inSize, &outSize)) {
            PTRACE(1, "H235\tEVP_DecryptFinal_ex() failed - incorrect padding ?");
			return 0;	// no usable payload
        }
    }
	rtpPadding = false;	// we return the real length of the decrypted data without padding
    return inSize + outSize;
}

PBYTEArray H235CryptoEngine::GenerateRandomKey()
{
    PBYTEArray result = GenerateRandomKey(m_algorithmOID);
    SetKey(result);
    return result;
}

PBYTEArray H235CryptoEngine::GenerateRandomKey(const PString & algorithmOID)
{
    PBYTEArray key;

    if (algorithmOID == ID_AES128) {
        key.SetSize(16);
#ifdef H323_H235_AES256
    } else if (m_algorithmOID == ID_AES192) {
        key.SetSize(24);
    } else if (m_algorithmOID == ID_AES256) {
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
: m_dh(*caps->GetDiffieHellMan()), m_context(oidAlgorithm), m_dhcontext(oidAlgorithm),
  m_isInitialised(false), m_isMaster(false), m_crytoMasterKey(0),
  m_frameBuffer(1500), m_padding(false)
{
    if (oidAlgorithm == ID_AES128) {
        m_dhkeyLen = 16;
#ifdef H323_H235_AES256
    } else if (oidAlgorithm == ID_AES192) {
        m_dhkeyLen = 24;
    } else if (oidAlgorithm == ID_AES256) {
        m_dhkeyLen = 32;
#endif
    } else {
        PTRACE(1, "Unsupported algorithm " << oidAlgorithm);
        m_dhkeyLen = 16;
    }
}

H235Session::~H235Session()
{

}

void H235Session::EncodeMediaKey(PBYTEArray & key)
{
    PTRACE(4, "H235Key\tEncode plain media key: " << endl << hex << m_crytoMasterKey);

    bool rtpPadding = false;
    key = m_dhcontext.Encrypt(m_crytoMasterKey, NULL, rtpPadding);

    PTRACE(4, "H235Key\tEncrypted key:" << endl << hex << key);
}

PBoolean H235Session::DecodeMediaKey(PBYTEArray & key)
{
    if (!m_isInitialised) {
       PTRACE(2, "H235Key\tLOGIC ERROR Session not initialised");
       return false;
    }

    PTRACE(4, "H235Key\tH235v3 encrypted key received, size=" << key.GetSize() << endl << hex << key);

    bool rtpPadding = false;
    m_crytoMasterKey = m_dhcontext.Decrypt(key, NULL, rtpPadding);
    m_context.SetKey(m_crytoMasterKey);

    PTRACE(4, "H235Key\tH235v3 key decrypted, size= " << m_crytoMasterKey.GetSize() << endl << hex << m_crytoMasterKey);
    return true;
}

PBoolean H235Session::IsActive()
{
    return !IsInitialised();
}

PBoolean H235Session::IsInitialised() const
{
    return m_isInitialised;
}

PBoolean H235Session::CreateSession(PBoolean isMaster)
{
    if (m_isInitialised)
        return false;

    m_isMaster = isMaster;
    PBYTEArray dhSessionkey;
    m_dh.ComputeSessionKey(dhSessionkey);
    PBYTEArray shortSessionKey;
    shortSessionKey.SetSize(m_dhkeyLen);
    memcpy(shortSessionKey.GetPointer(), dhSessionkey.GetPointer() + dhSessionkey.GetSize() - shortSessionKey.GetSize(), shortSessionKey.GetSize());

    m_dhcontext.SetKey(shortSessionKey);

    if (m_isMaster)
        m_crytoMasterKey = m_context.GenerateRandomKey();

    m_isInitialised = true;
    return true;
}

PBoolean H235Session::ReadFrame(DWORD & /*rtpTimestamp*/, RTP_DataFrame & frame)
{
    unsigned char ivSequence[6];
    memcpy(ivSequence, frame.GetSequenceNumberPtr(), 6);
    PBoolean padding = frame.GetPadding();
    m_frameBuffer.SetSize(frame.GetPayloadSize());
    memcpy(m_frameBuffer.GetPointer(),frame.GetPayloadPtr(), frame.GetPayloadSize());
    m_frameBuffer = m_context.Decrypt(m_frameBuffer, ivSequence, padding);
    frame.SetPayloadSize(m_frameBuffer.GetSize());
    memcpy(frame.GetPayloadPtr(), m_frameBuffer.GetPointer(), m_frameBuffer.GetSize());
    frame.SetPadding(padding);  // TODO: examine effect for RTP padding on codec decoding?
    m_frameBuffer.SetSize(0);
    return true;
}

PBoolean H235Session::ReadFrameInPlace(RTP_DataFrame & frame)
{
    memcpy(m_ivSequence, frame.GetSequenceNumberPtr(), 6);
    m_padding = frame.GetPadding();
    frame.SetPayloadSize(m_context.DecryptInPlace(frame.GetPayloadPtr(), frame.GetPayloadSize(), m_frameBuffer.GetPointer(), m_ivSequence, m_padding));
    memmove(frame.GetPayloadPtr(), m_frameBuffer.GetPointer(), frame.GetPayloadSize());
    frame.SetPadding(m_padding);
    return true;	// don't stop on decoding errors
}

PBoolean H235Session::WriteFrame(RTP_DataFrame & frame)
{
    unsigned char ivSequence[6];
    memcpy(ivSequence, frame.GetSequenceNumberPtr(), 6);
    PBoolean padding = frame.GetPadding();
    m_frameBuffer.SetSize(frame.GetPayloadSize());
    memcpy(m_frameBuffer.GetPointer(),frame.GetPayloadPtr(), frame.GetPayloadSize());
    m_frameBuffer = m_context.Encrypt(m_frameBuffer, ivSequence, padding);
    frame.SetPayloadSize(m_frameBuffer.GetSize());
    memcpy(frame.GetPayloadPtr(), m_frameBuffer.GetPointer(), m_frameBuffer.GetSize());
    frame.SetPadding(padding);
    m_frameBuffer.SetSize(0);
    return true;
}

PBoolean H235Session::WriteFrameInPlace(RTP_DataFrame & frame)
{
    memcpy(m_ivSequence, frame.GetSequenceNumberPtr(), 6);
    m_padding = frame.GetPadding();
    frame.SetPayloadSize(m_context.EncryptInPlace(frame.GetPayloadPtr(), frame.GetPayloadSize(), m_frameBuffer.GetPointer(), m_ivSequence, m_padding));
    memmove(frame.GetPayloadPtr(), m_frameBuffer.GetPointer(), frame.GetPayloadSize());
    frame.SetPadding(m_padding);
    return (frame.GetPayloadSize() > 0);
}

#endif

