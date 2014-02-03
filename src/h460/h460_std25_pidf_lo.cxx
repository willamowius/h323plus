/* h460_std25_pidf_lo.cxx
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

#include <ptlib.h>
#include <openh323buildopts.h>

#ifdef H323_H46025

#include <h460/h460_std25_pidf_lo.h>

/*
Example and test PIDF-LO NG911 Message

<?xml version="1.0" encoding="UTF-8"?>
<presence xmlns="urn:ietf:params:xml:ns:pidf" 
	xmlns:dm="urn:ietf:params:xml:ns:pidf:data-model" 
	xmlns:gp="urn:ietf:params:xml:ns:pidf:geopriv10" 
	xmlns:cl="urn:ietf:params:xml:ns:pidf:geopriv10:civicAddr" 
	entity="default@h323plus.org" 
	xmlns:gml="http://www.opengis.net/gml">
	<dm:device id="default">
		<gp:geopriv>
			<gp:location-info>
				<gml:Point srsName="urn:ogc:def:crs:EPSG::4326">
					<gml:pos>0.000 0.000</gml:pos>
				</gml:Point>
				<cl:civicAddress>
					<cl:country>US</cl:country>
					<cl:A1>New York</cl:A1>
					<cl:A3>New York</cl:A3>
					<cl:A6>Broadway</cl:A6>
					<cl:HNO>123</cl:HNO>
					<cl:PC>10027-0401</cl:PC>
				</cl:civicAddress>
			</gp:location-info>
			<gp:usage-rules/>
		</gp:geopriv>
		<dm:deviceID>mac:11234556667</dm:deviceID>
		<dm:timestamp>2014-02-03T10:08:44Z</dm:timestamp>
	</dm:device>
</presence>
*/


H323_H46025_Message::H323_H46025_Message()
: rtElement(NULL), locInformation(NULL)
{

}

H323_H46025_Message::H323_H46025_Message(const Device & device)
: rtElement(NULL), locInformation(NULL)
{
    BuildPIDF_LO(device);
}

H323_H46025_Message::H323_H46025_Message(const PString & msg)
: rtElement(NULL), locInformation(NULL)
{
    Load(msg);
}

PBoolean H323_H46025_Message::HasCivic()
{
    return false;
}

PBoolean H323_H46025_Message::AddCivic(Civic & civ)
{
    if (!locInformation)
        return false;

    PXMLElement * g = (PXMLElement *)locInformation->AddElement("cl:civicAddress");
    if (!civ.country) g->AddElement("cl:country",civ.country);
    if (!civ.state)   g->AddElement("cl:A1",civ.state);
    if (!civ.city)    g->AddElement("cl:A3",civ.city);
    if (!civ.street)  g->AddElement("cl:A6",civ.street);
    if (!civ.houseNo) g->AddElement("cl:HNO",civ.houseNo);
    if (!civ.postCode)g->AddElement("cl:PC",civ.postCode);
    return true;
}

void H323_H46025_Message::GetCivic(const Civic & /*civ*/)
{

}

PBoolean H323_H46025_Message::HasGeodetic()
{
    return false;
}

PBoolean H323_H46025_Message::AddGeodetic(Geodetic & geo)
{
    if (!locInformation)
        return false;

    PXMLElement * g = (PXMLElement *)locInformation->AddElement("gml:Point");
    g->SetAttribute("srsName", "urn:ogc:def:crs:EPSG::4326", true);
    g->AddElement("gml:pos", PString(PString::Decimal,geo.longitude,3) & " " & PString(PString::Decimal,geo.latitude,3));

    return true;
}

void H323_H46025_Message::GetGeodetic(const Geodetic & /*geo*/)
{

}


void H323_H46025_Message::BuildPIDF_LO(const Device & device)
{
    SetRootElement("presence");
    
    rtElement = GetRootElement();

    rtElement->SetAttribute("xmlns", "urn:ietf:params:xml:ns:pidf", true);
    rtElement->SetAttribute("xmlns:dm", "urn:ietf:params:xml:ns:pidf:data-model", true );
    rtElement->SetAttribute("xmlns:gp", "urn:ietf:params:xml:ns:pidf:geopriv10", true );
    rtElement->SetAttribute("xmlns:cl", "urn:ietf:params:xml:ns:pidf:geopriv10:civicAddr", true );
    rtElement->SetAttribute("xmlns:gml", "http://www.opengis.net/gml", true );
    rtElement->SetAttribute("entity", device.username);

    PString devName = device.username;
    PINDEX at = devName.Find('@');
    if (at != P_MAX_INDEX)
        devName = devName.Left(at);

    PXMLElement * e1 = (PXMLElement *)rtElement->AddElement("dm:device");
        e1->SetAttribute("id", devName);

    PXMLElement * e2 = (PXMLElement *)e1->AddElement("gp:geopriv");
    locInformation = (PXMLElement *)e2->AddElement("gp:location-info");

    e2->AddElement("gp:usage-rules");

    if (!device.deviceID)
        e1->AddElement("dm:deviceID", "mac:" + device.deviceID);

    PTime now;
    PString nowTime = now.AsString(PTime::RFC3339, PTime::UTC);
    nowTime = nowTime.Left(nowTime.GetLength()-1);  // Remove last Z
    e1->AddElement("dm:timestamp", nowTime);
}


#endif  // H323_H46025