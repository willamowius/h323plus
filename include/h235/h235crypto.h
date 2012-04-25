/*
 * h235crypto.h
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

#ifndef H235MEDIA_H
#define H235MEDIA_H

#include <ptlib.h>

extern "C" {
#include <openssl/evp.h>
}

class H235CryptoEngine : public PObject
{
    PCLASSINFO(H235CryptoEngine, PObject);

public:

 /**@name Constructor */
  //@{
    /** Create a H.235 crypto engine
     */
    H235CryptoEngine(const PString & algorithmOID);
    H235CryptoEngine(const PString & algorithmOID, const PBYTEArray & key);

   /** Destroy the crypto engine
     */
    ~H235CryptoEngine();
  //@}

    /** Set key (for key updates)
      */
    void SetKey(PBYTEArray key);

    /** Encrypt data
      */
    PBYTEArray Encrypt(const PBYTEArray & data, unsigned char * ivSequence, bool & rtpPadding);

    /** Decrypt data
      */
    PBYTEArray Decrypt(const PBYTEArray & data, unsigned char * ivSequence, bool & rtpPadding);

    /** Generate a random key of a size suitable for the alogorithm
      */
    PBYTEArray GenerateRandomKey();   // Use internal Algorithm and set

    PBYTEArray GenerateRandomKey(const PString & algorithmOID);  // Use assigned Algorithm

protected:
    static void SetIV(unsigned char * iv, unsigned char * ivSequence, unsigned ivLen);

    EVP_CIPHER_CTX m_encryptCtx, m_decryptCtx;
    PString m_algorithmOID;    // eg. "2.16.840.1.101.3.4.1.2"
    PBoolean m_initialised;
};


class RTP_DataFrame;
class H235_DiffieHellman;
class H235Capabilities;
class H235Session : public  PObject
{
     PCLASSINFO(H235Session, PObject);

public:

 /**@name Constructor */
  //@{
    /** Create a SSL Session Context
     */
    H235Session(H235Capabilities * caps,  const PString & oidAlgorithm);

   /** Destroy the SSL Session Context
     */
    ~H235Session();
  //@}

 /**@name General Public Functions */
  //@{
    /** Create Session
     */
    PBoolean CreateSession(PBoolean isMaster);

    /** Encode media key
      */
    void EncodeMediaKey(PBYTEArray & key);

    /** Decode media key
      */
    PBoolean DecodeMediaKey(PBYTEArray & key);

    /** Is Active 
      */
    PBoolean IsActive();

    /** Is Initialised
     */
    PBoolean IsInitialised();

    /** Read Frame
     */
    PBoolean ReadFrame(DWORD & rtpTimestamp, RTP_DataFrame & frame);

    /** Write Frame
     */
    PBoolean WriteFrame(RTP_DataFrame & frame);
  //@}

private:
    H235_DiffieHellman & m_dh;
    H235CryptoEngine     m_context;        /// Media encryption
    H235CryptoEngine     m_dhcontext;      /// Media key encryption
    PBoolean             m_isInitialised;  /// Is Initialised
    PBoolean             m_isMaster;

    PBYTEArray           m_dhSessionkey;
    PBYTEArray           m_crytoMasterKey;
    PBYTEArray           m_frameBuffer;
};

#endif // H235CRYPTO_H

