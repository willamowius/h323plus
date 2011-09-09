/*
 * h235con.h
 *
 * H.235 Encryption Context definitions class.
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
 * $Id$
 *
 */



/**Context for TLS Sockets.
   This class embodies a common environment for all connections made via TLS/SSL
   using the PTLS Sockets class. It includes such things as the version of SSL
   and certificates, CA's etc.
  */
struct ssl_ctx_st;
class H235Context : public PObject 
{
	PCLASSINFO(H235Context, PObject);

  public:

    /**Create a new context for TLS/SSL channels.
      */
    H235Context();

    /**Clean up the TLS/SSL context.
      */
    ~H235Context();

    /**Initialise the TLS/SSL context
      */
    void Initialise();

    /**Context Active
      */
    PBoolean IsActive();

    /**Get the internal TLS/SSL context structure.
      */
    operator ssl_ctx_st *() const { return m_context; }

  protected:
    void RandomSeed();

  private:

    PBoolean       m_isActive;
    ssl_ctx_st *   m_context;		/// Context Container

};