/*
 * main.cxx
 *
 * A simple H.323 "net telephone" application.
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
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
 * The Original Code is Portable Windows Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>
#include <ptlib/video.h>

#ifdef __GNUC__
#define H323_STATIC_LIB
#endif

#include "main.h"
#include "../../version.h"

#if defined(_MSC_VER) 
 #if PTLIB_VER > 280
   #define defVideoDriver "DirectShow"
 #else
   #define defVideoDriver "DirectShow2"
 #endif
#else
   #define defVideoDriver "*"
#endif

#define new PNEW

PCREATE_PROCESS(SimpleH323Process);


///////////////////////////////////////////////////////////////

SimpleH323Process::SimpleH323Process()
  : PProcess("H323Plus", "simple",
             MAJOR_VERSION, MINOR_VERSION, BUILD_TYPE, BUILD_NUMBER)
{
  endpoint = NULL;
}


SimpleH323Process::~SimpleH323Process()
{
  delete endpoint;
}


void SimpleH323Process::Main()
{
  cout << GetName()
       << " Version " << GetVersion(TRUE)
       << " by " << GetManufacturer()
       << " on " << GetOSClass() << ' ' << GetOSName()
       << " (" << GetOSVersion() << '-' << GetOSHardware() << ")\n\n";

  // Get and parse all of the command line arguments.
  PArgList & args = GetArguments();
  args.Parse(
             "a-auto-answer."
             "b-bandwidth:"
             "B-forward-busy:"
             "D-disable:"
             "e-silence."
             "f-fast-enable."
             "g-gatekeeper:"
             "h-help."
             "i-interface:"
             "j-jitter:"
             "l-listen."
             "n-no-gatekeeper."
#if PTRACING
             "o-output:"
#endif
             "-osptoken."
             "P-prefer:"
             "p-password:"
             "r-require-gatekeeper."
             "s-sound:"
             "-sound-in:"
             "-sound-out:"
             "-sound-buffers:"
#ifdef H323_VIDEO
             "v-video::"
#endif
             "T-h245tunneldisable."
             "-h245setupenable."
             "Q-h245qosdisable."
#ifdef H323_H235
             "m-mediaenc:"
             "-maxtoken:"
#endif
#ifdef H323_H46017
             "k-h46017:"
#endif
#ifdef H323_H46018
             "-h46018disable."
#endif
#ifdef H323_H46026
             "-h46026disable."
#endif
#ifdef H323_TLS
            "-tls."
            "-tls-cafile:"
            "-tls-cert:"
            "-tls-privkey:"
            "-tls-passphrase:"
#endif
#ifdef P_HAS_IPV6
             "-ipv6."
#endif
#if PTRACING
             "t-trace."
#endif
             "x-listenport:"
             "u-user:"
          , FALSE);


  if (args.HasOption('h') || (!args.HasOption('l') && args.GetCount() == 0)) {
    cout << "Usage : " << GetName() << " [options] -l\n"
            "      : " << GetName() << " [options] [alias@]hostname   (no gatekeeper)\n"
            "      : " << GetName() << " [options] alias[@hostname]   (with gatekeeper)\n"
            "Options:\n"
            "  -l --listen             : Listen for incoming calls.\n"
            "  -g --gatekeeper host    : Specify gatekeeper host.\n"
            "  -n --no-gatekeeper      : Disable gatekeeper discovery.\n"
            "  -r --require-gatekeeper : Exit if gatekeeper discovery fails.\n"
            "  -a --auto-answer        : Automatically answer incoming calls.\n"
            "  -u --user name          : Set local alias name(s) (defaults to login name).\n"
            "  -p --password pwd       : Set the H.235 password to use for calls.\n"
            "  -b --bandwidth bps      : Limit bandwidth usage to bps bits/second.\n"
            "  -j --jitter [min-]max   : Set minimum (optional) and maximum jitter buffer (in milliseconds).\n"
            "  -D --disable codec      : Disable the specified codec (may be used multiple times)\n"
            "  -P --prefer codec       : Prefer the specified codec (may be used multiple times)\n"
            "  -i --interface ipnum    : Select interface to bind to.\n"
            "  -B --forward-busy party : Forward to remote party if busy.\n"
            "  -e --silence            : Disable transmitter silence detection.\n"
            "  -f --fast-enable        : Enable fast start.\n"
            "  -T --h245tunneldisable  : Disable H245 tunnelling.\n"
            "     --h245setupenable    : Enable H245 in Setup.\n"
            "  -Q --h245qosdisable     : Disable H245 QoS Exchange.\n"
#ifdef H323_H235
            "  -m --mediaenc           : Enable Media encryption (value max cipher 128, 192 or 256).\n"
            "     --maxtoken           : Set max token size for H.235.6 (1024, 2048, 4096, ...).\n"
#endif
#ifdef H323_H46017
            "  -k --h46017             : Use H.460.17 Gatekeeper.\n"
#endif
#ifdef H323_H46018
            "     --h46018disable      : Disable H.460.18.\n"
#endif
#ifdef H323_H46026
            "     --h46026disable      : Disable H.460.26.\n"
#endif
#ifdef H323_TLS
            "     --tls                : TLS Enabled (must be set for TLS).\n"
            "     --tls-cafile         : TLS Certificate Authority File.\n"
            "     --tls-cert           : TLS Certificate File.\n"
            "     --tls-privkey        : TLS Private Key File.\n"
            "     --tls-passphrase     : TLS Private Key PassPhrase.\n"
#endif
#ifdef P_HAS_IPV6
            "     --ipv6               : Enable IPv6 Support.\n"
#endif
            "  -s --sound device       : Select sound input/output device.\n"
            "     --sound-in device    : Select sound input device.\n"
            "     --sound-out device   : Select sound output device.\n"
#ifdef H323_VIDEO
            "  -v --video device       : Select video input/output device.\n"
#endif
#if PTRACING 
            "  -t --trace              : Enable trace, use multiple times for more detail.\n"
            "  -o --output             : File for trace output, default is stderr.\n"
#endif
            "  -x --listenport         : Listening port (default 1720).\n"
            "  -h --help               : This help message.\n"
            << endl;
    return;
  }

#if PTRACING
  PTrace::Initialise(args.GetOptionCount('t'),
                     args.HasOption('o') ? (const char *)args.GetOptionString('o') : NULL,
                     PTrace::DateAndTime | PTrace::TraceLevel | PTrace::FileAndLine);
#endif

  // Create the H.323 endpoint and initialise it
  endpoint = new SimpleH323EndPoint;
  if (!endpoint->Initialise(args))
    return;

  // See if making a call or just listening.
  if (args.HasOption('l'))
    cout << "Waiting for incoming calls for \"" << endpoint->GetLocalUserName() << "\"\n";
  else {
    cout << "Initiating call to \"" << args[0] << "\"\n";
    endpoint->MakeCall(args[0], endpoint->currentCallToken);
  }
  cout << "Press X to exit." << endl;

  // Simplest possible user interface
  for (;;) {
    cout << "H323> " << flush;
    PCaselessString cmd;
    cin >> cmd;
    if (cmd == "X")
      break;

    PStringArray Cmd = cmd.Tokenise(" ",FALSE);
    if (Cmd[0] == "c") {
          if (!endpoint->currentCallToken.IsEmpty())
            cout << "Cannot make call whilst call in progress\n";
          else {
            PString str;
            str = Cmd[1].Trim();
            endpoint->MakeCall(str, endpoint->currentCallToken);
          }
    }
    else if (cmd.FindOneOf("HYN0123456789ABCDES") != P_MAX_INDEX) {
      H323Connection * connection = endpoint->FindConnectionWithLock(endpoint->currentCallToken);
      if (connection != NULL) {
        if (cmd == "H")
          connection->ClearCall();
        else if (cmd == "Y")
          connection->AnsweringCall(H323Connection::AnswerCallNow);
        else if (cmd == "N")
          connection->AnsweringCall(H323Connection::AnswerCallDenied);
#ifdef H323_H239
        else if (cmd == "S") {
          if (connection->OpenH239Channel())
              cout << "Application Session Open.." << endl;
          else
              cout << "Application Open Error: Remote may not support Feature!" << endl;
        } else if (cmd == "E") {
          if (connection->CloseH239Channel())
              cout << "Application Session Closed.." << endl;
        }
#endif
        else
          connection->SendUserInput(cmd);
        connection->Unlock();
      }
    }
  }

  cout << "Exiting " << GetName() << endl;
}


///////////////////////////////////////////////////////////////

SimpleH323EndPoint::SimpleH323EndPoint()
{
}


SimpleH323EndPoint::~SimpleH323EndPoint()
{
#ifdef H323_H235_AES256
      cout << "Removing Encryption Cache: ";
      EncryptionCacheRemove();
      cout << "ok.." << endl;
#endif
}


PBoolean SimpleH323EndPoint::Initialise(PArgList & args)
{

#if 0
  PDirectory DefaultDir = PProcess::Current().GetFile().GetDirectory();
  PPluginManager & pluginMgr = PPluginManager::GetPluginManager();
  pluginMgr.LoadPluginDirectory(DefaultDir);
#endif
  

  // Get local username, multiple uses of -u indicates additional aliases
  if (args.HasOption('u')) {
    PStringArray aliases = args.GetOptionString('u').Tokenise(',');
    SetLocalUserName(aliases[0]);
    for (PINDEX i = 1; i < aliases.GetSize(); i++)
      AddAliasName(aliases[i]);
  }

  // Load the base featureSet
  LoadBaseFeatureSet();

  // Set the various options  TODO Disable Silence until fix encryption
  //SetSilenceDetectionMode(args.HasOption('e') ? H323AudioCodec::NoSilenceDetection
  //                                            : H323AudioCodec::AdaptiveSilenceDetection);
  DisableFastStart(!args.HasOption('f'));
  DisableH245Tunneling(args.HasOption('T'));
  DisableH245QoS(args.HasOption('Q'));
  DisableH245inSetup(!args.HasOption("h245setupenable"));

  autoAnswer           = args.HasOption('a');
  busyForwardParty     = args.GetOptionString('B');

  if (args.HasOption('b')) {
    initialBandwidth = args.GetOptionString('b').AsUnsigned()*100;
    if (initialBandwidth == 0) {
      cerr << "Illegal bandwidth specified." << endl;
      return FALSE;
    }
  }

  if (args.HasOption('j')) {
    unsigned minJitter;
    unsigned maxJitter;
    PStringArray delays = args.GetOptionString('j').Tokenise(",-");
    if (delays.GetSize() < 2) {
      maxJitter = delays[0].AsUnsigned();
      minJitter = PMIN(GetMinAudioJitterDelay(), maxJitter);
    }
    else {
      minJitter = delays[0].AsUnsigned();
      maxJitter = delays[1].AsUnsigned();
    }
    if (minJitter >= 20 && minJitter <= maxJitter && maxJitter <= 1000)
      SetAudioJitterDelay(minJitter, maxJitter);
    else {
      cerr << "Jitter should be between 20 and 1000 milliseconds." << endl;
      return FALSE;
    }
  }

#ifdef P_HAS_IPV6
  if (args.HasOption("ipv6") && PIPSocket::IsIpAddressFamilyV6Supported()) {
    PIPSocket::SetDefaultIpAddressFamilyV6();
  }
#endif

  localLanguages.AppendString("en-us");
  
  if (!SetSoundDevice(args, "sound", PSoundChannel::Recorder))
    return FALSE;
  if (!SetSoundDevice(args, "sound", PSoundChannel::Player))
    return FALSE;
  if (!SetSoundDevice(args, "sound-in", PSoundChannel::Recorder))
    return FALSE;
  if (!SetSoundDevice(args, "sound-out", PSoundChannel::Player))
    return FALSE;

  PBoolean hasVideo = TRUE;
#ifdef H323_VIDEO
  videoDriver = defVideoDriver;
  if (args.HasOption("video")) {
      videoDriver = args.GetOptionString("video");
  }
  cout << "Using video driver " << videoDriver << endl;

  H323Capability::CapabilityFrameSize MaxVideoFrame = H323Capability::cifMPI;
  PString inputDriverName = videoDriver;

  PStringList devices = PVideoInputDevice::GetDriversDeviceNames(inputDriverName);
  if (devices.GetSize() == 0) {
      cout << "No Video Grabber available Disabling Video Support!" << endl;
      hasVideo = FALSE;
#if PTLIB_VER >= 2110
  } else {
    PVideoInputDevice::Capabilities caps;
    if (PVideoInputDevice::GetDeviceCapabilities(devices[0],inputDriverName,&caps)) {
      cout << "Video Device " << devices[0] << " capabilities." << endl;
      cout << "  Grabber capabilities." << endl;
      for (std::list<PVideoFrameInfo>::const_iterator r = caps.framesizes.begin(); r != caps.framesizes.end(); ++r) {
          cout << "        w: " << r->GetFrameWidth() << " h: " << r->GetFrameHeight() << " fmt: " 
               << r->GetColourFormat() << " fps: " << r->GetFrameRate() << endl;
          if ((r->GetFrameWidth() >= 1280) && (r->GetFrameHeight() >= 720)) {
              MaxVideoFrame = H323Capability::p720MPI;
          }
      }
    } else {
      cout << "InputDevice " << devices[0] << " capabilities not Available." << endl;
    }
#endif
 }

  if (MaxVideoFrame == H323Capability::p720MPI)
      cout << "High Definition Webcam detected." << endl << endl;
#else
  hasVideo = FALSE;
#endif

  // Set the default codecs available on sound cards.
  AddAllCapabilities(0, P_MAX_INDEX, "*");
  if (!hasVideo)
    RemoveCapability(H323Capability::e_Video);
#ifdef H323_VIDEO
  else 
    SetVideoFrameSize(MaxVideoFrame);
#endif

  AddAllUserInputCapabilities(0, P_MAX_INDEX);

  RemoveCapabilities(args.GetOptionString('D').Tokenise(','));
  ReorderCapabilities(args.GetOptionString('P').Tokenise(','));

  cout << "Local username: " << GetLocalUserName() << "\n"
    << "Silence compression is " << (GetSilenceDetectionMode() == H323AudioCodec::NoSilenceDetection ? "Dis" : "En") << "abled\n"
       << "Auto answer is " << autoAnswer << "\n"
       << "FastConnect is " << (IsFastStartDisabled() ? "Dis" : "En") << "abled\n"
       << "H245Tunnelling is " << (IsH245TunnelingDisabled() ? "Dis" : "En") << "abled\n"
       << "H245QoS is " << (IsH245QoSDisabled() ? "Dis" : "En") << "abled\n"
       << "Jitter buffer: "  << GetMinAudioJitterDelay() << '-' << GetMaxAudioJitterDelay() << " ms\n"
       << "Sound output device: \"" << GetSoundChannelPlayDevice() << "\"\n"
          "Sound  input device: \"" << GetSoundChannelRecordDevice() << "\"\n"
       <<  "Codecs (in preference order):\n" << setprecision(2) << GetCapabilities() << endl;

 ////////////////////////////////////////
#ifdef H323_H460
    // List all the available Features
    PStringList featurelist = H460_Feature::GetFeatureNames();
      cout << "Available Features: " << endl;
      for (PINDEX i = 0; i < featurelist.GetSize(); i++) {
          PStringList names = H460_Feature::GetFeatureFriendlyNames(featurelist[i]);
            for (PINDEX j = 0; j < names.GetSize(); j++)
                cout << featurelist[i] << "\t" << names[j] << endl;
      }
      cout << endl;

#ifdef H323_H46018
  if (args.HasOption("h46018disable")) {
      cout << "H.460.18 is Disabled" << endl << endl;
      H46018Enable(PFalse);
  }
#endif

#ifdef H323_H46026
  if (args.HasOption("h46026disable")) {
      cout << "H.460.26 is Disabled" << endl << endl;
      H46026Enable(PFalse);
  }
#endif

#ifdef H323_H460P
  PresenceAddFeature(e_preAudio);
  PresenceAddFeature(e_preVideo);
 
  PresenceAddFeatureH460();
#endif

#endif
/////////////////////////////////////////
// List all the available Features
      PStringArray natmethods = H323NatStrategy::GetRegisteredList();

      cout << "Available NAT Methods: " << endl;
      for (PINDEX i = 0; i < natmethods.GetSize(); i++) {
            cout << natmethods[i] << endl;
      }
      cout << endl;

////////////////////////////////////////

      PStringArray security = H235Authenticator::GetAuthenticatorList();
      cout << "Available Security: " << endl;
      for (PINDEX i = 0; i < security.GetSize(); i++) {
          cout << security[i] << endl;
#if PTLIB_VER >= 2110
          H235Authenticator::Capabilities caps;
          if (H235Authenticator::GetAuthenticatorCapabilities(security[i],&caps)) {
             for (list<H235Authenticator::Capability>::iterator j = 
                       caps.capabilityList.begin(); j != caps.capabilityList.end(); ++j) {
                 cout << "   " << j->m_identifier << " " << j->m_cipher << " " << j->m_description << endl;
              }
          }
#endif
      }
      cout << endl;

#ifdef H323_H235
      if (args.HasOption('m'))  {
        H235MediaCipher ncipher = encypt128;
#ifdef H323_H235_AES256
        unsigned maxtoken = 2048;
        unsigned cipher = args.GetOptionString('m').AsInteger();
        if (cipher >= encypt192) ncipher = encypt192;
        if (cipher >= encypt256) ncipher = encypt256;
        if (args.HasOption("maxtoken")) {
          maxtoken = args.GetOptionString("maxtoken").AsInteger();
        }
#else
        unsigned maxtoken = 1024;
#endif
        SetH235MediaEncryption(encyptRequest, ncipher, maxtoken);
#ifdef H323_H235_AES256
        if (ncipher > encypt128) {
            cout << "Enabled Media Encryption AES" << ncipher << " Loading...";
            EncryptionCacheInitialise();
            cout << "ok." << endl;
        } else
#endif
            cout << "Enabled Media Encryption AES" << ncipher << endl;
      }
#endif

////////////////////////////////////////

  // Start the listener thread for incoming calls.
  PString iface = args.GetOptionString('i');
  PString listenPort = args.GetOptionString('x');

  if (listenPort.IsEmpty())
    listenPort = "1720";

#if PTLIB_VER >= 2110
  if (iface.IsEmpty()) {
      PIPSocket::InterfaceTable interfaceTable;
      if (PIPSocket::GetInterfaceTable(interfaceTable)) {
          for (PINDEX j=0; j < interfaceTable.GetSize(); ++j) {
              if (interfaceTable[j].GetAddress().GetVersion() == ipVer && !interfaceTable[j].GetAddress().IsLoopback()) {
                 iface = interfaceTable[j].GetAddress().AsString();
                 break;
              }
          }
      }
  }
#endif

#ifdef H323_UPnP
   InitialiseUPnP();
#endif

  PIPSocket::Address interfaceAddress(iface);
  WORD interfacePort = (WORD)listenPort.AsInteger();

  H323ListenerTCP * listener = new H323ListenerTCP(*this, interfaceAddress, interfacePort);

  if (iface.IsEmpty())
    iface = "*";
  if (!StartListener(listener)) {
    cerr <<  "Could not open H.323 listener port on \"" << iface << '"' << endl;
    return FALSE;
  }

#ifdef H323_TLS   // Initialise TLS
    bool useTLS = args.HasOption("tls");
    if (useTLS) {
        DisableH245Tunneling(false);  // Tunneling must be used with TLS
        if (args.HasOption("tls-cafile"))
        useTLS = TLS_SetCAFile(args.GetOptionString("tls-cafile"));
        if (useTLS && args.HasOption("tls-cert"))
            useTLS = TLS_SetCertificate(args.GetOptionString("tls-cert"));
        if (useTLS && args.HasOption("tls-privkey")) {
            PString passphrase = PString();
            if (args.HasOption("tls-passphrase"))
                passphrase = args.GetOptionString("tls-passphrase");
            useTLS = TLS_SetPrivateKey(args.GetOptionString("tls-privkey"), passphrase);
        }

        if (useTLS && TLS_Initialise(interfaceAddress)) {
            cout << "Enabled TLS signal security." << endl;
        } else {
            cerr << "Could not enable TLS signal security." << endl;
        }
    }
#endif

  // Initialise the security info
  if (args.HasOption('p')) {
    SetGatekeeperPassword(args.GetOptionString('p'));
    cout << "Enabling H.235 security access to gatekeeper." << endl;
  }

#ifdef H323_H46017
    if (args.HasOption('k')) {
       PString gk17 = args.GetOptionString('k');
       if (H46017CreateConnection(gk17, false)) {
           cout << "Using H.460.17 Gatekeeper Tunneling." << endl;
           SetInitialBandwidth(384000);
           return true;
       }
       cout << "H.460.17 Gatekeeper Tunneling Failed." << endl;
       return false;
    }
#endif

  // Establish link with gatekeeper if required.
  if (args.HasOption('g') || !args.HasOption('n')) {
    H323TransportUDP * rasChannel;
    if (args.GetOptionString('i').IsEmpty())
      rasChannel  = new H323TransportUDP(*this);
    else {
      PIPSocket::Address interfaceAddress(args.GetOptionString('i'));
      rasChannel  = new H323TransportUDP(*this, interfaceAddress);
    }

    if (args.HasOption('g')) {
      PString gkName = args.GetOptionString('g');
      if (SetGatekeeper(gkName, rasChannel))
        cout << "Gatekeeper set: " << *gatekeeper << endl;
      else {
        cerr << "Error registering with gatekeeper at \"" << gkName << '"' << endl;
        return FALSE;
      }
    }
    else {
      cout << "Searching for gatekeeper..." << flush;
      if (DiscoverGatekeeper(rasChannel))
        cout << "\nGatekeeper found: " << *gatekeeper << endl;
      else {
        cerr << "\nNo gatekeeper found." << endl;
        if (args.HasOption('r')) 
          return FALSE;
      }
    }
  }

  if (args.HasOption("sound-buffers")) {
    soundChannelBuffers = args.GetOptionString("sound-buffers", "3").AsUnsigned();
    if (soundChannelBuffers < 2 || soundChannelBuffers > 99) {
      cout << "Illegal sound buffers specified." << endl;
      return FALSE;
    }

  }

  return TRUE;
}


PBoolean SimpleH323EndPoint::SetSoundDevice(PArgList & args,
                                        const char * optionName,
                                        PSoundChannel::Directions dir)
{
  if (!args.HasOption(optionName))
    return TRUE;

  PString dev = args.GetOptionString(optionName);

  if (dir == PSoundChannel::Player) {
    if (SetSoundChannelPlayDevice(dev))
      return TRUE;
  }
  else {
    if (SetSoundChannelRecordDevice(dev))
      return TRUE;
  }

  cerr << "Device for " << optionName << " (\"" << dev << "\") must be one of:\n";

  PStringArray names = PSoundChannel::GetDeviceNames(dir);
  for (PINDEX i = 0; i < names.GetSize(); i++)
    cerr << "  \"" << names[i] << "\"\n";

  return FALSE;
}


H323Connection * SimpleH323EndPoint::CreateConnection(unsigned callReference)
{
  return new SimpleH323Connection(*this, callReference);
}


PBoolean SimpleH323EndPoint::OnIncomingCall(H323Connection & connection,
                                        const H323SignalPDU &,
                                        H323SignalPDU &)
{
  if (currentCallToken.IsEmpty())
    return TRUE;

  if (busyForwardParty.IsEmpty()) {
    cout << "Incoming call from \"" << connection.GetRemotePartyName() << "\" rejected, line busy!" << endl;
    return FALSE;
  }

  cout << "Forwarding call to \"" << busyForwardParty << "\"." << endl;
  return !connection.ForwardCall(busyForwardParty);
}


H323Connection::AnswerCallResponse
                   SimpleH323EndPoint::OnAnswerCall(H323Connection & connection,
                                                    const PString & caller,
                                                    const H323SignalPDU &,
                                                    H323SignalPDU &)
{
  currentCallToken = connection.GetCallToken();

  if (autoAnswer) {
    cout << "Automatically accepting call." << endl;
    return H323Connection::AnswerCallNow;
  }

  cout << "Incoming call from \""
       << caller
       << "\", answer call (Y/n)? "
       << flush;

  return H323Connection::AnswerCallPending;
}


PBoolean SimpleH323EndPoint::OnConnectionForwarded(H323Connection & /*connection*/,
                                               const PString & forwardParty,
                                               const H323SignalPDU & /*pdu*/)
{
  if (MakeCall(forwardParty, currentCallToken)) {
    cout << "Call is being forwarded to host " << forwardParty << endl;
    return TRUE;
  }

  cout << "Error forwarding call to \"" << forwardParty << '"' << endl;
  return FALSE;
}


void SimpleH323EndPoint::OnConnectionEstablished(H323Connection & connection,
                                                 const PString & token)
{
  currentCallToken = token;
  cout << "In call with " << connection.GetRemotePartyName() << endl;
}


void SimpleH323EndPoint::OnConnectionCleared(H323Connection & connection,
                                             const PString & clearedCallToken)
{
  if (currentCallToken == clearedCallToken)
    currentCallToken = PString();

  PString remoteName = '"' + connection.GetRemotePartyName() + '"';
  switch (connection.GetCallEndReason()) {
    case H323Connection::EndedByRemoteUser :
      cout << remoteName << " has cleared the call";
      break;
    case H323Connection::EndedByCallerAbort :
      cout << remoteName << " has stopped calling";
      break;
    case H323Connection::EndedByRefusal :
      cout << remoteName << " did not accept your call";
      break;
    case H323Connection::EndedByNoAnswer :
      cout << remoteName << " did not answer your call";
      break;
    case H323Connection::EndedByTransportFail :
      cout << "Call with " << remoteName << " ended abnormally";
      break;
    case H323Connection::EndedByCapabilityExchange :
      cout << "Could not find common codec with " << remoteName;
      break;
    case H323Connection::EndedByNoAccept :
      cout << "Did not accept incoming call from " << remoteName;
      break;
    case H323Connection::EndedByAnswerDenied :
      cout << "Refused incoming call from " << remoteName;
      break;
    case H323Connection::EndedByNoUser :
      cout << "Gatekeeper could not find user " << remoteName;
      break;
    case H323Connection::EndedByNoBandwidth :
      cout << "Call to " << remoteName << " aborted, insufficient bandwidth.";
      break;
    case H323Connection::EndedByUnreachable :
      cout << remoteName << " could not be reached.";
      break;
    case H323Connection::EndedByHostOffline :
      cout << remoteName << " is not online.";
      break;
    case H323Connection::EndedByNoEndPoint :
      cout << "No phone running for " << remoteName;
      break;
    case H323Connection::EndedByConnectFail :
      cout << "Transport error calling " << remoteName;
      break;
    default :
      cout << "Call with " << remoteName << " completed";
  }
  PTime connectTime = connection.GetConnectionStartTime();
  if (connectTime.GetTimeInSeconds() != 0)
    cout << ", duration "
         << setprecision(0) << setw(5)
         << (PTime() - connectTime);

  cout << endl;
}


PBoolean SimpleH323EndPoint::OpenAudioChannel(H323Connection & connection,
                                          PBoolean isEncoding,
                                          unsigned bufferSize,
                                          H323AudioCodec & codec)
{
  if (H323EndPoint::OpenAudioChannel(connection, isEncoding, bufferSize, codec))
    return TRUE;

  cerr << "Could not open sound device ";
  if (isEncoding)
    cerr << GetSoundChannelRecordDevice();
  else
    cerr << GetSoundChannelPlayDevice();
  cerr << " - Check permissions or full duplex capability." << endl;

  return FALSE;
}

#ifdef H323_VIDEO
PBoolean SimpleH323EndPoint::OpenVideoChannel(H323Connection & /*connection*/,
                                    PBoolean isEncoding,
                                    H323VideoCodec & codec)
{


  PString deviceDriver = videoDriver;
  PStringList devices = isEncoding ? PVideoInputDevice::GetDriversDeviceNames(deviceDriver)
                                  : PVideoOutputDevice::GetDriversDeviceNames("*");
  // Look for a useful device
  PString deviceName;
  for (PINDEX i = 0; i < devices.GetSize(); i++) {
    PTRACE(4, devices[i]);
    PCaselessString devName = devices[i];
	if (devName != "*.yuv" && devName != "fake" && devName != "NULL") {
      deviceName = devName;
      break;
    }
  }
  if (deviceName.IsEmpty())
    deviceName = isEncoding ? "fake" : "NULL";

  PVideoDevice * device = isEncoding ? (PVideoDevice *)PVideoInputDevice::CreateDeviceByName(deviceName)
                                     : (PVideoDevice *)PVideoOutputDevice::CreateDeviceByName(deviceName);

  // codec needs a list of possible formats, otherwise the frame size isn't negotiated properly
#if PTLIB_VER >= 2110
  if (isEncoding) {
      PVideoInputDevice::Capabilities videoCaps;
      if (((PVideoInputDevice *)device)->GetDeviceCapabilities(deviceName,deviceDriver,&videoCaps)) {
          codec.SetSupportedFormats(videoCaps.framesizes);
      } else {
        // set fixed list of resolutions for drivers that don't provide a list
        PVideoInputDevice::Capabilities caps;
        PVideoFrameInfo cap;
        cap.SetColourFormat("YUV420P");
        cap.SetFrameRate(30);
        // sizes must be from largest to smallest
        cap.SetFrameSize(1280, 720);
        caps.framesizes.push_back(cap);
        cap.SetFrameSize(704, 576);
        caps.framesizes.push_back(cap);
        cap.SetFrameSize(352, 288);
        caps.framesizes.push_back(cap);
        codec.SetSupportedFormats(caps.framesizes);
      }
  }
#else
  if (isEncoding) {
    PVideoInputDevice::Capabilities caps;
    PVideoFrameInfo cap;
    cap.SetColourFormat("YUV420P");
    cap.SetFrameRate(30);
    // sizes must be from largest to smallest
    cap.SetFrameSize(1280, 720);
    caps.framesizes.push_back(cap);
    cap.SetFrameSize(704, 576);
    caps.framesizes.push_back(cap);
    cap.SetFrameSize(640, 400);
    caps.framesizes.push_back(cap);
    cap.SetFrameSize(352, 288);
    caps.framesizes.push_back(cap);
    codec.SetSupportedFormats(caps.framesizes);
  }
#endif

  if (!device->Open(deviceName, TRUE)) {
    PTRACE(1, "Failed to open the video device \"" << deviceName << '"');
    return FALSE;
  }

  if (!device->SetFrameSize(codec.GetWidth(), codec.GetHeight()) ||
      !device->SetFrameRate(codec.GetFrameRate()) ||
      !device->SetColourFormatConverter("YUV420P")) {
    PTRACE(1, "Failed to configure the video device \"" << deviceName << '"');
    return FALSE;
  }

  PVideoChannel * channel = new PVideoChannel;

  if (isEncoding)
    channel->AttachVideoReader((PVideoInputDevice *)device);
  else
    channel->AttachVideoPlayer((PVideoOutputDevice *)device);

  return codec.AttachChannel(channel,TRUE);
}

#ifdef H323_H239
PBoolean SimpleH323EndPoint::OpenExtendedVideoChannel(H323Connection & connection,
                                            PBoolean PTRACE_PARAM(isEncoding),
                                            H323VideoCodec & codec)
{

#ifdef P_APPSHARE
  PString deviceDriver = "Application";
#else
  PString deviceDriver = "*";
#endif
  PStringList devices = isEncoding ? PVideoInputDevice::GetDriversDeviceNames(deviceDriver)
                                  : PVideoOutputDevice::GetDriversDeviceNames("*");

  // Look for a useful device
  PString deviceName;
  for (PINDEX i = 0; i < devices.GetSize(); i++) {
    PTRACE(4, devices[i]);
    PCaselessString devName = devices[i];
    if (devName != "*.yuv" && devName != "NULL") {
      deviceName = devName;
      break;
    }
  }
  if (deviceName.IsEmpty())
    deviceName = isEncoding ? "fake" : "NULL";

  PVideoDevice * device = isEncoding ? (PVideoDevice *)PVideoInputDevice::CreateOpenedDevice(deviceDriver,deviceName)
                                     : (PVideoDevice *)PVideoOutputDevice::CreateOpenedDevice(deviceDriver,deviceName);

  if (isEncoding) {
      PVideoInputDevice::Capabilities videoCaps;
      if (((PVideoInputDevice *)device)->GetDeviceCapabilities(deviceName,deviceDriver,&videoCaps))
          codec.SetSupportedFormats(videoCaps.framesizes);
  }

  if (!device->SetColourFormatConverter("YUV420P") ||
      !device->SetFrameSizeConverter(codec.GetWidth(), codec.GetHeight(),PVideoFrameInfo::eScale)) {
    PTRACE(1, "Failed to open or configure the video device \"" << deviceName << '"');
    return FALSE;
  }

  PVideoChannel * channel = new PVideoChannel;

  if (isEncoding)
    channel->AttachVideoReader((PVideoInputDevice *)device);
  else
    channel->AttachVideoPlayer((PVideoOutputDevice *)device);

  return codec.AttachChannel(channel,TRUE);
}
#endif // H323_H239 
#endif // H323_VIDEO

#ifdef H323_UPnP
PBoolean SimpleH323EndPoint::OnUPnPAvailable(const PString & device, const PIPSocket::Address & publicIP, PNatMethod_UPnP * /*nat*/)
{
    cout << "UPnP Device " << device << " Public IP: " << publicIP << endl;
    // Use the PNatMethod pointer to create a TCP listener.
    return true;
}
#endif

#ifdef H323_H460P
void SimpleH323EndPoint::PresenceInstruction(const PString & locAlias,
                                    unsigned type, 
                                    const PString & subAlias,
                                    const PString & subDisplay)
{
    cout << "Inst " << locAlias << " type:" << type << " " << subAlias << " " << subDisplay << endl;
}
#endif

#ifdef H323_H235
void SimpleH323EndPoint::OnMediaEncryption(unsigned session, H323Channel::Directions dir, const PString & cipher) 
{
    cout << "Media Encryption " << session << " " << dir << " " << cipher << endl;
}
#endif

#ifdef H323_TLS
void SimpleH323EndPoint::OnSecureSignallingChannel(bool isSecured) 
{
	// at this point an endpoint could refuse a call with non-secured signalling connection
    cout << "TLS " << (isSecured ? "" : "NOT") << " enabled for call." << endl;
}
#endif
///////////////////////////////////////////////////////////////

SimpleH323Connection::SimpleH323Connection(SimpleH323EndPoint & ep, unsigned ref)
  : H323Connection(ep, ref)
{
#ifdef H323_H4609
    H4609EnableStats();
#endif
}


PBoolean SimpleH323Connection::OnStartLogicalChannel(H323Channel & channel)
{
  if (!H323Connection::OnStartLogicalChannel(channel))
    return FALSE;

  cout << "Started logical channel: ";

  switch (channel.GetDirection()) {
    case H323Channel::IsTransmitter :
      cout << "sending ";
      break;

    case H323Channel::IsReceiver :
      cout << "receiving ";
      break;

    default :
      break;
  }

  cout << channel.GetCapability() << endl;  

  return TRUE;
}



void SimpleH323Connection::OnUserInputString(const PString & value)
{
  cout << "User input received: \"" << value << '"' << endl;
}


// End of File ///////////////////////////////////////////////////////////////

