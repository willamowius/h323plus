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


class H224_H284Handler : public H224_Handler
{
  PCLASSINFO(H224_H284Handler, H224_Handler);
    
public:
    
    H224_H284Handler();
    ~H224_H284Handler();

    static PStringList GetHandlerName() {  return PStringArray("H.284"); };
    virtual PString GetName() const
            { return GetHandlerName()[0]; }

    virtual BYTE GetClientID() const  
            { return H284_CLIENT_ID; }

    virtual void SetRemoteSupport();
    virtual PBoolean HasRemoteSupport();

    virtual void SendExtraCapabilities() const;
    virtual void OnReceivedExtraCapabilities(const BYTE *capabilities, PINDEX size);
    virtual void OnReceivedMessage(const H224_Frame & message);

protected:
    PBoolean remoteSupport;
};

#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(H284, H224_Handler);
#endif



#endif // __H323PLUS_H284_H