/*
 * h2356.h
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
#include <ptlib/pluginmgr.h>
#include <h235.h>
#include <h235auth.h>

#ifdef H323_H235

#pragma once

class H235_DiffieHellman;
class H2356_Authenticator : public H235Authenticator
{
    PCLASSINFO(H2356_Authenticator, H235Authenticator);
  public:

	H2356_Authenticator();

	~H2356_Authenticator();

    virtual const char * GetName() const;

    virtual PBoolean PrepareTokens(
      PASN_Array & clearTokens,
      PASN_Array & cryptoTokens
    );

    virtual ValidationResult ValidateTokens(
      const PASN_Array & clearTokens,
      const PASN_Array & cryptoTokens,
      const PBYTEArray & rawPDU
    );

    virtual PBoolean IsCapability(
      const H235_AuthenticationMechanism & mechansim,
      const PASN_ObjectId & algorithmOID
    );

    virtual PBoolean SetCapability(
      H225_ArrayOf_AuthenticationMechanism & mechansim,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    virtual PBoolean IsSecuredSignalPDU(
      unsigned signalPDU,
      PBoolean received
    ) const;

    virtual PBoolean IsActive() const;
    virtual void Disable();

protected:
    H235_DiffieHellman * m_dh;

    PBoolean m_enabled;
    PBoolean m_active;
	  
};

////////////////////////////////////////////////////
/// PFactory Loader
static PFactory<H235Authenticator>::Worker<H2356_Authenticator> H2356_AuthenticatorFactory("H2356_Encryption");


///////////////////////////////////////////////////////////////////////////////////////
/**Diffie-Hellman parameters.
   This class embodies a set of Diffie Helman parameters as used by
   PTLSContext and Secure Socket classes.
  */
struct dh_st;
class H235_DiffieHellman : public PObject
{
  PCLASSINFO(H235_DiffieHellman, PObject);
public:

/**@name Constructor/Destructor */
//@{

    /**Create new Diffie-Hellman parameters (Used for Remote DH Parameters)
      */
    H235_DiffieHellman();

    /**Create a set of Diffie-Hellman parameters.
      */
    H235_DiffieHellman(
      const BYTE * pData, /// P data
      PINDEX pSize,       /// Size of P data
      const BYTE * gData, /// G data
      PINDEX gSize        /// Size of G data
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
    /**Get internal OpenSSL DH structure.
      */
    operator dh_st *() const { return dh; }

	/** Last Error Buffer*/
	PString GetLastError() { return LastError; }

   /** Check Parameters */
	PBoolean CheckParams();

   /** Generate Half Key */
	PBoolean GenerateHalfKey();

//@}

/**@name Encoding for the H245 Stream */
//@{
	/** Encode Prime */
	  void Encode_P(PASN_BitString & p);
	/** Decode Prime */
	  void Decode_P(PASN_BitString & p);
	/** Encode Generator */
	  void Encode_G(PASN_BitString & g);
	/** Decode Generator */
	  void Decode_G(PASN_BitString & g);
	/** Encode Public Half Key */
	  void Encode_HalfKey(PASN_BitString & hk);
	/** decode Public Half Key */
	  void Decode_HalfKey(PASN_BitString & hk);

//@}


  protected:

/**@name Protected Functions */
//@{
    /**Create Diffie-Hellman parameters from Scratch.
	Note: This function was built from the easy-tls example of the OpenSSL source code.
      */
	PBoolean CreateParams();
//@}

    PMutex vbMutex;				/// Mute

    dh_st * dh;
	int ParamSize;					  /// Key Size
	PString LastError;				  /// Last Error String;
};

#endif // H323_H235