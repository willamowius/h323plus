/*
 * ssl_support.cxx
 *
 * Support for Dynamic linking to OpenSSL for Media Encryption.
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

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

extern "C" {
#include <stdio.h>
#include <openssl/ssl.h>
#ifdef _WIN32
   #undef OPENSSL_SYS_WINDOWS
#endif
#include "h235/ssl_locl.h"
#ifndef OPENSSL_NO_COMP
#include <openssl/comp.h>
#endif
}

//////////////////////////////////////////////////////////////////////////////
// s3_enc.c

void ssl3_init_finished_mac(SSL *s)
    {
    EVP_MD_CTX_set_flags(&(s->s3->finish_dgst1),
        EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
    EVP_DigestInit_ex(&(s->s3->finish_dgst1),s->ctx->md5, NULL);
    EVP_DigestInit_ex(&(s->s3->finish_dgst2),s->ctx->sha1, NULL);
    }
    
void ssl3_cleanup_key_block(SSL *s)
    {
    if (s->s3->tmp.key_block != NULL)
        {
        OPENSSL_cleanse(s->s3->tmp.key_block,
            s->s3->tmp.key_block_length);
        OPENSSL_free(s->s3->tmp.key_block);
        s->s3->tmp.key_block=NULL;
        }
    s->s3->tmp.key_block_length=0;
    }
    
//////////////////////////////////////////////////////////////////////////////
// ssl_ciph.c 

#define SSL_ENC_DES_IDX        0
#define SSL_ENC_3DES_IDX    1
#define SSL_ENC_RC4_IDX        2
#define SSL_ENC_RC2_IDX        3
#define SSL_ENC_IDEA_IDX    4
#define SSL_ENC_eFZA_IDX    5
#define SSL_ENC_NULL_IDX    6
#define SSL_ENC_AES128_IDX    7
#define SSL_ENC_AES256_IDX    8
#define SSL_ENC_CAMELLIA128_IDX    9
#define SSL_ENC_CAMELLIA256_IDX    10
#define SSL_ENC_SEED_IDX        11
#define SSL_ENC_NUM_IDX        12

static const EVP_CIPHER *ssl_cipher_methods[SSL_ENC_NUM_IDX]={
    NULL,NULL,NULL,NULL,NULL,NULL,
    };

#define SSL_COMP_NULL_IDX    0
#define SSL_COMP_ZLIB_IDX    1
#define SSL_COMP_NUM_IDX    2

static STACK_OF(SSL_COMP) *ssl_comp_methods=NULL;

#define SSL_MD_MD5_IDX    0
#define SSL_MD_SHA1_IDX    1
#define SSL_MD_NUM_IDX    2
const EVP_MD *ssl_digest_methods[SSL_MD_NUM_IDX]={
    NULL,NULL,
    };

//////////////////////////////////////////////

void ssl_load_ciphers(void)
	{
	ssl_cipher_methods[SSL_ENC_DES_IDX]= 
		EVP_get_cipherbyname(SN_des_cbc);
	ssl_cipher_methods[SSL_ENC_3DES_IDX]=
		EVP_get_cipherbyname(SN_des_ede3_cbc);
	ssl_cipher_methods[SSL_ENC_RC4_IDX]=
		EVP_get_cipherbyname(SN_rc4);
	ssl_cipher_methods[SSL_ENC_RC2_IDX]= 
		EVP_get_cipherbyname(SN_rc2_cbc);
#ifndef OPENSSL_NO_IDEA
	ssl_cipher_methods[SSL_ENC_IDEA_IDX]= 
		EVP_get_cipherbyname(SN_idea_cbc);
#else
	ssl_cipher_methods[SSL_ENC_IDEA_IDX]= NULL;
#endif
	ssl_cipher_methods[SSL_ENC_AES128_IDX]=
	  EVP_get_cipherbyname(SN_aes_128_cbc);
	ssl_cipher_methods[SSL_ENC_AES256_IDX]=
	  EVP_get_cipherbyname(SN_aes_256_cbc);
	ssl_cipher_methods[SSL_ENC_CAMELLIA128_IDX]=
	  EVP_get_cipherbyname(SN_camellia_128_cbc);
	ssl_cipher_methods[SSL_ENC_CAMELLIA256_IDX]=
	  EVP_get_cipherbyname(SN_camellia_256_cbc);
	ssl_cipher_methods[SSL_ENC_SEED_IDX]=
	  EVP_get_cipherbyname(SN_seed_cbc);

	ssl_digest_methods[SSL_MD_MD5_IDX]=
		EVP_get_digestbyname(SN_md5);
	ssl_digest_methods[SSL_MD_SHA1_IDX]=
		EVP_get_digestbyname(SN_sha1);
	}

#ifndef OPENSSL_NO_COMP

int sk_comp_cmp(const SSL_COMP * const *a,
            const SSL_COMP * const *b)
    {
    return((*a)->id-(*b)->id);
    }

void load_builtin_compressions(void)
    {
    int got_write_lock = 0;

    CRYPTO_r_lock(CRYPTO_LOCK_SSL);
    if (ssl_comp_methods == NULL)
        {
        CRYPTO_r_unlock(CRYPTO_LOCK_SSL);
        CRYPTO_w_lock(CRYPTO_LOCK_SSL);
        got_write_lock = 1;
        
        if (ssl_comp_methods == NULL)
            {
            SSL_COMP *comp = NULL;

            MemCheck_off();
            ssl_comp_methods=sk_SSL_COMP_new(sk_comp_cmp);
            if (ssl_comp_methods != NULL)
                {
                comp=(SSL_COMP *)OPENSSL_malloc(sizeof(SSL_COMP));
                if (comp != NULL)
                    {
                    comp->method=COMP_zlib();
                    if (comp->method
                        && comp->method->type == NID_undef)
                        OPENSSL_free(comp);
                    else
                        {
                        comp->id=SSL_COMP_ZLIB_IDX;
                        comp->name=comp->method->name;
                        sk_SSL_COMP_push(ssl_comp_methods,comp);
                        }
                    }
                    sk_SSL_COMP_sort(ssl_comp_methods);
                }
            MemCheck_on();
            }
        }
    
    if (got_write_lock)
        CRYPTO_w_unlock(CRYPTO_LOCK_SSL);
    else
        CRYPTO_r_unlock(CRYPTO_LOCK_SSL);
    }
#endif    
    

int ssl_cipher_get_evp(const SSL_SESSION *s, const EVP_CIPHER **enc,
         const EVP_MD **md, SSL_COMP **comp)
    {
    int i;
    SSL_CIPHER *c;

    c=s->cipher;
    if (c == NULL) return(0);
    if (comp != NULL)
        {
        SSL_COMP ctmp;
#ifndef OPENSSL_NO_COMP
        load_builtin_compressions();
#endif

        *comp=NULL;
        ctmp.id=s->compress_meth;
        if (ssl_comp_methods != NULL)
            {
            i=sk_SSL_COMP_find(ssl_comp_methods,&ctmp);
            if (i >= 0)
                *comp=sk_SSL_COMP_value(ssl_comp_methods,i);
            else
                *comp=NULL;
            }
        }

    if ((enc == NULL) || (md == NULL)) return(0);

    switch (c->algorithms & SSL_ENC_MASK)
        {
    case SSL_DES:
        i=SSL_ENC_DES_IDX;
        break;
    case SSL_3DES:
        i=SSL_ENC_3DES_IDX;
        break;
    case SSL_RC4:
        i=SSL_ENC_RC4_IDX;
        break;
    case SSL_RC2:
        i=SSL_ENC_RC2_IDX;
        break;
    case SSL_IDEA:
        i=SSL_ENC_IDEA_IDX;
        break;
    case SSL_eNULL:
        i=SSL_ENC_NULL_IDX;
        break;
    case SSL_AES:
        switch(c->alg_bits)
            {
        case 128: i=SSL_ENC_AES128_IDX; break;
        case 256: i=SSL_ENC_AES256_IDX; break;
        default: i=-1; break;
            }
        break;
    case SSL_CAMELLIA:
        switch(c->alg_bits)
            {
        case 128: i=SSL_ENC_CAMELLIA128_IDX; break;
        case 256: i=SSL_ENC_CAMELLIA256_IDX; break;
        default: i=-1; break;
            }
        break;
    case SSL_SEED:
        i=SSL_ENC_SEED_IDX;
        break;

    default:
        i= -1;
        break;
        }

    if ((i < 0) || (i > SSL_ENC_NUM_IDX))
        *enc=NULL;
    else
        {
        if (i == SSL_ENC_NULL_IDX)
            *enc=EVP_enc_null();
        else
            *enc=ssl_cipher_methods[i];
        }

    switch (c->algorithms & SSL_MAC_MASK)
        {
    case SSL_MD5:
        i=SSL_MD_MD5_IDX;
        break;
    case SSL_SHA1:
        i=SSL_MD_SHA1_IDX;
        break;
    default:
        i= -1;
        break;
        }
    if ((i < 0) || (i > SSL_MD_NUM_IDX))
        *md=NULL;
    else
        *md=ssl_digest_methods[i];

    if ((*enc != NULL) && (*md != NULL))
        return(1);
    else
        return(0);
    }


    
//////////////////////////////////////////////////////////////////////////////////////////////////////
// from tl_enc.c

static void tls1_P_hash(const EVP_MD *md, const unsigned char *sec,
            int sec_len, unsigned char *seed, int seed_len,
            unsigned char *out, int olen)
    {
    int chunk;
    unsigned int j;
    HMAC_CTX ctx;
    HMAC_CTX ctx_tmp;
    unsigned char A1[EVP_MAX_MD_SIZE];
    unsigned int A1_len;
    
    chunk=EVP_MD_size(md);

    HMAC_CTX_init(&ctx);
    HMAC_CTX_init(&ctx_tmp);
    HMAC_CTX_set_flags(&ctx, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
    HMAC_CTX_set_flags(&ctx_tmp, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
    HMAC_Init_ex(&ctx,sec,sec_len,md, NULL);
    HMAC_Init_ex(&ctx_tmp,sec,sec_len,md, NULL);
    HMAC_Update(&ctx,seed,seed_len);
    HMAC_Final(&ctx,A1,&A1_len);

    for (;;)
        {
        HMAC_Init_ex(&ctx,NULL,0,NULL,NULL); /* re-init */
        HMAC_Init_ex(&ctx_tmp,NULL,0,NULL,NULL); /* re-init */
        HMAC_Update(&ctx,A1,A1_len);
        HMAC_Update(&ctx_tmp,A1,A1_len);
        HMAC_Update(&ctx,seed,seed_len);

        if (olen > chunk)
            {
            HMAC_Final(&ctx,out,&j);
            out+=j;
            olen-=j;
            HMAC_Final(&ctx_tmp,A1,&A1_len); /* calc the next A1 value */
            }
        else    /* last one */
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

static void tls1_PRF(const EVP_MD *md5, const EVP_MD *sha1,
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

void tls1_generate_key_block(SSL *s, unsigned char *km,
         unsigned char *tmp, int num)
    {
    unsigned char *p;
    unsigned char buf[SSL3_RANDOM_SIZE*2+
        TLS_MD_MAX_CONST_SIZE];
    p=buf;

    memcpy(p,TLS_MD_KEY_EXPANSION_CONST,
        TLS_MD_KEY_EXPANSION_CONST_SIZE);
    p+=TLS_MD_KEY_EXPANSION_CONST_SIZE;
    memcpy(p,s->s3->server_random,SSL3_RANDOM_SIZE);
    p+=SSL3_RANDOM_SIZE;
    memcpy(p,s->s3->client_random,SSL3_RANDOM_SIZE);
    p+=SSL3_RANDOM_SIZE;

    tls1_PRF(s->ctx->md5,s->ctx->sha1,buf,(int)(p-buf),
         s->session->master_key,s->session->master_key_length,
         km,tmp,num);
#ifdef KSSL_DEBUG
    printf("tls1_generate_key_block() ==> %d byte master_key =\n\t",
                s->session->master_key_length);
    {
        int i;
        for (i=0; i < s->session->master_key_length; i++)
                {
                printf("%02X", s->session->master_key[i]);
                }
        printf("\n");  }
#endif    /* KSSL_DEBUG */
    }

int tls1_change_cipher_state(SSL *s, int which)
    {
    static const unsigned char empty[]="";
    unsigned char *p,*mac_secret;
    unsigned char *exp_label,buf[TLS_MD_MAX_CONST_SIZE+
        SSL3_RANDOM_SIZE*2];
    unsigned char tmp1[EVP_MAX_KEY_LENGTH];
    unsigned char tmp2[EVP_MAX_KEY_LENGTH];
    unsigned char iv1[EVP_MAX_IV_LENGTH*2];
    unsigned char iv2[EVP_MAX_IV_LENGTH*2];
    unsigned char *ms,*key,*iv;
    int client_write;
    EVP_CIPHER_CTX *dd;
    const EVP_CIPHER *c;
#ifndef OPENSSL_NO_COMP
    const SSL_COMP *comp;
#endif
    const EVP_MD *m;
    int is_export,n,i,j,k,exp_label_len,cl;
    int reuse_dd = 0;

    is_export=SSL_C_IS_EXPORT(s->s3->tmp.new_cipher);
    c=s->s3->tmp.new_sym_enc;
    m=s->s3->tmp.new_hash;
#ifndef OPENSSL_NO_COMP
    comp=s->s3->tmp.new_compression;
#endif

#ifdef KSSL_DEBUG
    key_block=s->s3->tmp.key_block;

    printf("tls1_change_cipher_state(which= %d) w/\n", which);
    printf("\talg= %ld, comp= %p\n", s->s3->tmp.new_cipher->algorithms,
                (void *)comp);
    printf("\tevp_cipher == %p ==? &d_cbc_ede_cipher3\n", (void *)c);
    printf("\tevp_cipher: nid, blksz= %d, %d, keylen=%d, ivlen=%d\n",
                c->nid,c->block_size,c->key_len,c->iv_len);
    printf("\tkey_block: len= %d, data= ", s->s3->tmp.key_block_length);
    {
        int ki;
        for (ki=0; ki<s->s3->tmp.key_block_length; ki++)
        printf("%02x", key_block[ki]);  printf("\n");
        }
#endif    /* KSSL_DEBUG */

    if (which & SSL3_CC_READ)
        {
        if (s->enc_read_ctx != NULL)
            reuse_dd = 1;
        else if ((s->enc_read_ctx=(EVP_CIPHER_CTX *)OPENSSL_malloc(sizeof(EVP_CIPHER_CTX))) == NULL)
            goto err;
        else
            /* make sure it's intialized in case we exit later with an error */
            EVP_CIPHER_CTX_init(s->enc_read_ctx);
        dd= s->enc_read_ctx;
        s->read_hash=m;
#ifndef OPENSSL_NO_COMP
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
#endif
        /* this is done by dtls1_reset_seq_numbers for DTLS1_VERSION */
         if (s->version != DTLS1_VERSION)
            memset(&(s->s3->read_sequence[0]),0,8);
        mac_secret= &(s->s3->read_mac_secret[0]);
        }
    else
        {
        if (s->enc_write_ctx != NULL)
            reuse_dd = 1;
        else if ((s->enc_write_ctx=(EVP_CIPHER_CTX *)OPENSSL_malloc(sizeof(EVP_CIPHER_CTX))) == NULL)
            goto err;
        else
            /* make sure it's intialized in case we exit later with an error */
            EVP_CIPHER_CTX_init(s->enc_write_ctx);
        dd= s->enc_write_ctx;
        s->write_hash=m;
#ifndef OPENSSL_NO_COMP
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
#endif
        /* this is done by dtls1_reset_seq_numbers for DTLS1_VERSION */
         if (s->version != DTLS1_VERSION)
            memset(&(s->s3->write_sequence[0]),0,8);
        mac_secret= &(s->s3->write_mac_secret[0]);
        }

    if (reuse_dd)
        EVP_CIPHER_CTX_cleanup(dd);

    p=s->s3->tmp.key_block;
    i=EVP_MD_size(m);
    cl=EVP_CIPHER_key_length(c);
    j=is_export ? (cl < SSL_C_EXPORT_KEYLENGTH(s->s3->tmp.new_cipher) ?
                   cl : SSL_C_EXPORT_KEYLENGTH(s->s3->tmp.new_cipher)) : cl;
    /* Was j=(exp)?5:EVP_CIPHER_key_length(c); */
    k=EVP_CIPHER_iv_length(c);
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
#ifdef TLS_DEBUG
printf("which = %04X\nmac key=",which);
{ int z; for (z=0; z<i; z++) printf("%02X%c",ms[z],((z+1)%16)?' ':'\n'); }
#endif
    if (is_export)
        {
        /* In here I set both the read and write key/iv to the
         * same value since only the correct one will be used :-).
         */
        p=buf;
        memcpy(p,exp_label,exp_label_len);
        p+=exp_label_len;
        memcpy(p,s->s3->client_random,SSL3_RANDOM_SIZE);
        p+=SSL3_RANDOM_SIZE;
        memcpy(p,s->s3->server_random,SSL3_RANDOM_SIZE);
        p+=SSL3_RANDOM_SIZE;
        tls1_PRF(s->ctx->md5,s->ctx->sha1,buf,(int)(p-buf),key,j,
             tmp1,tmp2,EVP_CIPHER_key_length(c));
        key=tmp1;

        if (k > 0)
            {
            p=buf;
            memcpy(p,TLS_MD_IV_BLOCK_CONST,
                TLS_MD_IV_BLOCK_CONST_SIZE);
            p+=TLS_MD_IV_BLOCK_CONST_SIZE;
            memcpy(p,s->s3->client_random,SSL3_RANDOM_SIZE);
            p+=SSL3_RANDOM_SIZE;
            memcpy(p,s->s3->server_random,SSL3_RANDOM_SIZE);
            p+=SSL3_RANDOM_SIZE;
            tls1_PRF(s->ctx->md5,s->ctx->sha1,buf,p-buf,empty,0,
                 iv1,iv2,k*2);
            if (client_write)
                iv=iv1;
            else
                iv= &(iv1[k]);
            }
        }

    s->session->key_arg_length=0;
#ifdef KSSL_DEBUG
    {
        int ki;
    printf("EVP_CipherInit_ex(dd,c,key=,iv=,which)\n");
    printf("\tkey= ");
    for (ki=0; ki<c->key_len; ki++) printf("%02x", key[ki]);
    printf("\n");
    printf("\t iv= ");
    for (ki=0; ki<c->iv_len; ki++) printf("%02x", iv[ki]);
    printf("\n");
    }
#endif    /* KSSL_DEBUG */

    EVP_CipherInit_ex(dd,c,NULL,key,iv,(which & SSL3_CC_WRITE));
#ifdef TLS_DEBUG
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

int tls1_setup_key_block(SSL *s)
    {
    unsigned char *p1,*p2;
    const EVP_CIPHER *c;
    const EVP_MD *hash;
    int num;
    SSL_COMP *comp;

#ifdef KSSL_DEBUG
    printf ("tls1_setup_key_block()\n");
#endif    /* KSSL_DEBUG */

    if (s->s3->tmp.key_block_length != 0)
        return(1);

    if (!ssl_cipher_get_evp(s->session,&c,&hash,&comp))
        {
        SSLerr(SSL_F_TLS1_SETUP_KEY_BLOCK,SSL_R_CIPHER_OR_HASH_UNAVAILABLE);
        return(0);
        }

    s->s3->tmp.new_sym_enc=c;
    s->s3->tmp.new_hash=hash;

    num=EVP_CIPHER_key_length(c)+EVP_MD_size(hash)+EVP_CIPHER_iv_length(c);
    num*=2;

    ssl3_cleanup_key_block(s);

    if ((p1=(unsigned char *)OPENSSL_malloc(num)) == NULL)
        goto err;
    if ((p2=(unsigned char *)OPENSSL_malloc(num)) == NULL)
        goto err;

    s->s3->tmp.key_block_length=num;
    s->s3->tmp.key_block=p1;


#ifdef TLS_DEBUG
printf("client random\n");
{ int z; for (z=0; z<SSL3_RANDOM_SIZE; z++) printf("%02X%c",s->s3->client_random[z],((z+1)%16)?' ':'\n'); }
printf("server random\n");
{ int z; for (z=0; z<SSL3_RANDOM_SIZE; z++) printf("%02X%c",s->s3->server_random[z],((z+1)%16)?' ':'\n'); }
printf("pre-master\n");
{ int z; for (z=0; z<s->session->master_key_length; z++) printf("%02X%c",s->session->master_key[z],((z+1)%16)?' ':'\n'); }
#endif
    tls1_generate_key_block(s,p1,p2,num);
    OPENSSL_cleanse(p2,num);
    OPENSSL_free(p2);
#ifdef TLS_DEBUG
printf("\nkey block\n");
{ int z; for (z=0; z<num; z++) printf("%02X%c",p1[z],((z+1)%16)?' ':'\n'); }
#endif

    if (!(s->options & SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS))
        {
        /* enable vulnerability countermeasure for CBC ciphers with
         * known-IV problem (http://www.openssl.org/~bodo/tls-cbc.txt)
         */
        s->s3->need_empty_fragments = 1;

        if (s->session->cipher != NULL)
            {
            if ((s->session->cipher->algorithms & SSL_ENC_MASK) == SSL_eNULL)
                s->s3->need_empty_fragments = 0;
            
#ifndef OPENSSL_NO_RC4
            if ((s->session->cipher->algorithms & SSL_ENC_MASK) == SSL_RC4)
                s->s3->need_empty_fragments = 0;
#endif
            }
        }
        
    return(1);
err:
    SSLerr(SSL_F_TLS1_SETUP_KEY_BLOCK,ERR_R_MALLOC_FAILURE);
    return(0);
    }
    

    
//////////////////////////////////////////////////////////////////////////////////////////////    
// From ssl_cert.c
    
SESS_CERT *ssl_sess_cert_new(void)
    {
    SESS_CERT *ret;

    ret = (SESS_CERT *)OPENSSL_malloc(sizeof *ret);
    if (ret == NULL)
        {
        SSLerr(SSL_F_SSL_SESS_CERT_NEW, ERR_R_MALLOC_FAILURE);
        return NULL;
        }

    memset(ret, 0 ,sizeof *ret);
    ret->peer_key = &(ret->peer_pkeys[SSL_PKEY_RSA_ENC]);
    ret->references = 1;

    return ret;
    }

void ssl_sess_cert_free(SESS_CERT *sc)
    {
    int i;

    if (sc == NULL)
        return;

    i = CRYPTO_add(&sc->references, -1, CRYPTO_LOCK_SSL_SESS_CERT);
#ifdef REF_PRINT
    REF_PRINT("SESS_CERT", sc);
#endif
    if (i > 0)
        return;
#ifdef REF_CHECK
    if (i < 0)
        {
        fprintf(stderr,"ssl_sess_cert_free, bad reference count\n");
        abort(); /* ok */
        }
#endif

    /* i == 0 */
    if (sc->cert_chain != NULL)
        sk_X509_pop_free(sc->cert_chain, X509_free);
    for (i = 0; i < SSL_PKEY_NUM; i++)
        {
        if (sc->peer_pkeys[i].x509 != NULL)
            X509_free(sc->peer_pkeys[i].x509);
#if 0 /* We don't have the peer's private key.  These lines are just
       * here as a reminder that we're still using a not-quite-appropriate
       * data structure. */
        if (sc->peer_pkeys[i].privatekey != NULL)
            EVP_PKEY_free(sc->peer_pkeys[i].privatekey);
#endif
        }

#ifndef OPENSSL_NO_RSA
    if (sc->peer_rsa_tmp != NULL)
        RSA_free(sc->peer_rsa_tmp);
#endif
#ifndef OPENSSL_NO_DH
    if (sc->peer_dh_tmp != NULL)
        DH_free(sc->peer_dh_tmp);
#endif
#ifndef OPENSSL_NO_ECDH
    if (sc->peer_ecdh_tmp != NULL)
        EC_KEY_free(sc->peer_ecdh_tmp);
#endif

    OPENSSL_free(sc);
    }
    

///////////////////////////////////////////////////////////////////////////////
// ssl_lib.c

int ssl_init_wbio_buffer(SSL *s,int push)
    {
    BIO *bbio;

    if (s->bbio == NULL)
        {
        bbio=BIO_new(BIO_f_buffer());
        if (bbio == NULL) return(0);
        s->bbio=bbio;
        }
    else
        {
        bbio=s->bbio;
        if (s->bbio == s->wbio)
            s->wbio=BIO_pop(s->wbio);
        }
    (void)BIO_reset(bbio);
/*    if (!BIO_set_write_buffer_size(bbio,16*1024)) */
    if (!BIO_set_read_buffer_size(bbio,1))
        {
        SSLerr(SSL_F_SSL_INIT_WBIO_BUFFER,ERR_R_BUF_LIB);
        return(0);
        }
    if (push)
        {
        if (s->wbio != bbio)
            s->wbio=BIO_push(bbio,s->wbio);
        }
    else
        {
        if (s->wbio == bbio)
            s->wbio=BIO_pop(bbio);
        }
    return(1);
    }
    
///////////////////////////////////////////////////////////////////////////////
// ssl_both.c

int ssl3_setup_buffers(SSL *s)
    {
    unsigned char *p;
    unsigned int extra,headerlen;
    size_t len;

    if (SSL_version(s) == DTLS1_VERSION || SSL_version(s) == DTLS1_BAD_VER)
        headerlen = DTLS1_RT_HEADER_LENGTH;
    else
        headerlen = SSL3_RT_HEADER_LENGTH;

    if (s->s3->rbuf.buf == NULL)
        {
        if (s->options & SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER)
            extra=SSL3_RT_MAX_EXTRA;
        else
            extra=0;
        len = SSL3_RT_MAX_PACKET_SIZE + extra;
        if ((p=(unsigned char *)OPENSSL_malloc(len)) == NULL)
            goto err;
        s->s3->rbuf.buf = p;
        s->s3->rbuf.len = len;
        }

    if (s->s3->wbuf.buf == NULL)
        {
        len = SSL3_RT_MAX_PACKET_SIZE;
        len += headerlen + 256; /* extra space for empty fragment */
        if ((p=(unsigned char *)OPENSSL_malloc(len)) == NULL)
            goto err;
        s->s3->wbuf.buf = p;
        s->s3->wbuf.len = len;
        }
    s->packet= &(s->s3->rbuf.buf[0]);
    return(1);
err:
    SSLerr(SSL_F_SSL3_SETUP_BUFFERS,ERR_R_MALLOC_FAILURE);
    return(0);
    }

#endif


