
/*
 * h235support.h
 *
 * H46026 Media Tunneling class.
 *
 * h323plus library
 *
 * Copyright (c) 2013 Spranto Australia Pty. Ltd.
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
 * The Initial Developer of the Original Code is Spranto Australia Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef H_H235Support
#define H_H235Support

#pragma once

#include <h235/h2351.h>

///////////////////////////////////////////////////////////////////////////////////////
/**Diffie-Hellman parameters.
   This class embodies a set of Diffie Helman parameters as used by
   PTLSContext and Secure Socket classes.
  */
struct dh_st;
struct bignum_st;
class PASN_BitString;
class H235_DiffieHellman : public PObject
{
  PCLASSINFO(H235_DiffieHellman, PObject);
public:

/**@name Constructor/Destructor */
//@{

    /**Create a set of Diffie-Hellman parameters.
      */
    H235_DiffieHellman(
      const BYTE * pData, /// P data
      PINDEX pSize,       /// Size of P data
      const BYTE * gData, /// G data
      PINDEX gSize,       /// Size of G data
      PBoolean send       /// Whether to send P & G values in Tokens
    );

    /**Create a set of Diffie-Hellman parameters from file.
       Data must be stored in INI format with the section being the OID of the Algorithm
        Parameter names are as below and all values are base64 encoded. eg:
           [ALG OID]
           PRIME=
           GENERATOR=
           PUBLIC=  <optional>
           PRIVATE= <optional>
      */
    H235_DiffieHellman(
        const PConfig  & dhFile,                ///< Config file
        const PString & section                 ///< section of config file
    );

    /**Create a copy of the Diffie-Hellman parameters. from 
       H235_DiffieHellman structure
      */
    H235_DiffieHellman(
      const H235_DiffieHellman & dh
    );

    /**Create a copy of the Diffie-Hellman parameters.
      */
    H235_DiffieHellman & operator=(
      const H235_DiffieHellman & dh
    );

    /**Destroy and release storage for Diffie-Hellman parameters.
      */
    ~H235_DiffieHellman();
//@}

/**@name Public Functions */
//@{
    /**Clone the parameters
      */
    virtual PObject * Clone() const;

    /**Get internal OpenSSL DH structure.
      */
    operator dh_st *() const { return dh; }

   /** Check Parameters */
    PBoolean CheckParams() const;

   /** SetRemotePublicKey */
    void SetRemoteKey(bignum_st * remKey);

   /** Generate Half Key */
    PBoolean GenerateHalfKey();

   /** Compute Session key */
    PBoolean ComputeSessionKey(PBYTEArray & SessionKey);

   /** Get the Public Key */
    bignum_st * GetPublicKey() const;

   /** Get the Key Length */
    int GetKeyLength() const;

   /**Load Diffie-Hellman parameters from file. */
    PBoolean Load(
        const PConfig & dhFile,                 ///< Config file
        const PString & section                 ///< section of config file
    );

   /**Are the parameters loaded from file ok. */
    PBoolean LoadedFromFile();

    /**Save Diffie-Hellman parameters to file. */
    PBoolean Save(
      const PFilePath & keyFile,                 ///< Diffie-Hellman parameter file
      const PString & oid                        ///< OID section
    );

//@}

/**@name Encoding for the H245 Stream */
//@{
    /** Encode Prime */
      PBoolean Encode_P(PASN_BitString & p) const;
    /** Decode Prime */
      void Decode_P(const PASN_BitString & p);
    /** Encode Generator */
      PBoolean Encode_G(PASN_BitString & g) const;
    /** Decode Generator */
      void Decode_G(const PASN_BitString & g);
    /** Encode Public Half Key */
      void Encode_HalfKey(PASN_BitString & hk) const;
    /** decode Public Half Key */
      void Decode_HalfKey(const PASN_BitString & hk);

//@}

/**@name Miscellaneous */
//@{
    /**Whether to send Prime and Generator. */
    PBoolean GetToSend() const { return m_toSend; }

    /**Get the key size */
    int GetKeySize() const { return m_keySize; }

    /**Whether parameters are loaded from file */
    PBoolean LoadFile() const { return m_loadFromFile; }
//@}

  protected:

    PMutex vbMutex;                   /// Mutex

    dh_st * dh;                       /// Local DiffieHellman
    bignum_st * m_remKey;             /// Remote Public Key
    PBoolean m_toSend;                /// Whether P & G are transmitted
    int m_keySize;                    /// Key Size

    PBoolean m_loadFromFile;          /// Whether the settings have been loaded from file
};


#endif // H_H235Support