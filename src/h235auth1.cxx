/*
 * h235auth1.cxx
 *
 * H.235 security PDU's
 *
 * H323Plus Library
 *
 * Copyright (c) 1998-2001 Equivalence Pty. Ltd.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): Fürbass Franz <franz.fuerbass@infonova.at>
 *
 * $Id$
 *
 */

#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_SSL

#include <openssl/opensslv.h>
#include <openssl/evp.h>

#include "h235auth.h"
#include "h323pdu.h"

#ifdef _MSC_VER
#pragma comment(lib, P_SSL_LIB1)
#pragma comment(lib, P_SSL_LIB2)
#endif


#define REPLY_BUFFER_SIZE 1024


static const char OID_A[] = "0.0.8.235.0.2.1";
static const char OID_T[] = "0.0.8.235.0.2.5";
static const char OID_U[] = "0.0.8.235.0.2.6";

#define OID_VERSION_OFFSET 5


#define HASH_SIZE 12

static const BYTE SearchPattern[HASH_SIZE] = { // Must be 12 bytes
  't', 'W', 'e', 'l', 'V', 'e', '~', 'b', 'y', 't', 'e', 'S'
};

#ifndef SHA_DIGESTSIZE
#define SHA_DIGESTSIZE  20
#endif

#ifndef SHA_BLOCKSIZE
#define SHA_BLOCKSIZE   64
#endif

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)

namespace {

class EvpMdContext
{
  mutable EVP_MD_CTX ctx;

public:
  EvpMdContext()
  {
    EVP_MD_CTX_init(&ctx);
  }
  ~EvpMdContext()
  {
    EVP_MD_CTX_cleanup(&ctx);
  }
  operator EVP_MD_CTX* () const
  {
    return &ctx;
  }
};

} // namespace

#else // (OPENSSL_VERSION_NUMBER < 0x10100000L)

namespace {

class EvpMdContext
{
  EVP_MD_CTX* ctx;

public:
  EvpMdContext()
  {
    ctx = EVP_MD_CTX_new();
    OPENSSL_assert(ctx != NULL);
  }
  ~EvpMdContext()
  {
    EVP_MD_CTX_free(ctx);
  }
  operator EVP_MD_CTX* () const
  {
    return ctx;
  }
};

} // namespace

#endif // (OPENSSL_VERSION_NUMBER < 0x10100000L)

#define new PNEW


/////////////////////////////////////////////////////////////////////////////

/* Function to print the digest */
#if 0
static void pr_sha(FILE* fp, char* s, int t)
{
        int     i ;

        fprintf(fp, "0x") ;
        for (i = 0 ; i < t ; i++)
                fprintf(fp, "%02x", s[i]) ;
        fprintf(fp, "0x") ;
}
#endif

static void truncate(unsigned char*    d1,    /* data to be truncated */
                     char*             d2,    /* truncated data */
                     int               len)   /* length in bytes to keep */
{
  for (int i = 0; i < len; i++)
    d2[i] = d1[i];
}

/* Function to compute the digest */
static void hmac_sha (const unsigned char*    k,      /* secret key */
                      int      lk,              /* length of the key in bytes */
                      const unsigned char*    d,      /* data */
                      int      ld,              /* length of data in bytes */
                      char*    out,             /* output buffer, at least "t" bytes */
                      int      t)
{
        EvpMdContext ictx, octx;
        unsigned char isha[SHA_DIGESTSIZE], osha[SHA_DIGESTSIZE];
        unsigned char key[SHA_DIGESTSIZE];
        char buf[SHA_BLOCKSIZE];
        int i;

        const EVP_MD * sha1 = EVP_sha1();

        if (lk > SHA_BLOCKSIZE) {

                EvpMdContext tctx;

                EVP_DigestInit_ex(tctx, sha1, NULL);
                EVP_DigestUpdate(tctx, k, lk);
                EVP_DigestFinal_ex(tctx, key, NULL);

                k = key;
                lk = SHA_DIGESTSIZE;
        }

        /**** Inner Digest ****/

        EVP_DigestInit_ex(ictx, sha1, NULL);

        /* Pad the key for inner digest */
        for (i = 0 ; i < lk ; ++i) buf[i] = (char)(k[i] ^ 0x36);
        for (i = lk ; i < SHA_BLOCKSIZE ; ++i) buf[i] = 0x36;

        EVP_DigestUpdate(ictx, buf, SHA_BLOCKSIZE);
        EVP_DigestUpdate(ictx, d, ld) ;

        EVP_DigestFinal_ex(ictx, isha, NULL);

        /**** Outer Digest ****/

        EVP_DigestInit_ex(octx, sha1, NULL);

        /* Pad the key for outer digest */

        for (i = 0 ; i < lk ; ++i) buf[i] = (char)(k[i] ^ 0x5C);
        for (i = lk ; i < SHA_BLOCKSIZE ; ++i) buf[i] = 0x5C;

        EVP_DigestUpdate(octx, buf, SHA_BLOCKSIZE);
        EVP_DigestUpdate(octx, isha, SHA_DIGESTSIZE);

        EVP_DigestFinal_ex(octx, osha, NULL);

        /* truncate and print the results */
        t = t > SHA_DIGESTSIZE ? SHA_DIGESTSIZE : t;
        truncate(osha, out, t);

}

static void SHA1(const unsigned char * data, unsigned len, unsigned char * hash)
{
  const EVP_MD * sha1 = EVP_sha1();
  EvpMdContext ctx;
  if (EVP_DigestInit_ex(ctx, sha1, NULL)) {
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, hash, NULL);
  } else {
    PTRACE(1, "H235\tOpenSSH SHA1 implementation failed");
  }
}


/////////////////////////////////////////////////////////////////////////////

#if PTLIB_VER >= 2110
typedef H2351_Authenticator H235AuthenticatorStd1;
PPLUGIN_STATIC_LOAD(Std1,H235Authenticator);
H235SECURITY(Std1);
#endif

H2351_Authenticator::H2351_Authenticator()
{
  usage = AnyApplication; // Can be used either for GKAdmission or EPAuthenticstion
  m_requireGeneralID = true;
  m_checkSendersID = true;
  //m_fullQ931Checking = true; // H.235.1 clause 13.2 requires full Q.931 checking
  m_fullQ931Checking = false; // remain compatible with old versions for now
  m_verifyRandomNumber = true; // switch off check for possible bug in ASN decoder
}


PObject * H2351_Authenticator::Clone() const
{
  H2351_Authenticator * auth = new H2351_Authenticator(*this);

  // We do NOT copy these fields in Clone()
  auth->lastRandomSequenceNumber = 0;
  auth->lastTimestamp = 0;

  return auth;
}


const char * H2351_Authenticator::GetName() const
{
  return "H.235.1";
}

PStringArray H2351_Authenticator::GetAuthenticatorNames()
{
    return PStringArray("Std1");
}

#if PTLIB_VER >= 2110
PBoolean H2351_Authenticator::GetAuthenticationCapabilities(H235Authenticator::Capabilities * ids)
{
  H235Authenticator::Capability cap;
  cap.m_identifier = OID_A;
  cap.m_cipher     = "H2351";
  cap.m_description= "h2351";
  ids->capabilityList.push_back(cap);
  return true;
}
#endif

PBoolean H2351_Authenticator::IsMatch(const PString & identifier) const
{
  return (identifier == PString(OID_A));
}

PString AsString(BYTE* data, unsigned size) {
	PString result;
	unsigned i = 0;
	while (i < size) {
		result.sprintf("%0x", data[i]);
		result += " ";
		i++;
	}
	return result;
}

H225_CryptoH323Token * H2351_Authenticator::CreateCryptoToken()
{
  if (!IsActive())
    return NULL;

  H225_CryptoH323Token * cryptoToken = new H225_CryptoH323Token;

  // Create the H.225 crypto token in the H323 crypto token
  cryptoToken->SetTag(H225_CryptoH323Token::e_nestedcryptoToken);
  H235_CryptoToken & nestedCryptoToken = *cryptoToken;

  // We are doing hashed password
  nestedCryptoToken.SetTag(H235_CryptoToken::e_cryptoHashedToken);
  H235_CryptoToken_cryptoHashedToken & cryptoHashedToken = nestedCryptoToken;

  // tokenOID = "A"
  cryptoHashedToken.m_tokenOID = OID_A;

  // ClearToken
  H235_ClearToken & clearToken = cryptoHashedToken.m_hashedVals;

  // tokenOID = "T"
  clearToken.m_tokenOID  = OID_T;

  if (!remoteId) {
    clearToken.IncludeOptionalField(H235_ClearToken::e_generalID);
    clearToken.m_generalID = remoteId;
  }

  if (!localId) {
    clearToken.IncludeOptionalField(H235_ClearToken::e_sendersID);
    clearToken.m_sendersID = localId;
  }

  clearToken.IncludeOptionalField(H235_ClearToken::e_timeStamp);
  clearToken.m_timeStamp = (int)PTime().GetTimeInSeconds();

  clearToken.IncludeOptionalField(H235_ClearToken::e_random);
  clearToken.m_random = ++sentRandomSequenceNumber;

  // H235_HASHED
  H235_HASHED<H235_EncodedGeneralToken> & encodedToken = cryptoHashedToken.m_token;

  //  algorithmOID = "U"
  encodedToken.m_algorithmOID = OID_U;


  /*******
   * step 1
   *
   * set a pattern for the hash value
   *
   */

  encodedToken.m_hash.SetData(HASH_SIZE*8, SearchPattern);
  return cryptoToken;
}


PBoolean H2351_Authenticator::Finalise(PBYTEArray & rawPDU)
{
  if (!IsActive())
    return FALSE;

  // Find the pattern

  int foundat = -1;
  for (PINDEX i = 0; i <= rawPDU.GetSize() - HASH_SIZE; i++) {
    if (memcmp(&rawPDU[i], SearchPattern, HASH_SIZE) == 0) { // i'v found it !
      foundat = i;
      break;
    }
  }

  if (foundat == -1) {
    //Can't find the search pattern in the ASN1 packet.
    PTRACE(2, "H235RAS\tPDU not prepared for H2351_Authenticator");
    return FALSE;
  }

  // Zero out the search pattern
  memset(&rawPDU[foundat], 0, HASH_SIZE);

 /*******
  *
  * generate a HMAC-SHA1 key over the hole message
  * and save it in at (step 3) located position.
  * in the asn1 packet.
  */

  char key[HASH_SIZE];

  /** make a SHA1 hash before send to the hmac_sha1 */
  unsigned char secretkey[20];

  SHA1(password, password.GetLength(), secretkey);

  hmac_sha(secretkey, 20, rawPDU.GetPointer(), rawPDU.GetSize(), key, HASH_SIZE);

  memcpy(&rawPDU[foundat], key, HASH_SIZE);

  PTRACE(4, "H235RAS\tH2351_Authenticator hashing completed: \"" << password << '"');
  return TRUE;
}


static PBoolean CheckOID(const PASN_ObjectId & oid1, const PASN_ObjectId & oid2)
{
  if (oid1.GetSize() != oid2.GetSize())
    return FALSE;

  PINDEX i;
  for (i = 0; i < OID_VERSION_OFFSET; i++) {
    if (oid1[i] != oid2[i])
      return FALSE;
  }

  for (i++; i < oid1.GetSize(); i++) {
    if (oid1[i] != oid2[i])
      return FALSE;
  }

  return TRUE;
}

H235Authenticator::ValidationResult H2351_Authenticator::ValidateCryptoToken(
                                            const H225_CryptoH323Token & cryptoToken,
                                            const PBYTEArray & rawPDU)
{
  //verify the token is of correct type
  if (cryptoToken.GetTag() != H225_CryptoH323Token::e_nestedcryptoToken) {
    PTRACE(4, "H235\tNo nested crypto token!");
    return e_Absent;
  }

  const H235_CryptoToken & crNested = cryptoToken;
  if (crNested.GetTag() != H235_CryptoToken::e_cryptoHashedToken) {
    PTRACE(4, "H235\tNo crypto hash token!");
    return e_Absent;
  }

  const H235_CryptoToken_cryptoHashedToken & crHashed = crNested;

  //verify the crypto OIDs

  // "A" indicates that the whole messages is used for authentication.
  if (!CheckOID(crHashed.m_tokenOID, OID_A)) {
    PTRACE(2, "H235RAS\tH2351_Authenticator requires all fields are hashed, got OID " << crHashed.m_tokenOID);
    return e_Absent;
  }

  // "T" indicates that the hashed token of the CryptoToken is used for authentication.
  if (!CheckOID(crHashed.m_hashedVals.m_tokenOID, OID_T)) {
    PTRACE(2, "H235RAS\tH2351_Authenticator requires ClearToken, got OID " << crHashed.m_hashedVals.m_tokenOID);
    return e_Absent;
  }

  // "U" indicates that the HMAC-SHA1-96 alorigthm is used.
  if (!CheckOID(crHashed.m_token.m_algorithmOID, OID_U)) {
    PTRACE(2, "H235RAS\tH2351_Authenticator requires HMAC-SHA1-96, got OID " << crHashed.m_token.m_algorithmOID);
    return e_Absent;
  }

  //first verify the timestamp
  PTime now;
  int deltaTime = (int)now.GetTimeInSeconds() - crHashed.m_hashedVals.m_timeStamp;
  if (PABS(deltaTime) > timestampGracePeriod) {
    PTRACE(1, "H235RAS\tInvalid timestamp ABS(" << now.GetTimeInSeconds() << '-'
           << (int)crHashed.m_hashedVals.m_timeStamp << ") > " << timestampGracePeriod);
    //the time has elapsed
    return e_InvalidTime;
  }

  //verify the randomnumber
  if (lastTimestamp == crHashed.m_hashedVals.m_timeStamp &&
      lastRandomSequenceNumber == crHashed.m_hashedVals.m_random) {
    //a message with this timespamp and the same random number was already verified
    if (m_verifyRandomNumber) {
      // Innovaphone r6 - r11 sometimes sends random numbers with the high bit set that are all decoded into -1 by H323Plus
      // thus this check fails even is the numbers on the wire are different - this might be a bug in the PTLib ASN decoder TODO
      PTRACE(1, "H235RAS\tConsecutive messages with the same random and timestamp");
      return e_ReplyAttack;
    }
  }

  // If has connection then EP Authenticator so CallBack to Check SenderID and Set Password
  if (connection != NULL) {
    // Senders ID is required for signal authentication
    if (!crHashed.m_hashedVals.HasOptionalField(H235_ClearToken::e_sendersID)) {
      PTRACE(1, "H235RAS\tH2351_Authenticator requires senders ID.");
      return e_Error;
    }

    localId = crHashed.m_hashedVals.m_sendersID.GetValue();
    remoteId = PString::Empty();
    if (!connection->OnCallAuthentication(localId,password)) {
    PTRACE(1, "H235EP\tH2351_Authenticator Authentication Fail UserName \""
            << localId << "\", not Authorised. \"");
    return e_BadPassword;
    }
  } else {
     //verify the username
     if (!localId && crHashed.m_tokenOID[OID_VERSION_OFFSET] > 1) {
       // H.235.0v4 clause 8.2 says generalID "should" be included, but doesn't require it
       // H.235.1v4 clause 14 says generalID "shall" be included in ClearTokens, when the information is available
       // AudioCodes 4.6 and Innovaphone v6-v11 don't include a generalID
       if (m_requireGeneralID) {
         if (!crHashed.m_hashedVals.HasOptionalField(H235_ClearToken::e_generalID)) {
           PTRACE(1, "H235RAS\tH2351_Authenticator requires general ID.");
           return e_Error;
         }

         if (crHashed.m_hashedVals.m_generalID.GetValue() != localId) {
           PTRACE(1, "H235RAS\tGeneral ID is \"" << crHashed.m_hashedVals.m_generalID.GetValue()
                 << "\", should be \"" << localId << '"');
           return e_Error;
        }
      }
    }
  }

  if (!remoteId) {
    if (!crHashed.m_hashedVals.HasOptionalField(H235_ClearToken::e_sendersID) && m_checkSendersID) {
      PTRACE(1, "H235RAS\tH2351_Authenticator requires senders ID.");
      return e_Error;
    }

    if (crHashed.m_hashedVals.m_sendersID.GetValue() != remoteId && m_checkSendersID) {
      PTRACE(1, "H235RAS\tSenders ID is \"" << crHashed.m_hashedVals.m_sendersID.GetValue()
             << "\", should be \"" << remoteId << '"');
      return e_Error;
    }
  }


  /****
  * step 1
  * extract the variable hash and save it
  *
  */
  BYTE RV[HASH_SIZE];

  if (crHashed.m_token.m_hash.GetSize() != HASH_SIZE*8) {
    PTRACE(2, "H235RAS\tH2351_Authenticator requires a hash!");
    return e_Error;
  }

  const unsigned char *data = crHashed.m_token.m_hash.GetDataPointer();
  memcpy(RV, data, HASH_SIZE);

  unsigned char secretkey[20];
  SHA1(password, password.GetLength(), secretkey);


  /****
  * step 4
  * lookup the variable int the orginal ASN1 packet
  * and set it to 0.
  */
  PINDEX foundat = 0;
  bool found = false;

  const BYTE * asnPtr = rawPDU;
  PINDEX asnLen = rawPDU.GetSize();
  while (foundat < asnLen - HASH_SIZE) {
    for (PINDEX i = foundat; i <= asnLen - HASH_SIZE; i++) {
      if (memcmp(asnPtr+i, data, HASH_SIZE) == 0) { // i'v found it !
        foundat = i;
        found = true;
        break;
      }
    }

    if (!found) {
      if (foundat != 0)
        break;

      PTRACE(2, "H235RAS\tH2351_Authenticator could not locate embedded hash!");
      return e_Error;
    }

    found = false;

    memset((BYTE *)asnPtr+foundat, 0, HASH_SIZE);

    /****
    * step 5
    * generate a HMAC-SHA1 key over the hole packet
    *
    */

    char key[HASH_SIZE];
    hmac_sha(secretkey, 20, asnPtr, asnLen, key, HASH_SIZE);

    /****
    * step 6
    * compare the two keys
    *
    */
    if (memcmp(key, RV, HASH_SIZE) == 0) { // Keys are the same !! Ok
      // save the values for the next call
      lastRandomSequenceNumber = crHashed.m_hashedVals.m_random;
      lastTimestamp = crHashed.m_hashedVals.m_timeStamp;

      return e_OK;
    }

    // Put it back and look for another
    memcpy((BYTE *)asnPtr+foundat, data, HASH_SIZE);
    foundat++;
  }

  PTRACE(1, "H235RAS\tH2351_Authenticator hash does not match.");
  return e_BadPassword;
}


PBoolean H2351_Authenticator::IsCapability(const H235_AuthenticationMechanism & mechansim,
                                      const PASN_ObjectId & algorithmOID)
{
  return mechansim.GetTag() == H235_AuthenticationMechanism::e_pwdHash &&
         algorithmOID.AsString() == OID_U;
}


PBoolean H2351_Authenticator::SetCapability(H225_ArrayOf_AuthenticationMechanism & mechanisms,
                                      H225_ArrayOf_PASN_ObjectId & algorithmOIDs)
{
  return AddCapability(H235_AuthenticationMechanism::e_pwdHash, OID_U, mechanisms, algorithmOIDs);
}

PBoolean H2351_Authenticator::IsSecuredPDU(unsigned /*rasPDU*/, PBoolean /*received*/) const
{
  // TODO: should we exclude lightweight RRQ/RCF ?
  return true; // must secure all RAS messages
}

PBoolean H2351_Authenticator::IsSecuredSignalPDU(unsigned signalPDU, PBoolean received) const
{
  // H.235.1 clause 13.2 says all signalling PDUs shall be secured.
  if (m_fullQ931Checking) {
    return TRUE;
  }

  switch (signalPDU) {
    case H225_H323_UU_PDU_h323_message_body::e_setup:
      return received ? !remoteId.IsEmpty() : !localId.IsEmpty();

    default :
      return FALSE;
  }
}

PBoolean H2351_Authenticator::UseGkAndEpIdentifiers() const
{
  return TRUE;
}


#endif // H323_SSL


/////////////////////////////////////////////////////////////////////////////
