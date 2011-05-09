
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

#ifndef H323_STLDICTIONARY

#define H323Dictionary  PDictionary
#define H323DICTIONARY  PDICTIONARY

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


struct PSTLDictionaryOrder {
     int operator() ( unsigned p1, unsigned p2 ) const {
           return (p1 > p2);
     }
};

template <class K, class D> class PSTLDictionary : public PObject, 
                                                  public std::map< unsigned, std::pair<K, D*>, PSTLDictionaryOrder >
{
  public:
     PCLASSINFO(PSTLDictionary, PObject);
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

    PINDEX GetSize() const { return size(); }

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
          clear();
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
          for (std::map< unsigned, std::pair<K, D*>,PSTLDictionaryOrder>::const_iterator i = begin(); i != end(); ++i) {
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

          PAssert(ref < size(), psprintf("Index out of Bounds ref: %u sz: %u",ref,size()));
          std::map< unsigned, std::pair<K, D*>,PSTLDictionaryOrder>::const_iterator i = find(ref);
          return *(i->second.second);   
      };

       const K & InternalGetKeyAt(
           unsigned ref      ///< Ordinal position in dictionary for key.
           ) const
      {
          PWaitAndSignal m(dictMutex);

          PAssert(ref < size(), psprintf("Index out of Bounds ref: %u sz: %u",ref,size()));
          std::map< unsigned, std::pair<K, D*>,PSTLDictionaryOrder>::const_iterator i = find(ref);
          return i->second.first;   
      }

      D * InternalRemoveResort(unsigned pos) {
          unsigned newpos = pos;
          unsigned sz = size();
          D * dataPtr = NULL;
          std::map< unsigned, std::pair<K, D*>, PSTLDictionaryOrder >::iterator it = find(pos);
          if (it == end()) return NULL;
          if (disallowDeleteObjects)
            dataPtr = it->second.second;
          else
            delete it->second.second;  
          erase(it);
          
          for (unsigned i = pos+1; i < sz; ++i) {
             std::map< unsigned, std::pair<K, D*>, PSTLDictionaryOrder >::iterator j = find(i);
             DictionaryEntry entry =  make_pair(j->second.first,j->second.second) ;
             insert(pair<unsigned, std::pair<K, D*> >(newpos,entry));
             newpos++;
             erase(j);
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

          unsigned pos = size();
          DictionaryEntry entry = make_pair(key,obj);
          insert(pair<unsigned, std::pair<K, D*> >(pos,entry));
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

#endif  // PTLIB_VER < 2110

#endif // _PTLIB_EXTRAS_H







