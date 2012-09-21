/* H460_oid9.cxx
 *
 * Copyright (c) 2012 Spranto Int'l Pte Ltd. All Rights Reserved.
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
 * The Original Code is derived from and used in conjunction with the 
 * H323plus Project (www.h323plus.org/)
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */


#include <ptlib.h>
#include <openh323buildopts.h>

#ifdef H323_H460COM

#include "h460/h460_oid9.h"
#include "h323ep.h"

// Compatibility Feature
#define baseOID "1.3.6.1.4.1.17090.0.9"  // Remote Vendor Information
#define VendorProdOID      "1"    // PASN_String of productID
#define VendorVerOID       "2"    // PASN_String of versionID

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// Must Declare for Factory Loader.
H460_FEATURE(OID9);

H460_FeatureOID9::H460_FeatureOID9()
: H460_FeatureOID(baseOID), m_ep(NULL), m_con(NULL)
{

 PTRACE(6,"OID9\tInstance Created");

 FeatureCategory = FeatureSupported;

}

H460_FeatureOID9::~H460_FeatureOID9()
{
}

PStringArray H460_FeatureOID9::GetIdentifier()
{
    return PStringArray(baseOID);
}

void H460_FeatureOID9::AttachEndPoint(H323EndPoint * _ep)
{
   m_ep = _ep; 
}

void H460_FeatureOID9::AttachConnection(H323Connection * _conn)
{
   m_con = _conn;
}

PBoolean H460_FeatureOID9::OnSendAdmissionRequest(H225_FeatureDescriptor & pdu) 
{ 
    // Build Message
    PTRACE(6,"OID9\tSending ARQ ");
    H460_FeatureOID feat = H460_FeatureOID(baseOID);

    pdu = feat;
    return TRUE;  
}

void H460_FeatureOID9::OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu)
{
    PTRACE(6,"OID9\tReading ACF");
    H460_FeatureOID & feat = (H460_FeatureOID &)pdu;
    
    PString m_product,m_version = PString();
    
    if (feat.Contains(VendorProdOID)) 
       m_product = (const PString &)feat.Value(VendorProdOID);
    
    if (feat.Contains(VendorVerOID)) 
       m_version = (const PString &)feat.Value(VendorVerOID);

    if (m_product.GetLength() > 0)
       m_con->OnRemoteVendorInformation(m_product, m_version);
}

#endif