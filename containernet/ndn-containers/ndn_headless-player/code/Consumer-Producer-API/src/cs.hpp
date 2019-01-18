/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2016 Regents of the University of California.
 *
 * This file is part of Consumer/Producer API library.
 *
 * Consumer/Producer API library library is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * Consumer/Producer API library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with Consumer/Producer API, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of Consumer/Producer API authors and contributors.
 */

#ifndef NFD_TABLE_CS_HPP
#define NFD_TABLE_CS_HPP

#include "common.hpp"
#include "cs-entry.hpp"

#include <boost/multi_index/member.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/thread/mutex.hpp>

#include <queue>

namespace ndn {

typedef std::list<cs::Entry*> SkipListLayer;
typedef std::list<SkipListLayer*> SkipList;

class StalenessComparator
{
public:
  bool
  operator()(const cs::Entry* entry1, const cs::Entry* entry2) const
  {
    return entry1->getStaleTime() < entry2->getStaleTime();
  }
};

class UnsolicitedComparator
{
public:
  bool
  operator()(const cs::Entry* entry1, const cs::Entry* entry2) const
  {
    return entry1->isUnsolicited();
  }
};

// tags
class unsolicited;
class byStaleness;
class byArrival;

typedef boost::multi_index_container<
  cs::Entry*,
  boost::multi_index::indexed_by<

    // by arrival (FIFO)
    boost::multi_index::sequenced<
      boost::multi_index::tag<byArrival>
    >
  >
> CleanupIndex;

/** \brief represents Content Store
 */
class Cs : noncopyable
{
public:
  explicit
  Cs(int nMaxPackets = 65536); // ~500MB with average packet size = 8KB

  ~Cs();

  /** \brief inserts a Data packet
   *  This method does not consider the payload of the Data packet.
   *
   *  Packets are considered duplicate if the name matches.
   *  The new Data packet with the identical name, but a different payload
   *  is not placed in the Content Store
   *  \return{ whether the Data is added }
   */
  bool
  insert(const Data& data, bool isUnsolicited = false);

  /** \brief finds the best match Data for an Interest
   *  \return{ the best match, if any; otherwise 0 }
   */
  const Data*
  find(const Interest& interest);

  /** \brief deletes CS entry by the exact name
   */
  void
  erase(const Name& exactName);

  /** \brief sets maximum allowed size of Content Store (in packets)
   */
  void
  setLimit(size_t nMaxPackets);

  /** \brief returns maximum allowed size of Content Store (in packets)
   *  \return{ number of packets that can be stored in Content Store }
   */
  size_t
  getLimit() const;

  /** \brief returns current size of Content Store measured in packets
   *  \return{ number of packets located in Content Store }
   */
  size_t
  size() const;

protected:
  /** \brief removes one Data packet from Content Store based on replacement policy
   *  \return{ whether the Data was removed }
   */
  bool
  evictItem();

private:
  /** \brief returns True if the Content Store is at its maximum capacity
   *  \return{ True if Content Store is full; otherwise False}
   */
  bool
  isFull() const;

  /** \brief Computes the layer where new Content Store Entry is placed
   *
   *  Reference: "Skip Lists: A Probabilistic Alternative to Balanced Trees" by W.Pugh
   *  \return{ returns random layer (number) in a skip list}
   */
  size_t
  pickRandomLayer() const;

  /** \brief Inserts a new Content Store Entry in a skip list
   *  \return{ returns a pair containing a pointer to the CS Entry,
   *  and a flag indicating if the entry was newly created (True) or refreshed (False) }
   */
  std::pair<cs::Entry*, bool>
  insertToSkipList(const Data& data, bool isUnsolicited = false);

  /** \brief Removes a specific CS Entry from all layers of a skip list
   *  \return{ returns True if CS Entry was succesfully removed and False if CS Entry was not found}
   */
  bool
  eraseFromSkipList(cs::Entry* entry);

  /** \brief Implements child selector (leftmost, rightmost, undeclared).
   *  Operates on the first layer of a skip list.
   *
   *  startingPoint must be less than Interest Name.
   *  startingPoint can be equal to Interest Name only when the item is in the begin() position.
   *
   *  Iterates toward greater Names, terminates when CS entry falls out of Interest prefix.
   *  When childSelector = leftmost, returns first CS entry that satisfies other selectors.
   *  When childSelector = rightmost, it goes till the end, and returns CS entry that satisfies
   *  other selectors. Returned CS entry is the leftmost child of the rightmost child.
   *  \return{ the best match, if any; otherwise 0 }
   */
  const Data*
  selectChild(const Interest& interest, SkipListLayer::iterator startingPoint) const;

  /** \brief checks if Content Store entry satisfies Interest selectors (MinSuffixComponents,
   *  MaxSuffixComponents, Implicit Digest, MustBeFresh)
   *  \return{ true if satisfies all selectors; false otherwise }
   */
  bool
  doesComplyWithSelectors(const Interest& interest,
                          cs::Entry* entry,
                          bool doesInterestContainDigest) const;

  /** \brief interprets minSuffixComponent and name lengths to understand if Interest contains
   *  implicit digest of the data
   *  \return{ True if Interest name contains digest; False otherwise }
   */
  bool
  recognizeInterestWithDigest(const Interest& interest, cs::Entry* entry) const;

  /** \brief Prints contents of the skip list, starting from the top layer
   */
  void
  printSkipList() const;
  
private:
  SkipList m_skipList;
  CleanupIndex m_cleanupIndex;
  size_t m_nMaxPackets; // user defined maximum size of the Content Store in packets
  size_t m_nPackets;    // current number of packets in Content Store
  std::queue<cs::Entry*> m_freeCsEntries; // memory pool
  boost::mutex m_mutex;
};

} // namespace nfd

#endif // NFD_TABLE_CS_HPP
