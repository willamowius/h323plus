/* t140.cxx
 *
 * Copyright (c) 2013 Spranto International Pte Ltd. All Rights Reserved.
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
 * H323Plus Project (www.h323plus.org/)
 *
 * The Initial Developer of the Original Code is 
 * Spranto International Pte Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */
 
#include <ptlib.h>
#include <openh323buildopts.h>

#ifdef H224_T140

#include <h224/h224.h>

class T140_Frame : public H224_Frame
{

    PCLASSINFO(T140_Frame, H224_Frame);
    
public:    
    T140_Frame();
    ~T140_Frame();

};


T140_Frame::T140_Frame()
{

}
    
T140_Frame::~T140_Frame()
{

}

/////////////////////////////////////////////////////////////////
// Must Declare for Factory Loader.
H224_HANDLER(T140);
/////////////////////////////////////////////////////////////////

H224_T140Handler::H224_T140Handler()
: H224_Handler("T.140"), remoteSupport(false)
{

}
  
H224_T140Handler::~H224_T140Handler()
{

}

void H224_T140Handler::SetRemoteSupport()
{
    remoteSupport = true;
}

void H224_T140Handler::SendExtraCapabilities() const
{

}
    
void H224_T140Handler::OnReceivedExtraCapabilities(const BYTE * /*capabilities*/, PINDEX /*size*/)
{

}

void H224_T140Handler::OnReceivedMessage(const H224_Frame & /*message*/)
{

}
    
#endif // H224_T140