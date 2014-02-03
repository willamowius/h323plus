/* h460_std25_pidf_lo.h
 *
 * Copyright (c) 2014 Spranto International Pte Ltd. All Rights Reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the 'License'); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the General Public License (the  'GNU License'), in which case the
 * provisions of GNU License are applicable instead of those
 * above. If you wish to allow use of your version of this file only
 * under the terms of the GNU License and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GNU License. If you do not delete
 * the provisions above, a recipient may use your version of this file
 * under either the MPL or the GNU License.'
 *
 * Software distributed under the License is distributed on an 'AS IS'
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#pragma once

#include <ptclib/pxml.h>

class H323_H46025_Message : public PXML
{
    PCLASSINFO(H323_H46025_Message, PXML);

  public:
    struct Device {
        Device()
        : username(PString()), deviceID(PString()) {}

        PString username;  // Username of the user preferably a URI
        PString deviceID;  // Device ID usually a MAC address
    };

    struct Civic {  
        Civic()
        : country(PString()), state(PString()), city(PString()), 
        street(PString()), houseNo(PString()), postCode(PString()) {}

        PString country;
        PString state;
        PString city;
        PString street;
        PString houseNo;
        PString postCode;
    };

    struct Geodetic {  
        Geodetic()
        : latitude(0), longitude(0) {}

        float latitude;
        float longitude;
    };

    H323_H46025_Message();

    // Build a message given the device information
    H323_H46025_Message(const Device & device);

    // Parse received PIDF-LO XML
    H323_H46025_Message(const PString & msg);


    PBoolean HasCivic();
    PBoolean AddCivic(Civic & civ);
    void GetCivic(const Civic & geo);

    PBoolean HasGeodetic();
    PBoolean AddGeodetic(Geodetic & geo);
    void GetGeodetic(const Geodetic & geo);

  protected:
    void BuildPIDF_LO(const Device & device);

  private:
    PXMLElement* rtElement;
    PXMLElement* locInformation;

};