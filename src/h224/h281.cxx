/*
 * h281.h
 *
 * H.281 implementation for the OpenH323 Project.
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
 * Revision 1.4  2011/02/20 07:03:00  shorne
 * Improving H.224 stability by ensuring correct initialization and removing nested mutexes.
 *
 * Revision 1.3  2011/01/12 12:51:52  shorne
 * H.224 bi-directional support added
 *
 * Revision 1.2  2008/05/23 11:21:52  willamowius
 * switch BOOL to PBoolean to be able to compile with Ptlib 2.2.x
 *
 * Revision 1.1  2007/08/06 20:51:05  shorne
 * First commit of h323plus
 *
 * Revision 1.1  2006/06/22 11:07:23  shorne
 * Backport of FECC (H.224) from Opal
 *
 * Revision 1.3  2006/05/01 10:29:50  csoutheren
 * Added pragams for gcc < 4
 *
 * Revision 1.2  2006/04/24 12:53:50  rjongbloed
 * Port of H.224 Far End Camera Control to DevStudio/Windows
 *
 * Revision 1.1  2006/04/20 16:48:17  hfriederich
 * Initial version of H.224/H.281 implementation.
 *
 */

#include <ptlib.h>
#include <openh323buildopts.h>

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#endif

#ifdef __GNUC__
#pragma implementation "h281.h"
#pragma implementation "h281handler.h"
#endif

#ifdef H224_H281

#include <h224/h224.h>

#define MAX_H281_DATA_SIZE 

H281_Frame::H281_Frame()
: H224_Frame(3)
{
  SetHighPriority(TRUE);
	
  BYTE *data = GetClientDataPtr();
	
  // Setting RequestType to StartAction
  SetRequestType(StartAction);
	
  // Setting Pan / Tilt / Zoom and Focus Off
  // Setting timeout to zero
  data[1] = 0x00;
  data[2] = 0x00;
}

H281_Frame::~H281_Frame()
{
}

void H281_Frame::SetRequestType(RequestType requestType)
{
  BYTE *data = GetClientDataPtr();
	
  data[0] = (BYTE)requestType;
	
  switch(requestType) {
	
    case StartAction:
      SetClientDataSize(3);
      break;
    default:
      SetClientDataSize(2);
      break;
  }
}

H281_Frame::PanDirection H281_Frame::GetPanDirection() const
{
  RequestType requestType = GetRequestType();
	
  if(requestType != StartAction &&
	 requestType != ContinueAction &&
	 requestType != StopAction)
  {
	// not valid
	return IllegalPan;
  }
	
  BYTE *data = GetClientDataPtr();
	
  return (PanDirection)(data[1] & 0xc0);
}

void H281_Frame::SetPanDirection(PanDirection direction)
{
  RequestType requestType = GetRequestType();
	
  if(requestType != StartAction &&
     requestType != ContinueAction &&
	 requestType != StopAction)
  {
	// not valid
	return;
  }
	
  BYTE *data = GetClientDataPtr();
	
  data[1] &= 0x3f;
  data[1] |= (direction & 0xc0);
}

H281_Frame::TiltDirection H281_Frame::GetTiltDirection() const
{
  RequestType requestType = GetRequestType();
	
  if(requestType != StartAction &&
	 requestType != ContinueAction &&
	 requestType != StopAction)
  {
	// not valid
    return IllegalTilt;
  }

  BYTE *data = GetClientDataPtr();
	
  return (TiltDirection)(data[1] & 0x30);
}

void H281_Frame::SetTiltDirection(TiltDirection direction)
{
  RequestType requestType = GetRequestType();
	
  if(requestType != StartAction &&
	 requestType != ContinueAction &&
	 requestType != StopAction)
  {
	// not valid
	return;
  }
	
  BYTE *data = GetClientDataPtr();
	
  data[1] &= 0xcf;
  data[1] |= (direction & 0x30);
}

H281_Frame::ZoomDirection H281_Frame::GetZoomDirection() const
{
  RequestType requestType = GetRequestType();
	
  if(requestType != StartAction &&
	 requestType != ContinueAction &&
     requestType != StopAction)
  {
	// not valid
	return IllegalZoom;
  }
	
  BYTE *data = GetClientDataPtr();
	
  return (ZoomDirection)(data[1] & 0x0c);
}

void H281_Frame::SetZoomDirection(ZoomDirection direction)
{
  RequestType requestType = GetRequestType();
	
  if(requestType != StartAction &&
	 requestType != ContinueAction &&
	 requestType != StopAction)
  {
	// not valid
	return;
  }
	
  BYTE *data = GetClientDataPtr();
	
  data[1] &= 0xf3;
  data[1] |= (direction & 0x0c);
}

H281_Frame::FocusDirection H281_Frame::GetFocusDirection() const
{
  RequestType requestType = GetRequestType();
	
  if(requestType != StartAction &&
	 requestType != ContinueAction &&
     requestType != StopAction)
  {
	// not valid
	return IllegalFocus;
  }
	
  BYTE *data = GetClientDataPtr();

  return (FocusDirection)(data[1] & 0x03);
}

void H281_Frame::SetFocusDirection(FocusDirection direction)
{
  RequestType requestType = GetRequestType();
	
  if(requestType != StartAction &&
	 requestType != ContinueAction &&
	 requestType != StopAction)
  {
	// not valid
	return;
  }
	
  BYTE *data = GetClientDataPtr();

  data[1] &= 0xfc;
  data[1] |= (direction & 0x03);
}

BYTE H281_Frame::GetTimeout() const
{
  RequestType requestType = GetRequestType();
	
  if(requestType != StartAction) {
	return 0x00;
  }
	
  BYTE *data = GetClientDataPtr();
	
  return (data[2] & 0x0f);
}

void H281_Frame::SetTimeout(BYTE timeout)
{
  RequestType requestType = GetRequestType();

  if(requestType != StartAction) {
	return;
  }
	
  BYTE *data = GetClientDataPtr();
	
  data[2] = (timeout & 0x0f);
}

BYTE H281_Frame::GetVideoSourceNumber() const
{
  RequestType requestType = GetRequestType();

  if(requestType != SelectVideoSource &&
	 requestType != VideoSourceSwitched)
  {
    return 0x00;
  }
	
  BYTE *data = GetClientDataPtr();
	
  return (data[1] >> 4) & 0x0f;
}

void H281_Frame::SetVideoSourceNumber(BYTE videoSourceNumber)
{
  RequestType requestType = GetRequestType();

  if(requestType != SelectVideoSource &&
	 requestType != VideoSourceSwitched)
  {
    return;
  }
	
  BYTE *data = GetClientDataPtr();
	
  data[1] &= 0x0f;
  data[1] |= (videoSourceNumber << 4) & 0xf0;
}

H281_Frame::VideoMode H281_Frame::GetVideoMode() const
{  
  RequestType requestType = GetRequestType();

  if(requestType != SelectVideoSource &&
	 requestType != VideoSourceSwitched)
  {
    return IllegalVideoMode;
  }
	
  BYTE *data = GetClientDataPtr();
	
  return (VideoMode)(data[1] & 0x03);
}

void H281_Frame::SetVideoMode(VideoMode mode)
{
  RequestType requestType = GetRequestType();

  if(requestType != SelectVideoSource &&
	 requestType != VideoSourceSwitched)
  {
    return;
  }
	
  BYTE *data = GetClientDataPtr();
	
  data[1] &= 0xfc;
  data[1] |= (mode & 0x03);
}

BYTE H281_Frame::GetPresetNumber() const
{
  RequestType requestType = GetRequestType();

  if(requestType != StoreAsPreset &&
	 requestType != ActivatePreset)
  {
    return 0x00;
  }
	
  BYTE *data = GetClientDataPtr();
	
  return (data[1] >> 4) & 0x0f;
}

void H281_Frame::SetPresetNumber(BYTE presetNumber)
{
  RequestType requestType = GetRequestType();

  if(requestType != StoreAsPreset &&
	 requestType != ActivatePreset)
  {
    return;
  }
	
  BYTE *data = GetClientDataPtr();
	
  data[1] &= 0x0f;
  data[1] |= (presetNumber << 4) & 0xf0;
}

/////////////////////////////////////

H281VideoSource::H281VideoSource()
{
  // disabled camera with no options
  isEnabled = FALSE;
  firstOctet = 0x00;
  secondOctet = 0x00;
}

H281VideoSource::~H281VideoSource()
{
}

void H281VideoSource::SetVideoSourceNumber(BYTE number)
{
  // only accepting the default camera types
  if(number > 5)	{
    return;
  }
	
  firstOctet &= 0x0f;
  firstOctet |= (number << 4) & 0xf0;
}

void H281VideoSource::SetCanMotionVideo(PBoolean flag)
{
  if(flag) {
    firstOctet |= 0x04;
  } else {
    firstOctet &= 0xfb;
  }
}

void H281VideoSource::SetCanNormalResolutionStillImage(PBoolean flag)
{
  if(flag) {
    firstOctet |= 0x02;
  }	else {
    firstOctet &= 0xfd;
  }
}

void H281VideoSource::SetCanDoubleResolutionStillImage(PBoolean flag)
{
  if(flag) {
    firstOctet |= 0x01;
  } else {
    firstOctet &= 0xfe;
  }
}

void H281VideoSource::SetCanPan(PBoolean flag)
{
  if(flag) {
    secondOctet |= 0x80;
  }	else {
    secondOctet &= 0x7f;
  }
}

void H281VideoSource::SetCanTilt(PBoolean flag)
{
  if(flag) {
    secondOctet |= 0x40;
  }	else {
    secondOctet &= 0xbf;
  }
}

void H281VideoSource::SetCanZoom(PBoolean flag)
{
  if(flag) {
    secondOctet |= 0x20;
  } else {
    secondOctet &= 0xdf;
  }
}

void H281VideoSource::SetCanFocus(PBoolean flag)
{
  if(flag) {
    secondOctet |= 0x10;
  }	else {
    secondOctet &= 0xef;
  }
}

void H281VideoSource::Encode(BYTE *data) const
{
  data[0] = firstOctet;
  data[1] = secondOctet;
}

PBoolean H281VideoSource::Decode(const BYTE *data)
{	
  // only accepting the standard video sources
  BYTE videoSourceNumber = (data[0] >> 4) & 0x0f;
  if(videoSourceNumber > 5)	{
    return FALSE;
  }
		
  firstOctet = data[0];
  secondOctet = data[1];
	
  return TRUE;
}

////////////////////////////////
// Must Declare for Factory Loader.
H224_HANDLER(H281);
///////////////////////////////

H224_H281Handler::H224_H281Handler()
: H224_Handler("H.281")
{
	Initialise();
}

H224_H281Handler::H224_H281Handler(OpalH224Handler & theH224Handler)
: H224_Handler("H.281")
{
    AttachH224Handler(&theH224Handler);
	Initialise();
}

void H224_H281Handler::Initialise()
{
  remoteHasH281 = FALSE;
  localNumberOfPresets = 0;
  remoteNumberOfPresets = 0;
  shutDown = false;
	
  // set correct video source numbers
  for(BYTE srcnum = 0; srcnum < 6; srcnum++) {	  
    localVideoSources[srcnum].SetVideoSourceNumber(srcnum);
    remoteVideoSources[srcnum].SetVideoSourceNumber(srcnum);
  }
	
  // initiate the local cameras so that the main camera is enabled
  // and provides motion video (nothing more)
  localVideoSources[MainCamera].SetEnabled(TRUE);
  localVideoSources[MainCamera].SetCanMotionVideo(TRUE); 
  //localVideoSources[MainCamera].SetCanPan(TRUE);
  //localVideoSources[MainCamera].SetCanTilt(TRUE);
  //localVideoSources[MainCamera].SetCanZoom(TRUE);
	
  transmitFrame.SetRequestType(H281_Frame::IllegalRequest);
  transmitFrame.SetBS(TRUE);
  transmitFrame.SetES(TRUE);
	
  transmitTimer.SetNotifier(PCREATE_NOTIFIER(ContinueAction));
	
  requestedPanDirection = H281_Frame::NoPan;
  requestedTiltDirection = H281_Frame::NoTilt;
  requestedZoomDirection = H281_Frame::NoZoom;
  requestedFocusDirection = H281_Frame::NoFocus;
  receiveTimer.SetNotifier(PCREATE_NOTIFIER(StopActionLocally));
}

H224_H281Handler::~H224_H281Handler()
{

  shutDown = true;
  transmitTimer.Stop();
  receiveTimer.Stop();
}

H281VideoSource & H224_H281Handler::GetLocalVideoSource(VideoSource source)
{
  return localVideoSources[source];
}

H281VideoSource & H224_H281Handler::GetRemoteVideoSource(VideoSource source)
{
  return remoteVideoSources[source];
}

void H224_H281Handler::StartAction(H281_Frame::PanDirection panDirection,
                                  H281_Frame::TiltDirection tiltDirection,
                                  H281_Frame::ZoomDirection zoomDirection,
                                  H281_Frame::FocusDirection focusDirection)
{

  if(transmitFrame.GetRequestType() != H281_Frame::IllegalRequest) {

    if (transmitFrame.GetPanDirection() == panDirection &&
        transmitFrame.GetTiltDirection() == tiltDirection &&
        transmitFrame.GetZoomDirection() == zoomDirection &&
        transmitFrame.GetFocusDirection() == focusDirection)
    {
      // same request as before, simply continuing
      return;
    }

    StopAction();
  }

  transmitFrame.SetRequestType(H281_Frame::StartAction);
  transmitFrame.SetPanDirection(panDirection);
  transmitFrame.SetTiltDirection(tiltDirection);
  transmitFrame.SetZoomDirection(zoomDirection);
  transmitFrame.SetFocusDirection(focusDirection);
  transmitFrame.SetTimeout(0); //800msec

  m_h224Handler->TransmitClientFrame(H281_CLIENT_ID, transmitFrame);

  // send a ContinueAction every 400msec
  transmitTimer.RunContinuous(400);
}

void H224_H281Handler::StopAction()
{

  transmitFrame.SetRequestType(H281_Frame::StopAction);
	
  m_h224Handler->TransmitClientFrame(H281_CLIENT_ID, transmitFrame);
	
  transmitFrame.SetRequestType(H281_Frame::IllegalRequest);
  transmitTimer.Stop();
}

void H224_H281Handler::SelectVideoSource(BYTE videoSourceNumber, H281_Frame::VideoMode videoMode)
{

  if(transmitFrame.GetRequestType() != H281_Frame::IllegalRequest) {
    StopAction();
  }
	
  transmitFrame.SetRequestType(H281_Frame::SelectVideoSource);
  transmitFrame.SetVideoSourceNumber(videoSourceNumber);
  transmitFrame.SetVideoMode(videoMode);
	
  m_h224Handler->TransmitClientFrame(H281_CLIENT_ID, transmitFrame);
	
  transmitFrame.SetRequestType(H281_Frame::IllegalRequest);
}

void H224_H281Handler::StoreAsPreset(BYTE presetNumber)
{
	
  if(transmitFrame.GetRequestType() != H281_Frame::IllegalRequest) {
    StopAction();
  }
	
  transmitFrame.SetRequestType(H281_Frame::StoreAsPreset);
  transmitFrame.SetPresetNumber(presetNumber);
  
  m_h224Handler->TransmitClientFrame(H281_CLIENT_ID, transmitFrame);
	
  transmitFrame.SetRequestType(H281_Frame::IllegalRequest);
}

void H224_H281Handler::ActivatePreset(BYTE presetNumber)
{
	
  if(transmitFrame.GetRequestType() != H281_Frame::IllegalRequest) {
    StopAction();
  }
	
  transmitFrame.SetRequestType(H281_Frame::ActivatePreset);
  transmitFrame.SetPresetNumber(presetNumber);
	
  m_h224Handler->TransmitClientFrame(H281_CLIENT_ID, transmitFrame);
	
  transmitFrame.SetRequestType(H281_Frame::IllegalRequest);
}

void H224_H281Handler::SendExtraCapabilities() const
{
  BYTE capabilities[11];
	
  // The default implementation has no presets
  capabilities[0] = 0x00;
	
  PINDEX size = 1;
	
  for(PINDEX i = 1; i < 6; i++) {
		
	if(localVideoSources[i].IsEnabled()) {
      localVideoSources[i].Encode(capabilities+size);
      size += 2;
    }
  }
	
  m_h224Handler->SendExtraCapabilitiesMessage(H281_CLIENT_ID, capabilities, size);
}

void H224_H281Handler::SetRemoteSupport()
{
	remoteHasH281 = TRUE;
}

void H224_H281Handler::OnReceivedExtraCapabilities(const BYTE *capabilities, PINDEX size)
{
  remoteHasH281 = TRUE;
	
  remoteNumberOfPresets = (capabilities[0] & 0x0f);
	
  PINDEX i = 1;
	 
  while(i < size) {
		
    BYTE videoSource = (capabilities[i] >> 4) & 0x0f;
		
    if(videoSource <= 5) {
      remoteVideoSources[videoSource].SetEnabled(TRUE);
      remoteVideoSources[videoSource].Decode(capabilities + i);
      i += 2;
	  
    } else {
      // video sources from 6 to 15 are not supported but still need to be parsed
      do {
        i++;
      } while(capabilities[i] != 0);
			
      // scan past the pan/tilt/zoom/focus field
      i++;
    }
  }
	
  OnRemoteCapabilitiesUpdated();
}

void H224_H281Handler::OnReceivedMessage(const H224_Frame & msg)
{
  const H281_Frame & message = (const H281_Frame &)msg;
  H281_Frame::RequestType requestType = message.GetRequestType();
	
  if(requestType == H281_Frame::StartAction) {
	
    if(requestedPanDirection != H281_Frame::NoPan ||
	   requestedTiltDirection != H281_Frame::NoTilt ||
	   requestedZoomDirection != H281_Frame::NoZoom ||
	   requestedFocusDirection != H281_Frame::NoFocus)
    {
      // an action is already running and thus is stopped
      OnStopAction();
    }
		
    requestedPanDirection = message.GetPanDirection();
    requestedTiltDirection = message.GetTiltDirection();
    requestedZoomDirection = message.GetZoomDirection();
    requestedFocusDirection = message.GetFocusDirection();
		
    OnStartAction(requestedPanDirection,
				  requestedTiltDirection,
				  requestedZoomDirection,
				  requestedFocusDirection);
		
    // timeout is always 800 msec
    receiveTimer = 800;
		
  } else if(requestType == H281_Frame::ContinueAction) {
	
    H281_Frame::PanDirection panDirection = message.GetPanDirection();
    H281_Frame::TiltDirection tiltDirection = message.GetTiltDirection();
    H281_Frame::ZoomDirection zoomDirection = message.GetZoomDirection();
    H281_Frame::FocusDirection focusDirection = message.GetFocusDirection();
		
    // if request is valid, reset the timer. Otherwise ignore
    if(panDirection == requestedPanDirection &&
	   tiltDirection == requestedTiltDirection &&
	   zoomDirection == requestedZoomDirection &&
	   focusDirection == requestedFocusDirection &&
	   (panDirection != H281_Frame::NoPan ||
		tiltDirection != H281_Frame::NoTilt ||
		zoomDirection != H281_Frame::NoZoom ||
		focusDirection != H281_Frame::NoFocus))
	{
      receiveTimer = 800;
    }
	
  } else if(requestType == H281_Frame::StopAction){
    H281_Frame::PanDirection panDirection = message.GetPanDirection();
    H281_Frame::TiltDirection tiltDirection = message.GetTiltDirection();
    H281_Frame::ZoomDirection zoomDirection = message.GetZoomDirection();
    H281_Frame::FocusDirection focusDirection = message.GetFocusDirection();
		
    // if request is valid, stop the action. Otherwise ignore
    if(panDirection == requestedPanDirection &&
	   tiltDirection == requestedTiltDirection &&
	   zoomDirection == requestedZoomDirection &&
	   focusDirection == requestedFocusDirection &&
	   (panDirection != H281_Frame::NoPan ||
		tiltDirection != H281_Frame::NoTilt ||
		zoomDirection != H281_Frame::NoZoom ||
		focusDirection != H281_Frame::NoFocus))
    {
      requestedPanDirection = H281_Frame::NoPan;
      requestedTiltDirection = H281_Frame::NoTilt;
      requestedZoomDirection = H281_Frame::NoZoom;
      requestedFocusDirection = H281_Frame::NoFocus;
			
      OnStopAction();
    }

  }	else if(requestType == H281_Frame::SelectVideoSource) {
    OnSelectVideoSource(message.GetVideoSourceNumber(),
						message.GetVideoMode());
	  
  }	else if(requestType == H281_Frame::StoreAsPreset) {
    OnStoreAsPreset(message.GetPresetNumber());
	  
  } else if(requestType == H281_Frame::ActivatePreset) {
		OnActivatePreset(message.GetPresetNumber());
	  
  }	else {
	PTRACE(3, "H.281: Unknown Request: " << requestType);
  }
}

void H224_H281Handler::OnRemoteCapabilitiesUpdated()
{
}

void H224_H281Handler::OnStartAction(H281_Frame::PanDirection /*panDirection*/,
                                    H281_Frame::TiltDirection /*tiltDirection*/,
                                    H281_Frame::ZoomDirection /*zoomDirection*/,
                                    H281_Frame::FocusDirection /*focusDirection*/)
{
  // not handled
}

void H224_H281Handler::OnStopAction()
{
  // not handled
}

void H224_H281Handler::OnSelectVideoSource(BYTE /*videoSourceNumber*/, H281_Frame::VideoMode /*videoMode*/)
{
  // not handled
}

void H224_H281Handler::OnStoreAsPreset(BYTE /*presetNumber*/)
{
  // not handled
}

void H224_H281Handler::OnActivatePreset(BYTE /*presetNumber*/)
{
  // not handled
}

void H224_H281Handler::ContinueAction(PTimer &, INT)
{

  if (shutDown)
      return;
	
  transmitFrame.SetRequestType(H281_Frame::ContinueAction);
	
  m_h224Handler->TransmitClientFrame(H281_CLIENT_ID, transmitFrame);
}

void H224_H281Handler::StopActionLocally(PTimer &, INT)
{
  requestedPanDirection = H281_Frame::NoPan;
  requestedTiltDirection = H281_Frame::NoTilt;
  requestedZoomDirection = H281_Frame::NoZoom;
  requestedFocusDirection = H281_Frame::NoFocus;
	
  OnStopAction();
}

#endif // H224_H281

