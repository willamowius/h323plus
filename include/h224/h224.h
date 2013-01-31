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
 * $Log$
 * Revision 1.1  2013/01/28 23:52:23  shorne
 * Restructure files create h224 directory
 *
 * Revision 1.2  2008/05/23 11:19:21  willamowius
 * switch BOOL to PBoolean to be able to compile with Ptlib 2.2.x
 *
 * Revision 1.1  2007/08/06 20:50:48  shorne
 * First commit of h323plus
 *
 * Revision 1.1  2006/06/22 11:07:22  shorne
 * Backport of FECC (H.224) from Opal
 *
 * Revision 1.2  2006/04/23 18:52:19  dsandras
 * Removed warnings when compiling with gcc on Linux.
 *
 * Revision 1.1  2006/04/20 16:48:17  hfriederich
 * Initial version of H.224/H.281 implementation.
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

#define H224_HEADER_SIZE 6

#define H224_BROADCAST 0x0000

#define H281_CLIENT_ID 0x01
#define T140_CLIENT_ID 0x02
#define H284_CLIENT_ID 0x03

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

    void AttachH224Handler(OpalH224Handler * h224Handler);

    virtual PString GetName() const = 0;
    virtual BYTE GetClientID() const = 0;
    virtual void SetRemoteSupport() = 0;
    virtual void SendExtraCapabilities() const = 0;
    virtual void OnReceivedExtraCapabilities(const BYTE *capabilities, PINDEX size) =0;
    virtual void OnReceivedMessage(const H224_Frame & message) = 0;

    static PStringArray GetHandlerNames(PPluginManager * pluginMgr = NULL);
    static H224_Handler * CreateHandler(const PString & handlerName, PPluginManager * pluginMgr = NULL);

protected:
    OpalH224Handler * m_h224Handler;
    PString              m_h224Display;
};


/////////////////////////////////////////////////////////////////////

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
PCREATE_PLUGIN_STATIC(name, H224_Handler, &H224_##name##Handler_descriptor); \

#include <h224/h224handler.h>

#endif // __H323_H224_H

