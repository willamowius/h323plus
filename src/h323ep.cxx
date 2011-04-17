/*
 * h323ep.cxx
 *
 * H.323 protocol handler
 *
 * H323Plus Library
 *
 * Copyright (c) 1998-2000 Equivalence Pty. Ltd.
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
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Portions of this code were written with the assisance of funding from
 * Vovida Networks, Inc. http://www.vovida.com.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>
#include <ptlib/sound.h>

#ifdef __GNUC__
#pragma implementation "h323ep.h"
#endif

#include "openh323buildopts.h"

#include "h323ep.h"
#include "h323pdu.h"

#ifdef H323_H450
#include "h450/h450pdu.h"
#endif

#ifdef H323_H460
#include "h460/h460.h"
#include "h225.h"

// Std18 must be first for interop
#ifdef H323_H46018 
#include "h460/h460_std18.h"
#include "h460/h46018_h225.h"
#endif  

#ifdef H323_H46023
#include "h460/h460_std23.h"
#endif

#ifdef H323_H4609
#include "h460/h460_std9.h"
#endif

#ifdef H323_H460P
#include "h460/h460_oid3.h"
#endif

#endif  // H323_H460

#include "gkclient.h"

#ifdef H323_T38
#include "t38proto.h"
#endif

#include "../version.h"
#include "h323pluginmgr.h"

#include <ptlib/sound.h>
#include <ptclib/random.h>
#include <ptclib/pstun.h>
#include <ptclib/url.h>
#include <ptclib/pils.h>
#include <ptclib/enum.h>

#ifdef H323_H224
#include <h224handler.h>
#endif

#ifdef H323_FILE
#include "h323filetransfer.h"
#endif

#ifdef H323_GNUGK
#include "gnugknat.h"
#endif


#if defined(H323_RTP_AGGREGATE) || defined(H323_SIGNAL_AGGREGATE)
#include <ptclib/sockagg.h>
#endif

#ifndef IPTOS_PREC_CRITIC_ECP
#define IPTOS_PREC_CRITIC_ECP (5 << 5)
#endif

#ifndef IPTOS_LOWDELAY
#define IPTOS_LOWDELAY 0x10
#endif

#include "opalglobalstatics.cxx"
#include <algorithm>

//////////////////////////////////////////////////////////////////////////////////////

BYTE H323EndPoint::defaultT35CountryCode    = 9; // Country code for Australia
BYTE H323EndPoint::defaultT35Extension      = 0;
WORD H323EndPoint::defaultManufacturerCode  = 61; // Allocated by Australian Communications Authority, Oct 2000;

//////////////////////////////////////////////////////////////////////////////////////

class H225CallThread : public PThread
{
  PCLASSINFO(H225CallThread, PThread)

  public:
    H225CallThread(H323EndPoint & endpoint,
                   H323Connection & connection,
                   H323Transport & transport,
                   const PString & alias,
                   const H323TransportAddress & address);

  protected:
    void Main();

    H323Connection     & connection;
    H323Transport      & transport;
    PString              alias;
    H323TransportAddress address;
#ifdef H323_SIGNAL_AGGREGATE
    PBoolean                 useAggregator;
#endif
};


class H323ConnectionsCleaner : public PThread
{
  PCLASSINFO(H323ConnectionsCleaner, PThread)

  public:
    H323ConnectionsCleaner(H323EndPoint & endpoint);
    ~H323ConnectionsCleaner();

    void Signal() { wakeupFlag.Signal(); }

  protected:
    void Main();

    H323EndPoint & endpoint;
    PBoolean           stopFlag;
    PSyncPoint     wakeupFlag;
};


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

PString OpalGetVersion()
{
#define AlphaCode   "alpha"
#define BetaCode    "beta"
#define ReleaseCode "."

  return psprintf("%u.%u%s%u", MAJOR_VERSION, MINOR_VERSION, BUILD_TYPE, BUILD_NUMBER);
}


unsigned OpalGetMajorVersion()
{
  return MAJOR_VERSION;
}

unsigned OpalGetMinorVersion()
{
  return MINOR_VERSION;
}

unsigned OpalGetBuildNumber()
{
  return BUILD_NUMBER;
}



/////////////////////////////////////////////////////////////////////////////

H225CallThread::H225CallThread(H323EndPoint & endpoint,
                               H323Connection & c,
                               H323Transport & t,
                               const PString & a,
                               const H323TransportAddress & addr)
  : PThread(endpoint.GetSignallingThreadStackSize(),
            NoAutoDeleteThread,
            NormalPriority,
            "H225 Caller:%0x"),
    connection(c),
    transport(t),
    alias(a),
    address(addr)
{
#ifdef H323_SIGNAL_AGGREGATE
  useAggregator = endpoint.GetSignallingAggregator() != NULL;
  if (!useAggregator)
#endif
  {
    transport.AttachThread(this);
  }

  Resume();
}


void H225CallThread::Main()
{
  PTRACE(3, "H225\tStarted call thread");

  if (connection.Lock()) {
    H323Connection::CallEndReason reason = connection.SendSignalSetup(alias, address);

    // Special case, if we aborted the call then already will be unlocked
    if (reason != H323Connection::EndedByCallerAbort)
      connection.Unlock();

    // Check if had an error, clear call if so
    if (reason != H323Connection::NumCallEndReasons)
      connection.ClearCall(reason);
    else {
#ifdef H323_SIGNAL_AGGREGATE
      if (useAggregator) {
        connection.AggregateSignalChannel(&transport);
        SetAutoDelete(AutoDeleteThread);
        return;
      }
#endif
      connection.HandleSignallingChannel();
    }
  }
}


/////////////////////////////////////////////////////////////////////////////

H323ConnectionsCleaner::H323ConnectionsCleaner(H323EndPoint & ep)
  : PThread(ep.GetCleanerThreadStackSize(),
            NoAutoDeleteThread,
            NormalPriority,
            "H323 Cleaner"),
    endpoint(ep)
{
  Resume();
  stopFlag = FALSE;
}


H323ConnectionsCleaner::~H323ConnectionsCleaner()
{
  stopFlag = TRUE;
  wakeupFlag.Signal();
  PAssert(WaitForTermination(10000), "Cleaner thread did not terminate");
}


void H323ConnectionsCleaner::Main()
{
  PTRACE(3, "H323\tStarted cleaner thread");

  for (;;) {
    wakeupFlag.Wait();
    if (stopFlag)
      break;

    endpoint.CleanUpConnections();
  }

  PTRACE(3, "H323\tStopped cleaner thread");
}


/////////////////////////////////////////////////////////////////////////////

H323EndPoint::H323EndPoint()
  :
#ifdef H323_AUDIO_CODECS
#ifdef P_AUDIO
    soundChannelPlayDevice(PSoundChannel::GetDefaultDevice(PSoundChannel::Player)),
    soundChannelRecordDevice(PSoundChannel::GetDefaultDevice(PSoundChannel::Recorder)),
#endif
#endif
    signallingChannelConnectTimeout(0, 10, 0), // seconds
    signallingChannelCallTimeout(0, 0, 1),  // Minutes
    controlChannelStartTimeout(0, 0, 2),    // Minutes
    endSessionTimeout(0, 10),               // Seconds
    masterSlaveDeterminationTimeout(0, 30), // Seconds
    capabilityExchangeTimeout(0, 30),       // Seconds
    logicalChannelTimeout(0, 30),           // Seconds
    requestModeTimeout(0, 30),              // Seconds
    roundTripDelayTimeout(0, 10),           // Seconds
    roundTripDelayRate(0, 0, 1),            // Minutes
    noMediaTimeout(0, 0, 5),                // Minutes
    gatekeeperRequestTimeout(0, 5),         // Seconds
    rasRequestTimeout(0, 3)                // Seconds

#ifdef H323_H450
    ,
    callTransferT1(0,10),                   // Seconds
    callTransferT2(0,10),                   // Seconds
    callTransferT3(0,10),                   // Seconds
    callTransferT4(0,10),                   // Seconds
    callIntrusionT1(0,30),                  // Seconds
    callIntrusionT2(0,30),                  // Seconds
    callIntrusionT3(0,30),                  // Seconds
    callIntrusionT4(0,30),                  // Seconds
    callIntrusionT5(0,10),                  // Seconds
    callIntrusionT6(0,10),                   // Seconds
    nextH450CallIdentity(0)
#endif

{
  PString username = PProcess::Current().GetUserName();
  if (username.IsEmpty())
    username = PProcess::Current().GetName() & "User";
  localAliasNames.AppendString(username);

#if defined(P_AUDIO) && defined(_WIN32)
  PString DefaultAudioDriver = "WindowsMultimedia";
  SetSoundChannelPlayDriver(DefaultAudioDriver);
  SetSoundChannelRecordDriver(DefaultAudioDriver);
#endif

#ifdef H323_AUDIO_CODECS
  autoStartReceiveAudio = autoStartTransmitAudio = TRUE;
#endif

#ifdef H323_VIDEO
  autoStartReceiveVideo = autoStartTransmitVideo = TRUE;

#ifdef H323_H239
  autoStartReceiveExtVideo = autoStartTransmitExtVideo = FALSE;
#endif
#endif

#ifdef H323_T38
  autoStartReceiveFax = autoStartTransmitFax = FALSE;
#endif

#ifdef H323_AUDIO_CODECS
  minAudioJitterDelay = 50;  // milliseconds
  maxAudioJitterDelay = 250; // milliseconds
#endif

  autoCallForward = TRUE;
  disableFastStart = FALSE;
  disableH245Tunneling = FALSE;
  disableH245inSetup = TRUE;
  disableH245QoS = TRUE;
  disableDetectInBandDTMF = FALSE;
  canDisplayAmountString = FALSE;
  canEnforceDurationLimit = TRUE;

#ifdef H323_H450
  callIntrusionProtectionLevel = 3; //H45011_CIProtectionLevel::e_fullProtection;
#endif

#ifdef H323_AUDIO_CODECS
  defaultSilenceDetection = H323AudioCodec::AdaptiveSilenceDetection;
#endif

  defaultSendUserInputMode = H323Connection::SendUserInputAsString;

  terminalType = e_TerminalOnly;
  initialBandwidth = 100000; // Standard 10base LAN in 100's of bits/sec
  clearCallOnRoundTripFail = FALSE;

  t35CountryCode   = defaultT35CountryCode;   // Country code for Australia
  t35Extension     = defaultT35Extension;
  manufacturerCode = defaultManufacturerCode; // Allocated by Australian Communications Authority, Oct 2000

  rtpIpPorts.current = rtpIpPorts.base = 5000;
  rtpIpPorts.max = 5999;

  // use dynamic port allocation by default
  tcpPorts.current = tcpPorts.base = tcpPorts.max = 0;
  udpPorts.current = udpPorts.base = udpPorts.max = 0;

#ifdef P_STUN
  natMethods = new PNatStrategy();
  stun = NULL;
  disableSTUNTranslate = FALSE;
#endif

#ifdef _WIN32

#  if defined(H323_AUDIO_CODECS) && defined(P_AUDIO)
     // Windows MultiMedia stuff seems to need greater depth due to enormous
     // latencies in its operation, need to use DirectSound maybe?
     // for Win2000 and XP you need 5, for Vista you need 10! so set to 10! -SH
     soundChannelBuffers = 10;
#  endif

  rtpIpTypeofService = IPTOS_PREC_CRITIC_ECP|IPTOS_LOWDELAY;

#else

#  ifdef H323_AUDIO_CODECS
#ifdef P_AUDIO
     // Should only need double buffering for Unix platforms
     soundChannelBuffers = 2;
#endif
#  endif

  // Don't use IPTOS_PREC_CRITIC_ECP on Unix platforms as then need to be root
  rtpIpTypeofService = IPTOS_LOWDELAY;

#endif
  tcpIpTypeofService = IPTOS_LOWDELAY;

  masterSlaveDeterminationRetries = 10;
  gatekeeperRequestRetries = 2;
  rasRequestRetries = 2;
  sendGRQ = TRUE;

  cleanerThreadStackSize    = 30000;
  listenerThreadStackSize   = 30000;
  signallingThreadStackSize = 30000;
  controlThreadStackSize    = 30000;
  logicalThreadStackSize    = 30000;
  rasThreadStackSize        = 30000;
  jitterThreadStackSize     = 30000;
  useJitterBuffer			= true;

#ifdef H323_SIGNAL_AGGREGATE
  signallingAggregationSize = 25;
  signallingAggregator = NULL;
#endif

#ifdef H323_RTP_AGGREGATE
  rtpAggregationSize        = 10;
  rtpAggregator = NULL;
#endif

  channelThreadPriority     = PThread::HighestPriority;

  gatekeeper = NULL;

  connectionsActive.DisallowDeleteObjects();

#ifdef H323_H450
  secondaryConnectionsActive.DisallowDeleteObjects();
#endif

  connectionsCleaner = new H323ConnectionsCleaner(*this);

  srand((unsigned)time(NULL)+clock());

#ifndef DISABLE_CALLAUTH
  SetEPSecurityPolicy(SecNone);
  SetEPCredentials(PString(),PString());
  isSecureCall = FALSE;
#endif

#ifdef H323_H460
  disableH460 = false;

#ifdef H323_H46018
  // We must set time to live to 30 to ensure pinhole open
  registrationTimeToLive = PTimeInterval(0, 30);  
  m_h46018enabled = true;
#endif

#ifdef H323_H460P
  presenceHandler = NULL;
#endif
#endif

#ifdef H323_AEC 
  enableAEC = false;
#endif

#ifdef H323_GNUGK
  gnugk = NULL;
#endif

  m_useH225KeepAlive = PFalse;
  m_useH245KeepAlive = PFalse;

  PTRACE(3, "H323\tCreated endpoint.");
}


H323EndPoint::~H323EndPoint()
{

#if defined(H323_RTP_AGGREGATE) || defined (H323_SIGNAL_AGGREGATE)
  // delete aggregators
  {
    PWaitAndSignal m(connectionsMutex);
#ifdef H323_RTP_AGGREGATE
    if (rtpAggregator != NULL) {
      delete rtpAggregator;
      rtpAggregator = NULL;
    }
#endif
#ifdef H323_SIGNAL_AGGREGATE
    if (signallingAggregator != NULL) {
      delete signallingAggregator;
      signallingAggregator = NULL;
    }
#endif
  }
#endif

  // And shut down the gatekeeper (if there was one)
  RemoveGatekeeper();

#if H323_GNUGK
  delete gnugk;
#endif

  // Shut down the listeners as soon as possible to avoid race conditions
  listeners.RemoveAll();

  // Clear any pending calls on this endpoint
  ClearAllCalls();

  // Shut down the cleaner thread
  delete connectionsCleaner;

  // Clean up any connections that the cleaner thread missed
  CleanUpConnections();

#ifdef P_STUN
  delete natMethods;
#endif

#ifdef H323_H460P
  delete presenceHandler;
#endif

  PTRACE(3, "H323\tDeleted endpoint.");
}


void H323EndPoint::SetEndpointTypeInfo(H225_EndpointType & info) const
{
  info.IncludeOptionalField(H225_EndpointType::e_vendor);
  SetVendorIdentifierInfo(info.m_vendor);

  switch (terminalType) {
    case e_TerminalOnly :
    case e_TerminalAndMC :
      info.IncludeOptionalField(H225_EndpointType::e_terminal);
      break;
    case e_GatewayOnly :
    case e_GatewayAndMC :
    case e_GatewayAndMCWithDataMP :
    case e_GatewayAndMCWithAudioMP :
    case e_GatewayAndMCWithAVMP :
      info.IncludeOptionalField(H225_EndpointType::e_gateway);
	  if (SetGatewaySupportedProtocol(info.m_gateway.m_protocol))
		  info.m_gateway.IncludeOptionalField(H225_GatewayInfo::e_protocol);
      break;
    case e_GatekeeperOnly :
    case e_GatekeeperWithDataMP :
    case e_GatekeeperWithAudioMP :
    case e_GatekeeperWithAVMP :
      info.IncludeOptionalField(H225_EndpointType::e_gatekeeper);
      break;
    case e_MCUOnly :
    case e_MCUWithDataMP :
    case e_MCUWithAudioMP :
    case e_MCUWithAVMP :
      info.IncludeOptionalField(H225_EndpointType::e_mcu);
      info.m_mc = TRUE;
	  if (SetGatewaySupportedProtocol(info.m_mcu.m_protocol))
		  info.m_mcu.IncludeOptionalField(H225_McuInfo::e_protocol);
  }
}

PBoolean H323EndPoint::SetGatewaySupportedProtocol(H225_ArrayOf_SupportedProtocols & protocols) const
{
	PStringList prefixes;

	if (OnSetGatewayPrefixes(prefixes)) {
		H225_SupportedProtocols proto;
		proto.SetTag(H225_SupportedProtocols::e_h323);
		H225_H323Caps & caps = proto;
		caps.IncludeOptionalField(H225_H323Caps::e_supportedPrefixes);
         H225_ArrayOf_SupportedPrefix & pre = caps.m_supportedPrefixes;	
		 pre.SetSize(prefixes.GetSize());
		  for (PINDEX i=0; i < prefixes.GetSize(); i++) {
		     H225_SupportedPrefix p;
			   H225_AliasAddress & alias = p.m_prefix;
			   H323SetAliasAddress(prefixes[i],alias);
		     pre[i] = p;
		  }
		protocols.SetSize(1);
		protocols[0] = proto;
		return TRUE;
	}

	return FALSE;
}

PBoolean H323EndPoint::OnSetGatewayPrefixes(PStringList & /*prefixes*/) const
{
	return FALSE;
}

void H323EndPoint::SetVendorIdentifierInfo(H225_VendorIdentifier & info) const
{
  SetH221NonStandardInfo(info.m_vendor);

  info.IncludeOptionalField(H225_VendorIdentifier::e_productId);
  info.m_productId = PProcess::Current().GetManufacturer() & PProcess::Current().GetName();
  info.m_productId.SetSize(info.m_productId.GetSize()+2);

  info.IncludeOptionalField(H225_VendorIdentifier::e_versionId);
  info.m_versionId = PProcess::Current().GetVersion(TRUE) + " (H323plus v" + OpalGetVersion() + ')';
  info.m_versionId.SetSize(info.m_versionId.GetSize()+2);
}


void H323EndPoint::SetH221NonStandardInfo(H225_H221NonStandard & info) const
{
  info.m_t35CountryCode = t35CountryCode;
  info.m_t35Extension = t35Extension;
  info.m_manufacturerCode = manufacturerCode;
}


H323Capability * H323EndPoint::FindCapability(const H245_Capability & cap) const
{
  return capabilities.FindCapability(cap);
}


H323Capability * H323EndPoint::FindCapability(const H245_DataType & dataType) const
{
  return capabilities.FindCapability(dataType);
}


H323Capability * H323EndPoint::FindCapability(H323Capability::MainTypes mainType,
                                              unsigned subType) const
{
  return capabilities.FindCapability(mainType, subType);
}


void H323EndPoint::AddCapability(H323Capability * capability)
{
  capabilities.Add(capability);
}


PINDEX H323EndPoint::SetCapability(PINDEX descriptorNum,
                                   PINDEX simultaneousNum,
                                   H323Capability * capability)
{
  return capabilities.SetCapability(descriptorNum, simultaneousNum, capability);
}

PBoolean H323EndPoint::RemoveCapability(H323Capability::MainTypes capabilityType)
{
	return capabilities.RemoveCapability(capabilityType);
}

#ifdef H323_VIDEO
PBoolean H323EndPoint::SetVideoEncoder(unsigned frameWidth, unsigned frameHeight, unsigned frameRate)
{
    return capabilities.SetVideoEncoder(frameWidth, frameHeight, frameRate);
}

PBoolean H323EndPoint::SetVideoFrameSize(H323Capability::CapabilityFrameSize frameSize, 
		                  int frameUnits)
{
    return capabilities.SetVideoFrameSize(frameSize,frameUnits);
}
#endif

PINDEX H323EndPoint::AddAllCapabilities(PINDEX descriptorNum,
                                        PINDEX simultaneous,
                                        const PString & name)
{
    PINDEX reply = simultaneous;
	reply = capabilities.AddAllCapabilities(descriptorNum, simultaneous, name);
#ifdef H323_VIDEO
#ifdef H323_H239
	AddAllExtendedVideoCapabilities(descriptorNum, simultaneous);
#endif
#endif
  return reply;
}


void H323EndPoint::AddAllUserInputCapabilities(PINDEX descriptorNum,
                                               PINDEX simultaneous)
{
  H323_UserInputCapability::AddAllCapabilities(capabilities, descriptorNum, simultaneous);
}

#ifdef H323_VIDEO
#ifdef H323_H239
PBoolean H323EndPoint::OpenExtendedVideoSession(const PString & token)
{
  H323Connection * connection = FindConnectionWithLock(token);
 
  PBoolean success = FALSE;
  if (connection != NULL) {
    success = connection->OpenH239Channel();
    connection->Unlock();
  }
  return success;
}

PBoolean H323EndPoint::OpenExtendedVideoSession(const PString & token,H323ChannelNumber & /*num*/)
{
  return OpenExtendedVideoSession(token);
}

PBoolean H323EndPoint::CloseExtendedVideoSession(
		                       const PString & token         
	                           )
{
  H323Connection * connection = FindConnectionWithLock(token);
 
  PBoolean success = FALSE;
  if (connection != NULL) {
    success = connection->CloseH239Channel();
    connection->Unlock();
  }
  return success;
}

PBoolean H323EndPoint::CloseExtendedVideoSession(
		                       const PString & token,         
		                       const H323ChannelNumber & /*num*/  
	                           )
{
  return CloseExtendedVideoSession(token);
}

void H323EndPoint::AddAllExtendedVideoCapabilities(PINDEX descriptorNum,
                                                   PINDEX simultaneous)
{
  H323ExtendedVideoCapability::AddAllCapabilities(capabilities, descriptorNum, simultaneous);
}
#endif  // H323_H239
#endif  // H323_VIDEO

void H323EndPoint::RemoveCapabilities(const PStringArray & codecNames)
{
  capabilities.Remove(codecNames);
}


void H323EndPoint::ReorderCapabilities(const PStringArray & preferenceOrder)
{
  capabilities.Reorder(preferenceOrder);
}


PBoolean H323EndPoint::UseGatekeeper(const PString & address,
                                 const PString & identifier,
                                 const PString & localAddress)
{
  if (gatekeeper != NULL) {
    PBoolean same = TRUE;

    if (!address)
      same = gatekeeper->GetTransport().GetRemoteAddress().IsEquivalent(address);

    if (!same && !identifier)
      same = gatekeeper->GetIdentifier() == identifier;

    if (!same && !localAddress)
      same = gatekeeper->GetTransport().GetLocalAddress().IsEquivalent(localAddress);

    if (same) {
      PTRACE(2, "H323\tUsing existing gatekeeper " << *gatekeeper);
      return TRUE;
    }
  }

  H323Transport * transport = NULL;
  if (!localAddress.IsEmpty()) {
    H323TransportAddress iface(localAddress);
    PIPSocket::Address ip;
    WORD port = H225_RAS::DefaultRasUdpPort;
    if (iface.GetIpAndPort(ip, port))
      transport = new H323TransportUDP(*this, ip, port);
  }

  if (address.IsEmpty()) {
    if (identifier.IsEmpty())
      return DiscoverGatekeeper(transport);
    else
      return LocateGatekeeper(identifier, transport);
  }
  else {
    if (identifier.IsEmpty())
      return SetGatekeeper(address, transport);
    else
      return SetGatekeeperZone(address, identifier, transport);
  }
}


PBoolean H323EndPoint::SetGatekeeper(const PString & address, H323Transport * transport)
{
  H323Gatekeeper * gk = InternalCreateGatekeeper(transport);
  return InternalRegisterGatekeeper(gk, gk->DiscoverByAddress(address));
}


PBoolean H323EndPoint::SetGatekeeperZone(const PString & address,
                                     const PString & identifier,
                                     H323Transport * transport)
{
  H323Gatekeeper * gk = InternalCreateGatekeeper(transport);
  return InternalRegisterGatekeeper(gk, gk->DiscoverByNameAndAddress(identifier, address));
}


PBoolean H323EndPoint::LocateGatekeeper(const PString & identifier, H323Transport * transport)
{
  H323Gatekeeper * gk = InternalCreateGatekeeper(transport);
  return InternalRegisterGatekeeper(gk, gk->DiscoverByName(identifier));
}


PBoolean H323EndPoint::DiscoverGatekeeper(H323Transport * transport)
{
  H323Gatekeeper * gk = InternalCreateGatekeeper(transport);
  return InternalRegisterGatekeeper(gk, gk->DiscoverAny());
}


H323Gatekeeper * H323EndPoint::InternalCreateGatekeeper(H323Transport * transport)
{
  RemoveGatekeeper(H225_UnregRequestReason::e_reregistrationRequired);

  if (transport == NULL)
    transport = new H323TransportUDP(*this);

  H323Gatekeeper * gk = CreateGatekeeper(transport);

  gk->SetPassword(gatekeeperPassword);

  return gk;
}


PBoolean H323EndPoint::InternalRegisterGatekeeper(H323Gatekeeper * gk, PBoolean discovered)
{
  if (discovered) {
    if (gk->RegistrationRequest()) {
      gatekeeper = gk;
      return TRUE;
    }

    // RRQ was rejected continue trying
    gatekeeper = gk;
  }
  else // Only stop listening if the GRQ was rejected
    delete gk;

  return FALSE;
}


H323Gatekeeper * H323EndPoint::CreateGatekeeper(H323Transport * transport)
{
  return new H323Gatekeeper(*this, transport);
}


PBoolean H323EndPoint::IsRegisteredWithGatekeeper() const
{
  if (gatekeeper == NULL)
    return FALSE;

  return gatekeeper->IsRegistered();
}


PBoolean H323EndPoint::RemoveGatekeeper(int reason)
{
  PBoolean ok = TRUE;

  if (gatekeeper == NULL)
    return ok;

  ClearAllCalls();

  if (gatekeeper->IsRegistered()) // If we are registered send a URQ
    ok = gatekeeper->UnregistrationRequest(reason);

  delete gatekeeper;
  gatekeeper = NULL;

  return ok;
}

void H323EndPoint::ForceGatekeeperReRegistration()
{
	if (gatekeeper != NULL)
		RegInvokeReRegistration();
}

void H323EndPoint::SetGatekeeperPassword(const PString & password)
{
  gatekeeperPassword = password;

  if (gatekeeper != NULL) {
    gatekeeper->SetPassword(gatekeeperPassword);
    if (gatekeeper->IsRegistered()) // If we are registered send a URQ
      gatekeeper->UnregistrationRequest(H225_UnregRequestReason::e_reregistrationRequired);
    InternalRegisterGatekeeper(gatekeeper, TRUE);
  }
}

PBoolean H323EndPoint::GatekeeperCheckIP(const H323TransportAddress & /*oldAddr*/,
									 H323TransportAddress & /*newaddress*/)
{
	return FALSE;
}

void H323EndPoint::OnAdmissionRequest(H323Connection & /*connection*/)
{
}

void H323EndPoint::OnGatekeeperConfirm()
{
}

void H323EndPoint::OnGatekeeperReject()
{
}

void H323EndPoint::OnRegistrationConfirm(const H323TransportAddress & /*rasAddress*/)
{
}

void H323EndPoint::OnRegistrationReject()
{
}

void H323EndPoint::OnUnRegisterRequest()
{
}

void H323EndPoint::OnUnRegisterConfirm()
{
}

void H323EndPoint::OnRegisterTTLFail()
{
}

H235Authenticators H323EndPoint::CreateAuthenticators()
{
  H235Authenticators authenticators;

  PFactory<H235Authenticator>::KeyList_T keyList = PFactory<H235Authenticator>::GetKeyList();
  PFactory<H235Authenticator>::KeyList_T::const_iterator r;
  for (r = keyList.begin(); r != keyList.end(); ++r) {
    H235Authenticator * Auth = PFactory<H235Authenticator>::CreateInstance(*r);
    if ((Auth->GetApplication() == H235Authenticator::GKAdmission) ||
                  (Auth->GetApplication() == H235Authenticator::AnyApplication)) 
                               authenticators.Append(Auth);
  }

  return authenticators;
}

#ifndef DISABLE_CALLAUTH
H235Authenticators H323EndPoint::CreateEPAuthenticators() 
{
  H235Authenticators authenticators;

  PString username;
  PString password;

  if ((GetEPSecurityPolicy() != SecNone) || (isSecureCall)) {
	  if (GetEPCredentials(password, username)) {
             PFactory<H235Authenticator>::KeyList_T keyList = PFactory<H235Authenticator>::GetKeyList();
             PFactory<H235Authenticator>::KeyList_T::const_iterator r;
              for (r = keyList.begin(); r != keyList.end(); ++r) {
                H235Authenticator * Auth = PFactory<H235Authenticator>::CreateInstance(*r);
                if ((Auth->GetApplication() == H235Authenticator::EPAuthentication) ||
                     (Auth->GetApplication() == H235Authenticator::AnyApplication)) {
                        Auth->SetLocalId(username);
                        Auth->SetPassword(password);
                        authenticators.Append(Auth);
                }
              }
	    SetEPCredentials(PString(),PString());
	  }
	  isSecureCall = FALSE;
  }

  return authenticators;
}

PBoolean H323EndPoint::GetEPCredentials(PString & password, 
									PString & username)
{
  if (EPSecurityPassword.IsEmpty())
     return FALSE;
  else
     password = EPSecurityPassword;

  if (EPSecurityUserName.IsEmpty())
      username = GetLocalUserName();
  else
      username = EPSecurityUserName;  

  return TRUE;
}

void H323EndPoint::SetEPCredentials(PString password, PString username)
{
	EPSecurityPassword = password;			
	EPSecurityUserName = username;			
}

void H323EndPoint::SetEPSecurityPolicy(EPSecurityPolicy policy)
{
	CallAuthPolicy = policy;
}

H323EndPoint::EPSecurityPolicy H323EndPoint::GetEPSecurityPolicy()
{
	return CallAuthPolicy;
}

H235AuthenticatorList H323EndPoint::GetAuthenticatorList()
{
	return EPAuthList;
}

PBoolean H323EndPoint::OnCallAuthentication(const PString & username, 
										PString & password)
{
	if (EPAuthList.HasUserName(username)) {
		EPAuthList.LoadPassword(username, password);
		return TRUE;
	}

	return FALSE;
}

H323Connection * H323EndPoint::MakeAuthenticatedCall(const PString & remoteParty, 
		const PString & UserName, const PString & Password, PString & token, void * userData)
{
  isSecureCall = TRUE;
  SetEPCredentials(Password,UserName);
  return MakeCall(remoteParty, token, userData);
}
#endif

#ifdef H323_H460 
H323Connection * H323EndPoint::MakeSupplimentaryCall (
                        const PString & remoteParty,  ///* Remote party to call
                        PString & token,              ///* String to receive token for connection
                        void * userData               ///* user data to pass to CreateConnection
                        )
{

	return MakeCall(remoteParty, token, userData,true);
}
#endif

PBoolean H323EndPoint::StartListeners(const H323TransportAddressArray & ifaces)
{
  if (ifaces.IsEmpty())
    return StartListener("*");

  PINDEX i;

  for (i = 0; i < listeners.GetSize(); i++) {
    PBoolean remove = TRUE;
    for (PINDEX j = 0; j < ifaces.GetSize(); j++) {
      if (listeners[i].GetTransportAddress().IsEquivalent(ifaces[j])) {
        remove = FALSE;
        break;
      }
    }
    if (remove) {
      PTRACE(3, "H323\tRemoving listener " << listeners[i]);
      listeners.RemoveAt(i--);
    }
  }

  for (i = 0; i < ifaces.GetSize(); i++) {
    if (!ifaces[i])
      StartListener(ifaces[i]);
  }

  return listeners.GetSize() > 0;
}


PBoolean H323EndPoint::StartListener(const H323TransportAddress & iface)
{
  H323Listener * listener;

  if (iface.IsEmpty())
    listener = new H323ListenerTCP(*this, PIPSocket::GetDefaultIpAny(), DefaultTcpPort);
  else
    listener = iface.CreateListener(*this);

  if (H323EndPoint::StartListener(listener))
    return TRUE;

  PTRACE(1, "H323\tCould not start listener: " << iface);
  delete listener;
  return FALSE;
}


PBoolean H323EndPoint::StartListener(H323Listener * listener)
{
  if (listener == NULL)
    return FALSE;

  for (PINDEX i = 0; i < listeners.GetSize(); i++) {
    if (listeners[i].GetTransportAddress() == listener->GetTransportAddress()) {
      PTRACE(2, "H323\tAlready have listener for " << *listener);
      delete listener;
      return TRUE;
    }
  }

  // as the listener is not open, this will have the effect of immediately
  // stopping the listener thread. This is good - it means that the 
  // listener Close function will appear to have stopped the thread
  if (!listener->Open()) {
    listener->Resume(); // set the thread running so we can delete it later
    return FALSE;
  }

  PTRACE(3, "H323\tStarted listener " << *listener);
  listeners.Append(listener);
  listener->Resume();
  return TRUE;
}


PBoolean H323EndPoint::RemoveListener(H323Listener * listener)
{
  if (listener != NULL) {
    PTRACE(3, "H323\tRemoving listener " << *listener);
    return listeners.Remove(listener);
  }

  PTRACE(3, "H323\tRemoving all listeners");
  listeners.RemoveAll();
  return TRUE;
}


H323TransportAddressArray H323EndPoint::GetInterfaceAddresses(PBoolean excludeLocalHost,
                                                              H323Transport * associatedTransport)
{
  return H323GetInterfaceAddresses(listeners, excludeLocalHost, associatedTransport);
}

H323Connection * H323EndPoint::MakeCall(const PString & remoteParty,
                                        PString & token,
                                        void * userData,
										PBoolean supplimentary
										)
{
  return MakeCall(remoteParty, NULL, token, userData, supplimentary);
}


H323Connection * H323EndPoint::MakeCall(const PString & remoteParty,
                                        H323Transport * transport,
                                        PString & token,
                                        void * userData,
										PBoolean supplimentary
										)
{
  token = PString::Empty();

  PStringList Addresses;
  if (!ResolveCallParty(remoteParty, Addresses))
	return NULL;

  H323Connection * connection = NULL;
  for (PINDEX i = 0; i < Addresses.GetSize(); i++) {
       connection = InternalMakeCall(PString::Empty(),
                                     PString::Empty(),
                                     UINT_MAX,
                                     Addresses[i],
                                     transport,
                                     token,
                                     userData,
                                     supplimentary
									 );
    if (connection != NULL) {
        connection->Unlock();
	    break;
	}
  }

  return connection;
}


H323Connection * H323EndPoint::MakeCallLocked(const PString & remoteParty,
                                              PString & token,
                                              void * userData,
                                              H323Transport * transport)
{
  token = PString::Empty();

  PStringList Addresses;
  if (!ResolveCallParty(remoteParty, Addresses))
	return NULL;

  H323Connection * connection = NULL;
  for (PINDEX i = 0; i < Addresses.GetSize(); i++) {
      connection = InternalMakeCall(PString::Empty(),
                             PString::Empty(),
                             UINT_MAX,
                             Addresses[i],
                             transport,
                             token,
                             userData);
     if (connection != NULL) 
		    break;		 
  }

  return connection;

}

H323Connection * H323EndPoint::InternalMakeCall(const PString & trasferFromToken,
                                                const PString & callIdentity,
                                                unsigned capabilityLevel,
                                                const PString & remoteParty,
                                                H323Transport * transport,
                                                PString & newToken,
                                                void * userData,
												PBoolean supplimentary
												)
{
  PTRACE(2, "H323\tMaking call to: " << remoteParty);

  PString alias;
  H323TransportAddress address;
  if (!ParsePartyName(remoteParty, alias, address)) {
    PTRACE(2, "H323\tCould not parse \"" << remoteParty << '"');
    return NULL;
  }

  if (transport == NULL) {
    // Restriction: the call must be made on the same transport as the one
    // that the gatekeeper is using.
    if (gatekeeper != NULL)
      transport = gatekeeper->GetTransport().GetRemoteAddress().CreateTransport(*this);

    // assume address is an IP address/hostname
    else
      transport = address.CreateTransport(*this);

    if (transport == NULL) {
      PTRACE(1, "H323\tInvalid transport in \"" << remoteParty << '"');
      return NULL;
    }
  }

  H323Connection * connection;

  connectionsMutex.Wait();

  unsigned lastReference;
  if (newToken.IsEmpty()) {
    do {
      lastReference = Q931::GenerateCallReference();
      newToken = BuildConnectionToken(*transport, lastReference, FALSE);
    } while (connectionsActive.Contains(newToken));
  }
  else {
    lastReference = newToken.Mid(newToken.Find('/')+1).AsUnsigned();

    // Move old connection on token to new value and flag for removal
    PString adjustedToken;
    unsigned tieBreaker = 0;
    do {
      adjustedToken = newToken + "-replaced";
      adjustedToken.sprintf("-%u", ++tieBreaker);
    } while (connectionsActive.Contains(adjustedToken));
    connectionsActive.SetAt(adjustedToken, connectionsActive.RemoveAt(newToken));
    connectionsToBeCleaned += adjustedToken;
    PTRACE(3, "H323\tOverwriting call " << newToken << ", renamed to " << adjustedToken);
  }
  connectionsMutex.Signal();

  connection = CreateConnection(lastReference, userData, transport, NULL);
  if (connection == NULL) {
    PTRACE(1, "H323\tCreateConnection returned NULL");
    connectionsMutex.Signal();
    return NULL;
  }

  if (supplimentary) 
	  connection->SetNonCallConnection();     

  connection->Lock();

  connectionsMutex.Wait();
  connectionsActive.SetAt(newToken, connection);

  connectionsMutex.Signal();

  connection->AttachSignalChannel(newToken, transport, FALSE);

#ifdef H323_H450
  if (capabilityLevel == UINT_MAX)
    connection->HandleTransferCall(trasferFromToken, callIdentity);
  else {
    connection->HandleIntrudeCall(trasferFromToken, callIdentity);
    connection->IntrudeCall(capabilityLevel);
  }
#endif

  PTRACE(3, "H323\tCreated new connection: " << newToken);

  new H225CallThread(*this, *connection, *transport, alias, address);
  return connection;
}

#if P_DNS

struct LookupRecord {
  enum {
    CallDirect,
    LRQ
  };
  int type;
  PIPSocket::Address addr;
  WORD port;
};
/*
static PBoolean FindSRVRecords(std::vector<LookupRecord> & recs,
                    const PString & domain,
                    int type,
                    const PString & srv)
{
  PDNS::SRVRecordList srvRecords;
  PString srvLookupStr = srv + domain;
  PBoolean found = PDNS::GetRecords(srvLookupStr, srvRecords);
  if (found) {
    PDNS::SRVRecord * recPtr = srvRecords.GetFirst();
    while (recPtr != NULL) {
      LookupRecord rec;
      rec.addr = recPtr->hostAddress;
      rec.port = recPtr->port;
      rec.type = type;
      recs.push_back(rec);
      recPtr = srvRecords.GetNext();
      PTRACE(4, "H323\tFound " << rec.addr << ":" << rec.port << " with SRV " << srv << " using domain " << domain);
    }
  } 
  return found;
}

static PBoolean FindRoutes(const PString & domain, WORD port, std::vector<LookupRecord> & routes)
{
  PBoolean hasGK = FindSRVRecords(    routes, domain, LookupRecord::LRQ,        "_h323ls._udp.");
  hasGK = hasGK || FindSRVRecords(routes, domain, LookupRecord::LRQ,        "_h323rs._udp.");
  FindSRVRecords(                 routes, domain, LookupRecord::CallDirect, "_h323cs._tcp.");

  // see if the domain is actually a host
  PIPSocket::Address addr;
  if (PIPSocket::GetHostAddress(domain, addr)) {
    LookupRecord rec;
    rec.addr = addr;
    rec.port = port;
    rec.type = LookupRecord::CallDirect;
    PTRACE(4, "H323\tDomain " << domain << " is a host - using as call signal address");
    routes.push_back(rec);
  }

  if (routes.size() != 0) {
    PDNS::MXRecordList mxRecords;
    if (PDNS::GetRecords(domain, mxRecords)) {
      PDNS::MXRecord * recPtr = mxRecords.GetFirst();
      while (recPtr != NULL) {
        LookupRecord rec;
        rec.addr = recPtr->hostAddress;
        rec.port = 1719;
        rec.type = LookupRecord::LRQ;
        routes.push_back(rec);
        recPtr = mxRecords.GetNext();
        PTRACE(4, "H323\tFound " << rec.addr << ":" << rec.port << " with MX for domain " << domain);
      }
    } 
  }

  return routes.size() != 0;
}
*/
#endif

PBoolean H323EndPoint::ResolveCallParty(const PString & _remoteParty, PStringList & addresses)
{
  PString remoteParty = _remoteParty;

#if P_DNS
  // if there is no gatekeeper, 
  if (gatekeeper == NULL) {

     //if there is no '@', and there is no URL scheme, then attempt to use ENUM
    if ((_remoteParty.Find(':') == P_MAX_INDEX) && (remoteParty.Find('@') == P_MAX_INDEX)) {

	PString number = _remoteParty;
	if (number.Left(5) *= "h323:")
	   number = number.Mid(5);

    PINDEX i;
    for (i = 0; i < number.GetLength(); ++i)
       if (!isdigit(number[i]))
        break;
		if (i >= number.GetLength()) {
           PString str;
          if (PDNS::ENUMLookup(number, "E2U+h323", str)) {
		    if ((str.Find("//1") != P_MAX_INDEX) &&
		         (str.Find('@') != P_MAX_INDEX)) {
			   remoteParty = "h323:" + number + str.Mid(str.Find('@')-1);
		    } else {
              remoteParty = str;
		  }
		  PTRACE(4, "H323\tENUM converted remote party " << _remoteParty << " to " << remoteParty);
        } else {
          PTRACE(4, "H323\tENUM Cannot resolve remote party " << _remoteParty);
		  return false;
        }
      }
	}

     // attempt a DNS SRV lookup to detect a call signalling entry
	PBoolean found = FALSE;
    if (remoteParty.Find('@') != P_MAX_INDEX) {
       PString number = remoteParty;
       if (number.Left(5) != "h323:") 
          number = "h323:" + number;	  
				
	   PStringList str;

	   if (!found) str.RemoveAll();
	   if (!found && (PDNS::LookupSRV(number,"_h323cs._tcp.",str))) {
		   for (PINDEX i=0; i<str.GetSize(); i++) {
		     PString a = str[i].Mid(str[i].Find('@')+1);
	         PTRACE(4, "H323\tDNS SRV CS located remote party " << _remoteParty << " at " << a);
             addresses.AppendString(str[i]);
			 found = TRUE;
		   }
       }
/*
	   if (!found) str.RemoveAll();
	   if (!found && (PDNS::LookupSRV(number,"_h323ls._udp.",str))) {
		   for (PINDEX j=0; j<str.GetSize(); j++) {
			PString a = str[j].Mid(str[j].Find('@')+1);
				H323TransportAddress newAddr, gkAddr(a);
				H323Gatekeeper * gk = CreateGatekeeper(new H323TransportUDP(*this));
				PBoolean ok = gk->DiscoverByAddress(gkAddr);
				if (ok) 
				  ok = gk->LocationRequest(remoteParty, newAddr);
				delete gk;
				if (ok) {
				  PTRACE(3, "H323\tDNS SRV LR of \"" << remoteParty << "\" on gk " << gkAddr << " found " << newAddr);
				  addresses.AppendString(newAddr);
				  found = TRUE;
				}
		   }
	   }
*/
	   if (!found) {
           PTRACE(4, "H323\tDNS SRV Cannot resolve remote party " << remoteParty);
		   addresses = PStringList(remoteParty);
       }
	} else {
       addresses = PStringList(remoteParty);
    }
	return TRUE;
   }  
#endif
    addresses = PStringList(remoteParty);
	return TRUE;
}

PBoolean H323EndPoint::ParsePartyName(const PString & _remoteParty,
                                  PString & alias,
                                  H323TransportAddress & address)
{
  PString remoteParty = _remoteParty;

  // Support [address]##[Alias] dialing
  PINDEX hash = _remoteParty.Find("##");
  if (hash != P_MAX_INDEX) {
    remoteParty = "h323:" + _remoteParty.Mid(hash+2) + "@" + _remoteParty.Left(hash);
    PTRACE(4, "H323\tConverted " << _remoteParty << " to " << remoteParty);
  }

  //check if IPv6 address
  PINDEX ipv6 = remoteParty.Find("::");
  if (ipv6 != P_MAX_INDEX) {
      PINDEX at = remoteParty.Find('@');
      if (at != P_MAX_INDEX) {
          remoteParty = remoteParty.Left(at+1) + "[" + remoteParty.Mid(at+1) + "]";
      } else
          remoteParty = "[" + remoteParty + "]";
  }

  // convert the remote party string to a URL, with a default URL of "h323:"
  PURL url(remoteParty, "h323");

  // if the scheme does not match the prefix of the remote party, then
  // the URL parser got confused, so force the URL to be of type "h323"
  if ((remoteParty.Find('@') == P_MAX_INDEX) && (remoteParty.NumCompare(url.GetScheme()) != EqualTo)) {
    if (gatekeeper == NULL)
      url.Parse("h323:@" + remoteParty);
    else
      url.Parse("h323:" + remoteParty);
  }

  // get the various parts of the name
  PString hostOnly = PString();
  if (remoteParty.Find('@') != P_MAX_INDEX) {
    if (gatekeeper != NULL)
      alias = url.AsString();
    else {
	  alias = remoteParty.Left(remoteParty.Find('@'));
	  hostOnly = remoteParty.Mid(remoteParty.Find('@')+1);
    }
  } else {
     alias = url.GetUserName();
     hostOnly = url.GetHostName();
  } 
  address = hostOnly;
  
  // make sure the address contains the port, if not default
  if (!address && (url.GetPort() != 0))
    address.sprintf(":%u", url.GetPort());

  if (alias.IsEmpty() && address.IsEmpty()) {
    PTRACE(1, "H323\tAttempt to use invalid URL \"" << remoteParty << '"');
    return FALSE;
  }

  PBoolean gatekeeperSpecified = FALSE;
  PBoolean gatewaySpecified = FALSE;

  PCaselessString type = url.GetParamVars()("type");

  if (url.GetScheme() == "callto") {
    // Do not yet support ILS
    if (type == "directory") {
#if P_LDAP
      PString server = url.GetHostName();
      if (server.IsEmpty())
        server = ilsServer;
      if (server.IsEmpty())
        return FALSE;

      PILSSession ils;
      if (!ils.Open(server, url.GetPort())) {
        PTRACE(1, "H323\tCould not open ILS server at \"" << server
               << "\" - " << ils.GetErrorText());
        return FALSE;
      }

      PILSSession::RTPerson person;
      if (!ils.SearchPerson(alias, person)) {
        PTRACE(1, "H323\tCould not find "
               << server << '/' << alias << ": " << ils.GetErrorText());
        return FALSE;
      }

      if (!person.sipAddress.IsValid()) {
        PTRACE(1, "H323\tILS user " << server << '/' << alias
               << " does not have a valid IP address");
        return FALSE;
      }

      // Get the IP address
      address = H323TransportAddress(person.sipAddress);

      // Get the port if provided
      for (PINDEX i = 0; i < person.sport.GetSize(); i++) {
        if (person.sport[i] != 1503) { // Dont use the T.120 port
          address = H323TransportAddress(person.sipAddress, person.sport[i]);
          break;
        }
      }

      alias = PString::Empty(); // No alias for ILS lookup, only host
      return TRUE;
#else
      return FALSE;
#endif
    }

    if (url.GetParamVars().Contains("gateway"))
      gatewaySpecified = TRUE;
  }

  else if (url.GetScheme() == "h323") {
    if (type == "gw")
      gatewaySpecified = TRUE;
    else if (type == "gk")
      gatekeeperSpecified = TRUE;
    else if (!type) {
      PTRACE(1, "H323\tUnsupported host type \"" << type << "\" in h323 URL");
      return FALSE;
    }
  }

  // User explicitly asked to use a GK for lookup
  if (gatekeeperSpecified) {
    if (alias.IsEmpty()) {
      PTRACE(1, "H323\tAttempt to use explict gatekeeper without alias!");
      return FALSE;
    }

    if (address.IsEmpty()) {
      PTRACE(1, "H323\tAttempt to use explict gatekeeper without address!");
      return FALSE;
    }

    H323TransportAddress gkAddr = address;
    PTRACE(3, "H323\tLooking for \"" << alias << "\" on gatekeeper at " << gkAddr);

    H323Gatekeeper * gk = CreateGatekeeper(new H323TransportUDP(*this));

    PBoolean ok = gk->DiscoverByAddress(gkAddr);
    if (ok) {
      ok = gk->LocationRequest(alias, address);
      if (ok) {
        PTRACE(3, "H323\tLocation Request of \"" << alias << "\" on gk " << gkAddr << " found " << address);
      }
      else {
        PTRACE(1, "H323\tLocation Request failed for \"" << alias << "\" on gk " << gkAddr);
      }
    }
    else {
      PTRACE(1, "H323\tLocation Request discovery failed for gk " << gkAddr);
    }

    delete gk;

    return ok;
  }

  // User explicitly said to use a gw, or we do not have a gk to do look up
  if (gatekeeper == NULL
      || gatewaySpecified) {
    // If URL did not have a host, but user said to use gw, or we do not have
    // a gk to do a lookup so we MUST have a host, use alias must be host
    if (address.IsEmpty()) {
      address = alias;
      alias = PString::Empty();
      return TRUE;
    }
/*
#if P_DNS
    // if we have an address and the correct scheme, then check DNS
    if (!address && (url.GetScheme() *= "h323")) {
      std::vector<LookupRecord> routes;
      if (FindRoutes(hostOnly, url.GetPort(), routes)) {
        std::vector<LookupRecord>::const_iterator r;
        for (r = routes.begin(); r != routes.end(); ++r) {
          const LookupRecord & rec = *r;
          switch (rec.type) {
            case LookupRecord::CallDirect:
              address = H323TransportAddress(rec.addr, rec.port);
              PTRACE(3, "H323\tParty name \"" << url << "\" mapped to \"" << alias << "@" << address);
              return TRUE;
              break; 

            case LookupRecord::LRQ:
              {
                H323TransportAddress newAddr, gkAddr(rec.addr, rec.port);
                H323Gatekeeper * gk = CreateGatekeeper(new H323TransportUDP(*this));
                PBoolean ok = gk->DiscoverByAddress(gkAddr);
                if (ok) 
                  ok = gk->LocationRequest(alias, newAddr);
                delete gk;
                if (ok) {
                  address = newAddr;
                  PTRACE(3, "H323\tLocation Request of \"" << alias << "\" on gk " << gkAddr << " found " << address);
                  return TRUE;
                }
              } 
              break;
  
            default:
              break;
          }
        }
      }
    }
#endif   // P_DNS
*/
  } 

  if (!address)
    return TRUE;

  // We have a gk and user did not explicitly supply a host, so lets
  // do a check to see if it is an IP address or hostname
  if (alias.FindOneOf("$.:[") != P_MAX_INDEX) {
    H323TransportAddress test = alias;
    PIPSocket::Address ip;
    if (test.GetIpAddress(ip) && ip.IsValid()) {
      // The alias was a valid internet address, use it as such
      alias = PString::Empty();
      address = test;
    }
  }

  return TRUE;
}

#ifdef H323_H450

H323Connection * H323EndPoint::SetupTransfer(const PString & oldToken,
                                             const PString & callIdentity,
                                             const PString & remoteParty,
                                             PString & newToken,
                                             void * userData)
{
  newToken = PString::Empty();

  PStringList Addresses;
  if (!ResolveCallParty(remoteParty, Addresses))
	return NULL;

  H323Connection * connection = NULL;
     for (PINDEX i = 0; i < Addresses.GetSize(); i++) {
         connection = InternalMakeCall(oldToken,
                                       callIdentity,
                                       UINT_MAX,
                                       Addresses[i],
                                       NULL,
                                       newToken,
                                       userData);
        if (connection != NULL) {
            connection->Unlock();
			break;
		}
	 }

   return connection;
}


void H323EndPoint::TransferCall(const PString & token, 
                                const PString & remoteParty,
                                const PString & callIdentity)
{
  H323Connection * connection = FindConnectionWithLock(token);
  if (connection != NULL) {
    connection->TransferCall(remoteParty, callIdentity);
    connection->Unlock();
  }
}


void H323EndPoint::ConsultationTransfer(const PString & primaryCallToken,   
                                        const PString & secondaryCallToken)
{
  H323Connection * secondaryCall = FindConnectionWithLock(secondaryCallToken);
  if (secondaryCall != NULL) {
    secondaryCall->ConsultationTransfer(primaryCallToken);
    secondaryCall->Unlock();
  }
}


void H323EndPoint::HoldCall(const PString & token, PBoolean localHold)
{
  H323Connection * connection = FindConnectionWithLock(token);
  if (connection != NULL) {
    connection->HoldCall(localHold);
    connection->Unlock();
  }
}


H323Connection * H323EndPoint::IntrudeCall(const PString & remoteParty,
                                        PString & token,
                                        unsigned capabilityLevel,
                                        void * userData)
{
  return IntrudeCall(remoteParty, NULL, token, capabilityLevel, userData);
}


H323Connection * H323EndPoint::IntrudeCall(const PString & remoteParty,
                                        H323Transport * transport,
                                        PString & token,
                                        unsigned capabilityLevel,
                                        void * userData)
{
  token = PString::Empty();

 PStringList Addresses;
  if (!ResolveCallParty(remoteParty, Addresses))
	return NULL;

  H323Connection * connection = NULL;
     for (PINDEX i = 0; i < Addresses.GetSize(); i++) {
             connection = InternalMakeCall(PString::Empty(),
                                           PString::Empty(),
                                           capabilityLevel,
                                           Addresses[i],
                                           transport,
                                           token,
                                           userData);
		 if (connection != NULL) {
            connection->Unlock();
			break;
		 }
	 }
  return connection;
}

void H323EndPoint::OnReceivedInitiateReturnError()
{
}

#endif  // H323_H450


PBoolean H323EndPoint::ClearCall(const PString & token,
                             H323Connection::CallEndReason reason)
{
  return ClearCallSynchronous(token, reason, NULL);
}

void H323EndPoint::OnCallClearing(H323Connection * /*connection*/,
								  H323Connection::CallEndReason /*reason*/)
{
}

PBoolean H323EndPoint::ClearCallSynchronous(const PString & token,
                             H323Connection::CallEndReason reason)
{
  PSyncPoint sync;
  return ClearCallSynchronous(token, reason, &sync);
}

PBoolean H323EndPoint::ClearCallSynchronous(const PString & token,
                                        H323Connection::CallEndReason reason,
                                        PSyncPoint * sync)
{
  if (PThread::Current() == connectionsCleaner)
    sync = NULL;

  /*The hugely multi-threaded nature of the H323Connection objects means that
    to avoid many forms of race condition, a call is cleared by moving it from
    the "active" call dictionary to a list of calls to be cleared that will be
    processed by a background thread specifically for the purpose of cleaning
    up cleared connections. So that is all that this function actually does.
    The real work is done in the H323ConnectionsCleaner thread.
   */

  {
    PWaitAndSignal wait(connectionsMutex);

    // Find the connection by token, callid or conferenceid
    H323Connection * connection = FindConnectionWithoutLocks(token);
    if (connection == NULL) {
      PTRACE(3, "H323\tAttempt to clear unknown call " << token);
      return FALSE;
    }

    PTRACE(3, "H323\tClearing connection " << connection->GetCallToken()
                                           << " reason=" << reason);

    OnCallClearing(connection,reason);

    // Add this to the set of connections being cleaned, if not in already
    if (!connectionsToBeCleaned.Contains(connection->GetCallToken())) 
      connectionsToBeCleaned += connection->GetCallToken();

    // Now set reason for the connection close
    connection->SetCallEndReason(reason, sync);

    // Signal the background threads that there is some stuff to process.
    connectionsCleaner->Signal();
  }

  if (sync != NULL)
    sync->Wait();

  return TRUE;
}


void H323EndPoint::ClearAllCalls(H323Connection::CallEndReason reason,
                                 PBoolean wait)
{
  /*The hugely multi-threaded nature of the H323Connection objects means that
    to avoid many forms of race condition, a call is cleared by moving it from
    the "active" call dictionary to a list of calls to be cleared that will be
    processed by a background thread specifically for the purpose of cleaning
    up cleared connections. So that is all that this function actually does.
    The real work is done in the H323ConnectionsCleaner thread.
   */

  connectionsMutex.Wait();

  // Add all connections to the to be deleted set
  PINDEX i;
  for (i = 0; i < connectionsActive.GetSize(); i++) {
    H323Connection & connection = connectionsActive.GetDataAt(i);
    connectionsToBeCleaned += connection.GetCallToken();
    // Now set reason for the connection close
    connection.SetCallEndReason(reason, NULL);
  }

  // Signal the background threads that there is some stuff to process.
  connectionsCleaner->Signal();

  // Make sure any previous signals are removed before waiting later
  while (connectionsAreCleaned.Wait(0))
    ;

  connectionsMutex.Signal();

  if (wait)
    connectionsAreCleaned.Wait();
}

void H323EndPoint::CleanUpConnections()
{
  PTRACE(3, "H323\tCleaning up connections");

  // Lock the connections database.
  connectionsMutex.Wait();

  // Continue cleaning up until no more connections to clean
  while (connectionsToBeCleaned.GetSize() > 0) {
    // Just get the first entry in the set of tokens to clean up.
    PString token = connectionsToBeCleaned.GetKeyAt(0);
    H323Connection & connection = connectionsActive[token];

    // Unlock the structures here so does not block other uses of ClearCall()
    // for the possibly long time it takes to CleanUpOnCallEnd().
    connectionsMutex.Signal();

    // Clean up the connection, waiting for all threads to terminate
    connection.CleanUpOnCallEnd();
    connection.OnCleared();

    // Get the lock again as we remove the connection from our database
    connectionsMutex.Wait();

    // Remove the token from the set of connections to be cleaned up
    connectionsToBeCleaned -= token;

    // And remove the connection instance itself from the dictionary which will
    // cause its destructor to be called.
    H323Connection * connectionToDelete = connectionsActive.RemoveAt(token);

    // Unlock the structures yet again to avoid possible race conditions when
    // deleting the connection as well as the delte of a conncetion descendent
    // is application writer dependent and may cause deadlocks or just consume
    // lots of time.
    connectionsMutex.Signal();

    // Finally we get to delete it!
    delete connectionToDelete;

    // Get the lock again as we continue around the loop
    connectionsMutex.Wait();
  }

  // Finished with loop, unlock the connections database.
  connectionsMutex.Signal();

  // Signal thread that may be waiting on ClearAllCalls()
  connectionsAreCleaned.Signal();
}


PBoolean H323EndPoint::HasConnection(const PString & token)
{
  PWaitAndSignal wait(connectionsMutex);

  return FindConnectionWithoutLocks(token) != NULL;
}


H323Connection * H323EndPoint::FindConnectionWithLock(const PString & token)
{
  PWaitAndSignal mutex(connectionsMutex);

  /*We have a very yucky polling loop here as a semi permanant measure.
    Why? We cannot call Lock() inside the connectionsMutex critical section as
    it will cause a deadlock with something like a RELEASE-COMPLETE coming in
    on separate thread. But if we put it outside there is a small window where
    the connection could get deleted before the Lock() test is done.
    The solution is to attempt to get the mutex while inside the
    connectionsMutex but not block. That means a polling loop. There is
    probably a way to do this properly with mutexes but I don't have time to
    figure it out.
   */
  H323Connection * connection;
  while ((connection = FindConnectionWithoutLocks(token)) != NULL) {
    switch (connection->TryLock()) {
      case 0 :
        return NULL;
      case 1 :
        return connection;
    }
    // Could not get connection lock, unlock the endpoint lists so a thread
    // that has the connection lock gets a chance at the endpoint lists.
    connectionsMutex.Signal();
    PThread::Sleep(20);
    connectionsMutex.Wait();
  }

  return NULL;
}


H323Connection * H323EndPoint::FindConnectionWithoutLocks(const PString & token)
{
  if (token.IsEmpty())
    return NULL;

  H323Connection * conn_ptr = connectionsActive.GetAt(token);
  if (conn_ptr != NULL)
    return conn_ptr;

  PINDEX i;
  for (i = 0; i < connectionsActive.GetSize(); i++) {
    H323Connection & conn = connectionsActive.GetDataAt(i);
    if (conn.GetCallIdentifier().AsString() == token)
      return &conn;
  }

  for (i = 0; i < connectionsActive.GetSize(); i++) {
    H323Connection & conn = connectionsActive.GetDataAt(i);
    if (conn.GetConferenceIdentifier().AsString() == token)
      return &conn;
  }

  return NULL;
}


PStringList H323EndPoint::GetAllConnections()
{
  PStringList tokens;

  connectionsMutex.Wait();

  for (PINDEX i = 0; i < connectionsActive.GetSize(); i++)
    tokens.AppendString(connectionsActive.GetKeyAt(i));

  connectionsMutex.Signal();

  return tokens;
}


PBoolean H323EndPoint::OnIncomingCall(H323Connection & /*connection*/,
                                  const H323SignalPDU & /*setupPDU*/,
                                  H323SignalPDU & /*alertingPDU*/)
{
  return TRUE;
}

PBoolean H323EndPoint::OnIncomingCall(H323Connection & connection,
                             const H323SignalPDU & setupPDU,
                                   H323SignalPDU & alertingPDU,
                   H323Connection::CallEndReason & reason)
{
  reason = H323Connection::EndedByNoAccept;
  return connection.OnIncomingCall(setupPDU, alertingPDU);
}

PBoolean H323EndPoint::OnCallTransferInitiate(H323Connection & /*connection*/,
                                          const PString & /*remoteParty*/)
{
  return TRUE;
}


PBoolean H323EndPoint::OnCallTransferIdentify(H323Connection & /*connection*/)
{
  return TRUE;
}

void H323EndPoint::OnSendARQ(H323Connection & /*conn*/, H225_AdmissionRequest & /*arq*/)
{
}

void H323EndPoint::OnReceivedACF(H323Connection & /*conn*/, const H225_AdmissionConfirm & /*arq*/)
{
}

void H323EndPoint::OnReceivedARJ(H323Connection & /*conn*/, const H225_AdmissionReject & /*arq*/)
{
}

H323Connection::AnswerCallResponse
       H323EndPoint::OnAnswerCall(H323Connection & /*connection*/,
                                  const PString & PTRACE_PARAM(caller),
                                  const H323SignalPDU & /*setupPDU*/,
                                  H323SignalPDU & /*connectPDU*/)
{
  PTRACE(2, "H225\tOnAnswerCall from \"" << caller << '"');
  return H323Connection::AnswerCallNow;
}


PBoolean H323EndPoint::OnAlerting(H323Connection & /*connection*/,
                              const H323SignalPDU & /*alertingPDU*/,
                              const PString & /*username*/)
{
  PTRACE(1, "H225\tReceived alerting PDU.");
  return TRUE;
}


PBoolean H323EndPoint::OnConnectionForwarded(H323Connection & /*connection*/,
                                         const PString & /*forwardParty*/,
                                         const H323SignalPDU & /*pdu*/)
{
  return FALSE;
}


PBoolean H323EndPoint::ForwardConnection(H323Connection & connection,
                                     const PString & forwardParty,
                                     const H323SignalPDU & /*pdu*/)
{
  PString token = connection.GetCallToken();

 PStringList Addresses;
  if (!ResolveCallParty(forwardParty, Addresses))
	return FALSE;

  H323Connection * newConnection = NULL;
     for (PINDEX i = 0; i < Addresses.GetSize(); i++) {
          newConnection = InternalMakeCall(PString::Empty(),
                                           PString::Empty(),
                                           UINT_MAX,
                                           Addresses[i],
                                           NULL,
                                           token,
                                           NULL);
        if (newConnection != NULL)
             break;
	 }


	 if (newConnection == NULL) 
		 return FALSE;

     connection.SetCallEndReason(H323Connection::EndedByCallForwarded);
     newConnection->Unlock();
     return TRUE;
}


void H323EndPoint::OnConnectionEstablished(H323Connection & /*connection*/,
                                           const PString & /*token*/)
{
}


PBoolean H323EndPoint::IsConnectionEstablished(const PString & token)
{
  H323Connection * connection = FindConnectionWithLock(token);
  if (connection == NULL)
    return FALSE;

  PBoolean established = connection->IsEstablished();
  connection->Unlock();
  return established;
}


PBoolean H323EndPoint::OnOutgoingCall(H323Connection & /*connection*/,
                             const H323SignalPDU & /*connectPDU*/)
{
  PTRACE(1, "H225\tReceived connect PDU.");
  return TRUE;
}


void H323EndPoint::OnConnectionCleared(H323Connection & /*connection*/,
                                       const PString & /*token*/)
{
}


PString H323EndPoint::BuildConnectionToken(const H323Transport & transport,
                                           unsigned callReference,
                                           PBoolean fromRemote)
{
  PString token;

  if (fromRemote)
    token = transport.GetRemoteAddress();
  else
    token = "ip$localhost";

  token.sprintf("/%u", callReference);

  return token;
}


H323Connection * H323EndPoint::OnIncomingConnection(H323Transport * transport,
                                                    H323SignalPDU & setupPDU)
{
  unsigned callReference = setupPDU.GetQ931().GetCallReference();
  PString token = BuildConnectionToken(*transport, callReference, TRUE);

  connectionsMutex.Wait();
  H323Connection * connection = connectionsActive.GetAt(token);
  connectionsMutex.Signal();

  if (connection == NULL) {
    connection = CreateConnection(callReference, NULL, transport, &setupPDU);
    if (connection == NULL) {
      PTRACE(1, "H323\tCreateConnection returned NULL");
      return NULL;
    }

    PTRACE(3, "H323\tCreated new connection: " << token);

    connectionsMutex.Wait();
    connectionsActive.SetAt(token, connection);
    connectionsMutex.Signal();
  }

  connection->AttachSignalChannel(token, transport, TRUE);

  return connection;
}


H323Connection * H323EndPoint::CreateConnection(unsigned callReference,
                                                void * userData,
                                                H323Transport * /*transport*/,
                                                H323SignalPDU * /*setupPDU*/)
{
  return CreateConnection(callReference, userData);
}


H323Connection * H323EndPoint::CreateConnection(unsigned callReference, void * /*userData*/)
{
  return CreateConnection(callReference);
}

H323Connection * H323EndPoint::CreateConnection(unsigned callReference)
{
  return new H323Connection(*this, callReference);
}


#if PTRACING
static void OnStartStopChannel(const char * startstop, const H323Channel & channel)
{
  const char * dir;
  switch (channel.GetDirection()) {
    case H323Channel::IsTransmitter :
      dir = "send";
      break;

    case H323Channel::IsReceiver :
      dir = "receiv";
      break;

    default :
      dir = "us";
      break;
  }

  PTRACE(2, "H323\t" << startstop << "ed "
                     << dir << "ing logical channel: "
                     << channel.GetCapability());
}
#endif


PBoolean H323EndPoint::OnStartLogicalChannel(H323Connection & /*connection*/,
                                         H323Channel & PTRACE_PARAM(channel))
{
#if PTRACING
  OnStartStopChannel("Start", channel);
#endif
  return TRUE;
}


void H323EndPoint::OnClosedLogicalChannel(H323Connection & /*connection*/,
                                          const H323Channel & PTRACE_PARAM(channel))
{
#if PTRACING
  OnStartStopChannel("Stopp", channel);
#endif
}


#ifdef H323_AUDIO_CODECS

PBoolean H323EndPoint::OpenAudioChannel(H323Connection & /*connection*/,
                                    PBoolean isEncoding,
                                    unsigned bufferSize,
                                    H323AudioCodec & codec)
{
  codec.SetSilenceDetectionMode(GetSilenceDetectionMode());

#ifdef P_AUDIO

  int rate = codec.GetMediaFormat().GetTimeUnits() * 1000;

  PString deviceName;
  PString deviceDriver;
  if (isEncoding) {
    deviceName   = GetSoundChannelRecordDevice();
    deviceDriver = GetSoundChannelRecordDriver();
  } else {
    deviceName = GetSoundChannelPlayDevice();
    deviceDriver = GetSoundChannelPlayDriver();
  }

  PSoundChannel * soundChannel;
  if (!deviceDriver.IsEmpty()) 
    soundChannel = PSoundChannel::CreateChannel(deviceDriver);
  else {
    soundChannel = new PSoundChannel;
    deviceDriver = "default";
  }

  if (soundChannel == NULL) {
    PTRACE(1, "Codec\tCould not open a sound channel for " << deviceDriver);
    return FALSE;
  }

  if (soundChannel->Open(deviceName, isEncoding ? PSoundChannel::Recorder
                                                : PSoundChannel::Player,
                         1, rate, 16)) {
    PTRACE(3, "Codec\tOpened sound channel \"" << deviceName
           << "\" for " << (isEncoding ? "record" : "play")
           << "ing at " << rate << " samples/second using " << soundChannelBuffers
           << 'x' << bufferSize << " byte buffers.");
    soundChannel->SetBuffers(bufferSize, soundChannelBuffers);
    return codec.AttachChannel(soundChannel);
  }

  PTRACE(1, "Codec\tCould not open " << deviceDriver << " sound channel \"" << deviceName
         << "\" for " << (isEncoding ? "record" : "play")
         << "ing: " << soundChannel->GetErrorText());

  delete soundChannel;

#endif

  return FALSE;
}

#endif // H323_AUDIO_CODECS


#ifdef H323_VIDEO
PBoolean H323EndPoint::OpenVideoChannel(H323Connection & /*connection*/,
                                    PBoolean PTRACE_PARAM(isEncoding),
                                    H323VideoCodec & /*codec*/)
{
  PTRACE(1, "Codec\tCould not open video channel for "
         << (isEncoding ? "captur" : "display")
         << "ing: not yet implemented");
  return FALSE;
}

#ifdef H323_H239
PBoolean H323EndPoint::OpenExtendedVideoChannel(H323Connection & /*connection*/,
											PBoolean PTRACE_PARAM(isEncoding),
											H323VideoCodec & /*codec*/)
{
  PTRACE(1, "Codec\tCould not open extended video channel for "
         << (isEncoding ? "captur" : "display")
         << "ing: not yet implemented");
  return FALSE;
}
#endif // H323_H239
#endif // NO_H323_VIDEO


void H323EndPoint::OnRTPStatistics(const H323Connection & /*connection*/,
                                   const RTP_Session & /*session*/) const
{
}

void H323EndPoint::OnRTPFinalStatistics(const H323Connection & /*connection*/,
                                   const RTP_Session & /*session*/) const
{
}


void H323EndPoint::OnUserInputString(H323Connection & /*connection*/,
                                     const PString & /*value*/)
{
}


void H323EndPoint::OnUserInputTone(H323Connection & connection,
                                   char tone,
                                   unsigned /*duration*/,
                                   unsigned /*logicalChannel*/,
                                   unsigned /*rtpTimestamp*/)
{
  // don't pass through signalUpdate messages
  if (tone != ' ')
    connection.OnUserInputString(PString(tone));
}

#ifdef H323_GNUGK
void H323EndPoint::OnGatekeeperNATDetect(
                                   PIPSocket::Address publicAddr,   
                                   const PString & gkIdentifier,
				           H323TransportAddress & gkRouteAddress
                                   )
{
	if (gnugk != NULL) {
		if (gnugk->ReRegister(gkIdentifier))
			return;
		else {
		   	PTRACE(4, "GNUGK\tReRegistration Failure. Attempting new connection");
			if (!gnugk->CreateNewTransport()) {
			  PTRACE(4, "GNUGK\tNAT Support Failure: Retry from scratch");	
			   delete gnugk;
			   gnugk = NULL;
			}
		}
	}

	gnugk = new GNUGK_Feature(*this,gkRouteAddress,gkIdentifier);

	 if (gnugk->IsOpen()) {
 	     PTRACE(4, "GNUGK\tNat Address " << gkRouteAddress);

		 PNatMethod_GnuGk * natMethod = (PNatMethod_GnuGk *)natMethods->LoadNatMethod("GnuGk");
		 if (natMethods) {
			 natMethod->AttachEndPoint(this);
			 natMethod->SetAvailable();
			 natMethods->AddMethod(natMethod);
		 }
		 return; 
	 } 

	  PTRACE(4, "GNUGK\tConnection failed. Disabling support.");
	  delete gnugk;
      gnugk = NULL;
}

void H323EndPoint::OnGatekeeperOpenNATDetect(
                          const PString & /*gkIdentifier*/,
				   H323TransportAddress & /*gkRouteAddress*/
                                   )
{
}
#endif

PBoolean H323EndPoint::OnGatekeeperAliases(
		const H225_ArrayOf_AliasAddress & /*aliases*/  
		                              )
{
	return FALSE;
}

#ifdef H323_H248

void H323EndPoint::OnHTTPServiceControl(unsigned /*opeartion*/,
                                        unsigned /*sessionId*/,
                                        const PString & /*url*/)
{
}

void H323EndPoint::OnCallCreditServiceControl(const PString & amount, PBoolean mode, const unsigned & /*durationLimit*/)
{
    OnCallCreditServiceControl(amount, mode);
}

void H323EndPoint::OnCallCreditServiceControl(const PString & /*amount*/, PBoolean /*mode*/)
{
}

#ifdef H323_H350
void H323EndPoint::OnH350ServiceControl(const PString & /*url*/,
										const PString & /*BaseDN*/)
{
}
#endif

void H323EndPoint::OnServiceControlSession(unsigned type,
                                           unsigned sessionId,
                                           const H323ServiceControlSession & session,
                                           H323Connection * connection)
{
  session.OnChange(type, sessionId, *this, connection);
}

H323ServiceControlSession * H323EndPoint::CreateServiceControlSession(const H225_ServiceControlDescriptor & contents)
{
  switch (contents.GetTag()) {
    case H225_ServiceControlDescriptor::e_url :
      return new H323HTTPServiceControl(contents);

    case H225_ServiceControlDescriptor::e_callCreditServiceControl :
      return new H323CallCreditServiceControl(contents);
#ifdef H323_H350
	case H225_ServiceControlDescriptor::e_nonStandard  :
      return new H323H350ServiceControl(contents);
#endif
  }

  return NULL;
}

#endif // H323_H248

PBoolean H323EndPoint::OnConferenceInvite(PBoolean /*sending*/,                  
      const H323Connection * /*connection*/,  
      const H323SignalPDU & /*setupPDU */)
{
  return FALSE;
}

PBoolean H323EndPoint::OnSendCallIndependentSupplementaryService(const H323Connection * connection, 
														     H323SignalPDU & pdu )
{
  
#ifdef H323_H460

  if (!connection->IsNonCallConnection())
	  return false;

  H225_Setup_UUIE & setup = pdu.m_h323_uu_pdu.m_h323_message_body;
  setup.m_conferenceGoal.SetTag(H225_Setup_UUIE_conferenceGoal::e_callIndependentSupplementaryService);
/*
  // This is horrible however it's the easiest way to get the connection featureSet.
  H460_FeatureSet * featset = NULL;
  H323Connection* conn = FindConnectionWithLock(connection->GetCallToken());
  if (conn != NULL) {
	  featset = conn->GetFeatureSet();
	  conn->Unlock();
  }

  if (featset == NULL)
	  return false;

  H225_FeatureSet fs;
  if (featset->SendFeature(H460_MessageType::e_setup, fs)) {
	if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
        setup.IncludeOptionalField(H225_Setup_UUIE::e_supportedFeatures);
	    H225_ArrayOf_FeatureDescriptor & fsn = setup.m_supportedFeatures;
	    fsn = fs.m_supportedFeatures;
	} */
	PTRACE(6,"MyEP\tSending H.460 Call Independent Supplementary Service"); 
	return true;
//  } else
 
#endif
 // {
    return false;
//  }
}

PBoolean H323EndPoint::OnReceiveCallIndependentSupplementaryService(const H323Connection * connection, 
														        const H323SignalPDU & pdu)
{
#ifdef H323_H450
  if (pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_h4501SupplementaryService)) {
	  PTRACE(6,"MyEP\tReceived H.450 Call Independent Supplementary Service");
      return true;
  }
#endif

#ifdef H323_H460

  if (disableH460)
	  return false;

  H225_FeatureSet fs;
  const H225_Setup_UUIE & setup = pdu.m_h323_uu_pdu.m_h323_message_body;
		
  if (setup.HasOptionalField(H225_Setup_UUIE::e_supportedFeatures)) {
	  fs.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
	  fs.m_supportedFeatures = setup.m_supportedFeatures;
  }

  if (setup.HasOptionalField(H225_Setup_UUIE::e_neededFeatures)) {
	  fs.IncludeOptionalField(H225_FeatureSet::e_neededFeatures);
	  fs.m_supportedFeatures = setup.m_neededFeatures;
  }

  if (setup.HasOptionalField(H225_Setup_UUIE::e_desiredFeatures)) {
	  fs.IncludeOptionalField(H225_FeatureSet::e_desiredFeatures);
	  fs.m_supportedFeatures = setup.m_desiredFeatures;
  }

  H460_FeatureSet * featset = NULL;
  H323Connection* conn = FindConnectionWithLock(connection->GetCallToken());
  if (conn != NULL) {
	  featset = conn->GetFeatureSet();
	  conn->Unlock();
  }

  if (featset->SupportNonCallService(fs)) {
     PTRACE(6,"MyEP\tReceived H.460 Call Independent Supplementary Service");
     return true;
  } else 
#endif
  {
    PTRACE(6,"MyEP\tRejected CallIndependentSupplementaryService as no support in EndPoint.");
    return false;
  }
}

PBoolean H323EndPoint::OnNegotiateConferenceCapabilities(const H323SignalPDU & /* setupPDU */)
{
  return FALSE;
}

#ifdef H323_T120
OpalT120Protocol * H323EndPoint::CreateT120ProtocolHandler(const H323Connection &) const
{
  return NULL;
}
#endif


#ifdef H323_T38
OpalT38Protocol * H323EndPoint::CreateT38ProtocolHandler(const H323Connection &) const
{
  return NULL;
}
#endif

#ifdef H323_H224

OpalH224Handler * H323EndPoint::CreateH224ProtocolHandler(H323Channel::Directions dir, H323Connection & connection, 
														  unsigned sessionID) const
{
  return new OpalH224Handler(dir, connection, sessionID);
}


OpalH281Handler * H323EndPoint::CreateH281ProtocolHandler(OpalH224Handler & h224Handler) const
{
  return new OpalH281Handler(h224Handler);
}

#endif

#ifdef H323_FILE
PBoolean H323EndPoint::OpenFileTransferSession( const H323FileTransferList & list,
	                                            const PString & token, 
												H323ChannelNumber & num
												)
{
  H323Connection * connection = FindConnectionWithLock(token);
 
  PBoolean success = FALSE;
  if (connection != NULL) {
    success = connection->OpenFileTransferSession(list,num);
    connection->Unlock();
  }

  return success;
}

PBoolean H323EndPoint::OpenFileTransferChannel(H323Connection & connection,
											 PBoolean PTRACE_PARAM(isEncoder),
						                     H323FileTransferList & filelist
											) 
{
   PTRACE(2,"FT\tAttempt to open File Transfer session! Not implemented Yet!");
   return FALSE;
}
#endif

void H323EndPoint::SetLocalUserName(const PString & name)
{
  PAssert(!name, "Must have non-empty string in AliasAddress!");
  if (name.IsEmpty())
    return;

  localAliasNames.RemoveAll();
  localAliasNames.SetSize(0);
  localAliasNames.AppendString(name);
}


PBoolean H323EndPoint::AddAliasName(const PString & name)
{
  PAssert(!name, "Must have non-empty string in AliasAddress!");

  if (localAliasNames.GetValuesIndex(name) != P_MAX_INDEX)
    return FALSE;

  localAliasNames.AppendString(name);
  return TRUE;
}


PBoolean H323EndPoint::RemoveAliasName(const PString & name)
{
  PINDEX pos = localAliasNames.GetValuesIndex(name);
  if (pos == P_MAX_INDEX)
    return FALSE;

  PAssert(localAliasNames.GetSize() > 1, "Must have at least one AliasAddress!");
  if (localAliasNames.GetSize() < 2)
    return FALSE;

  localAliasNames.RemoveAt(pos);
  return TRUE;
}

#ifdef H323_AUDIO_CODECS

#ifdef P_AUDIO

PBoolean H323EndPoint::SetSoundChannelPlayDevice(const PString & name)
{
  if (PSoundChannel::GetDeviceNames(soundChannelPlayDriver,PSoundChannel::Player).GetValuesIndex(name) == P_MAX_INDEX)
    return FALSE;

  soundChannelPlayDevice = name;
  return TRUE;
}


PBoolean H323EndPoint::SetSoundChannelRecordDevice(const PString & name)
{
  if (PSoundChannel::GetDeviceNames(soundChannelRecordDriver,PSoundChannel::Recorder).GetValuesIndex(name) == P_MAX_INDEX)
    return FALSE;

  soundChannelRecordDevice = name;
  return TRUE;
}


PBoolean H323EndPoint::SetSoundChannelPlayDriver(const PString & name)
{
  PPluginManager & pluginMgr = PPluginManager::GetPluginManager(); 
  PStringList list = pluginMgr.GetPluginsProviding("PSoundChannel");
  if (list.GetValuesIndex(name) == P_MAX_INDEX)
    return FALSE;

  soundChannelPlayDriver = name;
  soundChannelPlayDevice.MakeEmpty();
  list = PSoundChannel::GetDeviceNames(name, PSoundChannel::Player);
  if (list.GetSize() == 0)
    return FALSE;

  soundChannelPlayDevice = list[0];
  return TRUE;
}


PBoolean H323EndPoint::SetSoundChannelRecordDriver(const PString & name)
{
  PPluginManager & pluginMgr = PPluginManager::GetPluginManager(); 
  PStringList list = pluginMgr.GetPluginsProviding("PSoundChannel");
  if (list.GetValuesIndex(name) == P_MAX_INDEX)
    return FALSE;

  soundChannelRecordDriver = name;
  list = PSoundChannel::GetDeviceNames(name, PSoundChannel::Recorder);
  if (list.GetSize() == 0)
    return FALSE;

  soundChannelRecordDevice = list[0];
  return TRUE;
}


void H323EndPoint::SetSoundChannelBufferDepth(unsigned depth)
{
  PAssert(depth > 1, PInvalidParameter);
  soundChannelBuffers = depth;
}

#endif  // P_AUDIO

#endif  // H323_AUDIO_CODECS


PBoolean H323EndPoint::IsTerminal() const
{
  switch (terminalType) {
    case e_TerminalOnly :
    case e_TerminalAndMC :
      return TRUE;

    default :
      return FALSE;
  }
}


PBoolean H323EndPoint::IsGateway() const
{
  switch (terminalType) {
    case e_GatewayOnly :
    case e_GatewayAndMC :
    case e_GatewayAndMCWithDataMP :
    case e_GatewayAndMCWithAudioMP :
    case e_GatewayAndMCWithAVMP :
      return TRUE;

    default :
      return FALSE;
  }
}


PBoolean H323EndPoint::IsGatekeeper() const
{
  switch (terminalType) {
    case e_GatekeeperOnly :
    case e_GatekeeperWithDataMP :
    case e_GatekeeperWithAudioMP :
    case e_GatekeeperWithAVMP :
      return TRUE;

    default :
      return FALSE;
  }
}


PBoolean H323EndPoint::IsMCU() const
{
  switch (terminalType) {
    case e_MCUOnly :
    case e_MCUWithDataMP :
    case e_MCUWithAudioMP :
    case e_MCUWithAVMP :
      return TRUE;

    default :
      return FALSE;
  }
}

#ifdef H323_AUDIO_CODECS

void H323EndPoint::SetAudioJitterDelay(unsigned minDelay, unsigned maxDelay)
{
  if (minDelay == 0 && maxDelay == 0) {
    // Disable jitter buffer
    minAudioJitterDelay = 0;
    maxAudioJitterDelay = 0;
    return;
  }

  PAssert(minDelay <= 10000 && maxDelay <= 10000, PInvalidParameter);

  if (minDelay < 10)
    minDelay = 10;
  minAudioJitterDelay = minDelay;

  if (maxDelay < minDelay)
    maxDelay = minDelay;
  maxAudioJitterDelay = maxDelay;
}

#endif

#ifdef P_STUN

PSTUNClient * H323EndPoint::GetSTUN(const PIPSocket::Address & ip) const
{
  if (ip.IsValid() && IsLocalAddress(ip))
    return NULL;

  return stun;
}

PNatMethod * H323EndPoint::GetPreferedNatMethod(const PIPSocket::Address & ip)
{

#if PTRACING
    PNatMethod * meth = NULL;
    const PNatList & list = natMethods->GetNATList();
    if (list.GetSize() > 0) {
      for (PINDEX i=0; i < list.GetSize(); i++) {
        PTRACE(6, "H323\tNAT Method " << i << " " << list[i].GetName() << " Ready: " << (list[i].IsAvailable(ip) ? "Yes" : "No"));   
        if (list[i].IsAvailable(ip)) {
             meth = &list[i];
             break;
        }
      }
    } else {
       PTRACE(6, "H323\tNo NAT Methods!");
    }
    return meth;
#else 
  return natMethods->GetMethod(ip);
#endif

}

PNatStrategy & H323EndPoint::GetNatMethods() 
{ 
	return *natMethods; 
}

void H323EndPoint::SetSTUNServer(const PString & server)
{
  natMethods->RemoveMethod("STUN");
  delete stun;

  if (server.IsEmpty())
    stun = NULL;
  else {
    stun = new PSTUNClient(server,
                           GetUDPPortBase(), GetUDPPortMax(),
                           GetRtpIpPortBase(), GetRtpIpPortMax());

    natMethods->AddMethod(stun);

    PTRACE(2, "H323\tSTUN server \"" << server << "\" replies " << stun->GetNatTypeName());
	
	STUNNatType((int)stun->GetNatType());
  }
}

#endif // P_STUN

void H323EndPoint::InternalTranslateTCPAddress(PIPSocket::Address & localAddr, const PIPSocket::Address & remoteAddr, 
											   const H323Connection * connection)
{
#ifdef P_STUN
  // if using STUN server, then translate internal local address to external if required
  PBoolean disableSTUN;
  if (connection != NULL)
    disableSTUN = !connection->HasNATSupport();
  else
    disableSTUN = disableSTUNTranslate;

  PIPSocket::Address addr;
  if (
       stun != NULL && !disableSTUN && (
        (stun->GetRTPSupport() == PSTUNClient::RTPSupported) ||
        (stun->GetRTPSupport() == PSTUNClient::RTPIfSendMedia) 
       ) && 
       localAddr.IsRFC1918() && 
       !remoteAddr.IsRFC1918() && 
       stun->GetExternalAddress(addr)
     )
  {
    localAddr = addr;
  }
  else
#endif // P_STUN
     TranslateTCPAddress(localAddr, remoteAddr);
}

PBoolean H323EndPoint::IsLocalAddress(const PIPSocket::Address & ip) const
{
  /* Check if the remote address is a private IP, broadcast, or us */
  return ip.IsRFC1918() || ip.IsBroadcast() || PIPSocket::IsLocalHost(ip);
}


void H323EndPoint::PortInfo::Set(unsigned newBase,
                                 unsigned newMax,
                                 unsigned range,
                                 unsigned dflt)
{
  if (newBase == 0) {
    newBase = dflt;
    newMax = dflt;
    if (dflt > 0)
      newMax += range;
  }
  else {
    if (newBase < 1024)
      newBase = 1024;
    else if (newBase > 65500)
      newBase = 65500;

    if (newMax <= newBase)
      newMax = newBase + range;
    if (newMax > 65535)
      newMax = 65535;
  }

  mutex.Wait();

  current = base = (WORD)newBase;
  max = (WORD)newMax;

  mutex.Signal();
}


WORD H323EndPoint::PortInfo::GetNext(unsigned increment)
{
  PWaitAndSignal m(mutex);

  if (current < base || current > (max-increment))
    current = base;

  if (current == 0)
    return 0;

  WORD p = current;
  current = (WORD)(current + increment);
  return p;
}


void H323EndPoint::SetTCPPorts(unsigned tcpBase, unsigned tcpMax)
{
  tcpPorts.Set(tcpBase, tcpMax, 99, 0);
}


WORD H323EndPoint::GetNextTCPPort()
{
  return tcpPorts.GetNext(1);
}


void H323EndPoint::SetUDPPorts(unsigned udpBase, unsigned udpMax)
{
  udpPorts.Set(udpBase, udpMax, 199, 0);

#ifdef P_STUN
    natMethods->SetPortRanges(GetUDPPortBase(), GetUDPPortMax(), GetRtpIpPortBase(), GetRtpIpPortMax());
#endif
}


WORD H323EndPoint::GetNextUDPPort()
{
  return udpPorts.GetNext(1);
}


void H323EndPoint::SetRtpIpPorts(unsigned rtpIpBase, unsigned rtpIpMax)
{
  rtpIpPorts.Set((rtpIpBase+1)&0xfffe, rtpIpMax&0xfffe, 999, 5000);

#ifdef P_STUN
  natMethods->SetPortRanges(GetUDPPortBase(), GetUDPPortMax(), GetRtpIpPortBase(), GetRtpIpPortMax());
#endif
}


WORD H323EndPoint::GetRtpIpPortPair()
{
  return rtpIpPorts.GetNext(2);
}

const PTimeInterval & H323EndPoint::GetNoMediaTimeout() const
{
  PWaitAndSignal m(noMediaMutex);
  
  return noMediaTimeout; 
}

PBoolean H323EndPoint::SetNoMediaTimeout(PTimeInterval newInterval) 
{
  PWaitAndSignal m(noMediaMutex);

  if (newInterval < 0)
    return FALSE;

  noMediaTimeout = newInterval; 
  return TRUE; 
}

PBoolean H323EndPoint::OnSendFeatureSet(unsigned pdu, H225_FeatureSet & feats, PBoolean advertise)
{
#ifdef H323_H460
    return features.SendFeature(pdu,feats,advertise);
#else
    return FALSE;
#endif
}

void H323EndPoint::OnReceiveFeatureSet(unsigned pdu, const H225_FeatureSet & feats)
{
#ifdef H323_H460
	features.ReceiveFeature(pdu,feats);
#endif
}

#ifdef H323_H460
H460_FeatureSet * H323EndPoint::GetGatekeeperFeatures()
{
	if (gatekeeper != NULL) {
		return &gatekeeper->GetFeatures();
	}

	return NULL;
}
#endif

void H323EndPoint::LoadBaseFeatureSet()
{

#ifdef H323_H460
  features.AttachEndPoint(this);
  features.LoadFeatureSet(H460_Feature::FeatureBase);
#endif

}

#ifdef H323_H46018
void H323EndPoint::H46018Enable(PBoolean enable) 
{ 
	m_h46018enabled = enable;
	if (enable) {
		// Must set reregistrations at between 15 and 45 sec
		// otherwise the Pinhole in NAT will close
		registrationTimeToLive = PTimeInterval(0, 30);  
	} else {
		// Set timer to whatever gk allocates...
		registrationTimeToLive = PTimeInterval();       
	}
}

PBoolean H323EndPoint::H46018IsEnabled() 
{ 
	return m_h46018enabled; 
}
#endif  // H323_H46018

#ifdef H323_H460P
void H323EndPoint::PresenceSetLocalState(const PStringList & alias, unsigned localstate, const PString & localdisplay)
{
	if (presenceHandler == NULL)
		presenceHandler = new H460PresenceHandler(*this);

	presenceHandler->SetPresenceState(alias,localstate, localdisplay);
}

void H323EndPoint::PresenceAddFeature(presenceFeature feat)
{
	if (presenceHandler == NULL)
		presenceHandler = new H460PresenceHandler(*this);

	presenceHandler->AddEndpointFeature(feat);
}

template <class PAIR>
class deletepair { // PAIR::second_type is a pointer type
public:
	void operator()(const PAIR & p) { delete p.second; }
};

template <class M>
inline void DeleteMap(const M & m)
{
	typedef typename M::value_type PAIR;
	std::for_each(m.begin(), m.end(), deletepair<PAIR>());
}

void H323EndPoint::PresenceAddFeatureH460()
{
	if (presenceHandler == NULL)
		presenceHandler = new H460PresenceHandler(*this);

	std::map<PString,H460_FeatureID*> plist;
	if (H460_Feature::PresenceFeatureList(plist,this)) {
		std::map<PString,H460_FeatureID*>::const_iterator it = plist.begin();
		while (it != plist.end()) {
			presenceHandler->AddEndpointH460Feature(*(it->second), it->first);
			it++;
		}
	}
	DeleteMap(plist);
}

void H323EndPoint::PresenceSetLocale(const presenceLocale & info)
{
	if (presenceHandler == NULL)
		presenceHandler = new H460PresenceHandler(*this);

	H460PresenceHandler::localeInfo & loc = presenceHandler->GetLocationInfo();

	loc.m_region = info.m_region;
	loc.m_country = info.m_country;
	loc.m_locale = info.m_locale;
	loc.m_countryCode = info.m_countryCode;
	loc.m_latitude = info.m_latitude;
	loc.m_longitude = info.m_longitude;
	loc.m_elevation = info.m_elevation;
}

void H323EndPoint::PresenceSetInstruction(const PString & epalias, unsigned type, const PStringList & list)
{
	if (presenceHandler == NULL)
		return;

	presenceHandler->AddInstruction(epalias,(H323PresenceHandler::InstType)type,list);
}

void H323EndPoint::PresenceSendAuthorization(const OpalGloballyUniqueID & id, const PString & epalias,PBoolean approved, const PStringList & list)
{
	if (presenceHandler == NULL)
		return;

	presenceHandler->AddAuthorization(id,epalias,approved,list);
}

void H323EndPoint::PresenceNotification(const PString & locAlias,
										const PString & subAlias,
										unsigned state, 
										const PString & display)
{
	PTRACE(4,"EP\tAlias " << subAlias << " PresenceState now " << state << " - "
		                  << H323PresenceNotification::GetStateString(state)
						  << " " << display);
}

void H323EndPoint::PresenceInstruction(const PString & locAlias, unsigned type, const PString & subAlias)
{
	PTRACE(4,"EP\tReceived Gatekeeper Instruction to " 
				<< H323PresenceInstruction::GetInstructionString(type) 
				<< " " << subAlias);
}

void H323EndPoint::PresenceAuthorization(const OpalGloballyUniqueID id,
									const PString & locAlias,
									const PStringList & Aliases)
{
	PTRACE(4,"EP\tReceived Presence Authorization " << id.AsString() << " from " << Aliases);
}
#endif  // H323_H460P

PBoolean H323EndPoint::OnFeatureInstance(int /*instType*/, const PString & /*identifer*/)
{
	return TRUE;
}

PBoolean H323EndPoint::HandleUnsolicitedInformation(const H323SignalPDU & )
{
  return FALSE;
}

#ifdef H323_RTP_AGGREGATE
PHandleAggregator * H323EndPoint::GetRTPAggregator()
{
  PWaitAndSignal m(connectionsMutex);
  if (rtpAggregationSize == 0)
    return NULL;

  if (rtpAggregator == NULL)
    rtpAggregator = new PHandleAggregator(rtpAggregationSize);

  return rtpAggregator;
}
#endif

#ifdef H323_SIGNAL_AGGREGATE
PHandleAggregator * H323EndPoint::GetSignallingAggregator()
{
  PWaitAndSignal m(connectionsMutex);
  if (signallingAggregationSize == 0)
    return NULL;

  if (signallingAggregator == NULL)
    signallingAggregator = new PHandleAggregator(signallingAggregationSize);

  return signallingAggregator;
}
#endif

#ifdef H323_GNUGK
void H323EndPoint::NATLostConnection(PBoolean lost)
{
	PTRACE(4,"GNUGK\tNAT Connection" << (lost ? "Lost" : " Re-established"));
	if (!lost)
		RegInvokeReRegistration();  
}
#endif

void H323EndPoint::RegInvokeReRegistration()
{
	 RegThread = PThread::Create(PCREATE_NOTIFIER(RegMethod), 0,
					PThread::AutoDeleteThread,
					PThread::NormalPriority,
					"regmeth:%x");
}

void H323EndPoint::RegMethod(PThread &, INT)
{
	PWaitAndSignal m(reregmutex);

	gatekeeper->ReRegisterNow();
}


