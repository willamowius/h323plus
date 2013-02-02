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

#define T140_BS 0x0008          // Back Space: Erases the last entered character.
#define T140_NEWLINE 0x2028     // Line separator.

#include <h224/h224.h>

T140_Frame::T140_Frame()
: H224_Frame(2)
{
  SetHighPriority(TRUE);
	
  BYTE *data = GetClientDataPtr();
  data[0] = 0x00;
  data[1] = 0x00;
}
    
T140_Frame::~T140_Frame()
{

}

T140_Frame::DataType T140_Frame::GetDataType() const
{
   BYTE *data = GetClientDataPtr();

   if ((data[0] == 0x00) && (data[1] == 0x08))
        return T140_Frame::BackSpace;
   else if ((data[0] == 0x20) && (data[1] == 0x28))
        return T140_Frame::NewLine;
   else
        return T140_Frame::TextData;
}

void T140_Frame::SetDataType(DataType type)
{
    BYTE *data = GetClientDataPtr();

    switch (type) {
        case T140_Frame::BackSpace :
            data[0] = 0x00;
            data[1] = 0x08;
            break;
        case T140_Frame::NewLine :
            data[0] = 0x20;
            data[1] = 0x28;
            break;
        default:
            break;
    }
}

void T140_Frame::SetCharacter(const PString & str)
{
    PWCharArray ucs2 = str.AsUCS2();
    memcpy(GetClientDataPtr(),ucs2.GetPointer(),2);
}
    
PString T140_Frame::GetCharacter() const
{
    PWCharArray ucs2;
    memcpy(ucs2.GetPointer(),GetClientDataPtr(),2);
    return ucs2;
}

/////////////////////////////////////////////////////////////////
// Must Declare for Factory Loader.
H224_HANDLER(T140);
/////////////////////////////////////////////////////////////////

H224_T140Handler::H224_T140Handler()
: H224_Handler("T140"), remoteSupport(false)
{

}
  
H224_T140Handler::~H224_T140Handler()
{

}

void H224_T140Handler::SetRemoteSupport()
{
    remoteSupport = true;
}

PBoolean H224_T140Handler::HasRemoteSupport()
{
    return remoteSupport;
}

void H224_T140Handler::SendExtraCapabilities() const
{

}
    
void H224_T140Handler::OnReceivedExtraCapabilities(const BYTE * /*capabilities*/, PINDEX /*size*/)
{

}

void H224_T140Handler::SendBackSpace()
{
    if (!remoteSupport)
        return;

    transmitFrame.SetDataType(T140_Frame::BackSpace);
    m_h224Handler->TransmitClientFrame(T140_CLIENT_ID, transmitFrame);
}
    
void H224_T140Handler::SendNewLine()
{
    if (!remoteSupport)
        return;

    transmitFrame.SetDataType(T140_Frame::NewLine);
    m_h224Handler->TransmitClientFrame(T140_CLIENT_ID, transmitFrame);
}
    
void H224_T140Handler::SendCharacter(const PString & text)
{
    if (!remoteSupport)
        return;

    transmitFrame.SetCharacter(text);
    m_h224Handler->TransmitClientFrame(T140_CLIENT_ID, transmitFrame);
}

void H224_T140Handler::OnReceivedMessage(const H224_Frame & msg)
{
    const T140_Frame & message = (const T140_Frame &)msg;

    switch (message.GetDataType()) {
        case T140_Frame::BackSpace :
            ReceivedBackSpace();
            break;
        case T140_Frame::NewLine :
            ReceivedNewLine();
            break;
        default: // Normal Text
            ReceivedText(message.GetCharacter());
    }
}
    
#endif // H224_T140