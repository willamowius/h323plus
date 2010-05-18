/* h460_std9.cxx
 *
 * Copyright (c) 2009 ISVO (Asia) Pte Ltd. All Rights Reserved.
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
 * The Initial Developer of the Original Code is ISVO (Asia) Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Log$
 * Revision 1.2  2009/09/29 07:23:03  shorne
 * Change the way unmatched features are cleaned up in call signalling. Removed advertisement of H.460.19 in Alerting and Connecting PDU
 *
 * Revision 1.1  2009/08/21 07:01:06  shorne
 * Added H.460.9 Support
 *
 *
 *
 *
 */

#include "ptlib.h"
#include "openh323buildopts.h"

#ifdef H323_H4609

#include <h323ep.h>
#include <h323con.h>
#include <h460/h460_std9.h>
#include <h460/h4609.h>


#ifdef _MSC_VER
#pragma warning(disable : 4239)
#endif

// Must Declare for Factory Loader.
H460_FEATURE(Std9);


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
H460_FeatureStd9::H460_FeatureStd9()
: H460_FeatureStd(9)
{
 PTRACE(6,"Std9\tInstance Created");

 qossupport = false;
 finalonly = false;

 EP = NULL;
 CON = NULL;
 FeatureCategory = FeatureSupported;
}

H460_FeatureStd9::~H460_FeatureStd9()
{
}

void H460_FeatureStd9::AttachEndPoint(H323EndPoint * _ep)
{
  PTRACE(6,"Std9\tEndpoint Attached");
   EP = _ep; 
}

void H460_FeatureStd9::AttachConnection(H323Connection * _con)
{
   CON = _con;
}

PBoolean H460_FeatureStd9::OnSendAdmissionRequest(H225_FeatureDescriptor & pdu)
{
	// Build Message
    H460_FeatureStd & feat = H460_FeatureStd(9); 
    
    pdu = feat;

	return true;
}

void H460_FeatureStd9::OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu)
{
   qossupport = true;
   CON->H4609EnableStats();

   H460_FeatureStd & feat = (H460_FeatureStd &)pdu;
   if (feat.Contains(0)) 
	   finalonly = true;

   CON->H4609StatsFinal(finalonly);
}

PBoolean H460_FeatureStd9::GenerateReport(H4609_ArrayOf_RTCPMeasures & report)
{

	H323Connection::H4609Statistics stat;
    PBoolean hasStats = CON->H4609DequeueStats(stat);

	while (hasStats) {
	   PINDEX size = report.GetSize();
	   report.SetSize(size+1);
	   H4609_RTCPMeasures & info = report[size];

	   // RTP Information
	   H225_TransportChannelInfo & rtp = info.m_rtpAddress;
	    rtp.IncludeOptionalField(H225_TransportChannelInfo::e_sendAddress);
		    stat.sendRTPaddr.SetPDU(rtp.m_sendAddress);
	    rtp.IncludeOptionalField(H225_TransportChannelInfo::e_recvAddress);  
		    stat.recvRTPaddr.SetPDU(rtp.m_recvAddress);

	   // RTCP Information
/*	   H225_TransportChannelInfo & rtcp = info.m_rtcpAddress;
	    rtp.IncludeOptionalField(H225_TransportChannelInfo::e_sendAddress);
		    stat.sendRTCPaddr.SetPDU(rtp.m_sendAddress);
	    rtp.IncludeOptionalField(H225_TransportChannelInfo::e_recvAddress);  
		    stat.recvRTCPaddr.SetPDU(rtp.m_recvAddress);
*/    
	   // Session ID
	   info.m_sessionId.SetValue(stat.sessionid);

		  if (stat.meanEndToEndDelay > 0) {
			info.IncludeOptionalField(H4609_RTCPMeasures::e_mediaSenderMeasures);
			H4609_RTCPMeasures_mediaSenderMeasures & send = info.m_mediaSenderMeasures;


		  if (stat.meanEndToEndDelay > 0) {
			send.IncludeOptionalField(H4609_RTCPMeasures_mediaSenderMeasures::e_meanEstimatedEnd2EndDelay);
			send.m_meanEstimatedEnd2EndDelay = stat.meanEndToEndDelay;	
		  }

		  if (stat.worstEndToEndDelay > 0) {
			send.IncludeOptionalField(H4609_RTCPMeasures_mediaSenderMeasures::e_worstEstimatedEnd2EndDelay);
			send.m_worstEstimatedEnd2EndDelay = stat.worstEndToEndDelay; 
		  }
		}

		if (stat.packetsReceived > 0) {
			info.IncludeOptionalField(H4609_RTCPMeasures::e_mediaReceiverMeasures);
			H4609_RTCPMeasures_mediaReceiverMeasures & recv = info.m_mediaReceiverMeasures;

		  if (stat.accumPacketLost > 0) {
  			recv.IncludeOptionalField(H4609_RTCPMeasures_mediaReceiverMeasures::e_cumulativeNumberOfPacketsLost);
			recv.m_cumulativeNumberOfPacketsLost = stat.accumPacketLost;
		  }
			
		  if (stat.packetLossRate > 0) {
			recv.IncludeOptionalField(H4609_RTCPMeasures_mediaReceiverMeasures::e_packetLostRate);
			recv.m_packetLostRate = stat.packetLossRate;
		  }
			
		  if (stat.worstJitter > 0) {
			recv.IncludeOptionalField(H4609_RTCPMeasures_mediaReceiverMeasures::e_worstJitter);
			recv.m_worstJitter = stat.worstJitter;
		  }

		  if (stat.bandwidth > 0) {
			recv.IncludeOptionalField(H4609_RTCPMeasures_mediaReceiverMeasures::e_estimatedThroughput);
			recv.m_estimatedThroughput = stat.bandwidth;
		  }

		  if (stat.fractionLostRate > 0) {
			recv.IncludeOptionalField(H4609_RTCPMeasures_mediaReceiverMeasures::e_fractionLostRate);
			recv.m_fractionLostRate = stat.fractionLostRate;
		  }

		  if (stat.meanJitter > 0) {
			recv.IncludeOptionalField(H4609_RTCPMeasures_mediaReceiverMeasures::e_meanJitter);
			recv.m_meanJitter = stat.meanJitter;
		  }
		}

	  // Get next call statistics record
      hasStats = CON->H4609DequeueStats(stat);
	}

	return (report.GetSize() > 0);
}

PBoolean H460_FeatureStd9::WriteStatisticsReport(H460_FeatureStd & msg, PBoolean final)
{
	// Generate the report
	PBoolean success = FALSE;
	H4609_QosMonitoringReportData qosdata;
	if (!final) {
		qosdata.SetTag(H4609_QosMonitoringReportData::e_periodic);
		H4609_PeriodicQoSMonReport & rep = qosdata;
		H4609_ArrayOf_PerCallQoSReport & percall = rep.m_perCallInfo;
		percall.SetSize(1);
		H4609_PerCallQoSReport & period = percall[0];
		period.m_callReferenceValue = CON->GetCallReference();
		period.m_conferenceID = CON->GetConferenceIdentifier();
		period.m_callIdentifier.m_guid = CON->GetCallIdentifier();
        success = GenerateReport(period.m_mediaChannelsQoS); 
	} else {
		qosdata.SetTag(H4609_QosMonitoringReportData::e_final);
		H4609_FinalQosMonReport & rep = qosdata;
        success = GenerateReport(rep.m_mediaInfo); 
	}

	if (success) {
	   PASN_OctetString rawstats;
	   rawstats.EncodeSubType(qosdata);
	   msg.Add(1,H460_FeatureContent(rawstats));
	}

	return success;
}


PBoolean H460_FeatureStd9::OnSendInfoRequestResponseMessage(H225_FeatureDescriptor & pdu)
{
   if (!qossupport)
	   return false;

    H460_FeatureStd & feat = H460_FeatureStd(9); 

	if (WriteStatisticsReport(feat,finalonly)) {
        pdu = feat;
        return true;
	}
	return true;

}

PBoolean H460_FeatureStd9::OnSendDisengagementRequestMessage(H225_FeatureDescriptor & pdu)
{
   if (!qossupport)
 	 return false;

   H460_FeatureStd & feat = H460_FeatureStd(9); 

   if (WriteStatisticsReport(feat,finalonly)) {
        pdu = feat;
        return true;
   }
   return false;
}


#ifdef _MSC_VER
#pragma warning(default : 4239)
#endif

#endif

