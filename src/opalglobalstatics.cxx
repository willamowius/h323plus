/*
 * opalglobalstatics.cxx
 *
 * Various global statics that need to be instantiated upon startup
 *
 * Portable Windows Library
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
 * The Original Code is Portable Windows Library.
 *
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$

 */

#ifndef _OPALGLOBALSTATIC_CXX
#define _OPALGLOBALSTATIC_CXX

#if defined(P_HAS_PLUGINS)
class PluginLoader : public PProcessStartup
{
  PCLASSINFO(PluginLoader, PProcessStartup);
  public:
    void OnStartup()
    { H323PluginCodecManager::Bootstrap(); }
};
#endif

//////////////////////////////////

#if defined(P_HAS_PLUGINS)
static PFactory<PPluginModuleManager>::Worker<H323PluginCodecManager> h323PluginCodecManagerFactory("H323PluginCodecManager", true);
static PFactory<PProcessStartup>::Worker<PluginLoader> h323pluginStartupFactory("H323PluginLoader", true);
#endif

//////////////////////////////////



#endif
