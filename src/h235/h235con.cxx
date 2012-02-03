/*
 * h235con.cxx
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

#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

#include "rtp.h"
#include "h235/h235con.h"
#include "h235/h235caps.h"
#include "h235/h2356.h"

extern "C" {
#include <openssl/ssl.h>
#ifdef _WIN32
   #undef OPENSSL_SYS_WINDOWS
#endif
#include "h235/ssl_locl.h"
#include <openssl/rand.h>
};



////////////////////////////////////////////////////////////////////////
void tls1_P_hash(const EVP_MD *md, const unsigned char *sec,
            int sec_len, unsigned char *seed, int seed_len,
            unsigned char *out, int olen)
    {
    int chunk,n;
    int j;
    HMAC_CTX ctx;
    HMAC_CTX ctx_tmp;
    unsigned char A1[EVP_MAX_MD_SIZE];
    unsigned int A1_len;
    
    chunk=EVP_MD_size(md);

    HMAC_CTX_init(&ctx);
    HMAC_CTX_init(&ctx_tmp);
    HMAC_Init_ex(&ctx,sec,sec_len,md, NULL);
    HMAC_Init_ex(&ctx_tmp,sec,sec_len,md, NULL);
    HMAC_Update(&ctx,seed,seed_len);
    HMAC_Final(&ctx,A1,&A1_len);

    n=0;
    for (;;)
        {
        HMAC_Init_ex(&ctx,NULL,0,NULL,NULL); // re-init 
        HMAC_Init_ex(&ctx_tmp,NULL,0,NULL,NULL); // re-init 
        HMAC_Update(&ctx,A1,A1_len);
        HMAC_Update(&ctx_tmp,A1,A1_len);
        HMAC_Update(&ctx,seed,seed_len);

        j = chunk;

        if (olen > chunk)
            {
            HMAC_Final(&ctx,out,(unsigned int *)&j);
            out+=j;
            olen-=j;
            HMAC_Final(&ctx_tmp,A1,&A1_len); // calc the next A1 value 
            }
        else    // last one 
            {
              HMAC_Final(&ctx,A1,&A1_len);
            memcpy(out,A1,olen);
            break;
            } 
        }

    HMAC_CTX_cleanup(&ctx);
    HMAC_CTX_cleanup(&ctx_tmp);
    OPENSSL_cleanse(A1,sizeof(A1));

    }


void tls1_PRF(const EVP_MD *md5, const EVP_MD *sha1,
             unsigned char *label, int label_len,
             const unsigned char *sec, int slen, unsigned char *out1,
             unsigned char *out2, int olen)
    {
    int len,i;
    const unsigned char *S1,*S2;

    len=slen/2;
    S1=sec;
    S2= &(sec[len]);
    len+=(slen&1); /* add for odd, make longer */

    
    tls1_P_hash(md5 ,S1,len,label,label_len,out1,olen);
    tls1_P_hash(sha1,S2,len,label,label_len,out2,olen);

    for (i=0; i<olen; i++)
        out1[i]^=out2[i]; 
    }

int tls_change_cipher_state(SSL *s, int which)
    {
    unsigned char *p,*key_block,*mac_secret;
    unsigned char *exp_label; //,buf[TLS_MD_MAX_CONST_SIZE+
    unsigned char tmp1[EVP_MAX_KEY_LENGTH];
    unsigned char tmp2[EVP_MAX_KEY_LENGTH];
    unsigned char iv1[EVP_MAX_IV_LENGTH*2];
    unsigned char iv2[EVP_MAX_IV_LENGTH*2];
    unsigned char *ms,*key,*iv,*er1,*er2;
    int client_write;
    EVP_CIPHER_CTX *dd;
    const EVP_CIPHER *c;
    const SSL_COMP *comp;
    const EVP_MD *m;

    int n,i,j,k,exp_label_len,cl;
    int reuse_dd = 0;

    c=s->s3->tmp.new_sym_enc;
    m=s->s3->tmp.new_hash;
    comp=s->s3->tmp.new_compression;
    key_block=s->s3->tmp.key_block;

    if (which & SSL3_CC_READ)
        {
        if (s->enc_read_ctx != NULL)
            reuse_dd = 1;
        else if ((s->enc_read_ctx=(EVP_CIPHER_CTX *)OPENSSL_malloc(sizeof(EVP_CIPHER_CTX))) == NULL)
            goto err;
        dd= s->enc_read_ctx;
#if OPENSSL_VERSION_NUMBER >= 0x01000000
        s->read_hash->digest=m;
#else
        s->read_hash=m;
#endif

        if (s->expand != NULL)
            {
            COMP_CTX_free(s->expand);
            s->expand=NULL;
            }
        if (comp != NULL)
            {
            s->expand=COMP_CTX_new(comp->method);
            if (s->expand == NULL)
                {
                SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,SSL_R_COMPRESSION_LIBRARY_ERROR);
                goto err2;
                }
            if (s->s3->rrec.comp == NULL)
                s->s3->rrec.comp=(unsigned char *)
                    OPENSSL_malloc(SSL3_RT_MAX_ENCRYPTED_LENGTH);
            if (s->s3->rrec.comp == NULL)
                goto err;
            }
        memset(&(s->s3->read_sequence[0]),0,8);
        mac_secret= &(s->s3->read_mac_secret[0]); 
        }
    else
        {
        if (s->enc_write_ctx != NULL)
            reuse_dd = 1;

        if ((s->enc_write_ctx == NULL) &&
            ((s->enc_write_ctx=(EVP_CIPHER_CTX *)
            OPENSSL_malloc(sizeof(EVP_CIPHER_CTX))) == NULL))
            goto err;
        dd= s->enc_write_ctx;
#if OPENSSL_VERSION_NUMBER >= 0x01000000
        s->write_hash->digest=m;
#else
        s->write_hash=m;
#endif
        if (s->compress != NULL)
            {
            COMP_CTX_free(s->compress);
            s->compress=NULL;
            }
        if (comp != NULL)
            {
            s->compress=COMP_CTX_new(comp->method);
            if (s->compress == NULL)
                {
                SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,SSL_R_COMPRESSION_LIBRARY_ERROR);
                goto err2;
                }
            }
        memset(&(s->s3->write_sequence[0]),0,8);
        mac_secret= &(s->s3->write_mac_secret[0]);
        }

    if (reuse_dd)
        EVP_CIPHER_CTX_cleanup(dd);
    EVP_CIPHER_CTX_init(dd);

    p=s->s3->tmp.key_block;
    i=EVP_MD_size(m);
    cl=EVP_CIPHER_key_length(c);
    j=0;

    // Was j=(exp)?5:EVP_CIPHER_key_length(c);
    k=EVP_CIPHER_iv_length(c);
    er1= &(s->s3->client_random[0]);
    er2= &(s->s3->server_random[0]);
    if (    (which == SSL3_CHANGE_CIPHER_CLIENT_WRITE) ||
        (which == SSL3_CHANGE_CIPHER_SERVER_READ))
        {
        ms=  &(p[ 0]); n=i+i;
        key= &(p[ n]); n+=j+j;
        iv=  &(p[ n]); n+=k+k;
        exp_label=(unsigned char *)TLS_MD_CLIENT_WRITE_KEY_CONST;
        exp_label_len=TLS_MD_CLIENT_WRITE_KEY_CONST_SIZE;
        client_write=1;
        }
    else
        {
        n=i;
        ms=  &(p[ n]); n+=i+j;
        key= &(p[ n]); n+=j+k;
        iv=  &(p[ n]); n+=k;
        exp_label=(unsigned char *)TLS_MD_SERVER_WRITE_KEY_CONST;
        exp_label_len=TLS_MD_SERVER_WRITE_KEY_CONST_SIZE;
        client_write=0;
        }

    if (n > s->s3->tmp.key_block_length)
        {
        SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,ERR_R_INTERNAL_ERROR);
        goto err2;
        }

    memcpy(mac_secret,ms,i);
#if 0
printf("which = %04X\nmac key=",which);
{ int z; for (z=0; z<i; z++) printf("%02X%c",ms[z],((z+1)%16)?' ':'\n'); }
#endif

    s->session->key_arg_length=0; 

    EVP_CipherInit_ex(dd,c,NULL,key,iv,(which & SSL3_CC_WRITE));

#if 0
printf("which = %04X\nkey=",which);
{ int z; for (z=0; z<EVP_CIPHER_key_length(c); z++) printf("%02X%c",key[z],((z+1)%16)?' ':'\n'); }
printf("\niv=");
{ int z; for (z=0; z<k; z++) printf("%02X%c",iv[z],((z+1)%16)?' ':'\n'); }
printf("\n");
#endif

    OPENSSL_cleanse(tmp1,sizeof(tmp1));
    OPENSSL_cleanse(tmp2,sizeof(tmp1));
    OPENSSL_cleanse(iv1,sizeof(iv1));
    OPENSSL_cleanse(iv2,sizeof(iv2));
    return(1);

err:
    SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,ERR_R_MALLOC_FAILURE);
err2:

    return(0);

}

////////////////////////////////////////////////////////////////////////

int  H235Session::session_count=0;

H235Session::H235Session(H235Capabilities * caps, const PString & algorithm)
: m_dh(*caps->GetDiffieHellMan()), m_algorithm(H2356_Authenticator::GetAlgFromOID(algorithm))
  , m_context(*caps->GetContext()), m_ssl(NULL), m_session(NULL), m_isServer(false)
  , m_isInitialised(false)
{
}


H235Session::~H235Session()
{
   SSL_SESSION_free(m_session);
   //SSL_free(m_ssl);
}

void H235Session::SetMasterKey(const PBYTEArray & key)
{
    m_session_key = key;
    m_isServer = true;
}

const PBYTEArray & H235Session::GetMasterKey()
{
    return m_session_key;
}

void H235Session::EncodeMasterKey(PASN_OctetString & /*key*/)
{
    PTRACE(4,"H235Key\tEncode Master Key."); 

}

void H235Session::DecodeMasterKey(const PASN_OctetString & key)
{
    PBYTEArray k = key.GetValue();
    PBYTEArray a;
    PTRACE(4,"H235Key\tH235v3 secret received. " << k);

    unsigned char * buf;
    unsigned char * buf1;
    int size = k.GetSize();
    if (size > 0) {
          buf =(unsigned char *)OPENSSL_malloc(size);
          memmove(buf,k.GetPointer(), size);
          buf1 = RawRead(buf,size);      
          if (size > 0) {
            a.SetSize(size);
            memmove(a.GetPointer(), buf1, size);
          }
          OPENSSL_free(buf);
          OPENSSL_free(buf1);
    }

  //  PTRACE(4,"H235Key\tH235v3 secret received. " << a);
}

void H235Session::SetCipher(const PString & oid)
{
  STACK_OF(SSL_CIPHER) *ciphers;
  ssl_cipher_st *c = NULL, *sc = NULL, *lc = NULL;
  PINDEX i;
    ciphers = SSL_get_ciphers(m_ssl);

        for (i=0; i<sk_SSL_CIPHER_num(ciphers); i++)
        {
        c = sk_SSL_CIPHER_value(ciphers,i);

        if (oid == c->name)   // Get the Local Cipher
            lc = c;        
        }
        sc = lc;

    if (sc == NULL)
        return;

    PTRACE(2,"H235SES\tCommon Cipher Set: " << sc->name);

    m_session->cipher = sc;
    m_session->cipher_id = sc->id;
}

PBoolean H235Session::SetDHSharedkey()
{
    int out = m_session_key.GetSize();
    unsigned char *buf = (unsigned char *)OPENSSL_malloc(out);

    memcpy(buf, m_session_key.GetPointer(), out);

//------------------------------------------------------------------------
// Setup for receiving the DH Shared Secret

unsigned char buf1[SSL3_RANDOM_SIZE*2+TLS_MD_MASTER_SECRET_CONST_SIZE];
unsigned char buff[SSL_MAX_MASTER_KEY_LENGTH];

    // Setup the stuff to munge 
    memcpy(buf1,TLS_MD_MASTER_SECRET_CONST,
        TLS_MD_MASTER_SECRET_CONST_SIZE);
    memcpy(&(buf1[TLS_MD_MASTER_SECRET_CONST_SIZE]),
        m_ssl->s3->client_random,SSL3_RANDOM_SIZE);
    memcpy(&(buf1[SSL3_RANDOM_SIZE+TLS_MD_MASTER_SECRET_CONST_SIZE]),
        m_ssl->s3->server_random,SSL3_RANDOM_SIZE);


    tls1_PRF(m_ssl->ctx->md5,m_ssl->ctx->sha1,
        buf1,TLS_MD_MASTER_SECRET_CONST_SIZE+SSL3_RANDOM_SIZE*2,buf,out,
        m_session->master_key,buff,sizeof(buff));


    memset(buf,0,out);

    PTRACE(2,"H235SES\tMaster Session Key Set!");

    OPENSSL_free(buf);

    return true;
}

PBoolean H235Session::IsActive()
{
    return !m_algorithm.IsEmpty();
}

PBoolean H235Session::IsInitialised() 
{ 
    return m_isInitialised; 
}

PBoolean H235Session::CreateSession()
{

  session_count++;
  m_session_id = session_count;

  ssl_ctx_st * ctx = m_context.GetContext();
  m_ssl = SSL_new(ctx);

  if (m_ssl == NULL) {
      PTRACE(2,"H235SES\tSSL Error in Creation");
      return false;
  } 

  m_session = SSL_get1_session(m_ssl);

  if (!ssl3_setup_buffers(m_ssl)) { 
       PTRACE(2,"H235SES\tError Setting Buffers!");
       return false; 
  }

  if (!ssl_init_wbio_buffer(m_ssl,0)) { 
      PTRACE(2,"H235SES\tError Setting BIO!");
      return false; 
  }

  ssl3_init_finished_mac(m_ssl);

  BUF_MEM *xbuf;
  if ((m_ssl->init_buf == NULL) 
     && ((xbuf=BUF_MEM_new()) == NULL)  
     && (!BUF_MEM_grow(xbuf,SSL3_RT_MAX_PLAIN_LENGTH))) {
                m_ssl->init_buf=xbuf;
                xbuf=NULL;
  }


    m_session = SSL_SESSION_new();
    m_session->key_arg_length = 0;

    m_session->sid_ctx_length= ctx->sid_ctx_length;
    if(m_session->sid_ctx_length > SSL_MAX_SID_CTX_LENGTH) {
          PTRACE(2,"H235SES\tError Setting Context ID!");
            SSL_SESSION_free(m_session);
          return false;                
    }

    memcpy(m_session->sid_ctx, ctx->sid_ctx, m_session->sid_ctx_length);

    //Session Cert
    m_session->sess_cert = ssl_sess_cert_new();
    ssl_sess_cert_free(m_session->sess_cert);

    m_session->timeout=SSL_get_default_timeout(m_ssl);

    //Set the Session ID
    m_session->ssl_version=m_ssl->version;
    m_session->session_id_length=SSL3_SSL_SESSION_ID_LENGTH;

    PThread::Sleep(50);
    if (!m_context.Generate_Session_Id(m_ssl, m_session->session_id, &m_session->session_id_length)){
        PTRACE(2, "H235SES\tSession ID Allocate Fail!");
        SSL_SESSION_free(m_session);
        return false;
    }

    m_session->verify_result = X509_V_OK;

    SetCipher(m_algorithm);

    if (m_session_key.GetSize() == 0) {
         m_dh.ComputeSessionKey(m_session_key);
         m_isServer = true;
    }

    // Same as tls1_generate_master_secret in OpenSSL
    SetDHSharedkey();

    int i= SSL_set_session(m_ssl,m_session);
    if (!i) {
        PTRACE(2, "H235SES\tTLS Error: Session Init Failure");
        return false;
    }

      if (!tls1_setup_key_block(m_ssl)) {
        PTRACE(2, "H235SES\tTLS Error: Setting Key Blocks");
    }

    PTRACE(2, "H235SES\tTLS Session Finalised."); 
    m_isInitialised = true;
    return true;
}

PBoolean H235Session::ReadFrame(DWORD & /*rtpTimestamp*/, RTP_DataFrame & frame)
{
    unsigned char * buf;
    unsigned char * buf1;
    int size = frame.GetPayloadSize();
    if (size > 0) {
          buf =(unsigned char *)OPENSSL_malloc(size);
          memmove(buf,frame.GetPayloadPtr(), size);
          buf1 = RawRead(buf,size);      
          if (size > 0) {
            frame.SetPayloadSize(size);
            memmove(frame.GetPayloadPtr(), buf1, size);
          }
          OPENSSL_free(buf);
    }
    return true;
}

PBoolean H235Session::WriteFrame(RTP_DataFrame & frame)
{
    unsigned char * buf;
    unsigned char * buf1;
    int len = frame.GetPayloadSize();
    if (len > 0) {                
       buf =(unsigned char *)OPENSSL_malloc(len);
       memmove(buf,frame.GetPayloadPtr(),len);
       buf1 = RawWrite(buf,len);
       if (len > 0) {
         frame.SetPayloadSize(len);
         memmove(frame.GetPayloadPtr(), buf1, len);    
       }
       OPENSSL_free(buf);
    }
    return true;
}

unsigned char * H235Session::RawRead(unsigned char * buffer,int & length)
{

ssl3_record_st * rr;
ssl3_buffer_st * rb;

int type = SSL3_RT_APPLICATION_DATA;
int enc_err, statechg;

if (m_ssl->s3->tmp.key_block == NULL) {
  if (!m_ssl->method->ssl3_enc->setup_key_block(m_ssl)) {
            length = 0;
            return NULL;
  }
}

if (m_isServer)
   statechg = SSL3_CHANGE_CIPHER_SERVER_READ;
else
   statechg = SSL3_CHANGE_CIPHER_CLIENT_READ;


if (!tls_change_cipher_state(m_ssl,statechg)) {
  PTRACE(2, "H235SES\tError Setting Cipher State");
  length = 0;
  return NULL;
} 

rr = &(m_ssl->s3->rrec);
rb = &(m_ssl->s3->rbuf);

        rb->buf = buffer;
        rb->len = length;
        rb->offset = 0;
    
    rr->input= rb->buf;
    rr->type = type;
    rr->length = length;
    rr->off = 0;
    rr->data=rr->input;
                 
  enc_err = m_ssl->method->ssl3_enc->enc(m_ssl, 0);

    if (enc_err <= 0) {
        if (enc_err == 0) {
            length = 0;
            return NULL;
        }
        if (enc_err == -1) {
            length = 0;
            return NULL;
        }
    }

#if 0
printf("dec %d\n",rr->length);
{ unsigned int z; for (z=0; z<rr->length; z++) printf("%02X%c",rr->data[z],((z+1)%16)?' ':'\n'); }
printf("\n");
#endif

    if (rr->length > SSL3_RT_MAX_PLAIN_LENGTH) {
        PTRACE(2, "H235SES\tError Data too long");
        length = 0;
        return NULL;
    }

    length = rr->length;
    return &rr->input[rr->off];
}


unsigned char * H235Session::RawWrite(unsigned char * buffer , int & length)
{

unsigned char * p;
unsigned char * plen;

ssl3_record_st * wr;
ssl3_buffer_st * wb;
int prefix_len = 0;
int type = SSL3_RT_APPLICATION_DATA;
int mac_size, statechg;


if (m_ssl->s3->tmp.key_block == NULL) {
    if (!m_ssl->method->ssl3_enc->setup_key_block(m_ssl)) {
        length = 0;
        return NULL;
    }
}

if (m_isServer)
    statechg = SSL3_CHANGE_CIPHER_SERVER_WRITE;
else
    statechg = SSL3_CHANGE_CIPHER_CLIENT_WRITE;


if (!tls_change_cipher_state(m_ssl,statechg)) {
  PTRACE(2, "H235SES\tError Setting Cipher State");
  length = 0;
  return NULL;
} 

wr= &(m_ssl->s3->wrec);
wb= &(m_ssl->s3->wbuf);

#if OPENSSL_VERSION_NUMBER >= 0x01000000
    mac_size = EVP_MD_CTX_size(m_ssl->write_hash);
#else
    mac_size = EVP_MD_size(m_ssl->write_hash);
#endif
    p = wb->buf + prefix_len;

    /* write the header */

    *(p++)=type&0xff;
    wr->type=type;

    *(p++)=(unsigned char)(m_ssl->version>>8);
    *(p++)=m_ssl->version&0xff;

    /* field where we are to write out packet length */
    plen=p; 
    p+=2;

    /* lets setup the record stuff. */
    wr->data=p;
    wr->length=(int)length;
    wr->input= buffer;

    length = 0;

    /* we now 'read' from wr->input, wr->length bytes into
     * wr->data */

    /* first we compress */
    if (m_ssl->compress != NULL) {

    } else {
        memcpy(wr->data,wr->input,wr->length);
        wr->input=wr->data;
    }

    /* we should still have the output to wr->data and the input
     * from wr->input.  Length should be wr->length.
     * wr->data still points in the wb->buf */

     int writeResult =m_ssl->method->ssl3_enc->enc(m_ssl, 1);

       if (writeResult < 0) {
           PTRACE(2, "H235SES\tError Writing");
           return NULL;
       }

    length = wr->length;

    return wr->input;
}

/////////////////////////////////////////////////////////////////
H235Context::H235Context()
: m_isActive(false)
{
    Initialise();
}

H235Context::~H235Context()
{
   SSL_CTX_free(m_context);
}

void H235Context::Initialise()
{
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    OpenSSL_add_all_ciphers();
    OpenSSL_add_all_digests();
    RandomSeed();
    
    m_context  = SSL_CTX_new(TLSv1_method());

    ssl_load_ciphers();
    
    m_isActive = true;
}

PBoolean H235Context::IsActive()
{
    return m_isActive;
}

void H235Context::RandomSeed()
{
    
    BYTE seed[256];
    for (size_t i = 0; i < sizeof(seed); i++)
      seed[i] = (BYTE)rand();
    RAND_seed(seed, sizeof(seed));

}

#define session_id_prefix "vs"
#define MAX_SESSION_ID_ATTEMPTS 10
int H235Context::Generate_Session_Id(const ssl_st *ssl, unsigned char *id, unsigned int *id_len)
{
    unsigned int count = 0;
    do    {
        RAND_pseudo_bytes(id, *id_len);
        /* Prefix the session_id with the required prefix. NB: If our
         * prefix is too long, clip it - but there will be worse effects
         * anyway, eg. the server could only possibly create 1 session
         * ID (ie. the prefix!) so all future session negotiations will
         * fail due to conflicts. */
        memcpy(id, session_id_prefix,
            (strlen(session_id_prefix) < *id_len) ?
            strlen(session_id_prefix) : *id_len);
        }
    while(SSL_has_matching_session_id(ssl, id, *id_len) &&
        (++count < MAX_SESSION_ID_ATTEMPTS));
    if(count >= MAX_SESSION_ID_ATTEMPTS)
        return 0;
    return 1;
}

#endif
