/* h284.h
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


#ifndef __H323PLUS_H284_H
#define __H323PLUS_H284_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <list>
#include <map>

#define H284_CLIENT_ID 0x03

////////////////////////////////////////////////////////////////

class H284_Instruction : public PBYTEArray
{
public:
    H284_Instruction();
    
    BYTE GetControlID() const;
    void SetControlID(BYTE cp);

    WORD GetIdentifer() const;
    void SetIdentifier(WORD id);

    unsigned GetAction() const;
    void SetAction(unsigned action);

    DWORD GetPosition() const;
    void SetPosition(DWORD position);

    int GetInstructionType();
    void SetInstructionType(int newType);

protected:
    int m_instType;
};
typedef std::list<H284_Instruction> H284_InstructionList;

////////////////////////////////////////////////////////////////

class H224_H284Handler;
class H284_Frame : public H224_Frame
{
    PCLASSINFO(H284_Frame, H224_Frame);
    
public:    
    H284_Frame();
    ~H284_Frame();

    void     AddInstruction(const H284_Instruction & inst);
    PBoolean ReadInstructions(H224_H284Handler & handler) const;
};

////////////////////////////////////////////////////////////////

class H224_H284Handler;
class H284_ControlPoint : public PBYTEArray 
{
public:

    enum Type {
        e_unknown            = 0,
        e_simple             = 1,
        e_relative           = 2,
        e_absolute           = 3
    };

    enum  Action {
        e_stop               = 0,
        e_positive           = 1,
        e_negative           = 2,
        e_continue           = 3
    };

    H284_ControlPoint(H224_H284Handler & handler, BYTE ctrlID=0);

    PString Name();

    void Set(BYTE id, PBoolean absolute = false, PBoolean viewport=false, WORD step=0, 
        DWORD min=0, DWORD max=0, DWORD current=0, 
        DWORD vportMin=0, DWORD vportMax=0
    );

    void BuildInstruction(Action act, DWORD value, H284_Instruction & inst);
    void HandleInstruction(const H284_Instruction & inst);

    PBoolean SetData(const BYTE * data, int & length);
    PBoolean Load(BYTE * data, int & length) const;

    unsigned GetControlType();

    BYTE GetControlID() const;
    PBoolean IsAbsolute() const;
    PBoolean IsViewPort() const;
    WORD GetStep() const;
    DWORD GetMin() const;
    DWORD GetMax() const;
    DWORD GetCurrent() const;
    void SetCurrent(DWORD newPosition);
    DWORD GetViewPortMin() const;
    DWORD GetViewPortMax() const;

protected :
    H224_H284Handler &    m_handler;
    Type                m_cpType;
    unsigned            m_lastInstruction;

};
typedef std::map<BYTE,H284_ControlPoint> H284_ControlMap;


////////////////////////////////////////////////////////////////

class H224_H284Handler : public H224_Handler
{
  PCLASSINFO(H224_H284Handler, H224_Handler);
    
public:
    
    enum ControlPointID {
        e_IllegalID             = 0x00,
        e_ForwardReverse        = 0x01,
        e_LeftRight             = 0x02,
        e_UpDown                = 0x03,
        e_NeckPan               = 0x07,
        e_NeckTilt              = 0x08,
        e_NeckRoll              = 0x09,
        e_CameraZoom            = 0x0c,
        e_CameraFocus           = 0x0d,
        e_CameraLighting        = 0x0e,
        e_CameraAuxLighting     = 0x0f
    };
    static PString ControlIDAsString(BYTE id);

    H224_H284Handler();
    ~H224_H284Handler();

    virtual PBoolean IsActive() const;

    static PStringList GetHandlerName() {  return PStringArray("H284"); };
    virtual PString GetName() const
            { return GetHandlerName()[0]; }

    virtual BYTE GetClientID() const  
            { return H284_CLIENT_ID; }

    virtual void SetRemoteSupport();
    virtual PBoolean HasRemoteSupport();

    void Add(ControlPointID id, PBoolean absolute = false, PBoolean viewport=false, WORD step=0, 
        DWORD min=0, DWORD max=0, DWORD current=0, 
        DWORD vportMin=0, DWORD vportMax=0
    );
    virtual PBoolean OnAddControlPoint(ControlPointID id,H284_ControlPoint & cp);

    PBoolean SendInstruction(ControlPointID id, H284_ControlPoint::Action action, unsigned value);
    virtual void ReceiveInstruction(ControlPointID id, H284_ControlPoint::Action action, unsigned value) const;

    void PostInstruction(H284_Instruction & inst);

    H284_ControlPoint * GetControlPoint(BYTE id);

    virtual void SendExtraCapabilities() const;
    virtual void OnReceivedExtraCapabilities(const BYTE *capabilities, PINDEX size);
    virtual void OnReceivedMessage(const H224_Frame & message);

protected:
    PBoolean            m_remoteSupport;
    unsigned            m_lastInstruction;
    PMutex              m_ctrlMutex;
    H284_ControlMap     m_controlMap;

    H284_Frame          m_transmitFrame;
};

#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(H284, H224_Handler);
#endif



#endif // __H323PLUS_H284_H