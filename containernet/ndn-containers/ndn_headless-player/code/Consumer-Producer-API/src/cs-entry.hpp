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

#ifndef NFD_TABLE_CS_ENTRY_HPP
#define NFD_TABLE_CS_ENTRY_HPP

#include "common.hpp"
#include <ndn-cxx/util/crypto.hpp>

namespace ndn {

namespace cs {

class Entry;

/** \brief represents a CS entry
 */
class Entry : noncopyable
{
public:
  typedef std::map<int, std::list<Entry*>::iterator > LayerIterators;

  Entry();

  /** \brief releases reference counts on shared objects
   */
  void
  release();

  /** \brief returns the name of the Data packet stored in the CS entry
   *  \return{ NDN name }
   */
  const Name&
  getName() const;

  /** \brief Data packet is unsolicited if this particular NDN node
   *  did not receive an Interest packet for it, or the Interest packet has already expired
   *  \return{ True if the Data packet is unsolicited; otherwise False  }
   */
  bool
  isUnsolicited() const;

  /** \brief returns the absolute time when Data becomes expired
   *  \return{ Time (resolution up to time::milliseconds) }
   */
  const time::steady_clock::TimePoint&
  getStaleTime() const;

  /** \brief returns the Data packet stored in the CS entry
   */
  const Data&
  getData() const;

  /** \brief changes the content of CS entry and recomputes digest
   */
  void
  setData(const Data& data, bool isUnsolicited);

  /** \brief changes the content of CS entry and modifies digest
   */
  void
  setData(const Data& data, bool isUnsolicited, const ndn::ConstBufferPtr& digest);

  /** \brief refreshes the time when Data becomes expired
   *  according to the current absolute time.
   */
  void
  updateStaleTime();

  /** \brief returns the digest of the Data packet stored in the CS entry.
   */
  const ndn::ConstBufferPtr&
  getDigest() const;

  /** \brief saves the iterator pointing to the CS entry on a specific layer of skip list
   */
  void
  setIterator(int layer, const LayerIterators::mapped_type& layerIterator);

  /** \brief removes the iterator pointing to the CS entry on a specific layer of skip list
   */
  void
  removeIterator(int layer);

  /** \brief returns the table containing <layer, iterator> pairs.
   */
  const LayerIterators&
  getIterators() const;

private:
  /** \brief prints <layer, iterator> pairs.
   */
  void
  printIterators() const;

private:
  time::steady_clock::TimePoint m_staleAt;
  shared_ptr<const Data> m_dataPacket;

  bool m_isUnsolicited;
  Name m_nameWithDigest;

  mutable ndn::ConstBufferPtr m_digest;

  LayerIterators m_layerIterators;
};

inline
Entry::Entry()
{
}

inline const Name&
Entry::getName() const
{
  return m_nameWithDigest;
}

inline const Data&
Entry::getData() const
{
  return *m_dataPacket;
}

inline bool
Entry::isUnsolicited() const
{
  return m_isUnsolicited;
}

inline const time::steady_clock::TimePoint&
Entry::getStaleTime() const
{
  return m_staleAt;
}

inline const Entry::LayerIterators&
Entry::getIterators() const
{
  return m_layerIterators;
}

} // namespace cs
} // namespace nfd

#endif // NFD_TABLE_CS_ENTRY_HPP
