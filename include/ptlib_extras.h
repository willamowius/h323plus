
// ptlib_extras.h:
/*
 * Ptlib Extras Implementation for the h323plus Library.
 *
 * Copyright (c) 2011 ISVO (Asia) Pte Ltd. All Rights Reserved.
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
 * H323plus Project (www.h323plus.org/)
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef _PTLIB_EXTRAS_H
#define _PTLIB_EXTRAS_H

#include "openh323buildopts.h"

#include <ptclib/delaychan.h>
#include <algorithm>
#include <queue>
#include <limits>

#include <ptclib/pnat.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////

#if PTLIB_VER < 2120
#define H323_INT INT
#else
#define H323_INT P_INT_PTR
#endif

#ifndef H323_STLDICTIONARY

#define H323Dictionary  PDictionary
#define H323DICTIONARY  PDICTIONARY

#define H323List  PList
#define H323LIST  PLIST

#else

#include <map>

template <class PAIR>
class deleteDictionaryEntry {
public:
	void operator()(const PAIR & p) { delete p.second.second; }
};


template <class E>
inline void deleteDictionaryEntries(const E & e)
{
	typedef typename E::value_type PT;
	std::for_each(e.begin(), e.end(), deleteDictionaryEntry<PT>());
}


struct PSTLSortOrder {
     int operator() ( unsigned p1, unsigned p2 ) const {
           return (p1 > p2);
     }
};

template <class K, class D> class PSTLDictionary : public PObject, 
                                                  public std::map< unsigned, std::pair<K, D*>, PSTLSortOrder >
{
     PCLASSINFO(PSTLDictionary, PObject);

  public:
  /**@name Construction */
  //@{
    /**Create a new, empty, dictionary.

       Note that by default, objects placed into the dictionary will be
       deleted when removed or when all references to the dictionary are
       destroyed.
     */
     PSTLDictionary() :disallowDeleteObjects(false) {}

     ~PSTLDictionary() {  RemoveAll(); }
  //@}

  /**@name Overrides from class PObject */
  //@{
    /**Make a complete duplicate of the dictionary. Note that all objects in
       the array are also cloned, so this will make a complete copy of the
       dictionary.
     */
    virtual PObject * Clone() const
      { return PNEW PSTLDictionary(*this); }
  //@}

  /**@name New functions for class */
  //@{
    /**Get the object contained in the dictionary at the \p key
       position. The hash table is used to locate the data quickly via the
       hash function provided by the \p key.

       The last key/data pair is remembered by the class so that subseqent
       access is very fast.

       @return
       reference to the object indexed by the key.
     */
    D & operator[](
      const K & key   ///< Key to look for in the dictionary.
    ) const
      {  unsigned pos = 0; return *InternalFindKey(key,pos); }

    /**Determine if the value of the object is contained in the hash table. The
       object values are compared, not the pointers.  So the objects in the
       collection must correctly implement the <code>PObject::Compare()</code>
       function. The hash table is used to locate the entry.

       @return
       true if the object value is in the dictionary.
     */
    PBoolean Contains(
      const K & key   ///< Key to look for in the dictionary.
    ) const { unsigned pos = 0;  return InternalContains(key,pos); }

    /**Remove an object at the specified \p key. The returned pointer is then
       removed using the <code>SetAt()</code> function to set that key value to
       NULL. If the <code>AllowDeleteObjects</code> option is set then the
       object is also deleted.

       @return
       pointer to the object being removed, or NULL if the key was not
       present in the dictionary. If the dictionary is set to delete objects
       upon removal, the value -1 is returned if the key existed prior to removal
       rather than returning an illegal pointer
     */
    virtual D * RemoveAt(
      const K & key   ///< Key for position in dictionary to get object.
    ) {
        return InternalRemoveKey(key);
      }

    /**Add a new object to the collection. If the objects value is already in
       the dictionary then the object is overrides the previous value. If the
       <code>AllowDeleteObjects</code> option is set then the old object is also deleted.

       The object is placed in the an ordinal position dependent on the keys
       hash function. Subsequent searches use the hash function to speed access
       to the data item.

       @return
       true if the object was successfully added.
     */
    virtual PBoolean SetAt(
      const K & key,  // Key for position in dictionary to add object.
      D * obj         // New object to put into the dictionary.
    ) { return InternalAddKey(key, obj); }

    /**Get the object at the specified key position. If the key was not in the
       collection then NULL is returned.

       @return
       pointer to object at the specified key.
     */
    virtual D * GetAt(
      const K & key   // Key for position in dictionary to get object.
    ) { unsigned pos = 0; return (D *)InternalFindKey(key,pos); }

    /**Get the key in the dictionary at the ordinal index position.

       The ordinal position in the dictionary is determined by the hash values
       of the keys and the order of insertion.

       The last key/data pair is remembered by the class so that subseqent
       access is very fast.

       @return
       reference to key at the index position.
     */
    const K & GetKeyAt(
      PINDEX pos  ///< Ordinal position in dictionary for key.
    ) const
      { return InternalGetKeyAt(pos); }

    /**Get the data in the dictionary at the ordinal index position.

       The ordinal position in the dictionary is determined by the hash values
       of the keys and the order of insertion.

       The last key/data pair is remembered by the class so that subseqent
       access is very fast.

       @return
       reference to data at the index position.
     */
    D & GetDataAt(
      PINDEX pos  ///< Ordinal position in dictionary for data.
    ) const
      { return InternalGetAt(pos); }

    PINDEX GetSize() const { return this->size(); }

   /**Remove all of the elements in the collection. This operates by
       continually calling <code>RemoveAt()</code> until there are no objects left.

       The objects are removed from the last, at index
       (GetSize()-1) toward the first at index zero.
     */
    virtual void RemoveAll()
      {  
          PWaitAndSignal m(dictMutex);

          if (!disallowDeleteObjects)
                deleteDictionaryEntries(*this);  
          this->clear();
      }

    PINLINE void AllowDeleteObjects(
      PBoolean yes = true   ///< New value for flag for deleting objects
      ) { disallowDeleteObjects = !yes; }

    /**Disallow the deletion of the objects contained in the collection. See
       the <code>AllowDeleteObjects()</code> function for more details.
     */
    void DisallowDeleteObjects() { disallowDeleteObjects = true; }
  //@}

    typedef struct std::pair<K,D*> DictionaryEntry;

  protected:

      PBoolean  disallowDeleteObjects;
      PMutex    dictMutex;

       D * InternalFindKey(
          const K & key,   ///< Key to look for in the dictionary.
          unsigned & ref        ///< Returned index
          ) const
      {
          typename std::map< unsigned, std::pair<K, D*>,PSTLSortOrder>::const_iterator i;
          for (i = this->begin(); i != this->end(); ++i) {
            if (i->second.first == key) {
               ref = i->first;
               return i->second.second;
            }
        }
        return NULL;
      };

       D & InternalGetAt(
          unsigned ref        ///< Returned index
          ) const
      {
          PWaitAndSignal m(dictMutex);

          PAssert(ref < this->size(), psprintf("Index out of Bounds ref: %u sz: %u",ref,this->size()));
          typename std::map< unsigned, std::pair<K, D*>,PSTLSortOrder>::const_iterator i = this->find(ref);
          PAssert(i != this->end(), psprintf("Item %u not found in collection sz: %u",ref,this->size()));
          return *(i->second.second);   
      };

       const K & InternalGetKeyAt(
           unsigned ref      ///< Ordinal position in dictionary for key.
           ) const
      {
          PWaitAndSignal m(dictMutex);

          PAssert(ref < this->size(), psprintf("Index out of Bounds ref: %u sz: %u",ref,this->size()));
          typename std::map< unsigned, std::pair<K, D*>,PSTLSortOrder>::const_iterator i = this->find(ref);
          PAssert(i != this->end(), psprintf("Item %u not found in collection sz: %u",ref,this->size()));
          return i->second.first;   
      }

      D * InternalRemoveResort(unsigned pos) {
          unsigned newpos = pos;
          unsigned sz = (unsigned)this->size();
          D * dataPtr = NULL;
          typename std::map< unsigned, std::pair<K, D*>, PSTLSortOrder >::iterator it = this->find(pos);
          if (it == this->end()) return NULL;
          if (disallowDeleteObjects)
            dataPtr = it->second.second;
          else
            delete it->second.second;  
          this->erase(it);
          
          for (unsigned i = pos+1; i < sz; ++i) {
             typename std::map< unsigned, std::pair<K, D*>, PSTLSortOrder >::iterator j = this->find(i);
             if (j != this->end()) {
                 DictionaryEntry entry =  make_pair(j->second.first,j->second.second) ;
                 this->insert(pair<unsigned, std::pair<K, D*> >(newpos,entry));
                 newpos++;
                 this->erase(j);
             }
          }

          return dataPtr;
      };

      D * InternalRemoveKey(
            const K & key   ///< Key to look for in the dictionary.
            )
      {
          PWaitAndSignal m(dictMutex);

          unsigned pos = 0;
          InternalFindKey(key,pos);
          return InternalRemoveResort(pos);
      }

      PBoolean InternalAddKey(
         const K & key,  // Key for position in dictionary to add object.
         D * obj         // New object to put into the dictionary.
         ) 
      { 
          PWaitAndSignal m(dictMutex);

          unsigned pos = (unsigned)this->size();
          DictionaryEntry entry = make_pair(key,obj);
          this->insert(pair<unsigned, std::pair<K, D*> >(pos,entry));
          return true;
      }


      PBoolean InternalContains(
          const K & key,   ///< Key to look for in the dictionary.
          unsigned & ref        ///< Returned index
          ) const
      {
        return (InternalFindKey(key,ref) != NULL);
      };
    
};

#define PSTLDICTIONARY(cls, K, D) typedef PSTLDictionary<K, D> cls

#define H323Dictionary  PSTLDictionary
#define H323DICTIONARY  PSTLDICTIONARY


//////////////////////////////////////////////////////////////////////////////////////

template <class PAIR>
class deleteListEntry {
public:
	void operator()(const PAIR & p) { delete p.second; }
};

template <class E>
inline void deleteListEntries(const E & e)
{
	typedef typename E::value_type PT;
	std::for_each(e.begin(), e.end(), deleteListEntry<PT>() );
}


template <class D> class PSTLList : public PObject, 
                                    public std::map< unsigned, D* , PSTLSortOrder >
{
     PCLASSINFO(PSTLList, PObject);

  public:
  /**@name Construction */
  //@{
    /**Create a new, empty, dictionary.

       Note that by default, objects placed into the dictionary will be
       deleted when removed or when all references to the dictionary are
       destroyed.
     */
     PSTLList() :disallowDeleteObjects(false) {}

     PSTLList(int /*dummy*/, const PObject * /*c*/)
        : disallowDeleteObjects(false) { }

     ~PSTLList() {  RemoveAll(); }
  //@}

  /**@name Overrides from class PObject */
  //@{
    /**Make a complete duplicate of the dictionary. Note that all objects in
       the array are also cloned, so this will make a complete copy of the
       dictionary.
     */
    virtual PObject * Clone() const
      { return PNEW PSTLList(*this); }
  //@}

    virtual PINDEX Append(
      D * obj   ///< New object to place into the collection.
      ) {  return InternalAddKey(obj);  }

    /**Insert a new object at the specified ordinal index. If the index is
       greater than the number of objects in the collection then the
       equivalent of the <code>Append()</code> function is performed.
       If not greater it will insert at the ordinal index and shuffle down ordinal values.

       @return
       index of the newly inserted object.
     */
    virtual PINDEX InsertAt(
      PINDEX index,   ///< Index position in collection to place the object.
      D * obj         ///< New object to place into the collection.
      ) { return InternalSetAt((unsigned)index,obj,false,true); }

    /**Remove the object at the specified ordinal index from the collection.
       If the AllowDeleteObjects option is set then the object is also deleted.

       Note if the index is beyond the size of the collection then the
       function will assert.

       @return
       pointer to the object being removed, or NULL if it was deleted.
     */
    virtual D * RemoveAt(
      PINDEX index   ///< Index position in collection to place the object.
      ) { return InternalRemoveKey((unsigned)index); }


    PBoolean Remove(
       D * obj   ///< Index position in collection to place the object.
       ) { unsigned index=0;
           if (!InternalFindIndex(index,obj))
              return false;
           return (InternalRemoveResort(index) != NULL);    
       }
          

    /**Set the object at the specified ordinal position to the new value. This
       will overwrite the existing entry. 
       This method will NOT delete the old object independently of the 
       AllowDeleteObjects option. Use <code>ReplaceAt()</code> instead.

       Note if the index is beyond the size of the collection then the
       function will assert.

       @return
       true if the object was successfully added.
     */
    virtual PBoolean SetAt(
      PINDEX index,   ///< Index position in collection to set.
      D * obj         ///< New value to place into the collection.
      ) {  return InternalSetAt((unsigned)index,obj);  }
    
    /**Set the object at the specified ordinal position to the new value. This
       will overwrite the existing entry. If the AllowDeleteObjects option is
       set then the old object is also deleted.
    
       Note if the index is beyond the size of the collection then the
       function will assert.
       
       @return
       true if the object was successfully replaced.
     */   
    virtual PBoolean ReplaceAt(
      PINDEX index,   ///< Index position in collection to set.
      D * obj         ///< New value to place into the collection.
      ) {  return InternalSetAt((unsigned)index,obj, true);  }

    /**Get the object at the specified ordinal position. If the index was
       greater than the size of the collection then NULL is returned.

       The object accessed in this way is remembered by the class and further
       access will be fast. Access to elements one either side of that saved
       element, and the head and tail of the list, will always be fast.

       @return
       pointer to object at the specified index.
     */
    virtual D * GetAt(
      PINDEX index  ///< Index position in the collection of the object.
    ) const { return InternalAt((unsigned)index); }


    D & operator[](PINDEX i) const { return InternalGetAt((unsigned)i); }

    /**Search the collection for the specific instance of the object. The
       object pointers are compared, not the values. A simple linear search
       from "head" of the list is performed.

       @return
       ordinal index position of the object, or P_MAX_INDEX.
     */
    virtual PINDEX GetObjectsIndex(
      const D * obj  ///< Object to find.
      ) const  { 
           unsigned index=0;
           if (InternalFindIndex(index,obj))
              return index;
           else 
              return P_MAX_INDEX;
    }

    /**Search the collection for the specified value of the object. The object
       values are compared, not the pointers.  So the objects in the
       collection must correctly implement the <code>PObject::Compare()</code>
       function. A simple linear search from "head" of the list is performed.

       @return
       ordinal index position of the object, or P_MAX_INDEX.
     */
    virtual PINDEX GetValuesIndex(
      const D & obj  ///< Object to find value of.
      ) const { 
          unsigned index=0;
           if (InternalIndex(index,obj))
              return index;
           else 
              return P_MAX_INDEX;
     }
  //@}


    PINDEX GetSize() const { return this->size(); }

    PBoolean IsEmpty() const { return (this->size() == 0); }

   /**Remove all of the elements in the collection. This operates by
       continually calling <code>RemoveAt()</code> until there are no objects left.

       The objects are removed from the last, at index
       (GetSize()-1) toward the first at index zero.
     */
    virtual void RemoveAll()
      {  
          //PWaitAndSignal m(dictMutex);

          if (IsEmpty()) return;

          if (!disallowDeleteObjects)
                deleteListEntries(*this);  
          this->clear();
      }

    void SetSize(PINDEX i)
      {  if (i == 0) RemoveAll();  }

    PINLINE void AllowDeleteObjects(
      PBoolean yes = true   ///< New value for flag for deleting objects
      ) { disallowDeleteObjects = !yes; }

    /**Disallow the deletion of the objects contained in the collection. See
       the <code>AllowDeleteObjects()</code> function for more details.
     */
    void DisallowDeleteObjects() { disallowDeleteObjects = true; }
  //@}

  protected:

      PBoolean  disallowDeleteObjects;
      PMutex    dictMutex;

      PBoolean InternalFindIndex(
          unsigned & ref,       ///< Returned index
          const D * data        ///< Data to match
          ) const
      {
          PWaitAndSignal m(dictMutex);

          if (data == NULL)
              return false;

          typename std::map< unsigned, D* , PSTLSortOrder>::const_iterator i;
          for (i = this->begin(); i != this->end(); ++i) {
            if (i->second == data) {
               ref = i->first;
               return true;
            }
          }
          return false;
      };

      PBoolean InternalIndex(
          unsigned & ref,       ///< Returned index
          const D & data        ///< Data to match
          ) const
      {
          PWaitAndSignal m(dictMutex);

          typename std::map< unsigned, D* , PSTLSortOrder>::const_iterator i;
          for (i = this->begin(); i != this->end(); ++i) {
            if (*(i->second) == data) {
               ref = i->first;
               return true;
            }
          }
          return false;
      };


       D & InternalGetAt(
          unsigned ref        ///< Returned index
          ) const
      {
          PWaitAndSignal m(dictMutex);

          PAssert(ref < this->size(), psprintf("Index out of Bounds ref: %u sz: %u",ref,this->size()));
          typename std::map< unsigned, D*, PSTLSortOrder>::const_iterator i = this->find(ref);
          PAssert(i != this->end() , psprintf("Index not found: %u sz: %u",ref,this->size()));
          return *(i->second);   
      };

      D * InternalAt(
          unsigned ref        ///< Returned index
          ) const
      {
          PWaitAndSignal m(dictMutex);

          PAssert(ref < this->size(), psprintf("Index out of Bounds ref: %u sz: %u",ref,this->size()));
          typename std::map< unsigned, D*, PSTLSortOrder>::const_iterator i = this->find(ref);
		  if (i != this->end()) return i->second;  
		  else return NULL;
      };


      D * InternalRemoveResort(unsigned pos) {
          unsigned newpos = pos;
          unsigned sz = (unsigned)this->size();
          D * dataPtr = NULL;
          typename std::map< unsigned, D*, PSTLSortOrder >::iterator it = this->find(pos);
          if (it == this->end()) return NULL;
          if (disallowDeleteObjects)
            dataPtr = it->second;
          else
            delete it->second;  
          this->erase(it);
          
          for (unsigned i = pos+1; i < sz; ++i) {
             typename std::map< unsigned, D*, PSTLSortOrder >::iterator j = this->find(i);
             PAssert(j != this->end() , psprintf("Index not found: %u sz: %u",i,this->size()));
             D* entry =  j->second;
             this->insert(std::pair<unsigned, D*>(newpos,entry));
             newpos++;
             this->erase(j);
          }

          return dataPtr;
      };

      D * InternalRemoveKey(
            unsigned pos   ///< Key to look for in the dictionary.
            )
      {
          PWaitAndSignal m(dictMutex);

          return InternalRemoveResort(pos);
      }

      PINDEX InternalAddKey(
         D * obj         // New object to put into list.
         ) 
      {
          PWaitAndSignal m(dictMutex);

          if (obj == NULL)
              return -1;

          unsigned pos = (unsigned)this->size();
          this->insert(std::pair<unsigned, D*>(pos,obj));
          return pos;
      }

      PINDEX InternalSetAt(
          unsigned ref,                ///< Index position in collection to set.
          D * obj,                     ///< New value to place into the collection.
          PBoolean replace = false,
          PBoolean reorder = false
          ) 
      {     
          if (obj == NULL)
              return -1;

          if (ref >= (unsigned)GetSize())
              return InternalAddKey(obj);

          PWaitAndSignal m(dictMutex);

          if (!reorder) {
              typename std::map< unsigned, D*, PSTLSortOrder>::iterator it = this->find(ref);
              if (it != this->end()) {
                  if (replace) 
                      delete it->second;  
                  this->erase(it);
              }
          } else {
              unsigned sz = (unsigned)GetSize();
              if (sz > 0) {
                  unsigned newpos = sz;
                  for (unsigned i = sz; i-- > ref; ) {
                     typename std::map< unsigned, D*, PSTLSortOrder >::iterator it = this->find(i);
                     if (it != this->end()) {
                         D* entry =  it->second;
                         this->insert(std::pair<unsigned, D*>(newpos,entry));
                         this->erase(it);
                         newpos--;
                     }
                  }
              }
          }
          this->insert(std::pair<unsigned, D*>(ref,obj));
          return ref;        
      }
};

#define H323List  PSTLList
#define H323LIST(cls, D) typedef H323List<D> cls;

#endif  // H323_STLDICTIONARY

#define H323_DECLARELIST(cls, T) \
  H323LIST(cls##_PTemplate, T); \
  PDECLARE_CLASS(cls, H323List<T>) \
  protected: \
    cls(int dummy, const cls * c) \
      : H323List<T>(dummy, c) { } \
  public: \
    cls() \
      : H323List<T>() { } \
    virtual PObject * Clone() const \
      { return PNEW cls(0, this); } \


#ifdef H323_FRAMEBUFFER

class H323FRAME {
public:
   
     struct Info {
        unsigned  m_sequence;
        unsigned  m_timeStamp;
        PBoolean  m_marker;
        PInt64    m_receiveTime;
     };

     int operator() ( const std::pair<H323FRAME::Info, PBYTEArray>& p1,
                      const std::pair<H323FRAME::Info, PBYTEArray>& p2 ) {
           return (p1.first.m_sequence > p2.first.m_sequence);
     }
};

typedef std::priority_queue< std::pair<H323FRAME::Info, PBYTEArray >, 
        std::vector< std::pair<H323FRAME::Info, PBYTEArray> >,H323FRAME > RTP_Sortedframes;


class H323_FrameBuffer : public PThread
{
    PCLASSINFO(H323_FrameBuffer, PThread);

protected:
    RTP_Sortedframes  m_buffer;
    PBoolean m_threadRunning;

    unsigned m_frameMarker;           // Number of complete frames;
    PBoolean m_frameOutput;           // Signal to start output
    unsigned m_frameStartTime;        // Time to of first packet
    PInt64   m_StartTimeStamp;        // Start Time Stamp
    float    m_calcClockRate;         // Calculated Clockrate (default 90)
    float    m_packetReceived;        // Packet Received count
    float    m_oddTimeCount;          // Odd time count
    float    m_lateThreshold;         // Threshold (percent) of late packets
    PBoolean m_increaseBuffer;        // Increase Buffer
    float    m_lossThreshold;         // Percentage loss
    float    m_lossCount;             // Actual Packet lost
    float    m_frameCount;            // Number of Frames Received from last Fast Picture Update
    unsigned m_lastSequence;          // Last Received Sequence Number
    PInt64   m_RenderTimeStamp;       // local realTime to render.

    PMutex bufferMutex;
    PAdaptiveDelay m_outputDelay;
    PBoolean  m_exit;

public:
    H323_FrameBuffer()
    : PThread(10000, NoAutoDeleteThread, HighestPriority, "FrameBuffer"), m_threadRunning(false),
      m_frameMarker(0), m_frameOutput(false), m_frameStartTime(0), 
      m_StartTimeStamp(0), m_calcClockRate(90),
      m_packetReceived(0), m_oddTimeCount(0), m_lateThreshold(5.0), m_increaseBuffer(false),
      m_lossThreshold(1.0), m_lossCount(0), m_frameCount(0), 
      m_lastSequence(0), m_RenderTimeStamp(0), m_exit(false)
    {}

    ~H323_FrameBuffer()
    { 
        if (m_threadRunning)
            m_exit = true;  
    }

    void Start()
    {
        m_threadRunning=true;
        Resume();
    }

	PBoolean IsRunning() 
	{
		return m_threadRunning;
	}

    virtual void FrameOut(PBYTEArray & /*frame*/, PInt64 /*receiveTime*/, unsigned /*clock*/, PBoolean /*fup*/, PBoolean /*flow*/) {};

    void Main() {

        PBYTEArray frame;
        PTimeInterval lastMarker;
        int delay=0;
        PBoolean fup=false;

        while (!m_exit) {
            while (m_frameOutput && m_frameMarker > 0) {
                if (m_buffer.empty()) {
                    if (m_frameMarker > 0)
                        m_frameMarker--;
                    break;
                }

                // fixed local render clock
                if (m_RenderTimeStamp == 0)
                    m_RenderTimeStamp = PTimer::Tick().GetMilliSeconds();

                PBoolean flow = false;

                H323FRAME::Info info;
                  bufferMutex.Wait();
                    info = m_buffer.top().first;
                    frame.SetSize(m_buffer.top().second.GetSize());
                    memcpy(frame.GetPointer(), m_buffer.top().second, m_buffer.top().second.GetSize());
                    unsigned lastTimeStamp = info.m_timeStamp;
                    m_buffer.pop();
                    if (info.m_marker && !m_buffer.empty()) { // Peek ahead for next timestamp
                        delay = (m_buffer.top().first.m_timeStamp - lastTimeStamp)/(unsigned)m_calcClockRate;
                        if (delay <= 0 || delay > 200 || (lastTimeStamp > m_buffer.top().first.m_timeStamp)) {
                           delay = 0;
                           m_RenderTimeStamp = PTimer::Tick().GetMilliSeconds();
                           fup = true;
                        } 
                    }
                  bufferMutex.Signal();

                  if (m_exit)
                      break;

                  m_frameCount++;
                  if (m_lastSequence) {
                     unsigned diff = info.m_sequence - m_lastSequence - 1;
                     if (diff > 0) {
                         PTRACE(5,"RTPBUF\tDetected loss of " << diff << " packets."); 
                         m_lossCount = m_lossCount + diff;
                     }
                  }
                  m_lastSequence = info.m_sequence;

                  if (!fup) 
                      fup = ((m_lossCount/m_frameCount)*100.0 > m_lossThreshold);

                  FrameOut(frame, info.m_receiveTime, (unsigned)m_calcClockRate, fup, flow);
                  frame.SetSize(0);
                  if (fup) {
                     m_lossCount = m_frameCount = 0;
                     fup = false;
                  }

                  if (info.m_marker && m_frameMarker > 0) {
                        if (m_increaseBuffer) {
                            delay = delay*2;
                            m_increaseBuffer=false;
                        }
                      m_RenderTimeStamp+=delay;
                      PInt64 nowTime = PTimer::Tick().GetMilliSeconds();
                      unsigned ldelay = (unsigned)((m_RenderTimeStamp > nowTime)? m_RenderTimeStamp - nowTime : 0);
                      if (ldelay > 200 || m_frameMarker > 5) ldelay = 0;
                      if (!ldelay)  m_RenderTimeStamp = nowTime;
                      m_frameMarker--;
                      m_outputDelay.Delay(ldelay);
                  } else 
                      PThread::Sleep(2);
            }
            PThread::Sleep(5);
        }
        bufferMutex.Wait();
        while (!m_buffer.empty()) 
           m_buffer.pop();
        bufferMutex.Signal();

        m_threadRunning = false;
    }

    virtual PBoolean FrameIn(unsigned seq,  unsigned time, PBoolean marker, unsigned payload, const PBYTEArray & frame)
    {

        if (!m_threadRunning) {  // Start thread on first frame.
            Resume();
            m_threadRunning = true;
        }

        if (m_exit)
            return false;

        PInt64 now = PTimer::Tick().GetMilliSeconds();
        // IF we haven't started or the clockrate goes out of bounds.
        if (!m_frameStartTime) {
            m_frameStartTime = time;
            m_StartTimeStamp = PTimer::Tick().GetMilliSeconds();
        } else if (marker && m_frameOutput) {
            m_calcClockRate = (float)(time - m_frameStartTime)/(PTimer::Tick().GetMilliSeconds() - m_StartTimeStamp);
            if (m_calcClockRate > 100 || m_calcClockRate < 40 || (m_calcClockRate == numeric_limits<unsigned int>::infinity( ))) {
                PTRACE(4,"RTPBUF\tErroneous ClockRate: Resetting...");
                m_calcClockRate = 90;
                m_frameStartTime = time;
                m_StartTimeStamp = PTimer::Tick().GetMilliSeconds();
            }
        }
            
        H323FRAME::Info info;
           info.m_sequence = seq;
           info.m_marker = marker;
           info.m_timeStamp = time;
           info.m_receiveTime = now;

        PBYTEArray * m_frame = new PBYTEArray(payload+12);
        memcpy(m_frame->GetPointer(),(PRemoveConst(PBYTEArray,&frame))->GetPointer(),payload+12);

        bufferMutex.Wait();
        m_packetReceived++;
        if (m_frameOutput && !m_buffer.empty() && info.m_sequence < m_buffer.top().first.m_sequence) {
            m_oddTimeCount++;
            PTRACE(6,"RTPBUF\tLate Packet Received " << (m_oddTimeCount/m_packetReceived)*100.0 << "%");
            if ((m_oddTimeCount/m_packetReceived)*100.0 > m_lateThreshold) {
                PTRACE(4,"RTPBUF\tLate Packet threshold reached increasing buffer.");
                m_increaseBuffer = true;
                m_packetReceived=0;
                m_oddTimeCount=0;
            }
        }
        m_buffer.push(pair<H323FRAME::Info, PBYTEArray>(info,*m_frame));
        delete m_frame;
        bufferMutex.Signal();

        if (marker) {
           m_frameMarker++;
           // Make sure we have a min of 3 frames in buffer to start
           if (!m_frameOutput && m_frameMarker > 2)
              m_frameOutput = true;
        }

        return true;
    }
};

#endif  // H323_FRAMEBUFFER

#if PTLIB_VER < 2130

#define H323UDPSocket PUDPSocket

#else
class H323UDPSocket : public PUDPSocket
{
    PCLASSINFO(H323UDPSocket, PUDPSocket);

    public:
        H323UDPSocket() {}
        virtual PBoolean IsAlternateAddress(const Address & /*address*/, WORD /*port*/) { return false; }
        virtual PBoolean DoPseudoRead(int & /*selectStatus*/) { return false; }

        virtual PBoolean Close() {
#if 0 //P_QWAVE - QoS not supported yet
            return PIPSocket::Close();
#else
            return PSocket::Close();
#endif
        }
};

#endif // H323UDPSocket

#if PTLIB_VER < 2130

#define H323NatMethod   PNatMethod
#define H323NatList     PNatList
#define H323NatStrategy PNatStrategy

#else

class H323NatMethod  : public PNatMethod
{
    PCLASSINFO(H323NatMethod,PNatMethod);

    public:
        H323NatMethod() : PNatMethod(0) {}

        virtual PString GetServer() const { return PString(); }
        virtual PCaselessString GetMethodName() const { return "dummy"; }

        PString GetName() { return GetMethodName(); }

        virtual bool IsAvailable(
          const PIPSocket::Address & binding = PIPSocket::GetDefaultIpAny()  ///< Interface to see if NAT is available on
        ) = 0;

    protected:
      struct PortInfo {
      PortInfo(WORD port = 0)
      : basePort(port), maxPort(port), currentPort(port) {}
          PMutex mutex;
          WORD   basePort;
          WORD   maxPort;
          WORD   currentPort;
    } singlePortInfo, pairedPortInfo;

    virtual PNATUDPSocket * InternalCreateSocket(Component /*component*/, PObject * /*context*/)  { return NULL; }
    virtual void InternalUpdate() {};
};

H323LIST(H323NatList, PNatMethod);

class H323NatStrategy : public PObject
{
public :

    H323NatStrategy() {};
    ~H323NatStrategy() { natlist.RemoveAll(); }

  void AddMethod(PNatMethod * method) { natlist.Append(method); }

  PNatMethod * GetMethod(const PIPSocket::Address & address = PIPSocket::GetDefaultIpAny()) {
      for (PINDEX i=0; i < natlist.GetSize(); ++i) {
        if (natlist[i].IsAvailable(address))
          return &natlist[i];
      }
      return NULL;
  }

  PNatMethod * GetMethodByName(const PString & name) {
      for (PINDEX i=0; i < natlist.GetSize(); ++i) {
        if (natlist[i].GetMethodName() == name) {
          return &natlist[i];
        }
      }
      return NULL;
  }

  PBoolean RemoveMethod(const PString & meth) {
      for (PINDEX i=0; i < natlist.GetSize(); ++i) {
        if (natlist[i].GetMethodName() == meth) {
          natlist.RemoveAt(i);
          return true;
        }
      }
      return false;
  }

  void SetPortRanges( WORD portBase, WORD portMax = 0, WORD portPairBase = 0, WORD portPairMax = 0) {
      for (PINDEX i=0; i < natlist.GetSize(); ++i)
        natlist[i].SetPortRanges(portBase, portMax, portPairBase, portPairMax);
  }

  H323NatList & GetNATList() {  return natlist; }

  PNatMethod * LoadNatMethod(const PString & name) {
    PPluginManager * pluginMgr = &PPluginManager::GetPluginManager();
    return (PNatMethod *)pluginMgr->CreatePlugin(name, "PNatMethod");
  }

  static PStringArray GetRegisteredList() {
    PPluginManager * pluginMgr = &PPluginManager::GetPluginManager();
    return pluginMgr->GetPluginsProviding("PNatMethod", false);
  }

private:
  H323NatList natlist;
};

#endif  // H323_NATMETHOD

#endif // _PTLIB_EXTRAS_H







