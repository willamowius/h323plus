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

#define T140_PACKETCOUNT 3

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

    int sz = str.GetLength()*2;
    SetClientDataSize(sz);
    PWCharArray ucs2;
    int j=0;
    for (PINDEX i=0; i < str.GetLength(); ++i) {
        ucs2 = str.Mid(i,1).AsUCS2();
        *(GetClientDataPtr()+j) = ucs2[0];
        *(GetClientDataPtr()+j+1) = ucs2[1];
        j+=2;
    }
}
    
PString T140_Frame::GetCharacter() const
{
    PString result;
    PWCharArray ucs2;
    for (PINDEX i = 0; i < GetClientDataSize(); i+=2) {
        ucs2[0] = *(GetClientDataPtr()+i);
        ucs2[1] = *(GetClientDataPtr()+i+i);
        result += PString(ucs2).Left(1);
    }
    return result;
}

/////////////////////////////////////////////////////////////////
// Must Declare for Factory Loader.
typedef H224_T140Handler H224_HandlerT140;
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
    if (m_direction == H323Channel::IsReceiver)
            OnRemoteSupportDetected();

    remoteSupport = true;
}

void H224_T140Handler::SetLocalSupport()
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

void H224_T140Handler::SendT140Frame(T140_Frame & frame)
{
    if (!m_h224Handler) return;

    for (PINDEX i=0; i < T140_PACKETCOUNT; ++i) {
        m_h224Handler->TransmitClientFrame(T140_CLIENT_ID, frame, (i>0));
        PThread::Sleep(10);
    }
}

void H224_T140Handler::SendBackSpace()
{
    if (!remoteSupport)
        return;

    PTRACE(4,"T140\tSend Backspace");
    transmitFrame.SetDataType(T140_Frame::BackSpace);
    SendT140Frame(transmitFrame);
}
    
void H224_T140Handler::SendNewLine()
{
    if (!remoteSupport)
        return;

    PTRACE(4,"T140\tSend NewLine");
    transmitFrame.SetDataType(T140_Frame::NewLine);
    SendT140Frame(transmitFrame);
}
    
void H224_T140Handler::SendCharacter(const PString & text)
{
    if (!remoteSupport)
        return;

    PTRACE(4,"T140\tSend Chars " << text);
    transmitFrame.SetCharacter(text);
    SendT140Frame(transmitFrame);
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