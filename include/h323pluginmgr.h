/*
 * h323plugins.h
 *
 * H.323 codec plugins handler
 *
 * Open H323 Library
 *
 * Copyright (C) 2004 Post Increment
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
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef __OPAL_H323PLUGINMGR_H
#define __OPAL_H323PLUGINMGR_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <ptlib/pluginmgr.h>
#include <codec/opalplugin.h>
#include <mediafmt.h>
#include <ptlib/pfactory.h>

#include <h323caps.h>

class H323Capability;
class OpalFactoryCodec;
class H323PluginCodecManager : public PPluginModuleManager
{
  PCLASSINFO(H323PluginCodecManager, PPluginModuleManager);
  public:
    H323PluginCodecManager(PPluginManager * pluginMgr = NULL);
    ~H323PluginCodecManager();

    void RegisterStaticCodec(const char * name,
                             PluginCodec_GetAPIVersionFunction getApiVerFn,
                             PluginCodec_GetCodecFunction getCodecFn);

    void OnLoadPlugin(PDynaLink & dll, INT code);

    static OpalMediaFormat::List GetMediaFormats();
    static void AddFormat(const OpalMediaFormat & fmt);
    static void AddFormat(OpalMediaFormat * fmt);

	static OpalFactoryCodec * CreateCodec(const PString & name);

    static void CodecListing(const PString & matchStr, PStringList & listing);

    virtual void OnShutdown();

    static void Bootstrap();

	static void Reboot();
/*
    H323Capability * CreateCapability(
          const PString & _mediaFormat,
          const PString & _baseName,
                 unsigned maxFramesPerPacket,
                 unsigned recommendedFramesPerPacket,
                 unsigned _pluginSubType);
*/
  protected:
    void CreateCapabilityAndMediaFormat(
      PluginCodec_Definition * _encoderCodec,
      PluginCodec_Definition * _decoderCodec
    );

    static OpalMediaFormat::List & GetMediaFormatList();
    static PMutex & GetMediaFormatMutex();

    void RegisterCodecs  (unsigned int count, void * codecList);
    void UnregisterCodecs(unsigned int count, void * codecList);

    void RegisterCapability(PluginCodec_Definition * encoderCodec, PluginCodec_Definition * decoderCodec);
    struct CapabilityListCreateEntry {
      CapabilityListCreateEntry(PluginCodec_Definition * e, PluginCodec_Definition * d)
        : encoderCodec(e), decoderCodec(d) { }
      PluginCodec_Definition * encoderCodec;
      PluginCodec_Definition * decoderCodec;
    };
    typedef vector<CapabilityListCreateEntry> CapabilityCreateListType;
    CapabilityCreateListType capabilityCreateList;

    bool m_skipRedefinitions;
};

#if (PTLIB_VAR < 2140) || defined(_FACTORY_LOAD)
static PFactory<PPluginModuleManager>::Worker<H323PluginCodecManager> h323PluginCodecManagerFactory("h323PluginCodecManager", true);
#endif

//////////////////////////////////////////////////////
//
//  this is the base class for codecs accesible via the abstract factory functions
//

/**Class for codcs which is accessible via the abstract factor functions.
   The code would be :

      PFactory<OpalFactoryCodec>::CreateInstance(conversion);

  to create an instance, where conversion is (eg) "L16:G.711-uLaw-64k"
*/
class OpalFactoryCodec : public PObject {
  PCLASSINFO(OpalFactoryCodec, PObject)
  public:
  /** Return the PluginCodec_Definition, which describes this codec */
    virtual const struct PluginCodec_Definition * GetDefinition()
    { return NULL; }

  /** Return the sourceFormat field of PluginCodec_Definition for this codec*/
    virtual PString GetInputFormat() const = 0;

  /** Return the destFormat field of PluginCodec_Definition for this codec*/
    virtual PString GetOutputFormat() const = 0;

    /** Take the supplied data and apply the conversion specified by CreateInstance call (above). When this method returns, toLen contains the number of bytes placed in the destination buffer. */
    virtual int Encode(const void * from,      ///< pointer to the source data
                         unsigned * fromLen,   ///< number of bytes in the source data to process
                             void * to,        ///< pointer to the destination buffer, which contains the output of the  conversion
		                 unsigned * toLen,     ///< Number of available bytes in the destination buffer
                     unsigned int * flag       ///< Typically, this is not used.
		       ) = 0;

    /** Return the  sampleRate field of PluginCodec_Definition for this codec*/
    virtual unsigned int GetSampleRate() const = 0;

    /** Return the  bitsPerSec field of PluginCodec_Definition for this codec*/
    virtual unsigned int GetBitsPerSec() const = 0;

    /** Return the  nmPerFrame field of PluginCodec_Definition for this codec*/
    virtual unsigned int GetFrameTime() const = 0;

    /** Return the samplesPerFrame  field of PluginCodec_Definition for this codec*/
    virtual unsigned int GetSamplesPerFrame() const = 0;

    /** Return the  bytesPerFrame field of PluginCodec_Definition for this codec*/
    virtual unsigned int GetBytesPerFrame() const = 0;

    /** Return the recommendedFramesPerPacket field of PluginCodec_Definition for this codec*/
    virtual unsigned int GetRecommendedFramesPerPacket() const = 0;

    /** Return the maxFramesPerPacket field of PluginCodec_Definition for this codec*/
    virtual unsigned int GetMaxFramesPerPacket() const = 0;

    /** Return the  rtpPayload field of PluginCodec_Definition for this codec*/
    virtual BYTE         GetRTPPayload() const = 0;

    /** Return the  sampleRate field of PluginCodec_Definition for this codec*/
    virtual PString      GetSDPFormat() const = 0;

    /** Set Media Format */
    virtual bool SetMediaFormat(OpalMediaFormat & /*fmt*/) { return false; }

    /** Update Media Options */
    virtual bool UpdateMediaOptions(OpalMediaFormat & /*fmt*/)  { return false; }

	/** Set a Custom format for the codec (video) */
	virtual bool SetCustomFormat(unsigned /*width*/, unsigned /*height*/, unsigned /*frameRate*/) { return false; }

	/** Set a Custom format for the codec (audio) */
	virtual bool SetCustomFormat(unsigned /*bitrate*/, unsigned /*samplerate*/) { return false; }

    /** Codec Control */
    virtual bool CodecControl(const char * /*name*/, void * /*parm*/, unsigned int * /*parmLen*/, int & /*retVal*/) { return false; }

};

#endif
