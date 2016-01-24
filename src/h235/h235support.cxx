/*
 * h235support.cxx
 *
 * H235 Support Libraries.
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


#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

#include "h235/h235support.h"
#include <h235.h>
#include <ptclib/cypher.h>

extern "C" {
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
};

////////////////////////////////////////////////////////////////////////////////////
// Diffie Hellman

H235_DiffieHellman::H235_DiffieHellman(const PConfig  & dhFile, const PString & section)
: dh(NULL), m_remKey(NULL), m_toSend(true), m_wasReceived(false), m_wasDHReceived(false), m_keySize(0), m_loadFromFile(false)
{
  if (Load(dhFile, section)) {
    if (dh->pub_key == NULL) {
      GenerateHalfKey();
    }
    m_keySize = BN_num_bytes(dh->pub_key);
  }
}

H235_DiffieHellman::H235_DiffieHellman(const PFilePath & dhPKCS3)
: dh(NULL), m_remKey(NULL), m_toSend(true), m_wasReceived(false), m_wasDHReceived(false), m_keySize(0), m_loadFromFile(false)
{
    FILE *paramfile;
    paramfile = fopen(dhPKCS3, "r");
    if (paramfile) {
        dh = PEM_read_DHparams(paramfile, NULL, NULL, NULL);
        fclose(paramfile);
        if (dh) {
            m_keySize = BN_num_bits(dh->p);
            m_loadFromFile = true;
        }
    }
}

H235_DiffieHellman::H235_DiffieHellman(const BYTE * pData, PINDEX pSize,
                                     const BYTE * gData, PINDEX gSize, 
                                     PBoolean send)
: m_remKey(NULL), m_toSend(send), m_wasReceived(false), m_wasDHReceived(false), m_keySize(pSize), m_loadFromFile(false)
{
  dh = DH_new();
  if (dh == NULL) {
    PTRACE(1, "H235_DH\tFailed to allocate DH");
    return;
  };

    dh->p = BN_bin2bn(pData, pSize, NULL);
    dh->g = BN_bin2bn(gData, gSize, NULL);
    if (dh->p != NULL && dh->g != NULL) {
        GenerateHalfKey();
        return;
    }

    PTRACE(1, "H235_DH\tFailed to generate half key");
    DH_free(dh);
    dh = NULL;
}

static DH * DH_dup(const DH * dh)
{
  if (dh == NULL)
    return NULL;

  DH * ret = DH_new();
  if (ret == NULL)
    return NULL;

  if (dh->p)
    ret->p = BN_dup(dh->p);
  if (dh->q)
    ret->q = BN_dup(dh->q);
  if (dh->g)
    ret->g = BN_dup(dh->g);
  if (dh->pub_key)
    ret->pub_key = BN_dup(dh->pub_key);
  if (dh->priv_key)
    ret->priv_key = BN_dup(dh->priv_key);

  return ret;
}

H235_DiffieHellman::H235_DiffieHellman(const H235_DiffieHellman & diffie)
: m_remKey(NULL), m_toSend(diffie.GetToSend()), m_wasReceived(ReceivedFromRemote()), m_wasDHReceived(diffie.DHReceived()), 
  m_keySize(diffie.GetKeySize()), m_loadFromFile(diffie.LoadFile())
{
  dh = DH_dup(diffie);
}


H235_DiffieHellman & H235_DiffieHellman::operator=(const H235_DiffieHellman & other)
{
  if (this != &other) {
    if (dh)
      DH_free(dh);
    dh = DH_dup(other);
    m_remKey = NULL;
    m_toSend = other.GetToSend();
    m_wasReceived = ReceivedFromRemote();
    m_wasDHReceived = other.DHReceived();
    m_keySize = other.GetKeySize();
    m_loadFromFile = other.LoadFile();
  }
  return *this;
}

H235_DiffieHellman::~H235_DiffieHellman()
{
  if (dh)
    DH_free(dh);
}

PObject * H235_DiffieHellman::Clone() const
{
  return new H235_DiffieHellman(*this);
}

PBoolean H235_DiffieHellman::CheckParams() const
{
  // TODO: FIX so it actually checks the whole DH 
  // including the strength of the DH Public/Private key pair - SH
  // currently it only checks p and g which are supplied by the standard

  PWaitAndSignal m(vbMutex);

  int i;
  if (!DH_check(dh, &i))
  {
    switch (i) {
     case DH_CHECK_P_NOT_PRIME:
         PTRACE(1, "H235_DH\tCHECK: p value is not prime");
         break;
     case DH_CHECK_P_NOT_SAFE_PRIME:
         PTRACE(1, "H235_DH\tCHECK: p value is not a safe prime");
         break;
     case DH_UNABLE_TO_CHECK_GENERATOR:
         PTRACE(1, "H235_DH\tCHECK: unable to check the generator value");
         break;
     case DH_NOT_SUITABLE_GENERATOR:
         PTRACE(1, "H235_DH\tCHECK: the g value is not a generator");
         break;
    }
    return FALSE;
  }

  return TRUE;
}

PBoolean H235_DiffieHellman::Encode_P(PASN_BitString & p) const
{
  PWaitAndSignal m(vbMutex);

  if (!m_toSend)
    return false;

  unsigned char * data = (unsigned char *)OPENSSL_malloc(BN_num_bytes(dh->p));
  if (data != NULL) {
    memset(data, 0, BN_num_bytes(dh->p));
    if (BN_bn2bin(dh->p, data) > 0) {
       p.SetData(BN_num_bits(dh->p), data);
    } else {
      PTRACE(1, "H235_DH\tFailed to encode P");
      OPENSSL_free(data);
      return false;
    }
  }
  OPENSSL_free(data);
  return true;
}

void H235_DiffieHellman::Decode_P(const PASN_BitString & p)
{
  if (p.GetSize() == 0)
    return;

  PWaitAndSignal m(vbMutex);
  const unsigned char * data = p.GetDataPointer();
  if (dh->p)
    BN_free(dh->p);
  dh->p = BN_bin2bn(data, p.GetDataLength() - 1, NULL);
}

PBoolean H235_DiffieHellman::Encode_G(PASN_BitString & g) const
{
    if (!m_toSend)
        return false;

    PWaitAndSignal m(vbMutex);
    int len_p = BN_num_bytes(dh->p);
    int len_g = BN_num_bytes(dh->g);
    int bits_p = BN_num_bits(dh->p);
    //int bits_g = BN_num_bits(dh->g); // unused

    if (len_p <= 128) { // Key lengths <= 1024 bits
        // Backwards compatibility G is padded out to the length of P
        unsigned char * data = (unsigned char *)OPENSSL_malloc(len_p);
        if (data != NULL) {
            memset(data, 0, len_p);
            if (BN_bn2bin(dh->g, data + len_p - len_g) > 0) {
                g.SetData(bits_p, data);
            }
            else {
                PTRACE(1, "H235_DH\tFailed to encode G");
                OPENSSL_free(data);
                return false;
            }
        }
        OPENSSL_free(data);
   } else {
        unsigned char * data = (unsigned char *)OPENSSL_malloc(len_g);
        if (data != NULL) {
            memset(data, 0, len_g);
            if (BN_bn2bin(dh->g, data) > 0) {
                g.SetData(8, data);
            }
            else {
                PTRACE(1, "H235_DH\tFailed to encode P");
                OPENSSL_free(data);
                return false;
            }
        }
        OPENSSL_free(data);
    }  
  return true;
}

void H235_DiffieHellman::Decode_G(const PASN_BitString & g)
{
  if (g.GetSize() == 0)
    return;

  PWaitAndSignal m(vbMutex);
  if (dh->g)
    BN_free(dh->g);
  dh->g = BN_bin2bn(g.GetDataPointer(), g.GetDataLength() - 1, NULL);
}


void H235_DiffieHellman::Encode_HalfKey(PASN_BitString & hk) const
{
  PWaitAndSignal m(vbMutex);

  int len_p = BN_num_bytes(dh->p);
  int len_key = BN_num_bytes(dh->pub_key);
  int bits_p = BN_num_bits(dh->p);

  if (len_key > len_p) {
    PTRACE(1, "H235_DH\tFailed to encode halfkey: len key > len prime");
    return;
  }

  // halfkey is padded out to the length of P
  unsigned char * data = (unsigned char *)OPENSSL_malloc(len_p);
  if (data != NULL) {
    memset(data, 0, len_p);
    if (BN_bn2bin(dh->pub_key, data + len_p - len_key) > 0) {
       hk.SetData(bits_p, data);
    } else {
      PTRACE(1, "H235_DH\tFailed to encode halfkey");
    }
  }
  OPENSSL_free(data);
}

void H235_DiffieHellman::Decode_HalfKey(const PASN_BitString & hk)
{
  PWaitAndSignal m(vbMutex);

  const unsigned char *data = hk.GetDataPointer();
  if (dh->pub_key)
    BN_free(dh->pub_key);
  dh->pub_key = BN_bin2bn(data, hk.GetDataLength() - 1, NULL);
}

void H235_DiffieHellman::SetRemoteKey(bignum_st * remKey)
{
  m_remKey = remKey;
}

void H235_DiffieHellman::SetRemoteHalfKey(const PASN_BitString & hk)
{
  const unsigned char *data = hk.GetDataPointer();
  if (m_remKey)
    BN_free(m_remKey);
  m_remKey = BN_bin2bn(data, hk.GetDataLength() - 1, NULL);

  if (m_remKey)
    m_wasReceived = true; 
}

PBoolean H235_DiffieHellman::GenerateHalfKey()
{
  if (dh && dh->pub_key)
    return true;

  PWaitAndSignal m(vbMutex);

  if (!DH_generate_key(dh)) {
      char buf[256];
      ERR_error_string(ERR_get_error(), buf);
      PTRACE(1, "H235_DH\tERROR generating DH halfkey " << buf);
      return FALSE;
  }

  return TRUE;
}

void H235_DiffieHellman::SetDHReceived(const PASN_BitString & p, const PASN_BitString & g) 
{
    PTRACE(4, "H235\tReplacing local DH parameters with those of remote");

    Decode_P(p);
    Decode_G(g);
    m_wasDHReceived = true; 
}

PBoolean H235_DiffieHellman::Load(const PConfig  & dhFile, const PString & section)
{
  if (dh) {
    DH_free(dh);
    dh = NULL;
  }

  dh = DH_new();
  if (dh == NULL)
    return false;

  PString str;
  PBYTEArray data;

  PBoolean ok = true;
  if (dhFile.HasKey(section, "PRIME")) {
    str = dhFile.GetString(section, "PRIME", "");
    PBase64::Decode(str, data);
    dh->p = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    ok = ok && (BN_num_bytes(dh->p) > 0);
  } else 
    ok = false;

  if (dhFile.HasKey(section, "GENERATOR")) {
    str = dhFile.GetString(section, "GENERATOR", "");
    PBase64::Decode(str, data);
    PBYTEArray temp(1);
    memcpy(temp.GetPointer(), data.GetPointer(), 1);
    memset(data.GetPointer(), 0, data.GetSize());
    memcpy(data.GetPointer() + data.GetSize()-1, temp.GetPointer(), 1);
    dh->g = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    ok = ok && (BN_num_bytes(dh->g) > 0);
  } else 
    ok = false;

  if (dhFile.HasKey(section, "PUBLIC")) {
    str = dhFile.GetString(section, "PUBLIC", "");
    PBase64::Decode(str, data);
    dh->pub_key = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    ok = ok && (BN_num_bytes(dh->pub_key) > 0);
  }
  
  if (dhFile.HasKey(section, "PRIVATE")) {
    str = dhFile.GetString(section, "PRIVATE", "");
    PBase64::Decode(str, data);
    dh->priv_key = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    ok = ok && (BN_num_bytes(dh->priv_key) > 0);
  }

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
  if (!dh || !dh->pub_key)
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

void H235_DiffieHellman::Generate(PINDEX keyLength, PINDEX keyGenerator, PStringToString & parameters)
{
    PString lOID;
    for (PINDEX i = 0; i < PARRAYSIZE(H235_DHCustom); ++i) {
        if (H235_DHCustom[i].sz == (unsigned)keyLength) {
            lOID = H235_DHCustom[i].parameterOID;
            break;
        }
    }

    if (lOID.IsEmpty())
        return;

    dh_st * vdh = DH_new();
    if (!DH_generate_parameters_ex(vdh, keyLength, keyGenerator, NULL)) {
        cout << "Error generating Key Pair\n";
        DH_free(vdh);
        vdh = NULL;
        return;
    }

    parameters.SetAt("OID", lOID);

    PString str = PString();
    int len = BN_num_bytes(vdh->p);
    unsigned char * data = (unsigned char *)OPENSSL_malloc(len);

    if (data != NULL && BN_bn2bin(vdh->p, data) > 0) {
        str = PBase64::Encode(data, len, "");
        parameters.SetAt("PRIME", str);
    }
    OPENSSL_free(data);

    len = BN_num_bytes(vdh->g);
    data = (unsigned char *)OPENSSL_malloc(len);
    if (data != NULL && BN_bn2bin(vdh->g, data) > 0) {
        str = PBase64::Encode(data, len, "");
        parameters.SetAt("GENERATOR", str);
    }
    OPENSSL_free(data);

    DH_free(vdh);
}


#endif  // H323_H235

