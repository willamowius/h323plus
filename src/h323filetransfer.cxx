/*
 * h323filetransfer.cxx
 *
 * H323 File Transfer class.
 *
 * h323plus library
 *
 * Copyright (c) 2008 ISVO (Asia) Pte. Ltd.
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
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>
#include <h323.h>

#ifdef H323_FILE

#ifdef __GNUC__
#pragma implementation "h323filetransfer.h"
#endif

#include "h323filetransfer.h"

#include <h323pdu.h>


static const char * FileTransferOID = "1.3.6.1.4.1.17090.1.2";
static const char * FileTransferListOID = "1.3.6.1.4.1.17090.1.2.1";

static struct  {
   int blocksize;
   int identifier;
} paramBlockSize[8] = {
  {  512,    1 },
  { 1024,    2 },
  { 1428,    4 },
  { 2048,    8 },
  { 4096,   16 },
  { 8192,   32 },
  { 16384,  64 },
  { 32768, 128 },
};

static int SetParameterBlockSize(int size)
{
    for (PINDEX i = 0; i < 8 ; i++) {
        if (paramBlockSize[i].blocksize == size)
            return paramBlockSize[i].identifier;
    }
    return 16;
}

static int GetParameterBlockSize(int size)
{
    for (PINDEX i = 0; i < 8 ; i++) {
        if (paramBlockSize[i].identifier == size)
            return paramBlockSize[i].blocksize;
    }
    return 16;
}

H323FileTransferCapability::H323FileTransferCapability()
: H323DataCapability(132000), m_blockOctets(4096)
{
    m_blockSize = SetParameterBlockSize(m_blockOctets);  // parameter block size
    m_transferMode = 1;                                     // Transfer mode is RTP encaptulated
}

H323FileTransferCapability::H323FileTransferCapability(unsigned maxBitRate, unsigned maxBlockSize)
: H323DataCapability(maxBitRate), m_blockOctets(maxBlockSize)
{
    m_blockSize = SetParameterBlockSize(m_blockOctets);  // parameter block size
    m_transferMode = 1;                                     // Transfer mode is RTP encaptulated
}

PBoolean H323FileTransferCapability::OnReceivedPDU(const H245_DataApplicationCapability & pdu)
{
  if (pdu.m_application.GetTag() != H245_DataApplicationCapability_application::e_genericDataCapability)
        return FALSE;

  //maxBitRate = pdu.m_maxBitRate*100;
  const H245_GenericCapability & genCapability = (const H245_GenericCapability &)pdu.m_application;
  return OnReceivedPDU(genCapability);
}

PBoolean H323FileTransferCapability::OnSendingPDU(H245_DataApplicationCapability & pdu) const
{
  pdu.m_maxBitRate = maxBitRate/100;
  pdu.m_application.SetTag(H245_DataApplicationCapability_application::e_genericDataCapability);
    
  H245_GenericCapability & genCapability = (H245_GenericCapability &)pdu.m_application;
  return OnSendingPDU(genCapability);
}

PObject::Comparison H323FileTransferCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323FileTransferCapability))
      return LessThan;

  const H323FileTransferCapability & other = (const H323FileTransferCapability &)obj;

  if (//(m_blockSize == other.GetBlockSize()) && 'We don't need to check the Max Block size
      (m_transferMode == other.GetTransferMode()))
                return EqualTo;

  return GreaterThan;
}

PObject * H323FileTransferCapability::Clone() const
{
   return new H323FileTransferCapability(*this);
}

PBoolean H323FileTransferCapability::OnReceivedPDU(const H245_GenericCapability & pdu)
{

   const H245_CapabilityIdentifier & capId = pdu.m_capabilityIdentifier; 

   if (capId.GetTag() != H245_CapabilityIdentifier::e_standard)
       return FALSE;
    
   const PASN_ObjectId & id = capId;
   if (id.AsString() != FileTransferOID)
       return FALSE;

   if (pdu.HasOptionalField(H245_GenericCapability::e_maxBitRate)) {
      const PASN_Integer & bitRate = pdu.m_maxBitRate;
      maxBitRate = bitRate*100;
   }

   if (!pdu.HasOptionalField(H245_GenericCapability::e_collapsing)) 
        return FALSE;

   const H245_ArrayOf_GenericParameter & params = pdu.m_collapsing;
   for (PINDEX j=0; j<params.GetSize(); j++) {
     const H245_GenericParameter & content = params[j];
     if (content.m_parameterIdentifier.GetTag() == H245_ParameterIdentifier::e_standard) {
       const PASN_Integer & id = (const PASN_Integer &)content.m_parameterIdentifier;
        if (content.m_parameterValue.GetTag() == H245_ParameterValue::e_booleanArray) {
          const PASN_Integer & val = (const PASN_Integer &)content.m_parameterValue;
             if (id == 1) {
               m_blockSize = val;
               m_blockOctets = GetParameterBlockSize(m_blockSize);
            }
            if (id == 2)
               m_transferMode = val;
        }
      }
    }

  return TRUE;
}

PBoolean H323FileTransferCapability::OnSendingPDU(H245_DataMode & /*pdu*/) const
{
   return false;
}

PBoolean H323FileTransferCapability::OnSendingPDU(H245_GenericCapability & pdu) const
{
   H245_CapabilityIdentifier & capId = pdu.m_capabilityIdentifier; 

   capId.SetTag(H245_CapabilityIdentifier::e_standard);
   PASN_ObjectId & id = capId;
   id.SetValue(FileTransferOID);

   pdu.IncludeOptionalField(H245_GenericCapability::e_maxBitRate);
   PASN_Integer & bitRate = pdu.m_maxBitRate;
   bitRate = maxBitRate/100;

   // Add the Block size parameter
   H245_GenericParameter * blockparam = new H245_GenericParameter;
   blockparam->m_parameterIdentifier.SetTag(H245_ParameterIdentifier::e_standard);
   (PASN_Integer &)blockparam->m_parameterIdentifier = 1;
   blockparam->m_parameterValue.SetTag(H245_ParameterValue::e_booleanArray);
   (PASN_Integer &)blockparam->m_parameterValue = SetParameterBlockSize(m_blockOctets);  
   
   // Add the Transfer Mode parameter
   H245_GenericParameter * modeparam = new H245_GenericParameter;
   modeparam->m_parameterIdentifier.SetTag(H245_ParameterIdentifier::e_standard);
   (PASN_Integer &)modeparam->m_parameterIdentifier = 2;
   modeparam->m_parameterValue.SetTag(H245_ParameterValue::e_booleanArray);
   (PASN_Integer &)modeparam->m_parameterValue = m_transferMode;  
   
   pdu.IncludeOptionalField(H245_GenericCapability::e_collapsing);
   pdu.m_collapsing.Append(blockparam);
   pdu.m_collapsing.Append(modeparam);

   return TRUE;
}

void H323FileTransferCapability::SetFileTransferList(const H323FileTransferList & list)
{
    m_filelist.clear();
    m_filelist = list;
    m_filelist.SetMaster(true);
}

unsigned H323FileTransferCapability::GetDefaultSessionID() const
{
    return OpalMediaFormat::DefaultFileSessionID;
}

unsigned H323FileTransferCapability::GetSubType() const
{
    return H245_DataApplicationCapability_application::e_genericDataCapability;
}

H323Channel * H323FileTransferCapability::CreateChannel(H323Connection & connection,   
      H323Channel::Directions direction,unsigned sessionID,const H245_H2250LogicalChannelParameters * /*param*/
) const
{
    RTP_Session *session;
    H245_TransportAddress addr;
    connection.GetControlChannel().SetUpTransportPDU(addr, H323Transport::UseLocalTSAP);
    session = connection.UseSession(sessionID, addr, direction);
    
  if(session == NULL) {
    return NULL;
  } 
  
  return new H323FileTransferChannel(connection, *this, direction, (RTP_UDP &)*session, sessionID, m_filelist);
}

/////////////////////////////////////////////////////////////////////////////////

H323FileTransferChannel::H323FileTransferChannel(H323Connection & connection,
                                 const H323Capability & capability,
                                 H323Channel::Directions theDirection,
                                 RTP_UDP & rtp,
                                 unsigned theSessionID,
                                 const H323FileTransferList & list
                                 )
 : H323Channel(connection, capability),
  rtpSession(rtp),
  rtpCallbacks(*(H323_RTP_Session *)rtp.GetUserData()),
  filelist(list)
{

  direction = theDirection;
  sessionID = theSessionID;
    
  rtpPayloadType = (RTP_DataFrame::PayloadTypes)101;

  // We create the fileHandler here if we are transmitting (ie. Opening a file Transfer request)
  if (theDirection == H323Channel::IsTransmitter)
     fileHandler = connection.CreateFileTransferHandler(sessionID, theDirection,filelist);
  else
     fileHandler = NULL;
}


H323FileTransferChannel::~H323FileTransferChannel()
{
}

H323Channel::Directions H323FileTransferChannel::GetDirection() const
{
  return direction;
}

PBoolean H323FileTransferChannel::SetInitialBandwidth()
{
  return TRUE;
}

void H323FileTransferChannel::Receive()
{

}

void H323FileTransferChannel::Transmit()
{

}

PBoolean H323FileTransferChannel::Open()
{
  return H323Channel::Open();
}

PBoolean H323FileTransferChannel::Start()
{
  if (fileHandler == NULL)
     return FALSE;

  if (!Open())
    return FALSE;
    
   fileHandler->SetPayloadType(rtpPayloadType);

  if (fileHandler->GetBlockSize() == 0)
      fileHandler->SetBlockSize((H323FileTransferCapability::blockSizes)
                                     ((H323FileTransferCapability *)capability)->GetOctetSize());
   
  if (fileHandler->GetBlockRate() == 0)
       fileHandler->SetMaxBlockRate(((H323FileTransferCapability *)capability)->GetBlockRate());

  return fileHandler->Start(direction);
}

void H323FileTransferChannel::Close()
{
  if (terminating) 
      return;

  if (fileHandler != NULL)
      fileHandler->Stop(direction);
}

PBoolean H323FileTransferChannel::OnSendingPDU(H245_OpenLogicalChannel & open) const
{
  open.m_forwardLogicalChannelNumber = (unsigned)number;

  if (direction == H323Channel::IsTransmitter)
      SetFileList(open,filelist);
        
  if (open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
      
    open.m_reverseLogicalChannelParameters.IncludeOptionalField(
        H245_OpenLogicalChannel_reverseLogicalChannelParameters::e_multiplexParameters);
            
    open.m_reverseLogicalChannelParameters.m_multiplexParameters.SetTag(
        H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters);
            
    return OnSendingPDU(open.m_reverseLogicalChannelParameters.m_multiplexParameters);
    
  }    else {
      
    open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
        H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters);
        
    return OnSendingPDU(open.m_forwardLogicalChannelParameters.m_multiplexParameters);
  }
}

void H323FileTransferChannel::OnSendOpenAck(const H245_OpenLogicalChannel & openPDU, 
                                        H245_OpenLogicalChannelAck & ack) const
{
  // set forwardMultiplexAckParameters option
  ack.IncludeOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters);
    
  // select H225 choice
  ack.m_forwardMultiplexAckParameters.SetTag(
    H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters);
    
  // get H225 params
  H245_H2250LogicalChannelAckParameters & param = ack.m_forwardMultiplexAckParameters;
    
  // set session ID
  param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID);
  const H245_H2250LogicalChannelParameters & openparam =
      openPDU.m_forwardLogicalChannelParameters.m_multiplexParameters;
    
  unsigned sessionID = openparam.m_sessionID;
  param.m_sessionID = sessionID;
    
  OnSendOpenAck(param);
}

PBoolean H323FileTransferChannel::OnReceivedPDU(const H245_OpenLogicalChannel & open,
                                     unsigned & errorCode)
{
  if (direction == H323Channel::IsReceiver) {
    number = H323ChannelNumber(open.m_forwardLogicalChannelNumber, TRUE);
    if (!GetFileList(open))
        return FALSE;
  }
    
  PBoolean reverse = open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
  const H245_DataType & dataType = reverse ? open.m_reverseLogicalChannelParameters.m_dataType
                                           : open.m_forwardLogicalChannelParameters.m_dataType;
    
  if (!capability->OnReceivedPDU(dataType, direction)) {
      
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotSupported;
    return FALSE;
  }
    
  if (reverse) {
    if (open.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() ==
            H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters) 
    {
      return OnReceivedPDU(open.m_reverseLogicalChannelParameters.m_multiplexParameters, errorCode);
    }
      
  } else {
    if (open.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() ==
            H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
    {
      return OnReceivedPDU(open.m_forwardLogicalChannelParameters.m_multiplexParameters, errorCode);
    }
  }

  errorCode = H245_OpenLogicalChannelReject_cause::e_unsuitableReverseParameters;
  return FALSE;
}

PBoolean H323FileTransferChannel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & ack)
{
  if (!ack.HasOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters)) {
    return FALSE;
  }
    
  if (ack.m_forwardMultiplexAckParameters.GetTag() !=
    H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters)
  {
    return FALSE;
  }
    
  return OnReceivedAckPDU(ack.m_forwardMultiplexAckParameters);
}

PBoolean H323FileTransferChannel::OnSendingPDU(H245_H2250LogicalChannelParameters & param) const
{
  param.m_sessionID = sessionID;
    
  param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaGuaranteedDelivery);
  param.m_mediaGuaranteedDelivery = FALSE;
    
  // unicast must have mediaControlChannel
  if (rtpSession.GetLocalControlPort() > 0) {
      H323TransportAddress mediaControlAddress(rtpSession.GetLocalAddress(), rtpSession.GetLocalControlPort());
      param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel);
      mediaControlAddress.SetPDU(param.m_mediaControlChannel);
  }
    
  if (direction == H323Channel::IsReceiver && rtpSession.GetLocalDataPort() > 0) {
    // set mediaChannel
    H323TransportAddress mediaAddress(rtpSession.GetLocalAddress(), rtpSession.GetLocalDataPort());
    param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
    mediaAddress.SetPDU(param.m_mediaChannel);
  }    
    
  // Set dynamic payload type, if is one
  int rtpPayloadType = GetDynamicRTPPayloadType();
  
  if (rtpPayloadType >= RTP_DataFrame::DynamicBase && rtpPayloadType < RTP_DataFrame::IllegalPayloadType) {
    param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_dynamicRTPPayloadType);
    param.m_dynamicRTPPayloadType = rtpPayloadType;
  }
    
  return TRUE;
}

void H323FileTransferChannel::OnSendOpenAck(H245_H2250LogicalChannelAckParameters & param) const
{
  // set mediaControlChannel
    if (rtpSession.GetLocalControlPort() > 0) {
      H323TransportAddress mediaControlAddress(rtpSession.GetLocalAddress(), rtpSession.GetLocalControlPort());
      param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel);
      mediaControlAddress.SetPDU(param.m_mediaControlChannel);
    }
    
  // set mediaChannel
    if (rtpSession.GetLocalDataPort() > 0) {
      H323TransportAddress mediaAddress(rtpSession.GetLocalAddress(), rtpSession.GetLocalDataPort());
      param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
      mediaAddress.SetPDU(param.m_mediaChannel);
    }

  // Set dynamic payload type, if is one
  int rtpPayloadType = GetDynamicRTPPayloadType();
  if (rtpPayloadType >= RTP_DataFrame::DynamicBase && rtpPayloadType < RTP_DataFrame::IllegalPayloadType) {
    param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_dynamicRTPPayloadType);
    param.m_dynamicRTPPayloadType = rtpPayloadType;
  }
}

PBoolean H323FileTransferChannel::OnReceivedPDU(const H245_H2250LogicalChannelParameters & param,
                           unsigned & errorCode)
{
  if (param.m_sessionID != sessionID) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_invalidSessionID;
    return FALSE;
  }
    
  PBoolean ok = FALSE;
    
  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel)) {        
    if (!ExtractTransport(param.m_mediaControlChannel, FALSE, errorCode)) {
      return FALSE;
    }
    
    ok = TRUE;
  }
    
  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaChannel)) {
    if (ok && direction == H323Channel::IsReceiver) {
        
    } else if (!ExtractTransport(param.m_mediaChannel, TRUE, errorCode)) {
      return FALSE;
    }
    
    ok = TRUE;
  }
    
  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_dynamicRTPPayloadType)) {
    SetDynamicRTPPayloadType(param.m_dynamicRTPPayloadType);
  }
    
  if (ok) {
    return TRUE;
  }
    
  errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
  return FALSE;
}

PBoolean H323FileTransferChannel::OnReceivedAckPDU(const H245_H2250LogicalChannelAckParameters & param)
{
  if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID)) {
  }
    
  if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel)) {
    return FALSE;
  }
    
  unsigned errorCode;
  if (!ExtractTransport(param.m_mediaControlChannel, FALSE, errorCode)) {
    return FALSE;
  }
    
  if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel)) {
    return FALSE;
  }
    
  if (!ExtractTransport(param.m_mediaChannel, TRUE, errorCode)) {
    return FALSE;
  }
    
  if (param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_dynamicRTPPayloadType)) {
    SetDynamicRTPPayloadType(param.m_dynamicRTPPayloadType);
  }
    
  return TRUE;
}

PBoolean H323FileTransferChannel::SetDynamicRTPPayloadType(int newType)
{
  if (newType == -1) {
    return TRUE;
  }
    
  if (newType < RTP_DataFrame::DynamicBase || newType >= RTP_DataFrame::IllegalPayloadType) {
    return FALSE;
  }
    
  if (rtpPayloadType < RTP_DataFrame::DynamicBase) {
    return FALSE;
  }
    
  rtpPayloadType = (RTP_DataFrame::PayloadTypes)newType;
    
  return TRUE;
}

PBoolean H323FileTransferChannel::ExtractTransport(const H245_TransportAddress & pdu,
                                                 PBoolean isDataPort,
                                                unsigned & errorCode)
{
  if (pdu.GetTag() != H245_TransportAddress::e_unicastAddress) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_multicastChannelNotAllowed;
    return FALSE;
  }
    
  H323TransportAddress transAddr = pdu;
    
  PIPSocket::Address ip;
  WORD port;
  if (transAddr.GetIpAndPort(ip, port)) {
    return rtpSession.SetRemoteSocketInfo(ip, port, isDataPort);
  }
    
  return FALSE;
}

PBoolean H323FileTransferChannel::RetreiveFileInfo(const H245_GenericInformation & info, H323FileTransferList & filelist)
{
      if (info.m_messageIdentifier.GetTag() != H245_CapabilityIdentifier::e_standard)
          return FALSE;

      const PASN_ObjectId &object_id = info.m_messageIdentifier;
      if (object_id != FileTransferListOID)   // Indicates File Information
           return FALSE;
 
      if (!info.HasOptionalField(H245_GenericInformation::e_messageContent))
          return FALSE;
    
       const H245_ArrayOf_GenericParameter & params = info.m_messageContent;

       int direction=0;
       long size =0;
       PString name = PString();
       for (PINDEX i =0; i < params.GetSize(); i++)
       {
           PASN_Integer & pid = params[i].m_parameterIdentifier; 
           H245_ParameterValue & paramval = params[i].m_parameterValue;
           int val = pid.GetValue();

           if (val == 1) {  // tftpDirection
                PASN_Integer & val = paramval;
                direction = val.GetValue();
           } else if (val == 2) {  // tftpFilename
                PASN_OctetString & val = paramval;
                name = val.AsString();
           } else if (val == 3) { // tftpFilesize
                PASN_Integer & val = paramval;
                size = val.GetValue();
           }
       }
       filelist.Add(name,"",size);
       if ((direction > 0) &&  (direction != filelist.GetDirection()))
           filelist.SetDirection((H323Channel::Directions)direction);

    return TRUE;
}

PBoolean H323FileTransferChannel::GetFileList(const H245_OpenLogicalChannel & open) 
{

  if (!open.HasOptionalField(H245_OpenLogicalChannel::e_genericInformation))
      return FALSE;

  const H245_ArrayOf_GenericInformation & cape = open.m_genericInformation;

  for (PINDEX i=0; i<cape.GetSize(); i++) {
      RetreiveFileInfo(cape[i], filelist);
  }

  // We create the fileHandler once we have received the filelist
  fileHandler = connection.CreateFileTransferHandler(sessionID, H323Channel::IsReceiver, filelist);
  
  return (fileHandler != NULL);    
}

static H245_GenericParameter * BuildGenericParameter(unsigned id,unsigned type, const PString & value)
{  

 H245_GenericParameter * content = new H245_GenericParameter();
      H245_ParameterIdentifier & paramid = content->m_parameterIdentifier;
        paramid.SetTag(H245_ParameterIdentifier::e_standard);
        PASN_Integer & pid = paramid;
        pid.SetValue(id);
      H245_ParameterValue & paramval = content->m_parameterValue;
        paramval.SetTag(type);
         if ((type == H245_ParameterValue::e_unsignedMin) ||
            (type == H245_ParameterValue::e_unsignedMax) ||
            (type == H245_ParameterValue::e_unsigned32Min) ||
            (type == H245_ParameterValue::e_unsigned32Max)) {
                PASN_Integer & val = paramval;
                val.SetValue(value.AsUnsigned());
         } else if (type == H245_ParameterValue::e_octetString) {
                PASN_OctetString & val = paramval;
                val.SetValue(value);
         }        
//            H245_ParameterValue::e_logical,
//            H245_ParameterValue::e_booleanArray,
//            H245_ParameterValue::e_genericParameter

     return content;
}

void H323FileTransferChannel::SetFileList(H245_OpenLogicalChannel & open, H323FileTransferList flist) const
{
  
  if (flist.GetSize() == 0)
      return;

  PINDEX i = 0;

  open.IncludeOptionalField(H245_OpenLogicalChannel::e_genericInformation);
  H245_ArrayOf_GenericInformation & cape = open.m_genericInformation;

  for (H323FileTransferList::const_iterator r = filelist.begin(); r != filelist.end(); ++r) {
      H245_GenericInformation * gcap = new H245_GenericInformation();
      gcap->m_messageIdentifier = *(new H245_CapabilityIdentifier(H245_CapabilityIdentifier::e_standard));
      PASN_ObjectId &object_id = gcap->m_messageIdentifier;
      object_id = FileTransferListOID;   // Indicates File Information

        i++;
      gcap->IncludeOptionalField(H245_GenericInformation::e_subMessageIdentifier);
      PASN_Integer & sub_id = gcap->m_subMessageIdentifier;
      sub_id = i; 

        gcap->IncludeOptionalField(H245_GenericInformation::e_messageContent);
        H245_ArrayOf_GenericParameter & params = gcap->m_messageContent;
            // tftpDirection
            params.Append(BuildGenericParameter(1,H245_ParameterValue::e_unsignedMin,flist.GetDirection()));
            // tftpFilename
            params.Append(BuildGenericParameter(2,H245_ParameterValue::e_octetString,r->m_Filename));
            // tftpFilesize
            if (flist.GetDirection() == H323Channel::IsTransmitter)
              params.Append(BuildGenericParameter(3,H245_ParameterValue::e_unsigned32Max,r->m_Filesize));

     cape.Append(gcap);
  }
}

///////////////////////////////////////////////////////////////////////////////////

H323FileTransferList::H323FileTransferList()
{
    saveDirectory = PProcess::Current().GetFile().GetDirectory();
    direction = H323Channel::IsBidirectional;
    master = false;
}

void H323FileTransferList::Add(const PString & filename, const PDirectory & directory, long filesize)
{
   H323File file;
   file.m_Filename = filename;
   file.m_Directory = directory;
   file.m_Filesize = filesize;
   push_back(file);
}
     
void H323FileTransferList::SetSaveDirectory(const PString directory)
{
   saveDirectory = directory;
}

const PDirectory & H323FileTransferList::GetSaveDirectory()
{
   return saveDirectory; 
}

void H323FileTransferList::SetDirection(H323Channel::Directions _direction)
{
   direction = _direction;
}
    
H323Channel::Directions H323FileTransferList::GetDirection()
{
   return direction;
}

PINDEX H323FileTransferList::GetSize()
{
    return size();
}


H323File * H323FileTransferList::GetAt(PINDEX i)
{
    PINDEX c=0;
    for (H323FileTransferList::iterator r = begin(); r != end(); ++r) {
        if (c == i)
            return &(*r);
        c++;
    }
    return NULL;
}

void H323FileTransferList::SetMaster(PBoolean state)
{
    master = state;
}

PBoolean H323FileTransferList::IsMaster()
{
    return master;
}

///////////////////////////////////////////////////////////////////////////////////

static PString errString[] = {
      "Not Defined.",
      "File Not Found.",
      "Access Violation.",
      "Disk Full/Allocation exceeded.",
      "Illegal TFTP operation.",
      "Unknown transfer ID.",
      "File Already Exists.",
      "No such user.",
      "Incomplete Block."
};

static PString tranState[] = {
      "Probing",         
      "Connected",         
      "Waiting",         
      "Sending",         
      "Receiving",       
      "Completed",      
      "Error"           
  };

static PString blkState[] = {
      "ok",             
      "partial",       
      "complete",       
      "Incomplete",     
      "Timeout",        
      "Ready"           
  };

#if PTRACING

PString DataPacketAnalysis(PBoolean isEncoder, const H323FilePacket & packet, PBoolean final)
{
    PString direct = (isEncoder ? "<- " : "-> " );

    if (!final) 
        return direct + "blk partial size : " + PString(packet.GetSize()) + " bytes";

    PString pload;
    int errcode = 0;
    PString errstr;
    switch (packet.GetPacketType()) {
        case H323FilePacket::e_PROB:
            pload = direct + "prb size : " + PString(packet.GetSize()) + " bytes";
            break;
        case H323FilePacket::e_RRQ:
            pload = direct + "rrq file " + packet.GetFileName() + " : " + PString(packet.GetFileSize()) + " bytes";
            break;
        case H323FilePacket::e_WRQ:
            pload = direct + "wrq file " + packet.GetFileName() + " : " + PString(packet.GetFileSize()) + " bytes";
            break;
        case H323FilePacket::e_DATA:
            pload = direct + "blk " + PString(packet.GetBlockNo()) + " : " + PString(packet.GetSize()) + " bytes";
            break;
        case H323FilePacket::e_ACK:
            pload = direct + "ack " + PString(packet.GetACKBlockNo());
            if (packet.GetFileSize() > 0)
                pload = pload + " : " + PString(packet.GetFileSize()) + " bytes";
            break;
        case H323FilePacket::e_ERROR:
            packet.GetErrorInformation(errcode,errstr);
            pload = direct + "err " + PString(errcode) + ": " + errstr;
            break;
        default:
            break;
    }

    return pload;
}
#endif

H323FileTransferHandler::H323FileTransferHandler(H323Connection & connection, 
                                                 unsigned sessionID, 
                                                 H323Channel::Directions dir,
                                                 H323FileTransferList & _filelist
                                                 )
 :filelist(_filelist), master(_filelist.IsMaster())
{

  H245_TransportAddress addr;
  connection.GetControlChannel().SetUpTransportPDU(addr, H323Transport::UseLocalTSAP);
  session = connection.UseSession(sessionID,addr,H323Channel::IsBidirectional);

  TransmitThread = NULL;
  ReceiveThread = NULL;
  blockRate = 0;
  blockSize = 0;
  msBetweenBlocks = 0;

  // Transmission Settings
  curFile = NULL;
  timestamp = 0;
  lastBlockNo = 0;
  lastBlockSize = 0;
  curFileName = PString();
  curFileSize = 0;
  curBlockSize = 0;
  curProgSize = 0;    
  rtpPayloadType = (RTP_DataFrame::PayloadTypes)101;
  responseTimeOut = 1500;

  transmitRunning = FALSE;
  receiveRunning = FALSE;

}

H323FileTransferHandler::~H323FileTransferHandler()
{
  // order is important
  session->Close(true);
  if (receiveRunning)
       exitReceive.Signal();

  if (transmitRunning)
       exitTransmit.Signal();
}

PBoolean H323FileTransferHandler::Start(H323Channel::Directions direction)
{
      // reset the current state 
      currentState = e_probing;

      StartTime = new PTime();
      transmitFrame.SetPayloadType(rtpPayloadType);
      TransmitThread = PThread::Create(PCREATE_NOTIFIER(Transmit), 0, PThread::AutoDeleteThread); 
      ReceiveThread = PThread::Create(PCREATE_NOTIFIER(Receive), 0, PThread::AutoDeleteThread);


  return TRUE;
}

PBoolean H323FileTransferHandler::Stop(H323Channel::Directions direction)
{
  PWaitAndSignal m(transferMutex);
    
  delete StartTime;
  StartTime = NULL;
  
  
  // CloseDown the Transmit/Receive Threads
  nextFrame.Signal();

  session->Close(true);
  if (direction == H323Channel::IsReceiver && receiveRunning)
       exitReceive.Signal();

  if (direction == H323Channel::IsTransmitter && transmitRunning)
       exitTransmit.Signal();

  return TRUE;
}

void H323FileTransferHandler::SetPayloadType(RTP_DataFrame::PayloadTypes _type)
{
    rtpPayloadType = _type;
}

void H323FileTransferHandler::SetBlockSize(H323FileTransferCapability::blockSizes size)
{
    blockSize = size;
}

void H323FileTransferHandler::SetMaxBlockRate(unsigned rate)
{
    blockRate = rate;
    msBetweenBlocks = (int)((1.000/((double)rate))*1000);
}

void H323FileTransferHandler::ChangeState(transferState newState)
{
   PWaitAndSignal m(stateMutex);

   if (currentState == newState)
       return;

   PTRACE(4,"FT\tState Change to " << tranState[newState]);

   currentState = newState;
   OnStateChange(currentState);
}

void H323FileTransferHandler::SetBlockState(receiveStates state) 
{
   PWaitAndSignal m(stateMutex);

    if (blockState == state)
        return;

    PTRACE(6,"FT\t    blk: " << blkState[state]);
   blockState = state;
}

PBoolean H323FileTransferHandler::TransmitFrame(H323FilePacket & buffer, PBoolean final)
{    

  // determining correct timestamp
  PTime currentTime = PTime();
  PTimeInterval timePassed = currentTime - *StartTime;
  transmitFrame.SetTimestamp((DWORD)timePassed.GetMilliSeconds() * 8);
  transmitFrame.SetMarker(final);
  
  transmitFrame.SetPayloadSize(buffer.GetSize());
  memmove(transmitFrame.GetPayloadPtr(),buffer.GetPointer(), buffer.GetSize());
    
  return (session && session->WriteData(transmitFrame));
}

PBoolean H323FileTransferHandler::ReceiveFrame(H323FilePacket & buffer, PBoolean & final)
{    

   RTP_DataFrame packet = RTP_DataFrame(1440);

   if(!session->ReadBufferedData(timestamp, packet)) 
      return FALSE;

    timestamp = packet.GetTimestamp();
   
   final = packet.GetMarker();
   buffer.SetSize(packet.GetPayloadSize());
   memmove(buffer.GetPointer(),packet.GetPayloadPtr(), packet.GetPayloadSize());
  return TRUE;
}

PBoolean Segment(PBYTEArray & lastBlock, const int size, int & segment, PBYTEArray & thearray)
{
    int bsize = lastBlock.GetSize();
    int newsize=0;
    if (bsize < (segment + size)) 
       newsize = bsize - segment;
    else 
        newsize = size;
    
    BYTE * seg = lastBlock.GetPointer();
    thearray.SetSize(newsize);
    memcpy(thearray.GetPointer(),seg+segment, newsize);
    segment = segment + newsize;

    if (segment == bsize) {
        segment = 0;
        return TRUE;
    }
    return FALSE;
}

void H323FileTransferHandler::Transmit(PThread &, INT)
{
    PBoolean success = TRUE;
    PBoolean datapacket = FALSE;
    H323File * f = NULL;
    PFilePath p;

    PBoolean read = (filelist.GetDirection() == H323Channel::IsTransmitter);
    PBoolean waitforResponse = FALSE;
    PBoolean lastFrame = FALSE;
    int fileid = 0;
    H323FilePacket lastBlock;
    PBYTEArray lastSegment;
    int offset = 0;
    int sentBlock = 0;

    transmitRunning = TRUE;

    while (success && !exitTransmit.Wait(0)) {
        
        H323FilePacket packet;
        PBoolean final = FALSE;
        switch (currentState) {
           case e_probing:
                probMutex.Wait(50);
                if (currentState != e_probing) 
                    continue;

                packet.BuildPROB();
                final = TRUE;
                break;
           case e_connect:
                packet.BuildACK(99);
                final = TRUE;
                ChangeState(e_waiting);        
                   break;
           case e_waiting:
                // if we have something to send/Receive
                 if (master) {
                     if (waitforResponse) {
                        if (blockState != recTimeOut) {
                            waitforResponse = FALSE;  
                            if (read)
                                ChangeState(e_sending);
                            break;
                        }
                     } else {
                        fileid++;
                        if (fileid > filelist.GetSize()) {
                             OnTransferComplete();
                             ChangeState(e_completed);
                             break;
                        }
                        //delete f;
                        f = filelist.GetAt(fileid-1);
                        if (f == NULL) {
                            ioerr = H323FileIOChannel::e_NotFound;
                            OnFileOpenError("",ioerr);
                            ChangeState(e_error);
                            break;
                        }

                        p = f->m_Directory + PDIR_SEPARATOR + f->m_Filename;
                        curFileName = f->m_Filename;
                     
                        if (read) {
                            curFileSize = f->m_Filesize;
                            delete curFile;
                            curFile = new H323FileIOChannel(p,read);
                            if (curFile->IsError(ioerr)) {
                                OnFileOpenError(p,ioerr);
                                ChangeState(e_error);
                                break;
                            }
                            OnFileStart(p, curFileSize,read);  // Notify to start send
                        }  
                     }   
                     if (!read) {
                       packet.BuildRequest(H323FilePacket::e_RRQ ,f->m_Filename, f->m_Filesize, blockSize);
                       final = TRUE;
                       waitforResponse = TRUE;
                     } else {
                       packet.BuildRequest(H323FilePacket::e_WRQ ,f->m_Filename, f->m_Filesize, blockSize);
                       final = TRUE;
                       waitforResponse = TRUE;
                     } 
                 } else {
                     if (blockState == recComplete) {  // We are waiting to shutdown
                        if (shutdownTimer.GetResetTime() == 0) 
                            shutdownTimer.SetInterval(responseTimeOut);
                        else {
                          if (shutdownTimer == 0)   // We have waited long enough without response
                            ChangeState(e_completed);   
                          else
                            continue;
                        }
                     } else
                         continue;
                 }
                break;
           case e_sending:
               // Signal we are ready to send file
               if (blockState == recReady) {
                    packet.BuildACK(0,curFileSize);
                   final = TRUE;
                   SetBlockState(recOK);
                   break;
               } 
          
                if (blockState != recPartial) {
                    if (blockState == recOK) {
                        if (lastFrame) {  
                            // We have successfully sent the last frame of data.
                            // switch back to waiting to queue the next file
                           OnFileComplete(curFileName);
                           delete curFile;
                           curFile = NULL;
                           curFileName = PString();
                           waitforResponse = FALSE;
                           lastFrame = FALSE;
                           lastBlockSize = 0;
                           lastBlockNo = 0;
                           SetBlockState(recComplete);
                           ChangeState(e_waiting);
                           continue;
                        }
                        offset = 0;
                        lastBlockNo++;
                        if (lastBlockNo > 99) lastBlockNo = 1;
                         lastBlock.BuildData(lastBlockNo,blockSize);
                         int size = blockSize;
                         curFile->Read(lastBlock.GetDataPtr(),size);
                         // Wait for Bandwidth limits
                         sendwait.Delay(msBetweenBlocks);
                         if (size < (int)blockSize) {
                             lastBlock.SetSize(4+size);
                             lastFrame = TRUE;
                         }
                         lastBlockSize = lastBlock.GetDataSize();
                        PTRACE(5,"FT\t" << DataPacketAnalysis(true,lastBlock,true));
                    } else if (blockState != recComplete) {
                        OnFileError(curFileName,lastBlockNo, TRUE);
                    }
               }
                // Segment and mark as recPartial
               datapacket = true;
               if (blockSize > H323FileTransferCapability::e_1428)
                  final = Segment(lastBlock, H323FileTransferCapability::e_1428, offset, packet);
               else {
                  packet.Attach(lastBlock.GetPointer(),lastBlock.GetSize());
                  final = TRUE;
               }

                if (final) {
                  SetBlockState(recComplete);
                  waitforResponse = TRUE;
                } else {
                  SetBlockState(recPartial);
                  waitforResponse = FALSE;
                }
                break;
           case e_receiving:
               // Signal we are ready to receive file.
               if (blockState == recReady) {
                    packet.BuildACK(0);
                   final = TRUE;
                   SetBlockState(recOK);
                   break;
               } else if (sentBlock == lastBlockNo) {
                   nextFrame.Wait(responseTimeOut);
               }

               if (curFileSize == curProgSize)
                    SetBlockState(recComplete);

               if ((blockState == recOK) || (blockState == recComplete)) {
                    packet.BuildACK(lastBlockNo);
                    if (lastBlockNo == 99) lastBlockNo = 0;
                    sentBlock = lastBlockNo;
                    final = TRUE;
                     if (blockState == recComplete) { // Switch to wait for next instruction    
                             lastBlockNo = 0;
                            curProgSize = 0;
                            curFile->Close();
                            ChangeState(e_waiting);
                            waitforResponse = FALSE;
                     } 
               } else if  (blockState == recIncomplete) {
                    OnFileError(curFileName,lastBlockNo, TRUE);
                    packet.BuildError(0,"");   // Indicate to remote to resend the last block
               } else {
                    // Do nothing
                    continue;
               }

                break;
           case e_error:
                packet.BuildError(ioerr,errString[ioerr]);
                final = TRUE;
                ChangeState(e_completed);
                break;
           case e_completed:
           default:
                success = false;
                break;
        }

        if (success && packet.GetSize() > 0) {
           success = TransmitFrame(packet,final);

           if (!datapacket) {
#if PTRACING       
              PTRACE(5,"FT\t" << DataPacketAnalysis(true,packet,final));       
#endif 
              packet.SetSize(0);
           }
            datapacket = false;
            if (waitforResponse) {
              SetBlockState(recTimeOut);
              nextFrame.Wait(responseTimeOut);
            }
        }
    }

    session->Close(false);
    exitTransmit.Acknowledge();
    transmitRunning = FALSE;

    PTRACE(6,"FILE\tClosing Transmit Thread");

    // close down the channel which will 
    // release the thread to close automatically
    if (receiveRunning)
        session->Close(true);

}

void H323FileTransferHandler::Receive(PThread &, INT)
{

    PBoolean success = TRUE;
    H323FilePacket packet;
    packet.SetSize(0);
    PFilePath p;

    receiveRunning = TRUE;
    while (success && !exitReceive.Wait(0)) {

       PBoolean final = FALSE;
       H323FilePacket buffer;
       success = ReceiveFrame(buffer, final);

       if (!success || buffer.GetSize() == 0)
           continue;

       if (currentState == e_receiving) {
             packet.Concatenate(buffer);
             if (!final) 
                continue;
             else {
                buffer.SetSize(0);
            }
       } else {
           packet = buffer;
       }

       if (packet.GetSize() == 0)
           continue;

#if PTRACING
       PTRACE(5,"FT\t" << DataPacketAnalysis(false,packet,final));
#endif

       int ptype = packet.GetPacketType();
       if (ptype == H323FilePacket::e_ERROR) {
           int errCode;
           PString errString;
           packet.GetErrorInformation(errCode,errString);
           if (errCode > 0) {
               OnError(errString);
               ChangeState(e_completed);
               nextFrame.Signal();
           }
       }

        switch (currentState) {
           case e_probing:
              ChangeState(e_connect);
              probMutex.Signal();
             break;
           case e_connect:
               // Do nothing
               break;
           case e_waiting:
               if (ptype == H323FilePacket::e_WRQ) {
                   p = filelist.GetSaveDirectory() + PDIR_SEPARATOR + packet.GetFileName();
                   curFileName = packet.GetFileName();
                   curFileSize = packet.GetFileSize();
                   curBlockSize = packet.GetBlockSize();
                   delete curFile;
                   curFile = new H323FileIOChannel(p,FALSE);
                   if (curFile->IsError(ioerr)) {
                        OnFileOpenError(p,ioerr);
                        ChangeState(e_error);
                        break;
                   }
                   SetBlockState(recReady);
                   ChangeState(e_receiving);
                   OnFileStart(p, curFileSize,FALSE);  // Notify to start receive
                   shutdownTimer.SetInterval(0); 
               } else if (ptype == H323FilePacket::e_RRQ) {
                   p = filelist.GetSaveDirectory() + PDIR_SEPARATOR + packet.GetFileName();
                   delete curFile;
                   curFile = new H323FileIOChannel(p,TRUE);
                   if (curFile->IsError(ioerr)) {
                        OnFileOpenError(p,ioerr);
                        ChangeState(e_error);
                        break;
                   }
                   curFileSize = curFile->GetFileSize();
                   SetBlockState(recReady);
                   ChangeState(e_sending);
                   OnFileStart(p, curFileSize,TRUE);  // Notify to start send
                   shutdownTimer.SetInterval(0);
               } else if ((ptype == H323FilePacket::e_ACK) && (packet.GetACKBlockNo() == 0)) {
                   // We have received acknowledgement
                   int size = packet.GetFileSize();
                   if (size > 0) {
                        curFileSize = size;
                        PStringList saveFile;
                        saveFile = curFileName.Tokenise(PDIR_SEPARATOR);
                        p = filelist.GetSaveDirectory() + PDIR_SEPARATOR + saveFile[saveFile.GetSize()-1];
                        delete curFile;
                        curFile = new H323FileIOChannel(p,false);
                        if (curFile->IsError(ioerr)) {
                            delete curFile;
                            curFile = NULL;
                            OnFileOpenError(p,ioerr);
                            ChangeState(e_error);
                            break;
                        }
                       SetBlockState(recOK);
                       ChangeState(e_receiving);
                       OnFileStart(curFileName, curFileSize, false);  // Notify to start receive
                       nextFrame.Signal();
                   } else {  
                       SetBlockState(recOK);
                       if (!master || (master && (filelist.GetDirection() == H323Channel::IsTransmitter)))
                          nextFrame.Signal();
                   }
                   break;
               }

               break;
           case e_receiving:
               if (ptype == H323FilePacket::e_DATA) {
                   PBoolean OKtoWrite = FALSE;
                   int blockNo = 0;
                   if ((packet.GetDataSize() == blockSize) ||  // We have complete block
                      (curFileSize == (curProgSize + packet.GetDataSize()))) {  // We have the last block
                   
                      if (packet.GetBlockNo() > lastBlockNo) {  // Make sure we have not already received this block
                            if (packet.GetBlockNo() != lastBlockNo + 1) {
                                // This is not the next block request to resend. We are in trouble if we are here!
                                SetBlockState(recIncomplete);
                                OnFileError(curFileName,lastBlockNo, FALSE);
                                packet.SetSize(0);
                                nextFrame.Signal();
                               break;
                            }

                            lastBlockNo = packet.GetBlockNo(); 
                            curProgSize = curProgSize + packet.GetDataSize(); 
                            
                            blockNo = lastBlockNo;
                            OKtoWrite = TRUE;
                      } 
                      SetBlockState(recOK);    
                   } else {
                        SetBlockState(recIncomplete);
                        OnFileError(curFileName,lastBlockNo, FALSE);
                   }
                          
                   // Now write away block to file.
                   if (OKtoWrite) {
                        curFile->Write(packet.GetDataPtr(),packet.GetDataSize());
                        OnFileProgress(curFileName,blockNo,curProgSize, FALSE);
                   }
                   // Signal to send confirmation
                   nextFrame.Signal();
               }
               packet.SetSize(0);
              break;
            case e_sending:
               if (ptype == H323FilePacket::e_ACK) {
                    if (packet.GetACKBlockNo() == 0)  // Control ACKs = 0 so ignore.
                        continue;
                    if (packet.GetACKBlockNo() == lastBlockNo) {
                        curProgSize = curProgSize + lastBlockSize;
                        OnFileProgress(curFileName,lastBlockNo,curProgSize, TRUE);
                        SetBlockState(recOK);    
                    } else {
                        PTRACE(6,"FT\tExpecting block " << lastBlockNo << " Received " << packet.GetACKBlockNo());
                    }
               } else if (ptype == H323FilePacket::e_ERROR) {
                    OnFileError(curFileName,lastBlockNo, TRUE);
                    SetBlockState(recIncomplete);
               }
               nextFrame.Signal();
               break;
           case e_error:
           case e_completed:
           default:
                success = FALSE;
                break;
        }
        packet.SetSize(0);
    }

    exitReceive.Acknowledge();
    receiveRunning = FALSE;

    PTRACE(6,"FILE\tClosing Receive Thread");
}

///////////////////////////////////////////////////////////////////////////

static PString opStr[] = {
      "00",
      "01",
      "02",
      "03",
      "04",
      "05"
  };

void H323FilePacket::attach(PString & data)
{
    SetSize(data.GetSize());
    memcpy(theArray, (const char *)data, data.GetSize());
}

void H323FilePacket::BuildPROB()
{
  PString header = opStr[e_PROB];
  Attach(header,header.GetSize());
}

void H323FilePacket::BuildRequest(opcodes code, const PString & filename, int filesize, int blocksize)
{
   PString fn = filename;
   fn.Replace("0","*",true);
   PString header = opStr[code] + fn + "0octet0blksize0" + PString(blocksize) 
                        + "0tsize0" + PString(filesize) + "0";
   attach(header);

}

void H323FilePacket::BuildData(int blockid, int size)
{
   PString blkstr;
   if (blockid < 10)
       blkstr = "0" + PString(blockid);
   else
       blkstr = blockid;

   PString data = opStr[e_DATA] + blkstr;

   SetSize(size+4);
   memcpy(theArray, (const char *)data, data.GetSize()); 
}

void H323FilePacket::BuildACK(int blockid, int filesize)
{
   PString blkstr;
   if (blockid < 10)
       blkstr = "0" + PString(blockid);
   else
       blkstr = blockid;

   PString header = opStr[e_ACK] + blkstr;

   if (filesize > 0)
       header = header + "0tsize0" + PString(filesize) + "0";
   attach(header);
}

void H323FilePacket::BuildError(int errorcode,PString errmsg)
{
   PString blkerr;
   if (errorcode < 10)
       blkerr = "0" + PString(errorcode);
   else
       blkerr = PString(errorcode);

   PString header = opStr[e_ERROR] + blkerr + errmsg + "0";
   attach(header);
}

PString H323FilePacket::GetFileName() const
{
  if ((GetPacketType() != e_RRQ) &&
       (GetPacketType() != e_WRQ))
          return PString();

  PString data(theArray, GetSize());

  PStringArray ar = (data.Mid(2)).Tokenise('0');

  ar[0].Replace("*","0",true);
  return ar[0];
}

unsigned H323FilePacket::GetFileSize() const
{
  if ((GetPacketType() != e_RRQ) &&
       (GetPacketType() != e_WRQ) &&
       (GetPacketType() != e_ACK))
          return 0;

  PString data(theArray, GetSize());

  int i = data.Find("tsize");
  if (i == P_MAX_INDEX)
      return 0;

  i = data.Find('0',i);
  int l = data.GetLength()-1-i;

  return data.Mid(i,l).AsUnsigned();
}

unsigned H323FilePacket::GetBlockSize() const
{
  if ((GetPacketType() != e_RRQ) &&
       (GetPacketType() != e_WRQ))
          return 0;

  PString data(theArray, GetSize());

  int i = data.Find("blksize");
  i = data.Find('0',i);
  int j = data.Find("tsize",i)-1;
  int l = j-i;

  return data.Mid(i,l).AsUnsigned();
}

void H323FilePacket::GetErrorInformation(int & ErrCode, PString & ErrStr) const
{
  if (GetPacketType() != e_ERROR) 
          return;

  PString data(theArray, GetSize());
  PString ar = data.Mid(2);

  ErrCode = (ar.Left(2)).AsInteger();
  ErrStr = ar.Mid(2,ar.GetLength()-3);
}
    
BYTE * H323FilePacket::GetDataPtr() 
{
    return (BYTE *)(theArray+4); 
}

unsigned H323FilePacket::GetDataSize() const
{
  if (GetPacketType() == e_DATA)
    return GetSize() - 4; 
  else
    return 0;
}

int H323FilePacket::GetBlockNo() const
{
  if (GetPacketType() != e_DATA) 
          return 0;

  PString data(theArray, GetSize());
  return data.Mid(2,2).AsInteger();
}

int H323FilePacket::GetACKBlockNo() const
{
  if (GetPacketType() != e_ACK) 
          return 0;

  PString data(theArray, GetSize());
  return data.Mid(2,2).AsInteger();
}

H323FilePacket::opcodes H323FilePacket::GetPacketType() const
{
  PString data(theArray, GetSize());
  return (opcodes)data.Mid(1,1).AsUnsigned();
}

////////////////////////////////////////////////////////////////////

H323FileIOChannel::H323FileIOChannel(PFilePath _file, PBoolean read)
{
    if (!CheckFile(_file,read,IOError))
        return;

    PFile::OpenMode mode;
    if (read) 
        mode = PFile::ReadOnly;
    else 
        mode = PFile::WriteOnly;

    PFile * file = new PFile(_file,mode);
    fileopen = file->IsOpen();

    if (!fileopen) {
        IOError = e_AccessDenied;
        delete file;
        file = NULL;
        filesize = 0;
        return;
    }

    filesize = file->GetLength();

   if (read) 
       SetReadChannel(file, TRUE);
   else 
       SetWriteChannel(file, TRUE);
}
    
H323FileIOChannel::~H323FileIOChannel()
{
}

PBoolean H323FileIOChannel::IsError(fileError & err)
{
    err = IOError;
    return (err > 0);
}

PBoolean H323FileIOChannel::CheckFile(PFilePath _file, PBoolean read, fileError & errCode)
{
    PBoolean exists = PFile::Exists(_file);
    
    if (read && !exists) {
        errCode = e_NotFound;
        return FALSE;
    }

    if (!read && exists) {
        errCode = e_FileExists;
        return FALSE;
    }

    PFileInfo info;
    PFile::GetInfo(_file,info);

    if (read && (info.permissions < PFileInfo::UserRead)) {
        errCode = e_AccessDenied;
        return FALSE;
    }

    errCode = e_OK;
    return TRUE;
}

PBoolean H323FileIOChannel::Open()
{
  PWaitAndSignal mutex(chanMutex);

    if (fileopen)
        return TRUE;

    return TRUE;
}
    
PBoolean H323FileIOChannel::Close()
{
  PWaitAndSignal mutex(chanMutex);

  if (!fileopen)
        return TRUE;

  PIndirectChannel::Close();
  return TRUE;
}

unsigned H323FileIOChannel::GetFileSize()
{
    return filesize;
}

PBoolean H323FileIOChannel::Read(void * buffer, PINDEX & amount)
{
    PWaitAndSignal mutex(chanMutex);

    if (!fileopen)
        return FALSE;

    PBoolean result = PIndirectChannel::Read(buffer, amount);
    amount = GetLastReadCount();

    return result;
}
   
PBoolean H323FileIOChannel::Write(const void * buf, PINDEX amount)
{
    PWaitAndSignal mutex(chanMutex);

    if (!fileopen)
        return FALSE;

    return PIndirectChannel::Write(buf, amount);
}

#endif  // H323_FILE



