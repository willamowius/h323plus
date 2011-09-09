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

#include "h235/h235con.h"

extern "C" {
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
};


H235Context::H235Context()
: m_isActive(false), m_context(NULL)
{

}

H235Context::~H235Context()
{
   SSL_CTX_free(m_context);
}

void H235Context::Initialise()
{
    SSL_load_error_strings();

  // Add all Algorithms;
    OpenSSL_add_ssl_algorithms();

  // Add All Ciphers Trim later
	OpenSSL_add_all_ciphers();

  // Add All Digests
	OpenSSL_add_all_digests();

  

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

#endif