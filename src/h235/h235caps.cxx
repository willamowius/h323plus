/*
 * h235caps.cxx
 *
 * H.235 Capability wrapper class.
 *
 * h323plus library
 *
 * Copyright (c) 2011 Spranto Australia Pty Ltd.
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
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte. Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id $
 *
 *
 */


#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

#include "h235/h235caps.h"


const char * ALG_AES128 = "2.16.840.1.101.3.4.1.2";
const char * ALG_AES256 = "2.16.840.1.101.3.4.1.42";

/////////////////////////////////////////////////////////////////////////////

H235SecurityCapability::H235SecurityCapability(unsigned capabilityNo)
: m_capNumber(capabilityNo)
{
}

PObject * H235SecurityCapability::Clone() const
{
  return new H235SecurityCapability(*this);
}

H323Capability::MainTypes H235SecurityCapability::GetMainType() const
{
  return e_Security;
}

unsigned H235SecurityCapability::GetSubType()  const
{
    return 0;
}

PString H235SecurityCapability::GetFormatName() const
{
  return "Security";
}

H323Channel * H235SecurityCapability::CreateChannel(H323Connection &,
                                                      H323Channel::Directions,
                                                      unsigned,
                                                      const H245_H2250LogicalChannelParameters *) const
{
  PTRACE(1, "Codec\tCannot create Security channel");
  return NULL;
}

H323Codec * H235SecurityCapability::CreateCodec(H323Codec::Direction) const
{
  PTRACE(1, "Codec\tCannot create security codec");
  return NULL;
}

PBoolean H235SecurityCapability::OnSendingPDU(H245_Capability & pdu) const
{

  pdu.SetTag(H245_Capability::e_h235SecurityCapability);
  H245_H235SecurityCapability & sec = pdu;

  H245_EncryptionAuthenticationAndIntegrity & encAuth = sec.m_encryptionAuthenticationAndIntegrity;
  encAuth.IncludeOptionalField(H245_EncryptionAuthenticationAndIntegrity::e_encryptionCapability);

  H245_EncryptionCapability & enc = encAuth.m_encryptionCapability;

  enc.SetSize(1);
  H245_MediaEncryptionAlgorithm & alg128 = enc[0];
  alg128.SetTag(H245_MediaEncryptionAlgorithm::e_algorithm);
  PASN_ObjectId & id = alg128;
  id.SetValue(ALG_AES128);

  H245_CapabilityTableEntryNumber & capNo = sec.m_mediaCapability;
  capNo = m_capNumber;

  return TRUE;
}

PBoolean H235SecurityCapability::OnSendingPDU(H245_DataType &) const
{
  PTRACE(1, "Codec\tCannot have Security Capability in DataType");
  return FALSE;
}

PBoolean H235SecurityCapability::OnSendingPDU(H245_ModeElement &) const
{
  PTRACE(1, "Codec\tCannot have Security Capability in ModeElement");
  return FALSE;
}

PBoolean H235SecurityCapability::OnReceivedPDU(const H245_Capability & pdu)
{

  H323Capability::OnReceivedPDU(pdu);

  return TRUE;
}

PBoolean H235SecurityCapability::OnReceivedPDU(const H245_DataType &, PBoolean)
{
  PTRACE(1, "Codec\tCannot have Security Capability in DataType");
  return FALSE;
}

/////////////////////////////////////////////////////////////////////////////

H323SecureRealTimeCapability::H323SecureRealTimeCapability(H323Capability & childCapability)
				: ChildCapability(childCapability), nrtpqos(NULL)
{
}


H323SecureRealTimeCapability::H323SecureRealTimeCapability(RTP_QOS * _rtpqos,H323Capability & childCapability)
				: ChildCapability(childCapability), nrtpqos(_rtpqos)
{
}

H323SecureRealTimeCapability::~H323SecureRealTimeCapability()
{
}


void H323SecureRealTimeCapability::AttachQoS(RTP_QOS * _rtpqos)
{
	  delete nrtpqos;  
	  nrtpqos = _rtpqos;
}


#ifdef H323_AEC
void H323SecureRealTimeCapability::AttachAEC(PAec * _AEC)
{
	  aec = _AEC;
}
#endif


H323Channel * H323SecureRealTimeCapability::CreateChannel(H323Connection & connection,
                                                    H323Channel::Directions dir,
                                                    unsigned sessionID,
                                 const H245_H2250LogicalChannelParameters * param) const
{

  RTP_Session * session;			  // Session

  if (param != NULL) {
	session = connection.UseSession(param->m_sessionID, param->m_mediaControlChannel, dir, nrtpqos);
  } else {
    // Make a fake transport address from the connection so gets initialised with
    // the transport type (IP, IPX, multicast etc).
    H245_TransportAddress addr;
    connection.GetControlChannel().SetUpTransportPDU(addr, H323Transport::UseLocalTSAP);

    session = connection.UseSession(sessionID, addr, dir, nrtpqos);
  }

  if (session == NULL)
    return NULL;
/*
  // If Successful Key Exchange done on in H225 replace tls session
	  if (Ptls.con.CallSA.Contains(connection.GetCallToken())) {
		  PTLSsession & authSession = *(PTLSsession *)Ptls.con.CallSA.GetAt(connection.GetCallToken());

		     if (dir == H323Channel::IsReceiver) 
			    Ptls.CloseSession();

		//	 if (authSession.vsState == PTLSsession::vsNoAuthentication) 
		//		 authSession.vsState = PTLSsession::vsWaiting;

			 return new H323SecureRTPChannel(connection, *this, dir, *session, authSession);  
	  } else 
		  Ptls.StartSession(); 

	return new H323SecureRTPChannel(connection, *this, dir, *session, Ptls); 
*/
   return connection.CreateRealTimeLogicalChannel(*this, dir, sessionID, param, nrtpqos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

H323SecureAudioCapability::H323SecureAudioCapability(H323Capability & childCapability,
													 PTLSChType Ch
													 )
   : H323SecureRealTimeCapability(childCapability)
{
	rxFramesInPacket = GetRxFramesInPacket();
	txFramesInPacket = GetTxFramesInPacket();
	chtype = Ch;

#ifdef H323_AEC
        aec = NULL;
#endif
}

H323SecureAudioCapability::H323SecureAudioCapability(H323Capability & childCapability,
													 RTP_QOS * _rtpqos,
													 PTLSChType Ch
													 )
   : H323SecureRealTimeCapability(_rtpqos,childCapability)
{
	rxFramesInPacket = GetRxFramesInPacket();
	txFramesInPacket = GetTxFramesInPacket();
	nrtpqos = _rtpqos;
	chtype = Ch;

#ifdef H323_AEC
    aec = NULL;
#endif
}

H323SecureAudioCapability::~H323SecureAudioCapability()
{
}

H323Capability::MainTypes H323SecureAudioCapability::GetMainType() const
{
  return e_Audio;
}


PObject * H323SecureAudioCapability::Clone() const
{

 PTRACE(4, "H235RTP\tCloning Capability: " << GetFormatName());

	PBoolean IsClone = FALSE;
	PTLSChType ch = PTLSChNew;

	switch (chtype) {
	case PTLSChNew:	
		   ch = PTLSChClone;
		   IsClone = TRUE;
		break;
	case PTLSChClone:
		   ch = PTLSChannel;
		break;
	case PTLSChannel:
		   ch = PTLSChannel;
		break;
	}

    H323Capability * Ccap = (H323Capability *)ChildCapability.Clone();

	H323SecureAudioCapability * child = 
        new H323SecureAudioCapability(*Ccap, nrtpqos, ch);

#ifdef H323_AEC
	child->AttachAEC(aec);
#endif

    return child;
}

PString H323SecureAudioCapability::GetFormatName() const
{
  return ChildCapability.GetFormatName() + " #";
}


unsigned H323SecureAudioCapability::GetSubType() const
{
  return ChildCapability.GetSubType();
}

unsigned H323SecureAudioCapability::GetDefaultSessionID() const
{
  return RTP_Session::DefaultAudioSessionID;
}

void H323SecureAudioCapability::SetTxFramesInPacket(unsigned frames)
{

	txFramesInPacket = frames;
    ChildCapability.SetTxFramesInPacket(frames);
}


unsigned H323SecureAudioCapability::GetTxFramesInPacket() const
{

  return ChildCapability.GetTxFramesInPacket();
}


unsigned H323SecureAudioCapability::GetRxFramesInPacket() const
{

  return ChildCapability.GetRxFramesInPacket();
}


PBoolean H323SecureAudioCapability::OnSendingPDU(H245_Capability & cap) const
{
 	return ChildCapability.OnSendingPDU(cap);

}


PBoolean H323SecureAudioCapability::OnSendingPDU(H245_DataType & dataType) const
{
		return ChildCapability.OnSendingPDU(dataType);
}


PBoolean H323SecureAudioCapability::OnSendingPDU(H245_ModeElement & mode) const
{
		return ChildCapability.OnSendingPDU(mode);
}


PBoolean H323SecureAudioCapability::OnReceivedPDU(const H245_Capability & cap)
{
	return ChildCapability.OnReceivedPDU(cap);
}


PBoolean H323SecureAudioCapability::OnReceivedPDU(const H245_DataType & dataType, PBoolean receiver)
{
	return ChildCapability.OnReceivedPDU(dataType,receiver);
}


H323Codec * H323SecureAudioCapability::CreateCodec(H323Codec::Direction direction) const
{
 PTRACE(4, "H235RTP\tCodec creation:");
/*
 H323SecureAudioCodec * child = 
			(H323SecureAudioCodec *)ChildCapability.CreateCodec(direction);

#ifdef H323_AEC
  if (aec != NULL)
    child->AttachAEC(aec);
#endif

    return child;
*/
    return ChildCapability.CreateCodec(direction);
}

///////////////////////////////////////////////////////////////////////////// 

H323SecureVideoCapability::H323SecureVideoCapability(H323Capability & childCapability,
													 PTLSChType Ch
													 )
   : H323SecureRealTimeCapability(childCapability)
{
	chtype = Ch;
}

H323Capability::MainTypes H323SecureVideoCapability::GetMainType() const
{ 
    return e_Video;
}


PObject * H323SecureVideoCapability::Clone() const
{
	PTRACE(4, "H235RTP\tCloning Capability: " << GetFormatName());

	PBoolean IsClone = FALSE;
	PTLSChType ch = PTLSChNew;

	switch (chtype) {
	case PTLSChNew:	
//cout << "New Clone." << "\n";
		   ch = PTLSChClone;
		   IsClone = TRUE;
		break;
	case PTLSChClone:
		   ch = PTLSChannel;
		break;
	case PTLSChannel:
		   ch = PTLSChannel;
		break;
	}

  H323VideoCapability * Ccap = (H323VideoCapability *)ChildCapability.Clone();

  return new H323SecureVideoCapability(*Ccap,ch);
}

PObject::Comparison H323SecureVideoCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323SecureVideoCapability))
    return LessThan;

  Comparison result = H323Capability::Compare(obj);
  if (result != EqualTo) 
    return result;

  const H323SecureVideoCapability & other = (const H323SecureVideoCapability &)obj;

  return ChildCapability.Compare(other.GetChildCapability());
}

unsigned H323SecureVideoCapability::GetDefaultSessionID() const
{
  return RTP_Session::DefaultVideoSessionID;
}



////////////////////////////////////////////////////////////////////////
// PDU Sending

PBoolean H323SecureVideoCapability::OnSendingPDU(H245_Capability & cap) const
{
	return ChildCapability.OnSendingPDU(cap);
}


PBoolean H323SecureVideoCapability::OnSendingPDU(H245_DataType & dataType) const
{
	return ChildCapability.OnSendingPDU(dataType);
}


PBoolean H323SecureVideoCapability::OnSendingPDU(H245_ModeElement & mode) const
{
    return ChildCapability.OnSendingPDU(mode);
}


PBoolean H323SecureVideoCapability::OnReceivedPDU(const H245_Capability & cap)
{
	return ChildCapability.OnReceivedPDU(cap);
}


PBoolean H323SecureVideoCapability::OnReceivedPDU(const H245_DataType & dataType,PBoolean receiver)
{
	return ChildCapability.OnReceivedPDU(dataType,receiver);
}

//////////////////////////////////////////////////////////////////////////////
// Child Capability Intercept

PBoolean H323SecureVideoCapability::OnSendingPDU(H245_VideoCapability & pdu) const
{
	return ((H323VideoCapability &)ChildCapability).OnSendingPDU(pdu);
}

PBoolean H323SecureVideoCapability::OnSendingPDU(H245_VideoMode & pdu) const
{
	return ((H323VideoCapability &)ChildCapability).OnSendingPDU(pdu);
}

PBoolean H323SecureVideoCapability::OnReceivedPDU(const H245_VideoCapability & pdu)
{
	return ((H323VideoCapability &)ChildCapability).OnReceivedPDU(pdu);
}


PString H323SecureVideoCapability::GetFormatName() const
{
  return ChildCapability.GetFormatName() + " #";
}


unsigned H323SecureVideoCapability::GetSubType() const
{
  return ChildCapability.GetSubType();
}


H323Codec * H323SecureVideoCapability::CreateCodec(H323Codec::Direction direction) const
{
    return ChildCapability.CreateCodec(direction);
}


/////////////////////////////////////////////////////////////////////////////

H235Capabilities::H235Capabilities()
{
}

H235Capabilities::H235Capabilities(const H323Capabilities & original)
{

  for (PINDEX i = 0; i < original.GetSize(); i++)
    WrapCapability(original[i]);

  const H323CapabilitiesSet & origSet = original.GetSet();
  PINDEX outerSize = origSet.GetSize();
  set.SetSize(outerSize);
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    PINDEX middleSize = origSet[outer].GetSize();
    set[outer].SetSize(middleSize);
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      PINDEX innerSize = origSet[outer][middle].GetSize();
      for (PINDEX inner = 0; inner < innerSize; inner++)
        set[outer][middle].Append(FindCapability(origSet[outer][middle][inner].GetCapabilityNumber()));
    }
  }
}

H235Capabilities::H235Capabilities(const H323Connection & /*connection*/, const H245_TerminalCapabilitySet & /*pdu*/)
{

}

void H235Capabilities::WrapCapability(H323Capability & Cap)
{
    switch (Cap.GetMainType()) {
        case H323Capability::e_Audio:
            
            break;
        case H323Capability::e_Video:

            break;
        case H323Capability::e_Data:
        case H323Capability::e_UserInput:
        case H323Capability::e_ExtendVideo:
        default:
            break;
    }
}

#endif

