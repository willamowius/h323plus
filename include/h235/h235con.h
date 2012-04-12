/*
 * h235con.h
 *
 * H.235 Encryption Context definitions class.
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
 */

#ifndef H235CON_H
#define H235CON_H

// for definition of EVP_CIPHER_CTX
#include <openssl/evp.h>

struct ssl_st;
struct ssl_ctx_st;
struct ssl_session_st;
class PASN_OctetString;
class RTP_DataFrame;
class H235Context;
class H235_DiffieHellman;
class H235Capabilities;

class H235MediaSession : public PObject
{
    PCLASSINFO(H235MediaSession, PObject);

public:

 /**@name Constructor */
  //@{
    /** Create a H.235 media session
     */
    H235MediaSession(const PString & sslAlgorithm, const PBYTEArray & key);

   /** Destroy the media session
     */
	~H235MediaSession();
  //@}

    /** Set media key
      */
    void SetKey(const PBYTEArray & key);

    /** Encode data
      */
    PBYTEArray Encode(const PBYTEArray & data);

    /** Decode data
      */
    PBYTEArray Decode(const PBYTEArray & data);

protected:
	EVP_CIPHER_CTX encode_ctx, decode_ctx;
};


class H235Session : public  PObject
{
	 PCLASSINFO(H235Session, PObject);

public:

 /**@name Constructor */
  //@{
    /** Create a SSL Session Context
     */
    H235Session(H235Context & context, H235_DiffieHellman & dh, const PString & sslAlgorithm);
    H235Session(H235Capabilities * caps,  const PString & oidAlgorithm);

   /** Destroy the SSL Session Context
     */
	~H235Session();
  //@}

 /**@name General Public Functions */
  //@{
    /** Create Session - call SetMediaKey() before!
     */
    PBoolean CreateSession();

    /** Set media key
      */
    void SetMediaKey(const PBYTEArray & key);

    /** Get media key
      */
    const PBYTEArray & GetMediaKey();

    /** Encode media key
      */
    void EncodeMediaKey(PBYTEArray & key);

    /** Decode media key
      */
    void DecodeMediaKey(PBYTEArray & key);

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

protected:

	/** Raw Read a Encrypted DataFrame from SSL */ 
	unsigned char * RawRead(unsigned char * buffer,int & length);

	/** Raw Write a unEncrypted DataFrame to SSL */
	unsigned char * RawWrite(unsigned char * buffer,int & length);

    /** Set Cipher */
    void SetCipher(const PString & oid);

    /** put media key into OpenSSL session - called in CreateSession()  */
    PBoolean InitMediaKey();

private:
    H235_DiffieHellman & m_dh;
    PString              m_algorithm;
    H235Context        & m_context;
    ssl_st             * m_ssl;		       /// SSL Object
	ssl_session_st     * m_session;        /// SSL Session Object
	static int session_count;              /// Session Count
	int                  m_session_id;     /// Session Identifier.
    PBYTEArray           m_session_key;    /// Session Key
    PBoolean             m_isServer;       /// Server Cipher Mode
    PBoolean             m_isInitialised;  /// Is Initialised
};



/**Context for SSL Connections.
  */
struct ssl_st;
struct ssl_ctx_st;
class H235Context : public PObject 
{
	PCLASSINFO(H235Context, PObject);

  public:

    /**Create a new context for TLS/SSL channels.
      */
    H235Context();

    /**Clean up the TLS/SSL context.
      */
    ~H235Context();

    /**Initialise the TLS/SSL context
      */
    void Initialise();

    /**Context Active
      */
    PBoolean IsActive();

    /**Get the internal TLS/SSL context structure.
      */
    operator ssl_ctx_st *() const { return m_context; }

    /**Get context structure.
      */
    ssl_ctx_st * GetContext() const { return m_context; }

    /**Generate session ID.
      */
    int Generate_Session_Id(const ssl_st * ssl, unsigned char *id, unsigned int *id_len);

  protected:
    void RandomSeed();

  private:

    PBoolean       m_isActive;
    ssl_ctx_st *   m_context;		/// Context Container

};

#endif // H235CON_H

