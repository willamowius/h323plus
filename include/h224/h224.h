/*
 * h224.h
 *
 * H.224 PDU implementation for the OpenH323 Project.
 *
 * Copyright (c) 2006 Network for Educational Technology, ETH Zurich.
 * Written by Hannes Friederich.
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
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef __H323_H224_H
#define __H323_H224_H

#ifdef P_USE_PRAGMA
#pragma interface
#pragma once
#endif

#include <ptlib/pluginmgr.h>
#include <q922.h>
#include <channels.h>

#define H224_HEADER_SIZE 6

#define H224_BROADCAST 0x0000


class H224_Frame : public Q922_Frame
{
  PCLASSINFO(H224_Frame, Q922_Frame);
    
public:
    
  H224_Frame(PINDEX clientDataSize = 254);
  ~H224_Frame();
    
  PBoolean IsHighPriority() const { return (GetLowOrderAddressOctet() == 0x71); }
  void SetHighPriority(PBoolean flag);
    
  WORD GetDestinationTerminalAddress() const;
  void SetDestinationTerminalAddress(WORD destination);
    
  WORD GetSourceTerminalAddress() const;
  void SetSourceTerminalAddress(WORD source);
    
  // Only standard client IDs are supported at the moment
  BYTE GetClientID() const;
  void SetClientID(BYTE clientID);
    
  PBoolean GetBS() const;
  void SetBS(PBoolean bs);
    
  PBoolean GetES() const;
  void SetES(PBoolean es);
    
  PBoolean GetC1() const;
  void SetC1(PBoolean c1);
    
  PBoolean GetC0() const;
  void SetC0(PBoolean c0);
    
  BYTE GetSegmentNumber() const;
  void SetSegmentNumber(BYTE segmentNumber);
    
  BYTE *GetClientDataPtr() const { return (GetInformationFieldPtr() + H224_HEADER_SIZE); }
    
  PINDEX GetClientDataSize() const { return (GetInformationFieldSize() - H224_HEADER_SIZE); }
  void SetClientDataSize(PINDEX size) { SetInformationFieldSize(size + H224_HEADER_SIZE); }
    
  PBoolean Decode(const BYTE *data, PINDEX size);
};

class OpalH224Handler;
class H224_Handler : public PObject
{
    PCLASSINFO(H224_Handler, PObject);

public:
    H224_Handler(const PString & name);
    ~H224_Handler();

    virtual PBoolean IsActive(H323Channel::Directions /*dir*/) const { return true; }

    void AttachH224Handler(OpalH224Handler * h224Handler);

    virtual PString GetName() const = 0;
    virtual BYTE GetClientID() const = 0;
    virtual void SetRemoteSupport() = 0;
    virtual void SetLocalSupport() = 0;
    virtual void OnRemoteSupportDetected() = 0;
    virtual void SendExtraCapabilities() const = 0;
    virtual void OnReceivedExtraCapabilities(const BYTE *capabilities, PINDEX size) =0;
    virtual void OnReceivedMessage(const H224_Frame & message) = 0;

    static PStringArray GetHandlerNames(PPluginManager * pluginMgr = NULL);
    static H224_Handler * CreateHandler(const PString & handlerName, PPluginManager * pluginMgr = NULL);

protected:
    OpalH224Handler *        m_h224Handler;
    int                      m_direction;
    PString                  m_h224Display;
};


/////////////////////////////////////////////////////////////////////

#if PTLIB_VER >= 2130

PCREATE_PLUGIN_DEVICE(H224_Handler);
#define H224_HANDLER_EX(name, extra) \
PCREATE_PLUGIN(name, H224_Handler, H224_Handler##name, PPlugin_H224_Handler, \
  virtual PStringArray GetDeviceNames(P_INT_PTR /*userData*/) const { return H224_Handler##name::GetHandlerName(); } \
  virtual bool ValidateDeviceName(const PString & deviceName, int /*userData*/) const { return (deviceName == H224_Handler##name::GetHandlerName()[0]); } \
extra)
#define H224_HANDLER(name) H224_HANDLER_EX(name, )

#else

template <class className> class H224PluginServiceDescriptor : public PDevicePluginServiceDescriptor
{
  public:
    virtual PObject *   CreateInstance(int /*userData*/) const { return new className; }
    virtual PStringArray GetDeviceNames(int /*userData*/) const { 
               return className::GetHandlerName(); 
    }
    virtual bool  ValidateDeviceName(const PString & deviceName, int /*userData*/) const { 
            return (deviceName == className::GetHandlerName()[0]);
    }
};

#define H224_HANDLER(name)    \
    static H224PluginServiceDescriptor<H224_##name##Handler> H224_##name##Handler_descriptor; \
PCREATE_PLUGIN_STATIC(name, H224_Handler, &H224_##name##Handler_descriptor);
#endif

#include <h224/h224handler.h>

#endif // __H323_H224_H

