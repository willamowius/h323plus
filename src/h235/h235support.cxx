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
#include <openssl/opensslv.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
};

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)

inline void DH_get0_pqg(const DH *dh,
                 const BIGNUM **p, const BIGNUM **q, const BIGNUM **g)
{
    if (p != NULL)
        *p = dh->p;
    if (q != NULL)
        *q = dh->q;
    if (g != NULL)
        *g = dh->g;
}

inline int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g)
{
    /* If the fields p and g in d are NULL, the corresponding input
     * parameters MUST be non-NULL.  q may remain NULL.
     */
    if ((dh->p == NULL && p == NULL)
        || (dh->g == NULL && g == NULL))
        return 0;

    if (p != NULL) {
        BN_free(dh->p);
        dh->p = p;
    }
    if (q != NULL) {
        BN_free(dh->q);
        dh->q = q;
    }
    if (g != NULL) {
        BN_free(dh->g);
        dh->g = g;
    }

    if (q != NULL) {
        dh->length = BN_num_bits(q);
    }

    return 1;
}

inline void DH_get0_key(const DH *dh, const BIGNUM **pub_key, const BIGNUM **priv_key)
{
    if (pub_key != NULL)
        *pub_key = dh->pub_key;
    if (priv_key != NULL)
        *priv_key = dh->priv_key;
}

inline int DH_set0_key(DH *dh, BIGNUM *pub_key, BIGNUM *priv_key)
{
    /* If the field pub_key in dh is NULL, the corresponding input
     * parameters MUST be non-NULL.  The priv_key field may
     * be left NULL.
     */
    if (dh->pub_key == NULL && pub_key == NULL)
        return 0;

    if (pub_key != NULL) {
        BN_free(dh->pub_key);
        dh->pub_key = pub_key;
    }
    if (priv_key != NULL) {
        BN_free(dh->priv_key);
        dh->priv_key = priv_key;
    }

    return 1;
}

#endif // OPENSSL_VERSION_NUMBER < 0x10100000L


////////////////////////////////////////////////////////////////////////////////////
// Diffie Hellman

H235_DiffieHellman::H235_DiffieHellman(const PConfig  & dhFile, const PString & section)
: dh(NULL), m_remKey(NULL), m_toSend(true), m_wasReceived(false), m_wasDHReceived(false), m_keySize(0), m_loadFromFile(false)
{
  if (Load(dhFile, section)) {
    const BIGNUM *pub_key = NULL;
    DH_get0_key(dh, &pub_key, NULL);
    if (pub_key == NULL) {
      GenerateHalfKey();
      DH_get0_key(dh, &pub_key, NULL);
    }
    m_keySize = BN_num_bytes(pub_key);
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
            const BIGNUM* dh_p = NULL;
            DH_get0_pqg(dh, &dh_p, NULL, NULL);
            m_keySize = BN_num_bits(dh_p);
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

    BIGNUM* p = BN_bin2bn(pData, pSize, NULL);
    BIGNUM* g = BN_bin2bn(gData, gSize, NULL);
    if (p != NULL && g != NULL) {
        DH_set0_pqg(dh, p, NULL, g);
        GenerateHalfKey();
        return;
    }

    if (g)
        BN_free(g);
    if (p)
        BN_free(p);
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

  const BIGNUM *p = NULL, *q = NULL, *g = NULL;
  DH_get0_pqg(dh, &p, &q, &g);

  if (p)
    p = BN_dup(p);
  if (q)
    q = BN_dup(q);
  if (g)
    g = BN_dup(g);
  DH_set0_pqg(ret, const_cast< BIGNUM* >(p), const_cast< BIGNUM* >(q), const_cast< BIGNUM* >(g));

  const BIGNUM *pub_key = NULL, *priv_key = NULL;
  DH_get0_key(dh, &pub_key, &priv_key);
  if (pub_key)
    pub_key = BN_dup(pub_key);
  if (priv_key)
    priv_key = BN_dup(priv_key);
  DH_set0_key(ret, const_cast< BIGNUM* >(pub_key), const_cast< BIGNUM* >(priv_key));

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
  if (m_remKey)
    BN_free(m_remKey);
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

  const BIGNUM *dh_p = NULL;
  DH_get0_pqg(dh, &dh_p, NULL, NULL);

  unsigned char * data = (unsigned char *)OPENSSL_malloc(BN_num_bytes(dh_p));
  if (data != NULL) {
    memset(data, 0, BN_num_bytes(dh_p));
    if (BN_bn2bin(dh_p, data) > 0) {
       p.SetData(BN_num_bits(dh_p), data);
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
  DH_set0_pqg(dh, BN_bin2bn(p.GetDataPointer(), p.GetDataLength() - 1, NULL), NULL, NULL);
}

PBoolean H235_DiffieHellman::Encode_G(PASN_BitString & g) const
{
    if (!m_toSend)
        return false;

    PWaitAndSignal m(vbMutex);

    const BIGNUM *dh_p = NULL, *dh_g = NULL;
    DH_get0_pqg(dh, &dh_p, NULL, &dh_g);

    int len_p = BN_num_bytes(dh_p);
    int len_g = BN_num_bytes(dh_g);
    int bits_p = BN_num_bits(dh_p);
    //int bits_g = BN_num_bits(dh_g); // unused

    if (len_p <= 128) { // Key lengths <= 1024 bits
        // Backwards compatibility G is padded out to the length of P
        unsigned char * data = (unsigned char *)OPENSSL_malloc(len_p);
        if (data != NULL) {
            memset(data, 0, len_p);
            if (BN_bn2bin(dh_g, data + len_p - len_g) > 0) {
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
            if (BN_bn2bin(dh_g, data) > 0) {
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
  DH_set0_pqg(dh, NULL, NULL, BN_bin2bn(g.GetDataPointer(), g.GetDataLength() - 1, NULL));
}


void H235_DiffieHellman::Encode_HalfKey(PASN_BitString & hk) const
{
  PWaitAndSignal m(vbMutex);

  const BIGNUM *dh_p = NULL;
  DH_get0_pqg(dh, &dh_p, NULL, NULL);
  const BIGNUM *pub_key = NULL;
  DH_get0_key(dh, &pub_key, NULL);

  int len_p = BN_num_bytes(dh_p);
  int len_key = BN_num_bytes(pub_key);
  int bits_p = BN_num_bits(dh_p);

  if (len_key > len_p) {
    PTRACE(1, "H235_DH\tFailed to encode halfkey: len key > len prime");
    return;
  }

  // halfkey is padded out to the length of P
  unsigned char * data = (unsigned char *)OPENSSL_malloc(len_p);
  if (data != NULL) {
    memset(data, 0, len_p);
    if (BN_bn2bin(pub_key, data + len_p - len_key) > 0) {
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
  DH_set0_key(dh, BN_bin2bn(data, hk.GetDataLength() - 1, NULL), NULL);
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
  if (dh) {
    const BIGNUM *pub_key = NULL;
    DH_get0_key(dh, &pub_key, NULL);
    if (pub_key)
      return true;
  }

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
  BIGNUM *dh_p = NULL, *dh_g = NULL, *pub_key = NULL, *priv_key = NULL;

  PBoolean ok = true;
  if (dhFile.HasKey(section, "PRIME")) {
    str = dhFile.GetString(section, "PRIME", "");
    PBase64::Decode(str, data);
    dh_p = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    ok = ok && (BN_num_bytes(dh_p) > 0);
  } else
    ok = false;

  if (dhFile.HasKey(section, "GENERATOR")) {
    str = dhFile.GetString(section, "GENERATOR", "");
    PBase64::Decode(str, data);
    PBYTEArray temp(1);
    memcpy(temp.GetPointer(), data.GetPointer(), 1);
    memset(data.GetPointer(), 0, data.GetSize());
    memcpy(data.GetPointer() + data.GetSize()-1, temp.GetPointer(), 1);
    dh_g = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    ok = ok && (BN_num_bytes(dh_g) > 0);
  } else
    ok = false;

  if (dhFile.HasKey(section, "PUBLIC")) {
    str = dhFile.GetString(section, "PUBLIC", "");
    PBase64::Decode(str, data);
    pub_key = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    ok = ok && (BN_num_bytes(pub_key) > 0);
  }

  if (dhFile.HasKey(section, "PRIVATE")) {
    str = dhFile.GetString(section, "PRIVATE", "");
    PBase64::Decode(str, data);
    priv_key = BN_bin2bn(data.GetPointer(), data.GetSize(), NULL);
    ok = ok && (BN_num_bytes(priv_key) > 0);
  }

  if (ok) {
    if (DH_set0_pqg(dh, dh_p, NULL, dh_g)) {
      dh_p = NULL;
      dh_g = NULL;
    } else {
      ok = false;
    }
  }

  if (ok) {
    if (DH_set0_key(dh, pub_key, priv_key)) {
      pub_key = NULL;
      priv_key = NULL;
    } else {
      ok = false;
    }
  }

  if (ok /*&& CheckParams()*/)
    m_loadFromFile = true;
  else {
    if (priv_key)
      BN_free(priv_key);
    if (pub_key)
      BN_free(pub_key);
    if (dh_g)
      BN_free(dh_g);
    if (dh_p)
      BN_free(dh_p);
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
  if (!dh)
    return false;

  const BIGNUM *pub_key = NULL, *priv_key = NULL;
  DH_get0_key(dh, &pub_key, &priv_key);
  if (!pub_key)
    return false;

  const BIGNUM *dh_p = NULL, *dh_g = NULL;
  DH_get0_pqg(dh, &dh_p, NULL, &dh_g);

  PConfig config(dhFile, oid);
  PString str = PString();
  int len = BN_num_bytes(pub_key);
  unsigned char * data = (unsigned char *)OPENSSL_malloc(len);

  if (data != NULL && BN_bn2bin(dh_p, data) > 0) {
    str = PBase64::Encode(data, len, "");
    config.SetString("PRIME",str);
  }
  OPENSSL_free(data);

  data = (unsigned char *)OPENSSL_malloc(len);
  if (data != NULL && BN_bn2bin(dh_g, data) > 0) {
    str = PBase64::Encode(data, len, "");
    config.SetString("GENERATOR",str);
  }
  OPENSSL_free(data);

  data = (unsigned char *)OPENSSL_malloc(len);
  if (data != NULL && BN_bn2bin(pub_key, data) > 0) {
    str = PBase64::Encode(data, len, "");
    config.SetString("PUBLIC",str);
  }
  OPENSSL_free(data);

  data = (unsigned char *)OPENSSL_malloc(len);
  if (data != NULL && BN_bn2bin(priv_key, data) > 0) {
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
  const BIGNUM *pub_key = NULL;
  DH_get0_key(dh, &pub_key, NULL);
  return const_cast< BIGNUM* >(pub_key);
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

    const BIGNUM *vdh_p = NULL, *vdh_g = NULL;
    DH_get0_pqg(vdh, &vdh_p, NULL, &vdh_g);

    PString str = PString();
    int len = BN_num_bytes(vdh_p);
    unsigned char * data = (unsigned char *)OPENSSL_malloc(len);

    if (data != NULL && BN_bn2bin(vdh_p, data) > 0) {
        str = PBase64::Encode(data, len, "");
        parameters.SetAt("PRIME", str);
    }
    OPENSSL_free(data);

    len = BN_num_bytes(vdh_g);
    data = (unsigned char *)OPENSSL_malloc(len);
    if (data != NULL && BN_bn2bin(vdh_g, data) > 0) {
        str = PBase64::Encode(data, len, "");
        parameters.SetAt("GENERATOR", str);
    }
    OPENSSL_free(data);

    DH_free(vdh);
}


#endif  // H323_H235

