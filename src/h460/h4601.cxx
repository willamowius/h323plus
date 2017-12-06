// H4601.cxx: implementation of the H460 class.
/*
 * H.460 Implementation for the h323plus Library.
 *
 * Copyright (c) 2004 ISVO (Asia) Pte Ltd. All Rights Reserved.
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
 * The Original Code is derived from and used in conjunction with the
 * OpenH323 Project (www.openh323.org/)
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 *
*/

#include <ptlib.h>

#include "openh323buildopts.h"
#ifdef H323_H460

#ifdef __GNUC__
#pragma implementation "h4601.h"
#endif

#include <ptlib/pluginmgr.h>
#include "h460/h460.h"
#include "h460/h4601.h"

#include <h323.h>
#include <list>

///////////////////////////////////////////////////////////////////////

OpalOID::OpalOID()
{

}

OpalOID::OpalOID(const char * str )
{
    SetValue(str);
}

OpalOID OpalOID::operator+(const char * str)
{
    PString nStr = AsString() + "." + str;
    OpalOID newVal(nStr);
    return newVal;
}

H460_FeatureID::H460_FeatureID()
{
    SetTag(H225_GenericIdentifier::e_standard);
    PASN_Integer & val = *this;
    val.SetValue(0);
}

H460_FeatureID::H460_FeatureID(unsigned ID)
{
    SetTag(H225_GenericIdentifier::e_standard);
    PASN_Integer & val = *this;
    val.SetValue(ID);
}

H460_FeatureID::H460_FeatureID(OpalOID ID)
{
    SetTag(H225_GenericIdentifier::e_oid);
    PASN_ObjectId & val = *this;
    val = ID;
}

H460_FeatureID::H460_FeatureID(PString ID)
{
    SetTag(H225_GenericIdentifier::e_nonStandard);
    OpalGloballyUniqueID uid(ID);
    H225_GloballyUniqueID & val = *this;
    val = uid;
}

H460_FeatureID::H460_FeatureID(H225_GenericIdentifier ID)
{
    SetTag(ID.GetTag());
    H225_GenericIdentifier & val = *this;
    val= ID;
}

PObject * H460_FeatureID::Clone() const
{
  return new H460_FeatureID(*this);
}

PObject::Comparison H460_FeatureID::Compare(const PObject & obj) const
{
  PAssert(PIsDescendant(&obj, H460_FeatureID), PInvalidCast);

  const H460_FeatureID & id  = (const H460_FeatureID &)obj;

  if (id.IDString() == IDString())
        return EqualTo;
  else
        return LessThan;
}


PINLINE H460_FeatureID & H460_FeatureID::operator=(unsigned ID)
{
    SetTag(H225_GenericIdentifier::e_standard);
    PASN_Integer & val = *this;
    val.SetValue(ID);
    return *this;
};

PINLINE H460_FeatureID & H460_FeatureID::operator=(OpalOID ID)
{
    SetTag(H225_GenericIdentifier::e_oid);
    PASN_ObjectId & val = *this;
    val.SetValue(ID.AsString());
    return *this;
};

PINLINE H460_FeatureID & H460_FeatureID::operator=(PString ID)
{
    SetTag(H225_GenericIdentifier::e_nonStandard);
    OpalGloballyUniqueID uid(ID);
    H225_GloballyUniqueID & val = *this;
    val = uid;
    return *this;
};

PString H460_FeatureID::IDString() const
{
    if (GetFeatureType() == H225_GenericIdentifier::e_standard) {
            const PASN_Integer & jint = *this;
            return "Std " + PString(jint);
    }

    if (GetFeatureType() == H225_GenericIdentifier::e_oid) {
            const PASN_ObjectId & obj = *this;
            return "OID " + obj.AsString();
    }

    if (GetFeatureType() == H225_GenericIdentifier::e_nonStandard) {
            const H225_GloballyUniqueID & gui = *this;
            const OpalGloballyUniqueID opalguid(gui);
            return "NonStd " + opalguid.AsString();
    }

    return PString("unknown");
}

//////////////////////////////////////////////////////////////////////
H460_FeatureContent::H460_FeatureContent()
{

}

H460_FeatureContent::H460_FeatureContent(const PASN_OctetString & param)
{
    SetTag(H225_Content::e_raw);
    PASN_OctetString & val = *this;
    val.SetValue(param);
}

H460_FeatureContent::H460_FeatureContent(const PString & param)
{
    SetTag(H225_Content::e_text);
    PASN_IA5String & val = *this;
    val = param;
}

H460_FeatureContent::H460_FeatureContent(PASN_BMPString & param)
{
    SetTag(H225_Content::e_unicode);
    PASN_BMPString & val = *this;
    val.SetValue(param);
}

H460_FeatureContent::H460_FeatureContent(const PBoolean & param)
{
    SetTag(H225_Content::e_bool);
    PASN_Boolean & val = *this;
    val.SetValue(param);
}

H460_FeatureContent::H460_FeatureContent(unsigned param, unsigned len)
{


    if (len == 8) {
      SetTag(H225_Content::e_number8);
      PASN_Integer & val = *this;
      val.SetConstraints(PASN_Object::FixedConstraint, 0, 255);
      val.SetValue(param);
    }
    else if (len == 16) {
      SetTag(H225_Content::e_number16);
      PASN_Integer & val = *this;
      val.SetConstraints(PASN_Object::FixedConstraint, 0, 65535);
      val.SetValue(param);
    }
    else if (len == 32) {
      SetTag(H225_Content::e_number32);
      PASN_Integer & val = *this;
      val.SetConstraints(PASN_Object::FixedConstraint, 0, 4294967295U);
      val.SetValue(param);
    }
    else {
       SetTag(H225_Content::e_number8);
      PASN_Integer & val = *this;
      val.SetConstraints(PASN_Object::FixedConstraint, 0, 255);
      val.SetValue(param);
    }
}

H460_FeatureContent::H460_FeatureContent(const H460_FeatureID & id)
{
    SetTag(H225_Content::e_id);
    H225_GenericIdentifier & val = *this;
    val = id;
}

H460_FeatureContent::H460_FeatureContent(const H225_AliasAddress & add)
{
    SetTag(H225_Content::e_alias);
    H225_AliasAddress & val = *this;
    val = add;
}

H460_FeatureContent::H460_FeatureContent(const PURL & add)
{

    H225_AliasAddress alias;
    alias.SetTag(H225_AliasAddress::e_url_ID);
    PASN_IA5String & url = alias;
    url = add.AsString();

    SetTag(H225_Content::e_alias);
    H225_AliasAddress & val = *this;
    val = alias;
}

H460_FeatureContent::H460_FeatureContent(const H323TransportAddress & add)
{
    SetTag(H225_Content::e_transport);
    H225_TransportAddress & val = *this;
    add.SetPDU(val);
}

H460_FeatureContent::H460_FeatureContent(const OpalGloballyUniqueID & guid)
{
    SetTag(H225_Content::e_id);
    H225_GenericIdentifier & val = *this;
    val.SetTag(H225_GenericIdentifier::e_nonStandard);
    H225_GloballyUniqueID & id = val;
    id.SetValue(guid.AsString());
}

H460_FeatureContent::H460_FeatureContent(const H460_FeatureTable & table)
{
    SetTag(H225_Content::e_compound);
    H225_ArrayOf_EnumeratedParameter & val = *this;
    //val.SetConstraints(PASN_Object::FixedConstraint, 1, 512);
    val = table;
}

H460_FeatureContent::H460_FeatureContent(H460_Feature * add)
{
    SetTag(H225_Content::e_nested);
    H225_ArrayOf_GenericData & val = *this;
    //val.SetConstraints(PASN_Object::FixedConstraint, 1, 16);
    val.Append(add);
    val.SetSize(val.GetSize()+1);
}

H460_FeatureContent::H460_FeatureContent(const H225_Content & param)
{
//    OnReceivedPDU(param);
}

/////////////////////////////////////////////////////////////////////

H460_FeatureParameter::H460_FeatureParameter()
{
   m_id = H460_FeatureID(0);
}

H460_FeatureParameter::H460_FeatureParameter(unsigned Identifier)
{
   m_id = H460_FeatureID(Identifier);
}

H460_FeatureParameter::H460_FeatureParameter(const PString & Identifier)
{
   m_id = H460_FeatureID(Identifier);
}

H460_FeatureParameter::H460_FeatureParameter(const OpalOID & Identifier)
{
    m_id = H460_FeatureID(Identifier);
}

H460_FeatureParameter::H460_FeatureParameter(const H225_EnumeratedParameter & param)
{
//    OnReceivedPDU(param);
}

H460_FeatureParameter::H460_FeatureParameter(const H460_FeatureID & ID)
{
   m_id = ID;
}

H460_FeatureContent H460_FeatureParameter::operator=( const PASN_OctetString & value)
{
    SetTag(e_content);
    m_content = H460_FeatureContent(value);
    return m_content;
}

H460_FeatureContent H460_FeatureParameter::operator=( const PString & value )
{

// Check if url;
    PURL * url = new PURL();    // BUG ?
    if (url->Parse(value,"http"))   // Parameter is an Http Address
        m_content = H460_FeatureContent(*url);


    if (value.Find(":") != P_MAX_INDEX) {
        PStringArray Cmd = value.Tokenise(":", FALSE);

        if (Cmd.GetSize() == 2) {    // Parameter is an Address
            H323TransportAddress * add = new H323TransportAddress(PIPSocket::Address(Cmd[0]),(short)Cmd[1].AsUnsigned());    // BUG: leak ?
            m_content = H460_FeatureContent(*add);
        }
    }
    SetTag(e_content);
    return m_content;

}

H460_FeatureContent  H460_FeatureParameter::operator=( const PASN_BMPString & value)
{
    m_content = H460_FeatureContent(value);
    SetTag(e_content);
    return m_content;
}

H460_FeatureContent  H460_FeatureParameter::operator=( const PBoolean & value )
{
    m_content = H460_FeatureContent(value);
    SetTag(e_content);
    return m_content;
}

H460_FeatureContent  H460_FeatureParameter::operator=( const unsigned & value )
{

  if (value == 0)
     m_content = H460_FeatureContent(value,32);
  else if (value < 16)
     m_content = H460_FeatureContent(value,8);
  else if (value < 256)
     m_content = H460_FeatureContent(value,16);
  else
     m_content = H460_FeatureContent(value,32);

  SetTag(e_content);
  return m_content;
}

H460_FeatureContent  H460_FeatureParameter::operator=( const H460_FeatureID & value )
{
    m_content = H460_FeatureContent(value);
    SetTag(e_content);
    return m_content;
}

H460_FeatureContent  H460_FeatureParameter::operator=( const H225_AliasAddress & value )
{
    m_content = H460_FeatureContent(value);
    SetTag(e_content);
    return m_content;
}

H460_FeatureContent  H460_FeatureParameter::operator=( const H323TransportAddress & value )
{
    m_content = H460_FeatureContent(value);
    SetTag(e_content);
    return m_content;
}

H460_FeatureContent  H460_FeatureParameter::operator=( const H460_FeatureTable & value )
{
    m_content = H460_FeatureContent(value);
    SetTag(e_content);
    return m_content;
}

H460_FeatureContent  H460_FeatureParameter::operator=( H460_Feature * value )
{
    m_content = H460_FeatureContent(value);
    SetTag(e_content);
    return m_content;
}

H460_FeatureParameter::operator PASN_OctetString &()
{
    PASN_OctetString & content = m_content;
    return content;
};

H460_FeatureParameter::operator PString ()
{
    PASN_IA5String & content = m_content;
    return content.GetValue();
};

H460_FeatureParameter::operator PASN_BMPString &()
{
    PASN_BMPString & content = m_content;
    return content;
};

H460_FeatureParameter::operator PBoolean ()
{
    PASN_Boolean & content = m_content;
    PBoolean con = content;
    return con;
};

H460_FeatureParameter::operator unsigned()
{
    PASN_Integer & content = m_content;
    return content;
};

H460_FeatureParameter::operator H460_FeatureID &()
{
    H225_GenericIdentifier & content = m_content;
    return (H460_FeatureID &)content;
};

H460_FeatureParameter::operator H225_AliasAddress &()
{
    H225_AliasAddress & content = m_content;
    return content;
};

H460_FeatureParameter::operator H323TransportAddress ()
{
    H225_TransportAddress & content = m_content;
    return content;

};

H460_FeatureParameter::operator H225_ArrayOf_EnumeratedParameter &()
{
    H225_ArrayOf_EnumeratedParameter & content = m_content;
    return content;
};

H460_FeatureParameter::operator PURL ()
{
    H225_AliasAddress & content = m_content;
    PASN_IA5String & Surl = content;
    return Surl.GetValue();
};

H460_FeatureParameter::operator OpalGloballyUniqueID ()
{
    H225_GenericIdentifier & content = m_content;
    if (content.GetTag() == H225_GenericIdentifier::e_nonStandard) {
       H225_GloballyUniqueID & id = content;
       return OpalGloballyUniqueID((PASN_OctetString &)id);
    }
    return OpalGloballyUniqueID();
};

/////////////////////////////////////////////////////////////////////

H460_FeatureTable::H460_FeatureTable()
{
    SetConstraintBounds(PASN_Object::FixedConstraint, 1, 512);
    SetSize(0);
}

H460_FeatureTable::H460_FeatureTable(const H225_ArrayOf_EnumeratedParameter & Xparams)
{
    SetConstraintBounds(PASN_Object::FixedConstraint, 1, 512);
//    OnReceivedPDU(Xparams);
}

H460_FeatureParameter & H460_FeatureTable::AddParameter(const H460_FeatureID & id)
{
PTRACE(6, "H460\tAdd ID: " << id );

     H460_FeatureParameter param = H460_FeatureParameter(id);
     PINDEX i = GetSize();
     SetSize(i+1);
     (*this)[i] = param;
     return (*this)[i];
}

H460_FeatureParameter & H460_FeatureTable::AddParameter(const H460_FeatureID & id, const H460_FeatureContent & con)
{
PTRACE(6, "H460\tAdd ID: " << id  << " content " << con);

      H460_FeatureParameter & Nparam = AddParameter(id);
      Nparam.addContent(con);
      return Nparam;
}

void H460_FeatureTable::AddParameter(H225_EnumeratedParameter & Xparam)
{
     PINDEX i = GetSize();
     SetSize(i+1);
     (*this)[i] = Xparam;
}

//void H460_FeatureTable::SetParameter(H460_FeatureParameter & Nparam, H225_EnumeratedParameter & Xparam) const
//{
//    Nparam.OnSendingPDU(Xparam);
//}

H460_FeatureParameter & H460_FeatureTable::GetParameter(PINDEX id)
{
    return (*this)[id];
}

H460_FeatureParameter & H460_FeatureTable::GetParameter(const H460_FeatureID & id)
{

    PINDEX num = GetParameterIndex(id);
    return GetParameter(num);
}


PINDEX H460_FeatureTable::GetParameterIndex(const H460_FeatureID & id)
{
    PINDEX i;

    for (i = 0; i < GetSize(); i++) {
        H460_FeatureParameter & fparam = (*this)[i];
        H460_FeatureID param = fparam.ID();
        if (param == id)
            return i;
    }

    return GetSize();
}

PBoolean H460_FeatureTable::HasParameter(const H460_FeatureID & id) {

PTRACE(6, "H460\tCheck has Parameter " << id);

    if (GetParameterIndex(id) < GetSize())
        return TRUE;

    return FALSE;

}

void H460_FeatureTable::RemoveParameter(PINDEX id)
{
    RemoveAt(id);
}

void H460_FeatureTable::RemoveParameter(const H460_FeatureID & id)
{
    PINDEX j = GetParameterIndex(id);

    if (j < GetSize())
        RemoveParameter(j);

}

void H460_FeatureTable::ReplaceParameter(const H460_FeatureID & id, const H460_FeatureContent & con)
{

PTRACE(6, "H460\tReplace ID: " << id  << " content " << con);

    PINDEX j = GetParameterIndex(id);

    if (j == GetSize())
        return;

    RemoveAt(j);

    AddParameter(id,con);
}


PBoolean H460_FeatureTable::ParameterIsUnique(const H460_FeatureID & id)
{
    PINDEX i;
    PINDEX j =0;

    for (i = 0; i < GetSize(); i++) {
        H460_FeatureParameter & param = GetParameter(i);

        if (param.ID() == id) {
            j++;
        }
    }
    if (j <= 1)
       return TRUE;
    else
       return FALSE;
}

H460_FeatureParameter & H460_FeatureTable::operator[](PINDEX id)
{
    return (H460_FeatureParameter &)array[id];
}

/////////////////////////////////////////////////////////////////////

H460_Feature::H460_Feature()
: FeatureCategory(FeatureSupported), FeatureInstance(FeatureBaseAll),
  ep(NULL), con(NULL)
{

}

H460_Feature::H460_Feature(unsigned identifier)
: FeatureCategory(FeatureSupported), FeatureInstance(FeatureBaseAll),
  ep(NULL), con(NULL)
{
    SetFeatureID(H460_FeatureID(identifier));
}

H460_Feature::H460_Feature(PString identifier)
: FeatureCategory(FeatureSupported), FeatureInstance(FeatureBaseAll),
  ep(NULL), con(NULL)
{
    SetFeatureID(H460_FeatureID(identifier));
}

H460_Feature::H460_Feature(OpalOID identifier)
: FeatureCategory(FeatureSupported), FeatureInstance(FeatureBaseAll),
  ep(NULL), con(NULL)
{
    SetFeatureID(H460_FeatureID(identifier));
}

H460_Feature::H460_Feature(const H225_FeatureDescriptor & descriptor)
: FeatureCategory(FeatureSupported), FeatureInstance(FeatureBaseAll),
  ep(NULL), con(NULL)
{
    SetFeatureID(descriptor.m_id);
    if (descriptor.HasOptionalField(H225_FeatureDescriptor::e_parameters)) {
        IncludeOptionalField(H225_FeatureDescriptor::e_parameters);
        m_parameters = descriptor.m_parameters;
    }
}

PString H460_Feature::GetFeatureIDAsString()
{
   return  ((H460_FeatureID)m_id).IDString();
}

PBoolean H460_Feature::FeatureAdvertised(int mtype)
{
     switch (mtype) {
        case H460_MessageType::e_gatekeeperRequest:
        case H460_MessageType::e_gatekeeperConfirm:
        case H460_MessageType::e_gatekeeperReject:
        case H460_MessageType::e_registrationRequest:
        case H460_MessageType::e_registrationConfirm:
        case H460_MessageType::e_registrationReject:
        case H460_MessageType::e_setup:
        case H460_MessageType::e_callProceeding:
            return true;
        default:
            return false;
     }
}

H460_FeatureParameter & H460_Feature::AddParameter(H460_FeatureID * id, const H460_FeatureContent & con)
{
    if (!HasOptionalField(e_parameters)) {
        IncludeOptionalField(e_parameters);
        m_parameters.SetSize(0);
    }

    return ((H460_FeatureTable &)m_parameters).AddParameter(*id, con);
}

H460_FeatureParameter & H460_Feature::AddParameter(H460_FeatureID * id)
{
    if (!HasOptionalField(e_parameters)) {
        IncludeOptionalField(e_parameters);
        m_parameters.SetSize(0);
    }

    return ((H460_FeatureTable &)m_parameters).AddParameter(*id);
}

void H460_Feature::AddParameter(H460_FeatureParameter * param)
{
    if (!HasOptionalField(e_parameters)) {
        IncludeOptionalField(e_parameters);
        m_parameters.SetSize(0);
    }

    ((H460_FeatureTable &)m_parameters).AddParameter(*param);
}

void H460_Feature::RemoveParameter(PINDEX id)
{
    if (!HasOptionalField(e_parameters))
        return;

    if (m_parameters.GetSize() > id)
          ((H460_FeatureTable &)m_parameters).RemoveParameter(id);

    if (m_parameters.GetSize() == 0)
        RemoveOptionalField(e_parameters);
}

void H460_Feature::ReplaceParameter(const H460_FeatureID & id, const H460_FeatureContent & con)
{
    if (!HasOptionalField(e_parameters)) {
       PTRACE(1, "H460\tLOGIC ERROR: NO Parameters or index out of bounds");
       return;
    }
    ((H460_FeatureTable &)m_parameters).ReplaceParameter(id, con);
}

H460_FeatureParameter & H460_Feature::GetFeatureParameter(PINDEX id)
{
    if (!HasOptionalField(e_parameters) || m_parameters.GetSize() <= id ) {
       PTRACE(1, "H460\tLOGIC ERROR: NO Parameters or index out of bounds");
       return *(new H460_FeatureParameter());    // BUG: memory leak but is never called - SH
    }
    return (H460_FeatureParameter &)m_parameters[id];

}

H460_FeatureParameter & H460_Feature::GetFeatureParameter(const H460_FeatureID & id)
{
    if (!HasOptionalField(e_parameters)) {
       PTRACE(1, "H460\tLOGIC ERROR: NO Parameters or index out of bounds");
       return *(new H460_FeatureParameter());    // BUG: memory leak but is never called - SH
    }

    return ((H460_FeatureTable &)m_parameters).GetParameter(id);
}

PBoolean H460_Feature::HasFeatureParameter(const H460_FeatureID & id)
{
    return Contains(id);
}

PBoolean H460_Feature::Contains(const H460_FeatureID & id)
{
    PTRACE(6, "H460\tCheck for Parameter " << id);

    if (HasOptionalField(e_parameters)) {
        H460_FeatureTable & Table = (H460_FeatureTable &)m_parameters;
        if (Table.HasParameter(id))
            return true;
    }

    return false;
}

int H460_Feature::GetParameterCount()
{
    if (!HasOptionalField(e_parameters))
        return 0;

    return m_parameters.GetSize();
}


H460_FeatureParameter & H460_Feature::Value(const H460_FeatureID & id)
{
    if (HasOptionalField(e_parameters)) {
        H460_FeatureTable & Table = (H460_FeatureTable &)m_parameters;
        if (Table.HasParameter(id))
            return Table.GetParameter(id);
    }
    PAssertAlways("LOGIC ERROR: Must call <if (.Contains)> before .Value");
    return *(new H460_FeatureParameter());    // BUG: memory leak but is never called - SH
}

H460_FeatureParameter & H460_Feature::operator()(PINDEX id)
{
    return GetFeatureParameter(id);
}

H460_FeatureParameter & H460_Feature::operator()(const H460_FeatureID & id)
{
    return GetFeatureParameter(id);
}

H460_FeatureTable & H460_Feature::GetCurrentTable()
{
    return ((H460_FeatureTable &)m_parameters);
}

void H460_Feature::SetCurrentTable(H460_FeatureTable & table)
{
    if (!HasOptionalField(e_parameters))
        IncludeOptionalField(e_parameters);

    m_parameters.SetSize(0);
    m_parameters = table;
}

void H460_Feature::SetCurrentTable(H460_FeatureParameter & param)
{
    H225_ArrayOf_EnumeratedParameter & table = param;
    SetCurrentTable((H460_FeatureTable &)table);
}

void H460_Feature::SetDefaultTable()
{
    // Not used!
}

void H460_Feature::AttachEndPoint(H323EndPoint * _ep)
{
   ep = _ep;
}

void H460_Feature::AttachConnection(H323Connection * _con)
{
   con = _con;
}

/////////////////////////////////////////////////////////////////////

static const char H460FeaturePluginBaseClass[] = "H460_Feature";

#if PTLIB_VER < 2130

#if PTLIB_VER >= 2110
template <> H460_Feature * PDevicePluginFactory<H460_Feature>::Worker::Create(const PDefaultPFactoryKey & type) const
#else
template <> H460_Feature * PDevicePluginFactory<H460_Feature>::Worker::Create(const PString & type) const
#endif
{
  return H460_Feature::CreateFeature(type);
}

typedef PDevicePluginAdapter<H460_Feature> PDevicePluginH460;
PFACTORY_CREATE(PFactory<PDevicePluginAdapterBase>, PDevicePluginH460, H460FeaturePluginBaseClass, true);

#endif

PStringList H460_Feature::GetFeatureNames(PPluginManager * pluginMgr)
{
  if (pluginMgr == NULL)
    pluginMgr = &PPluginManager::GetPluginManager();

#if PTLIB_VER >= 2130
  return pluginMgr->GetPluginsProviding(H460FeaturePluginBaseClass,false);
#else
  return pluginMgr->GetPluginsProviding(H460FeaturePluginBaseClass);
#endif
}

PStringList H460_Feature::GetFeatureFriendlyNames(const PString & feature, PPluginManager * pluginMgr)
{
  if (pluginMgr == NULL)
    pluginMgr = &PPluginManager::GetPluginManager();

#if PTLIB_VER >= 2130
  return pluginMgr->GetPluginDeviceNames(feature, H460FeaturePluginBaseClass);
#else
  return pluginMgr->GetPluginsDeviceNames(feature, H460FeaturePluginBaseClass);
#endif
}

H460_Feature * H460_Feature::CreateFeature(const PString & featurename, int pduType, PPluginManager * pluginMgr)
{
  if (pluginMgr == NULL)
    pluginMgr = &PPluginManager::GetPluginManager();

#if PTLIB_VER >= 2130
  return (H460_Feature *)pluginMgr->CreatePlugin(featurename, H460FeaturePluginBaseClass,pduType);
#else
  return (H460_Feature *)pluginMgr->CreatePluginsDeviceByName(featurename, H460FeaturePluginBaseClass,pduType);
#endif
}

PBoolean H460_Feature::FeatureList(int type, H460FeatureList & plist, H323EndPoint * ep, PPluginManager * pluginMgr)
{
  if (pluginMgr == NULL)
    pluginMgr = &PPluginManager::GetPluginManager();

   PStringList featurelist = H460_Feature::GetFeatureNames(pluginMgr);

   for (PINDEX i=0; i<featurelist.GetSize(); i++) {
     if (ep && !ep->OnFeatureInstance(type,featurelist[i]))
           continue;

     PDevicePluginServiceDescriptor * desc =
            (PDevicePluginServiceDescriptor *)pluginMgr->GetServiceDescriptor(featurelist[i], H460FeaturePluginBaseClass);

     if (desc != NULL && desc->ValidateDeviceName(featurelist[i], type)) {
         PString feat = featurelist[i].Left(3);
         if (feat == "Std") {            // Std feature
                plist.insert(pair<PString,H460_FeatureID*>(featurelist[i], new H460_FeatureID(featurelist[i].Mid(3).AsInteger())));
         } else if (feat == "OID") {        // OID feature
                plist.insert(pair<PString,H460_FeatureID*>(featurelist[i], new H460_FeatureID(OpalOID(desc->GetDeviceNames(1)[0]))));
         } else    {   // NonStd Feature
                plist.insert(pair<PString,H460_FeatureID*>(featurelist[i], new H460_FeatureID(feat)));
         }
     }
   }
   return (plist.size() > 0);
}

/////////////////////////////////////////////////////////////////////

H460_FeatureStd::H460_FeatureStd(unsigned Identifier)
    : H460_Feature(Identifier)
{
}

PObject * H460_FeatureStd::Clone() const
{
    return new H460_FeatureStd(*this);
}

void H460_FeatureStd::Add(unsigned id)
{
    H460_FeatureID * feat_id = new H460_FeatureID(id);
    AddParameter(feat_id);
    delete feat_id;
}

void H460_FeatureStd::Add(unsigned id, const H460_FeatureContent & con)
{
    H460_FeatureID * feat_id = new H460_FeatureID(id);
    AddParameter(feat_id, con);
    delete feat_id;
}

void H460_FeatureStd::Remove(unsigned id)
{
    RemoveParameter(H460_FeatureID(id));
}

void H460_FeatureStd::Replace(unsigned id, const H460_FeatureContent & con)
{
    ReplaceParameter(H460_FeatureID(id),con);
}

PBoolean H460_FeatureStd::HasParameter(unsigned id)
{
    return HasFeatureParameter(H460_FeatureID(id));
}

H460_FeatureParameter & H460_FeatureStd::GetParameter(unsigned id)
{
    return GetFeatureParameter(H460_FeatureID(id));
}


/////////////////////////////////////////////////////////////////////

H460_FeatureNonStd::H460_FeatureNonStd(PString Identifier)
    : H460_Feature(Identifier)
{
}

PObject * H460_FeatureNonStd::Clone() const
{
    return new H460_FeatureNonStd(*this);
}

void H460_FeatureNonStd::Add(const PString & id)
{
    H460_FeatureID * feat_id = new H460_FeatureID(id);
    AddParameter(feat_id);
    delete feat_id;
}

void H460_FeatureNonStd::Add(const PString & id, const H460_FeatureContent & con)
{
    H460_FeatureID * feat_id = new H460_FeatureID(id);
    AddParameter(feat_id, con);
    delete feat_id;
}

void H460_FeatureNonStd::Remove(const PString & id)
{
    RemoveParameter(H460_FeatureID(id));
}

void H460_FeatureNonStd::Replace(const PString & id, const H460_FeatureContent & con)
{
    ReplaceParameter(H460_FeatureID(id),con);
}

PBoolean H460_FeatureNonStd::HasParameter(PString id)
{
    return HasFeatureParameter(H460_FeatureID(id));
}

H460_FeatureParameter & H460_FeatureNonStd::operator[](PString id)
{
    return GetFeatureParameter(H460_FeatureID(id));
}

/////////////////////////////////////////////////////////////////////

H460_FeatureOID::H460_FeatureOID(OpalOID Identifier)
    : H460_Feature(Identifier)
{
}

PObject * H460_FeatureOID::Clone() const
{
    return new H460_FeatureOID(*this);
}

void H460_FeatureOID::Add(const PString & id)
{
    PString val = GetBase() + "." + id;
    H460_FeatureID * feat_id = new H460_FeatureID(OpalOID(val));
    AddParameter(feat_id);
    delete feat_id;
}

void H460_FeatureOID::Add(const PString & id, const H460_FeatureContent & con)
{
    PString val = GetBase() + "." + id;
    H460_FeatureID * feat_id = new H460_FeatureID(OpalOID(val));
    AddParameter(feat_id, con);
    delete feat_id;
}

void H460_FeatureOID::Remove(const PString & id)
{
    PString val = GetBase() + "." + id;
    RemoveParameter(H460_FeatureID(OpalOID(val)));
}

void H460_FeatureOID::Replace(const PString & id, const H460_FeatureContent & con)
{
    PString val = GetBase() + "." + id;
    ReplaceParameter(H460_FeatureID(OpalOID(val)),con);
}


PBoolean H460_FeatureOID::HasParameter(OpalOID id)
{
    return HasFeatureParameter(H460_FeatureID(id));
}


H460_FeatureParameter & H460_FeatureOID::operator[](OpalOID id)
{
    PString val = GetBase() + "." + id.AsString();
    return GetFeatureParameter(H460_FeatureID(OpalOID(val)));
}

PBoolean H460_FeatureOID::Contains(const PString & id)
{
    PString val = GetBase() + "." + id;
    return  H460_Feature::Contains(OpalOID(val));
}

H460_FeatureParameter & H460_FeatureOID::Value(const PString & id)
{
    PString val = GetBase() + "." + id;
    return  H460_Feature::Value(OpalOID(val));
}

PString H460_FeatureOID::GetBase()
{
    OpalOID id = (H460_FeatureID)m_id;
    return id.AsString();
}

/////////////////////////////////////////////////////////////////////
H460_FeatureSet::H460_FeatureSet()
{
    ep = NULL;
    baseSet = NULL;
}

H460_FeatureSet::~H460_FeatureSet()
{
    // Delete Objects in derived FeatureSets unless shared
    if (baseSet) {
        for (PINDEX i=0; i < Features.GetSize(); ++i) {
            H460_Feature * feat = &Features.GetDataAt(i);
            if (feat && (feat->GetFeaturePurpose() != H460_Feature::FeatureBaseAll))
                delete feat;
        }
    }
    Features.RemoveAll();
}

H460_FeatureSet::H460_FeatureSet(H460_FeatureSet * _base)
{
    Features.DisallowDeleteObjects();   // Derived FeatureSets should not delete Objects.
    AttachBaseFeatureSet(_base);
    AttachEndPoint(_base->GetEndPoint());
}

H460_FeatureSet::H460_FeatureSet(const H225_FeatureSet & fs)
{
    Features.DisallowDeleteObjects();   // Built FeatureSet should not delete Objects.
    ep = NULL;
    baseSet = NULL;
    CreateFeatureSet(fs);
}

H460_FeatureSet::H460_FeatureSet(const H225_ArrayOf_GenericData & generic)
{
    Features.DisallowDeleteObjects();   // Built FeatureSet should not delete Objects.
    ep = NULL;
    baseSet = NULL;

    for (PINDEX i=0; i < generic.GetSize(); i++) {
       AddFeature((H460_Feature *)&generic[i]);
    }
}

PBoolean H460_FeatureSet::ProcessFirstPDU(const H225_FeatureSet & fs)
{

PTRACE(6,"H460\tCreate Common FeatureSet");

    H460_FeatureSet remote = H460_FeatureSet(fs);

 /// Remove the features the remote does not support.
    for (PINDEX i=Features.GetSize()-1;  i > -1;  i--) {
        H460_Feature & feat = Features.GetDataAt(i);
        H460_FeatureID id = feat.GetFeatureID();
        if (!remote.HasFeature(id) && !feat.CommonFeature())
             RemoveFeature(id);
        else
           PTRACE(4,"H460\tUse Common Feature " << id);
    }


    return TRUE;
}

PBoolean H460_FeatureSet::RemoveUnCommonFeatures()
{

PTRACE(4,"H460\tRemoving UnCommon Features");

 /// Remove the features that have not been negotiated for the call.
    for (PINDEX i=Features.GetSize()-1; i > -1;  i--) {
        H460_Feature & feat = Features.GetDataAt(i);
        H460_FeatureID id = feat.GetFeatureID();
        if (!feat.CommonFeature())
             RemoveFeature(id);
    }

    return TRUE;
}

PBoolean H460_FeatureSet::CreateFeatureSet(const H225_FeatureSet & fs)
{
    PTRACE(6,"H460\tCreate FeatureSet from FeatureSet PDU");

      if (fs.HasOptionalField(H225_FeatureSet::e_neededFeatures)) {
         const H225_ArrayOf_FeatureDescriptor & fsn = fs.m_neededFeatures;
          for (PINDEX i=0; i < fsn.GetSize(); i++) {
              AddFeature((H460_Feature *)&fsn[i]);
          }
      }

      if (fs.HasOptionalField(H225_FeatureSet::e_desiredFeatures)) {
        const H225_ArrayOf_FeatureDescriptor & fsd = fs.m_desiredFeatures;
          for (PINDEX i=0; i < fsd.GetSize(); i++) {
              AddFeature((H460_Feature *)&fsd[i]);
          }
      }

      if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
        const H225_ArrayOf_FeatureDescriptor & fss = fs.m_supportedFeatures;
          for (PINDEX i=0; i < fss.GetSize(); i++) {
              AddFeature((H460_Feature *)&fss[i]);
          }
      }
      return TRUE;
}


PBoolean H460_FeatureSet::LoadFeatureSet(int inst, H323Connection * con)
{

  if ((ep) && (ep->FeatureSetDisabled()))
     return FALSE;

  H460FeatureList featurelist;
  H460_Feature::FeatureList(inst,featurelist,ep);

  	 H460FeatureList::const_iterator it = featurelist.begin();
	 while (it != featurelist.end()) {
        H460_Feature * feat = NULL;
        if (baseSet && baseSet->HasFeature(*it->second)) {
            H460_Feature * tempfeat = baseSet->GetFeature(*it->second);
            if (tempfeat->GetFeaturePurpose() == H460_Feature::FeatureBaseAll)
                feat = tempfeat;
            else
                feat = (H460_Feature*)tempfeat->Clone();
        } else {
            feat = H460_Feature::CreateFeature(it->first,inst);
            if ((feat) && (ep))
                feat->AttachEndPoint(ep);
        }

        if (feat) {
            if (con)
                feat->AttachConnection(con);

           AddFeature(feat);
           PTRACE(4,"H460\tLoaded Feature " << it->first);
        }
        it++;
      }

  DeleteFeatureList(featurelist);
  return TRUE;
}

PBoolean H460_FeatureSet::LoadFeature(const PString & featid)
{

    H460_Feature * newfeat = H460_Feature::CreateFeature(featid);

    if (newfeat != NULL)
        return AddFeature(newfeat);
    else
        return FALSE;
}

H460_FeatureSet * H460_FeatureSet::DeriveNewFeatureSet()
{
    return new H460_FeatureSet(this);
}


PBoolean H460_FeatureSet::AddFeature(H460_Feature * Nfeat)
{

    PTRACE(4, "H460\tLoaded " << Nfeat->GetFeatureIDAsString());

    return Features.SetAt(Nfeat->GetFeatureID(),Nfeat);

}

void H460_FeatureSet::RemoveFeature(H460_FeatureID id)
{
  PStringStream info;
  info << "H460\tRemoved ";
    switch (id.GetFeatureType()) {
       case H460_FeatureID::e_standard:
            info << "Std Feature " << (unsigned)id << "\n";
            break;
       case H460_FeatureID::e_oid:
            info << "OID Feature " << (OpalOID)id << "\n";
            break;
       case H460_FeatureID::e_nonStandard:
            const H225_GloballyUniqueID h225_guid = ((H225_GloballyUniqueID &)(id));
            const OpalGloballyUniqueID guid(h225_guid);
            info << "NonStd Feature " << guid.AsString() << "\n";
            break;
    }
    PTRACE(4, info);

    Features.RemoveAt(id);
}


#if PTRACING
PString featureType(PINDEX id)
{
    switch (id) {
    case 1: return "Needed";
    case 2: return "Desired";
    case 3: return "Supported";
    default: return "?";
    }
}
#endif

PBoolean H460_FeatureSet::CreateFeatureSetPDU(H225_FeatureSet & fs, unsigned MessageID, PBoolean advertise)
{
    PTRACE(6,"H460\tCreate FeatureSet " << PTracePDU(MessageID) << " PDU");

    PBoolean buildPDU = FALSE;

    for (PINDEX i = 0; i < Features.GetSize(); i++) {    // Iterate thro the features
       H460_Feature & feat = Features.GetDataAt(i);

        PTRACE(6,"H460\tExamining " << feat.GetFeatureIDAsString());
        if (advertise != feat.FeatureAdvertised(MessageID)) {
            PTRACE(6,"H460\tIgnoring " << feat.GetFeatureIDAsString() << " not Advertised.");
            continue;
        }

        PINDEX lastPos;
        H225_FeatureDescriptor featdesc;
        if (CreateFeaturePDU(feat,featdesc,MessageID)) {

#if PTRACING
          PTRACE(6,"H460\tLoading Feature " << feat.GetFeatureIDAsString() << " as "
            << featureType(feat.FeatureCategory) << " feature to " << PTracePDU(MessageID)
            << " PDU\n" << featdesc );
#endif

/// For some completely silly reason the ITU decided to send/receive H460 Messages in two places,
/// for some messages it is included in the messsage body FeatureSet (to Advertise it) and others
/// in the genericData field (as actual information). Even though a Feature is generic data. It is beyond
/// me to determine Why it is so. Anyway if a message is to be carried in the genericData field it is given
/// the default category of supported.

     PINDEX cat;
     if (advertise)
         cat = feat.FeatureCategory;
     else
         cat = H460_Feature::FeatureSupported;


      buildPDU = TRUE;
       switch (cat) {            // Add it to the correct feature list
           case H460_Feature::FeatureNeeded:

              if (featdesc.GetDataLength() > 0) {
                if (!fs.HasOptionalField(H225_FeatureSet::e_neededFeatures))
                    fs.IncludeOptionalField(H225_FeatureSet::e_neededFeatures);

                     H225_ArrayOf_FeatureDescriptor & fsn = fs.m_neededFeatures;
                        lastPos = fsn.GetSize();
                        fsn.SetSize(lastPos+1);
                        fsn[lastPos] = featdesc;
              }
             break;

           case H460_Feature::FeatureDesired:

              if (featdesc.GetDataLength() > 0) {
                if (!fs.HasOptionalField(H225_FeatureSet::e_desiredFeatures))
                  fs.IncludeOptionalField(H225_FeatureSet::e_desiredFeatures);

                     H225_ArrayOf_FeatureDescriptor & fsd = fs.m_desiredFeatures;
                        lastPos = fsd.GetSize();
                        fsd.SetSize(lastPos+1);
                        fsd[lastPos] = featdesc;
              }
             break;

           case H460_Feature::FeatureSupported:

              if (featdesc.GetDataLength() > 0) {
                 if (!fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures))
                   fs.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);

                     H225_ArrayOf_FeatureDescriptor & fss = fs.m_supportedFeatures;
                        lastPos = fss.GetSize();
                        fss.SetSize(lastPos+1);
                        fss[lastPos] = featdesc;
              }
             break;
          }
       }
    }

#if PTRACING
    PTRACE(6,"H460\tFeatureSet for " << PTracePDU(MessageID) << " PDU\n" << fs);
#endif

    return buildPDU;
}

void H460_FeatureSet::ReadFeatureSetPDU(const H225_FeatureSet & fs, unsigned MessageID, PBoolean genericData)
{

PTRACE(6,"H460\tRead FeatureSet " << PTracePDU(MessageID) << " PDU");

    if (!genericData) {
       // Generate Common Set of Features.
       switch (MessageID) {
         case H460_MessageType::e_gatekeeperRequest:
         case H460_MessageType::e_gatekeeperConfirm:
         case H460_MessageType::e_registrationRequest:
         case H460_MessageType::e_registrationConfirm:
         case H460_MessageType::e_setup:
             ProcessFirstPDU(fs);
             break;
         default:
             break;
       }
    }

      H460_FeatureID ID;

      if (fs.HasOptionalField(H225_FeatureSet::e_neededFeatures)
          && fs.m_neededFeatures.GetSize()>0) {
        const H225_ArrayOf_FeatureDescriptor & fsn = fs.m_neededFeatures;
          for (PINDEX i=fsn.GetSize()-1; i >= 0; --i) {  // iterate backwards
              H225_FeatureDescriptor & fd = fsn[i];
              ID = GetFeatureIDPDU(fd);

              if (HasFeature(ID))
                    ReadFeaturePDU(Features[ID],fd,MessageID);
          }
      }

      if (fs.HasOptionalField(H225_FeatureSet::e_desiredFeatures)
          && fs.m_desiredFeatures.GetSize()>0) {
        const H225_ArrayOf_FeatureDescriptor & fsd = fs.m_desiredFeatures;
          for (PINDEX i=fsd.GetSize()-1; i >= 0; --i) {  // iterate backwards
              H225_FeatureDescriptor & fd = fsd[i];
              ID = GetFeatureIDPDU(fd);

              if (HasFeature(ID))
                    ReadFeaturePDU(Features[ID],fd,MessageID);
          }
      }

      if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)
          && fs.m_supportedFeatures.GetSize()>0) {
        const H225_ArrayOf_FeatureDescriptor & fss = fs.m_supportedFeatures;
          for (PINDEX i=fss.GetSize()-1; i >= 0; --i) {  // iterate backwards
              H225_FeatureDescriptor & fd = fss[i];
              ID = GetFeatureIDPDU(fd);

              if (HasFeature(ID))
                    ReadFeaturePDU(Features[ID],fd,MessageID);
          }
      }

      if (MessageID == H460_MessageType::e_connect)
                RemoveUnCommonFeatures();
}

PBoolean H460_FeatureSet::SupportNonCallService(const H225_FeatureSet & fs) const
{
    H460_FeatureID ID;

    PBoolean nonCallService = false;
    if (fs.HasOptionalField(H225_FeatureSet::e_neededFeatures)) {
        const H225_ArrayOf_FeatureDescriptor & fsn = fs.m_neededFeatures;
          for (PINDEX i=0; i < fsn.GetSize(); i++) {
              const H225_FeatureDescriptor & fd = fsn[i];
              ID = GetFeatureIDPDU(fd);
              if (SupportNonCallService(ID)) {
                nonCallService = true;
                break;
              }
          }
        if (nonCallService) return true;
      }

      if (fs.HasOptionalField(H225_FeatureSet::e_desiredFeatures)) {

        const H225_ArrayOf_FeatureDescriptor & fsd = fs.m_desiredFeatures;
          for (PINDEX i=0; i < fsd.GetSize(); i++) {
              const H225_FeatureDescriptor & fd = fsd[i];
              ID = GetFeatureIDPDU(fd);
              if (SupportNonCallService(ID)) {
                nonCallService = true;
                break;
              }
          }
        if (nonCallService) return true;
      }

      if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
        const H225_ArrayOf_FeatureDescriptor & fss = fs.m_supportedFeatures;
          for (PINDEX i=0; i < fss.GetSize(); i++) {
              const H225_FeatureDescriptor & fd = fss[i];
              ID = GetFeatureIDPDU(fd);
              if (SupportNonCallService(ID)) {
                nonCallService = true;
                break;
              }
          }
        if (nonCallService) return true;
      }

      return false;
}

H460_FeatureID H460_FeatureSet::GetFeatureIDPDU(const H225_FeatureDescriptor & pdu) const
{

    H460_FeatureID fid;
    const H225_GenericIdentifier & id = pdu.m_id;

      if ((id.GetTag() == H225_GenericIdentifier::e_standard)) {
          const PASN_Integer & sid = id;
          unsigned iid = sid.GetValue();
          fid = H460_FeatureID(iid);
      }

      if ((id.GetTag() == H225_GenericIdentifier::e_oid)) {
          const PASN_ObjectId & oid = id;
          const OpalOID  & aid = (const OpalOID &)oid;
          fid = H460_FeatureID(aid);

      }

      if ((id.GetTag() == H225_GenericIdentifier::e_nonStandard)) {
          const H225_GloballyUniqueID & uns = id;
          PString uid = uns.AsString();
          fid = H460_FeatureID(uid);
      }

    return fid;
}

PBoolean H460_FeatureSet::CreateFeaturePDU(H460_Feature & Feat, H225_FeatureDescriptor & pdu,unsigned MessageID)
{

PTRACE(6,"H460\tEncoding " << PTracePDU(MessageID) << " PDU for " << Feat.GetFeatureIDAsString() );

    switch (MessageID) {
      case H460_MessageType::e_gatekeeperRequest:
            return Feat.OnSendGatekeeperRequest(pdu);

      case H460_MessageType::e_gatekeeperConfirm:
            return Feat.OnSendGatekeeperConfirm(pdu);

      case H460_MessageType::e_gatekeeperReject:
            return Feat.OnSendGatekeeperReject(pdu);

      case H460_MessageType::e_registrationRequest:
            return Feat.OnSendRegistrationRequest(pdu);

      case H460_MessageType::e_registrationConfirm:
            return Feat.OnSendRegistrationConfirm(pdu);

      case H460_MessageType::e_registrationReject:
            return Feat.OnSendRegistrationReject(pdu);

      case H460_MessageType::e_admissionRequest:
            return Feat.OnSendAdmissionRequest(pdu);

      case H460_MessageType::e_admissionConfirm:
            return Feat.OnSendAdmissionConfirm(pdu);

      case H460_MessageType::e_admissionReject:
            return Feat.OnSendAdmissionReject(pdu);

      case H460_MessageType::e_locationRequest:
            return Feat.OnSendLocationRequest(pdu);

      case H460_MessageType::e_locationConfirm:
            return Feat.OnSendLocationConfirm(pdu);

      case H460_MessageType::e_locationReject:
            return Feat.OnSendLocationReject(pdu);

      case H460_MessageType::e_nonStandardMessage:
            return Feat.OnSendNonStandardMessage(pdu);

      case H460_MessageType::e_serviceControlIndication:
            return Feat.OnSendServiceControlIndication(pdu);

      case H460_MessageType::e_serviceControlResponse:
            return Feat.OnSendServiceControlResponse(pdu);

      case H460_MessageType::e_unregistrationRequest:
               Feat.OnSendUnregistrationRequest(pdu);
               break;

      case H460_MessageType::e_inforequest:
           return Feat.OnSendInfoRequestMessage(pdu);

      case H460_MessageType::e_inforequestresponse:
           return Feat.OnSendInfoRequestResponseMessage(pdu);

      case H460_MessageType::e_disengagerequest:
           return Feat.OnSendDisengagementRequestMessage(pdu);

      case H460_MessageType::e_disengageconfirm:
           return Feat.OnSendDisengagementConfirmMessage(pdu);

/*
      case H460_MessageType::e_Endpoint:
            return Feat.OnSendEndpoint(pdu);
*/
      case H460_MessageType::e_setup:
            return Feat.OnSendSetup_UUIE(pdu);

      case H460_MessageType::e_alerting:
            return Feat.OnSendAlerting_UUIE(pdu);

      case H460_MessageType::e_callProceeding:
            return Feat.OnSendCallProceeding_UUIE(pdu);

      case H460_MessageType::e_connect:
            return Feat.OnSendCallConnect_UUIE(pdu);

      case H460_MessageType::e_facility:
            return Feat.OnSendFacility_UUIE(pdu);

      case H460_MessageType::e_releaseComplete:
            return Feat.OnSendReleaseComplete_UUIE(pdu);

      default:
            return Feat.OnSendUnAllocatedPDU(pdu);
    }

    return FALSE;
}

void H460_FeatureSet::ReadFeaturePDU(H460_Feature & Feat, const H225_FeatureDescriptor & pdu,unsigned MessageID)
{

PTRACE(6,"H460\tDecoding " << PTracePDU(MessageID) << " PDU for " << Feat.GetFeatureIDAsString() );

    switch (MessageID) {
      case H460_MessageType::e_gatekeeperRequest:
               Feat.OnReceiveGatekeeperRequest(pdu);
               break;

      case H460_MessageType::e_gatekeeperConfirm:
               Feat.OnReceiveGatekeeperConfirm(pdu);
               break;
      case H460_MessageType::e_gatekeeperReject:
               Feat.OnReceiveGatekeeperReject(pdu);
               break;
      case H460_MessageType::e_registrationRequest:
               Feat.OnReceiveRegistrationRequest(pdu);
               break;
      case H460_MessageType::e_registrationConfirm:
               Feat.OnReceiveRegistrationConfirm(pdu);
               break;
      case H460_MessageType::e_registrationReject:
               Feat.OnReceiveRegistrationReject(pdu);
               break;
      case H460_MessageType::e_admissionRequest:
               Feat.OnReceiveAdmissionRequest(pdu);
               break;
      case H460_MessageType::e_admissionConfirm:
               Feat.OnReceiveAdmissionConfirm(pdu);
               break;
      case H460_MessageType::e_admissionReject:
               Feat.OnReceiveAdmissionReject(pdu);
               break;
      case H460_MessageType::e_locationRequest:
               Feat.OnReceiveLocationRequest(pdu);
               break;
      case H460_MessageType::e_locationConfirm:
               Feat.OnReceiveLocationConfirm(pdu);
               break;
      case H460_MessageType::e_locationReject:
               Feat.OnReceiveLocationReject(pdu);
               break;
      case H460_MessageType::e_nonStandardMessage:
               Feat.OnReceiveNonStandardMessage(pdu);
               break;
      case H460_MessageType::e_serviceControlIndication:
               Feat.OnReceiveServiceControlIndication(pdu);
               break;
      case H460_MessageType::e_serviceControlResponse:
               Feat.OnReceiveServiceControlResponse(pdu);
               break;

      case H460_MessageType::e_unregistrationRequest:
               Feat.OnReceiveUnregistrationRequest(pdu);
               break;

      case H460_MessageType::e_inforequest:
               Feat.OnReceiveInfoRequestMessage(pdu);
               break;

      case H460_MessageType::e_inforequestresponse:
               Feat.OnReceiveInfoRequestResponseMessage(pdu);
               break;

      case H460_MessageType::e_disengagerequest:
               Feat.OnReceiveDisengagementRequestMessage(pdu);
               break;

      case H460_MessageType::e_disengageconfirm:
               Feat.OnReceiveDisengagementConfirmMessage(pdu);
               break;

//      case H460_MessageType::e_Endpoint:
//        return Feat.OnReceiveEndpoint(pdu);

      case H460_MessageType::e_setup:
               Feat.OnReceiveSetup_UUIE(pdu);
               break;
      case H460_MessageType::e_alerting:
               Feat.OnReceiveAlerting_UUIE(pdu);
               break;
      case H460_MessageType::e_callProceeding:
               Feat.OnReceiveCallProceeding_UUIE(pdu);
               break;
      case H460_MessageType::e_connect:
               Feat.OnReceiveCallConnect_UUIE(pdu);
               break;
      case H460_MessageType::e_facility:
               Feat.OnReceiveFacility_UUIE(pdu);
               break;
      case H460_MessageType::e_releaseComplete :
               Feat.OnReceiveReleaseComplete_UUIE(pdu);
               break;
      default:
               Feat.OnReceivedUnAllocatedPDU(pdu);
    }
}

PString H460_FeatureSet::PTracePDU(PINDEX id) const
{
    switch (id) {
        case H460_MessageType::e_gatekeeperRequest : return "GK Request";
        case H460_MessageType::e_gatekeeperConfirm : return "GK Confirm";
        case H460_MessageType::e_gatekeeperReject : return "GK Reject";
        case H460_MessageType::e_registrationRequest : return  "Reg Request";
        case H460_MessageType::e_registrationConfirm : return  "Reg Confirm";
        case H460_MessageType::e_registrationReject : return  "Reg Reject";
        case H460_MessageType::e_admissionRequest : return "Adm Reqest";
        case H460_MessageType::e_admissionConfirm : return "Adm Confirm";
        case H460_MessageType::e_admissionReject : return  "Adm Reject";
        case H460_MessageType::e_locationRequest : return  "Loc Request";
        case H460_MessageType::e_locationConfirm : return "Loc Confirm";
        case H460_MessageType::e_locationReject : return "Loc Reject";
        case H460_MessageType::e_nonStandardMessage : return  "NonStd";
        case H460_MessageType::e_serviceControlIndication :return "Ctrl Indication";
        case H460_MessageType::e_serviceControlResponse :return "Ctrl Response";
        case H460_MessageType::e_unregistrationRequest:return "Unreg Request";
        case H460_MessageType::e_inforequest:return "Info Request";
        case H460_MessageType::e_inforequestresponse:return "Info Response";
        case H460_MessageType::e_disengagerequest:return "Dis Request";
        case H460_MessageType::e_disengageconfirm:return "Dis Confirm";
//        case H460_MessageType::e_Endpoint :return "Endpoint";
        case H460_MessageType::e_setup :return "Setup";
        case H460_MessageType::e_alerting :return "Alerting";
        case H460_MessageType::e_callProceeding :return "CallProceed";
        case H460_MessageType::e_connect :return "Connect";
        case H460_MessageType::e_facility :return "Facility";
        case H460_MessageType::e_releaseComplete : return "ReleaseComplete";
        default: return "?";
    }
}

void H460_FeatureSet::DisableAllFeatures(int msgtype)
{
    if (Features.GetSize() > 0) {
        PTRACE(4,"H460\tRemoving all H.460 Features remote/Gk expected to advertise " << PTracePDU(msgtype));
        std::list<H460_FeatureID> removelist;
        for (PINDEX i =0; i < Features.GetSize(); i++) {
           H460_Feature & feat = Features.GetDataAt(i);
           if (feat.FeatureAdvertised(msgtype)) {
               PTRACE(4,"H460\tRemoving " << feat.GetFeatureIDAsString());
               removelist.push_back(feat.GetFeatureID());
               if (feat.GetFeaturePurpose() !=  H460_Feature::FeatureBaseAll)
                 delete &feat;
           } else {
               PTRACE(4,"H460\tPreserving " << feat.GetFeatureIDAsString());
           }
        }

        while (!removelist.empty()) {
            Features.RemoveAt(removelist.front());
            removelist.pop_front();
        }
    }
}

void H460_FeatureSet::ReceiveFeature(unsigned id, const H225_FeatureSet & Message, PBoolean genericData)
{
    ReadFeatureSetPDU(Message,id, genericData);
}

PBoolean H460_FeatureSet::SendFeature(unsigned id, H225_FeatureSet & Message, PBoolean advertise)
{
    return CreateFeatureSetPDU(Message,id, advertise);
}

void H460_FeatureSet::AttachEndPoint(H323EndPoint * _ep)
{
   PTRACE(4,"H460\tEndpoint Attached");
   ep = _ep;
}

void H460_FeatureSet::AttachBaseFeatureSet(H460_FeatureSet * _baseSet)
{
   baseSet = _baseSet;
}

PBoolean H460_FeatureSet::HasFeature(const H460_FeatureID & id)
{
    for (PINDEX i =0; i < Features.GetSize(); i++) {
       H460_Feature & feat = Features.GetDataAt(i);
        if (feat.GetFeatureID() == id) {
            return TRUE;
        }
    }
    return FALSE;
}

PBoolean H460_FeatureSet::SupportNonCallService(const H460_FeatureID & id) const
{
    for (PINDEX i =0; i < Features.GetSize(); i++) {
       H460_Feature & feat = Features.GetDataAt(i);
        if (feat.GetFeatureID() == id) {
            return feat.SupportNonCallService();
        }
    }
    return FALSE;
}

H460_Feature * H460_FeatureSet::GetFeature(const H460_FeatureID & id)
{
    if (HasFeature(id))
       return &Features[id];
    else
       return NULL;
}

#endif // H323_H460


