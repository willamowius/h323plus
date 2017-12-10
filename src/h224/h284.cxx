/* h284.cxx
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

#ifdef H224_H284

#include <h224/h224.h>


H284_Frame::H284_Frame()
{

}

H284_Frame::~H284_Frame()
{

}

void H284_Frame::AddInstruction(const H284_Instruction & inst)
{
  int sz = GetClientDataSize();
  SetClientDataSize(sz + inst.GetSize());
  memcpy(GetClientDataPtr()+sz,inst,inst.GetSize());
}

PBoolean H284_Frame::ReadInstructions(H224_H284Handler & handler) const
{
    int size = GetClientDataSize();
    int sz = 0;

    BYTE info[4];
    H284_ControlPoint * cp = NULL;
    int msgSize=0;
    while (sz < size) {
        memcpy(info,GetClientDataPtr()+sz,4);
        cp = handler.GetControlPoint(info[0]);
        if (cp) {
            switch (cp->GetControlType()) {
                case H284_ControlPoint::e_unknown:
                case H284_ControlPoint::e_relative:
                case H284_ControlPoint::e_absolute:
                    msgSize = 8;
                    break;
                case H284_ControlPoint::e_simple:
                default:
                    msgSize = 4;
                    break;
            }
            H284_Instruction inst;
            memcpy(inst.GetPointer(),GetClientDataPtr()+sz,msgSize);
            cp->HandleInstruction(inst);
            sz+=msgSize;
        } else {
            sz+=4;
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////

#define H284_CPSIZE 24

H284_ControlPoint::H284_ControlPoint(H224_H284Handler & handler, BYTE ctrlID)
:     PBYTEArray(H284_CPSIZE), m_handler(handler), m_cpType(e_unknown),
    m_lastInstruction(0), m_isActive(false)
{
    theArray[0] = ctrlID;
}

PString H284_ControlPoint::Name() const
{
    return H224_H284Handler::ControlIDAsString(theArray[0]);
}

PBoolean H284_ControlPoint::IsActive() const
{
    return m_isActive;
}

void H284_ControlPoint::SetData(PBoolean absolute, PBoolean viewport, unsigned step,
        unsigned min, unsigned max, unsigned current, unsigned vportMin, unsigned vportMax)
{
    int sz = H284_CPSIZE;
    if (!absolute) sz = 4;
    else if (!viewport) sz = 16;

    SetSize(sz);
    if (absolute) theArray[1] |= 0x80;
    if (viewport) theArray[1] |= 0x40;
    *(PUInt16b *)&theArray[2] = (WORD)step;

    if (absolute) {
        *(PUInt32b *)&theArray[4] = (DWORD)min;
        *(PUInt32b *)&theArray[8] = (DWORD)max;
        *(PUInt32b *)&theArray[12] = (DWORD)current;
        if (viewport) {
            *(PUInt32b *)&theArray[16] = (DWORD)vportMin;
            *(PUInt32b *)&theArray[20] = (DWORD)vportMax;
        }
    }

    if (!absolute) m_cpType = e_simple;
    else if (!GetStep()) m_cpType = e_absolute;
    else m_cpType = e_relative;

    m_isActive = true;
}

void H284_ControlPoint::Set(BYTE id, PBoolean absolute, PBoolean viewport, WORD step,
        DWORD min, DWORD max, DWORD current, DWORD vportMin, DWORD vportMax)
{
    theArray[0] = id;
    SetData(absolute, viewport, step, min, max, current, vportMin, vportMax);
}

PBoolean H284_ControlPoint::SetData(const BYTE * data, int & length)
{
    // Get the first 2 BYTES
    BYTE info[2];
    memcpy(info,(const void *)data,2);
    if (info[0] != GetControlID())
        return false;

    bool absolute = ((info[1]&0x80) != 0);
    bool viewport = ((info[1]&0x40) != 0);

    if (!absolute) m_cpType = e_simple;
    else if (!GetStep()) m_cpType = e_absolute;
    else m_cpType = e_relative;

    int sz = H284_CPSIZE;
    if (!absolute) sz = 4;
    else if (!viewport) sz = 16;

    SetSize(sz);
    memcpy(theArray+1,(const void *)data,sz-1);

    length += sz;
    m_isActive = true;
    return true;
}

PBoolean H284_ControlPoint::Load(BYTE * data, int & length) const
{
    int sz = GetSize();
    memcpy((void *)data,theArray,sz);
    length += sz;
    return true;
}

BYTE H284_ControlPoint::GetControlID() const
{
    return theArray[0];
}

PBoolean H284_ControlPoint::IsAbsolute() const
{
    return ((theArray[1]&0x80) != 0);
}

PBoolean H284_ControlPoint::IsViewPort() const
{
    return ((theArray[1]&0x40) != 0);
}

WORD H284_ControlPoint::GetStep() const
{
    return *(PUInt16b *)&theArray[2];
}

DWORD H284_ControlPoint::GetMin() const
{
    if (!IsAbsolute()) return 0;

    return *(PUInt32b *)&theArray[4];
}

DWORD H284_ControlPoint::GetMax() const
{
    if (!IsAbsolute()) return 0;

    return *(PUInt32b *)&theArray[8];
}

DWORD H284_ControlPoint::GetCurrent() const
{
    if (!IsAbsolute()) return 0;

    return *(PUInt32b *)&theArray[12];
}

void H284_ControlPoint::SetCurrent(DWORD newPosition)
{
    *(PUInt32b *)&theArray[12] = newPosition;
}

DWORD H284_ControlPoint::GetViewPortMin() const
{
    if (!IsAbsolute() || !IsViewPort()) return 0;

    return *(PUInt32b *)&theArray[16];
}

DWORD H284_ControlPoint::GetViewPortMax() const
{
    if (!IsAbsolute() || IsViewPort()) return 0;

    return *(PUInt32b *)&theArray[20];
}

void H284_ControlPoint::BuildInstruction(Action act, DWORD value, H284_Instruction & inst)
{
    inst.SetControlID(GetControlID());
    inst.SetAction(act);
    inst.SetInstructionType(m_cpType);
    if (m_cpType == e_simple)
        inst.SetSize(4);
    else
        inst.SetPosition(value);
}

void H284_ControlPoint::HandleInstruction(const H284_Instruction & inst)
{
    unsigned id = (unsigned)inst.GetIdentifer();

    if (m_cpType != e_simple &&  id <= m_lastInstruction) {
        PTRACE(5,"H284\tCP: " << Name() << " ignore duplicate instruction: " << id);
        return;
    }
    m_handler.ReceiveInstruction((H224_H284Handler::ControlPointID)GetControlID(),
                                (Action)inst.GetAction(),inst.GetPosition());
    m_lastInstruction = id;
}

unsigned H284_ControlPoint::GetControlType()
{
    return m_cpType;
}

/////////////////////////////////////////////////////////////////

H284_Instruction::H284_Instruction()
: PBYTEArray(8), m_instType(H284_ControlPoint::e_unknown)
{
}

BYTE H284_Instruction::GetControlID() const
{
    return theArray[0];
}

void H284_Instruction::SetControlID(BYTE cp)
{
    theArray[0] = cp;
}

WORD H284_Instruction::GetIdentifer() const
{
    return *(PUInt16b *)&theArray[1];
}

void H284_Instruction::SetIdentifier(WORD id)
{
   *(PUInt16b *)&theArray[1] = id;
}

unsigned H284_Instruction::GetAction() const
{
    return (theArray[3]>>6)&3;
}

void H284_Instruction::SetAction(unsigned action)
{
    switch (action) {
        case H284_ControlPoint::e_stop:      theArray[3] |= 0x00; break;
        case H284_ControlPoint::e_positive:  theArray[3] |= 0x40; break;
        case H284_ControlPoint::e_negative:  theArray[3] |= 0x80; break;
        case H284_ControlPoint::e_continue:  theArray[3] |= 0xC0; break;
        default: break;
    }
}

DWORD H284_Instruction::GetPosition() const
{
    if (GetSize() <= 4)
        return 0;

    return *(PUInt32b *)&theArray[4];
}

void H284_Instruction::SetPosition(DWORD position)
{
    if (GetSize() <= 4)
        return;

    *(PUInt32b *)&theArray[4] = position;
}

int H284_Instruction::GetInstructionType()
{
    return m_instType;
}

void H284_Instruction::SetInstructionType(int newType)
{
    m_instType = newType;
}

/////////////////////////////////////////////////////////////////
// Must Declare for Factory Loader.
typedef H224_H284Handler H224_HandlerH284;
H224_HANDLER(H284);
/////////////////////////////////////////////////////////////////

template <class PAIR>
class deletepair { // PAIR::second_type is a pointer type
public:
    void operator()(const PAIR & p) { delete p.second; }
};

template <class M>
inline void DeleteObjectsInMap(M & m)
{
    typedef typename M::value_type PAIR;
    std::for_each(m.begin(), m.end(), deletepair<PAIR>());
	m.clear(); // delete pointers to deleted objects
}

////////////////////////////////////

H224_H284Handler::H224_H284Handler()
: H224_Handler("H284"), m_remoteSupport(false), m_lastInstruction(0)
{
    m_transmitFrame.SetClientDataSize(0);
}

H224_H284Handler::~H224_H284Handler()
{
    PWaitAndSignal m(m_ctrlMutex);
    DeleteObjectsInMap(m_controlMap);
}

PString H224_H284Handler::ControlIDAsString(BYTE id)
{
    PString str;
    switch (id) {
        case e_ForwardReverse :     str = "Forward/Reverse"; break;
        case e_LeftRight:           str = "Left/Right"; break;
        case e_UpDown :             str = "Up/Down"; break;
        case e_NeckPan :            str = "Neck/Pan"; break;
        case e_NeckTilt :           str = "Neck/Tilt"; break;
        case e_NeckRoll :           str = "Neck/Roll"; break;
        case e_CameraZoom :         str = "CameraZoom"; break;
        case e_CameraFocus :        str = "CameraFocus"; break;
        case e_CameraLighting :     str = "CameraLighting"; break;
        case e_CameraAuxLighting :  str = "CameraAuxLighting"; break;
        default:                    str = "Error/Unknown";
    }
    return str;
};

PBoolean H224_H284Handler::IsActive(H323Channel::Directions /*dir*/) const
{
    return (m_controlMap.size() != 0);
}

void H224_H284Handler::SetRemoteSupport()
{
    if (m_direction == H323Channel::IsReceiver)
        OnRemoteSupportDetected();

    m_remoteSupport = true;
}

void H224_H284Handler::SetLocalSupport()
{
    m_remoteSupport = true;
}

PBoolean H224_H284Handler::HasRemoteSupport()
{
    if (m_direction == H323Channel::IsReceiver)
        OnRemoteSupportDetected();

    return m_remoteSupport;
}

H284_ControlPoint * H224_H284Handler::GetControlPoint(BYTE id)
{
    PWaitAndSignal m(m_ctrlMutex);

    H284_ControlMap::iterator iter = m_controlMap.find(id);
    if (iter != m_controlMap.end())
        return iter->second;
    else
        return NULL;
}

void H224_H284Handler::SendExtraCapabilities() const
{
    PWaitAndSignal m(m_ctrlMutex);

    PBYTEArray extraCaps(1200);
    int sz = 0;
    H284_ControlMap::const_iterator iter = m_controlMap.begin();
    while (iter != m_controlMap.end()) {
        H284_ControlPoint * cp = iter->second;
        if (cp)
            cp->Load(extraCaps.GetPointer()+sz,sz);
        ++iter;
    }

    extraCaps.SetSize(sz);
    m_h224Handler->SendExtraCapabilitiesMessage(H284_CLIENT_ID, extraCaps.GetPointer(), extraCaps.GetSize());
}

void H224_H284Handler::OnReceivedExtraCapabilities(const BYTE * extraCaps, PINDEX size)
{
    PWaitAndSignal m(m_ctrlMutex);

    PINDEX sz = 0;
    while (sz < size) {
        BYTE info[2];
        memcpy(info,extraCaps+sz,2);
        BYTE id = info[0];
        bool absolute = ((info[1]&0x80) != 0);
        bool viewport = ((info[1]&0x40) != 0);
        int step = 0;
        if (OnReceivedControlData(id,extraCaps+sz,step)) {
            PTRACE(6,"H284\tP: " << sz << " found " << ControlIDAsString(id));
        } else {
            int step = H284_CPSIZE;
            if (!absolute) step = 4;
            else if (!viewport) step = 16;
            PTRACE(6,"H284\tP: " << sz << " skip " << id << " (" << ControlIDAsString(id) << ") step " << step);
        }
        sz += step;
    }
}

void H224_H284Handler::Add(ControlPointID id)
{
    PWaitAndSignal m(m_ctrlMutex);

    H284_ControlPoint * cp = new H284_ControlPoint(*this,id);

    if (OnAddControlPoint(id,*cp))
        m_controlMap.insert(std::pair<BYTE,H284_ControlPoint*>(id,cp));
}

void H224_H284Handler::Add(ControlPointID id, PBoolean absolute, PBoolean viewport, WORD step,
        DWORD min, DWORD max, DWORD current, DWORD vportMin, DWORD vportMax)
{
    PWaitAndSignal m(m_ctrlMutex);

    H284_ControlPoint * cp = new H284_ControlPoint(*this);
    cp->Set(id, absolute, viewport, step, min, max, current, vportMin, vportMax);

    if (OnAddControlPoint(id,*cp))
        m_controlMap.insert(std::pair<BYTE,H284_ControlPoint*>(id,cp));
}

PBoolean H224_H284Handler::OnAddControlPoint(ControlPointID id,H284_ControlPoint & /*cp*/)
{
    PTRACE(4,"H284\tAdd COntrol Point " << ControlIDAsString(id));
    return true;
}

PBoolean H224_H284Handler::OnReceivedControlData(BYTE id, const BYTE * data, int & length)
{
    H284_ControlPoint * cp = GetControlPoint(id);
    if (cp && cp->SetData(data,length))
		return true;

	return false;
}

void H224_H284Handler::ReceiveInstruction(ControlPointID id, H284_ControlPoint::Action action, unsigned value) const
{
    PTRACE(4,"H284\tInstruction Rec'd " << ControlIDAsString(id) << " action " << action << " " << value);
}

PBoolean H224_H284Handler::SendInstruction(ControlPointID id, H284_ControlPoint::Action action, unsigned value)
{

    H284_ControlPoint * cp = GetControlPoint(id);
    if (!cp || !cp->IsActive()) {
        PTRACE(4,"H284\tInstruction for " << ControlIDAsString(id) << " ignored as no remote support");
        return false;
    }

    H284_Instruction inst;
    cp->BuildInstruction(action,value,inst);
    PostInstruction(inst);
    PTRACE(4,"H284\tInstruction Sent " << ControlIDAsString(id) << " action " << action << " " << value);
    return true;
}

void H224_H284Handler::PostInstruction(H284_Instruction & inst)
{
    m_lastInstruction++;
    inst.SetIdentifier(m_lastInstruction);

    //   TODO: Need to put this on the resend queue etc.
    int sendCount = 1;
    switch (inst.GetInstructionType()) {
        case H284_ControlPoint::e_absolute:
           sendCount = 3;
        case H284_ControlPoint::e_simple:
        case H284_ControlPoint::e_relative:
        default:
            break;
    }

    for (PINDEX i=0; i < sendCount; ++i) {
        m_transmitFrame.AddInstruction(inst);
        m_h224Handler->TransmitClientFrame(H284_CLIENT_ID, m_transmitFrame);
        PThread::Sleep(10);
    }
    m_transmitFrame.SetClientDataSize(0);
}


void H224_H284Handler::OnReceivedMessage(const H224_Frame & message)
{
    ((const H284_Frame &)message).ReadInstructions(*this);
}

#endif // H224_H284


