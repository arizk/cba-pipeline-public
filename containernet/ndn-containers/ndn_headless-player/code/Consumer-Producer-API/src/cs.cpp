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

#include "cs.hpp"
#include <ndn-cxx/util/crypto.hpp>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>

#define SKIPLIST_MAX_LAYERS 32
#define SKIPLIST_PROBABILITY 25         // 25% (p = 1/4)

namespace ndn {

Cs::Cs(int nMaxPackets)
  : m_nMaxPackets(nMaxPackets)
  , m_nPackets(0)
{
  SkipListLayer* zeroLayer = new SkipListLayer();
  m_skipList.push_back(zeroLayer);

  for (size_t i = 0; i < m_nMaxPackets; i++)
    m_freeCsEntries.push(new cs::Entry());
}

Cs::~Cs()
{
  // evict all items from CS
  while (evictItem())
    ;

  BOOST_ASSERT(m_freeCsEntries.size() == m_nMaxPackets);

  while (!m_freeCsEntries.empty())
    {
      delete m_freeCsEntries.front();
      m_freeCsEntries.pop();
    }
}

size_t
Cs::size() const
{
  return m_nPackets; // size of the first layer in a skip list
}

void
Cs::setLimit(size_t nMaxPackets)
{
  size_t oldNMaxPackets = m_nMaxPackets;
  m_nMaxPackets = nMaxPackets;

  while (isFull())
    {
      if (!evictItem())
        break;
    }
    
  if (m_nMaxPackets >= oldNMaxPackets) 
    {
      for (size_t i = oldNMaxPackets; i < m_nMaxPackets; i++) 
        {
          m_freeCsEntries.push(new cs::Entry());
        }
    }
  else 
    {
      for (size_t i = oldNMaxPackets; i > m_nMaxPackets; i--) 
      {
        delete m_freeCsEntries.front();
        m_freeCsEntries.pop();
      }
    }
}

size_t
Cs::getLimit() const
{
  return m_nMaxPackets;
}

//Reference: "Skip Lists: A Probabilistic Alternative to Balanced Trees" by W.Pugh
std::pair<cs::Entry*, bool>
Cs::insertToSkipList(const Data& data, bool isUnsolicited)
{
  BOOST_ASSERT(m_cleanupIndex.size() <= size());
  BOOST_ASSERT(m_freeCsEntries.size() > 0);
  
  m_mutex.lock();

  // take entry for the memory pool
  cs::Entry* entry = m_freeCsEntries.front();
  m_freeCsEntries.pop();
  m_nPackets++;
  entry->setData(data, isUnsolicited);

  bool insertInFront = false;
  bool isIterated = false;
  SkipList::reverse_iterator topLayer = m_skipList.rbegin();
  SkipListLayer::iterator updateTable[SKIPLIST_MAX_LAYERS];
  SkipListLayer::iterator head = (*topLayer)->begin();

  if (!(*topLayer)->empty())
    {
      //start from the upper layer towards bottom
      int layer = m_skipList.size() - 1;
      for (SkipList::reverse_iterator rit = topLayer; rit != m_skipList.rend(); ++rit)
        {
          //if we didn't do any iterations on the higher layers, start from the begin() again
          if (!isIterated)
            head = (*rit)->begin();

          updateTable[layer] = head;

          if (head != (*rit)->end())
            {
              // it can happen when begin() contains the element in front of which we need to insert
              if (!isIterated && ((*head)->getName() >= entry->getName()))
                {
                  --updateTable[layer];
                  insertInFront = true;
                }
              else
                {
                  SkipListLayer::iterator it = head;

                  while ((*it)->getName() < entry->getName())
                    {
                      head = it;
                      updateTable[layer] = it;
                      isIterated = true;

                      ++it;
                      if (it == (*rit)->end())
                        break;
                    }
                }
            }

          if (layer > 0)
            head = (*head)->getIterators().find(layer - 1)->second; // move HEAD to the lower layer

          layer--;
        }
    }
  else
    {
      updateTable[0] = (*topLayer)->begin(); //initialization
    }

  head = updateTable[0];
  ++head; // look at the next slot to check if it contains a duplicate

  bool isCsEmpty = (size() == 0);
  bool isInBoundaries = (head != (*m_skipList.begin())->end());
  bool isNameIdentical = false;
  if (!isCsEmpty && isInBoundaries)
    {
      isNameIdentical = (*head)->getName() == entry->getName();
    }

  //check if this is a duplicate packet
  if (isNameIdentical)
    {
      (*head)->setData(data, isUnsolicited, entry->getDigest()); //updates stale time

      // new entry not needed, returning to the pool
      entry->release();
      m_freeCsEntries.push(entry);
      m_nPackets--;

      m_mutex.unlock();
      return std::make_pair(*head, false);
    }

  size_t randomLayer = pickRandomLayer();

  while (m_skipList.size() < randomLayer + 1)
    {
      SkipListLayer* newLayer = new SkipListLayer();
      m_skipList.push_back(newLayer);

      updateTable[(m_skipList.size() - 1)] = newLayer->begin();
    }

  size_t layer = 0;
  for (SkipList::iterator i = m_skipList.begin();
       i != m_skipList.end() && layer <= randomLayer; ++i)
    {
      if (updateTable[layer] == (*i)->end() && !insertInFront)
        {
          (*i)->push_back(entry);
          SkipListLayer::iterator last = (*i)->end();
          --last;
          entry->setIterator(layer, last);
        }
      else if (updateTable[layer] == (*i)->end() && insertInFront)
        {
          (*i)->push_front(entry);
          entry->setIterator(layer, (*i)->begin());
        }
      else
        {
          ++updateTable[layer]; // insert after
          SkipListLayer::iterator position = (*i)->insert(updateTable[layer], entry);
          entry->setIterator(layer, position); // save iterator where item was inserted
        }
      layer++;
    }

  m_mutex.unlock();
  return std::make_pair(entry, true);
}

bool
Cs::insert(const Data& data, bool isUnsolicited)
{  
  if (isFull())
    {
      evictItem();
    }

  //pointer and insertion status
  std::pair<cs::Entry*, bool> entry = insertToSkipList(data, isUnsolicited);

  //new entry
  if (static_cast<bool>(entry.first) && (entry.second == true))
    {
      m_cleanupIndex.push_back(entry.first);
      return true;
    }

  return false;
}

size_t
Cs::pickRandomLayer() const
{
  int layer = -1;
  int randomValue;

  do
    {
      layer++;
      randomValue = rand() % 100 + 1;
    }
  while ((randomValue < SKIPLIST_PROBABILITY) && (layer < SKIPLIST_MAX_LAYERS));

  return static_cast<size_t>(layer);
}

bool
Cs::isFull() const
{
  if (size() >= m_nMaxPackets) //size of the first layer vs. max size
    return true;

  return false;
}

bool
Cs::eraseFromSkipList(cs::Entry* entry)
{
  m_mutex.lock();
  bool isErased = false;

  const std::map<int, std::list<cs::Entry*>::iterator>& iterators = entry->getIterators();

  if (!iterators.empty())
    {
      int layer = 0;

      for (SkipList::iterator it = m_skipList.begin(); it != m_skipList.end(); )
        {
          std::map<int, std::list<cs::Entry*>::iterator>::const_iterator i = iterators.find(layer);

          if (i != iterators.end())
            {
              (*it)->erase(i->second);
              entry->removeIterator(layer);
              isErased = true;

              //remove layers that do not contain any elements (starting from the second layer)
              if ((layer != 0) && (*it)->empty())
                {
                  delete *it;
                  it = m_skipList.erase(it);
                }
              else
                ++it;

              layer++;
            }
          else
            break;
      }
    }

  //delete entry;
  if (isErased)
  {
    entry->release();
    m_freeCsEntries.push(entry);
    m_nPackets--;
  }

  m_mutex.unlock();
  return isErased;
}

bool
Cs::evictItem()
{
  if (!m_cleanupIndex.get<byArrival>().empty())
  {
    eraseFromSkipList(*m_cleanupIndex.get<byArrival>().begin());
    m_cleanupIndex.get<byArrival>().erase(m_cleanupIndex.get<byArrival>().begin());
    return true;
  }

  return false;
}

const Data*
Cs::find(const Interest& interest)
{
  m_mutex.lock();
  
  bool isIterated = false;
  SkipList::const_reverse_iterator topLayer = m_skipList.rbegin();
  SkipListLayer::iterator head = (*topLayer)->begin();

  if (!(*topLayer)->empty())
    {
      //start from the upper layer towards bottom
      int layer = m_skipList.size() - 1;
      for (SkipList::const_reverse_iterator rit = topLayer; rit != m_skipList.rend(); ++rit)
        {
          //if we didn't do any iterations on the higher layers, start from the begin() again
          if (!isIterated)
            head = (*rit)->begin();

          if (head != (*rit)->end())
            {
              // it happens when begin() contains the element we want to find
              if (!isIterated && (interest.getName().isPrefixOf((*head)->getName())))
                {
                  if (layer > 0)
                    {
                      layer--;
                      continue; // try lower layer
                    }
                  else
                    {
                      isIterated = true;
                    }
                }
              else
                {
                  SkipListLayer::iterator it = head;

                  while ((*it)->getName() < interest.getName())
                    {
                      head = it;
                      isIterated = true;

                      ++it;
                      if (it == (*rit)->end())
                        break;
                    }
                }
            }

          if (layer > 0)
            {
              head = (*head)->getIterators().find(layer - 1)->second; // move HEAD to the lower layer
            }
          else //if we reached the first layer
            {
              if (isIterated)
              {
                m_mutex.unlock();
                return selectChild(interest, head);
              }
            }

          layer--;
        }
    }

  m_mutex.unlock();
  return 0;
}

const Data*
Cs::selectChild(const Interest& interest, SkipListLayer::iterator startingPoint) const
{
  BOOST_ASSERT(startingPoint != (*m_skipList.begin())->end());

  if (startingPoint != (*m_skipList.begin())->begin())
    {
      BOOST_ASSERT((*startingPoint)->getName() < interest.getName());
    }

  bool hasLeftmostSelector = (interest.getChildSelector() <= 0);
  bool hasRightmostSelector = !hasLeftmostSelector;

  if (hasLeftmostSelector)
    {
      bool doesInterestContainDigest = recognizeInterestWithDigest(interest, *startingPoint);
      bool isInPrefix = false;

      if (doesInterestContainDigest)
        {
          isInPrefix = interest.getName().getPrefix(-1).isPrefixOf((*startingPoint)->getName());
        }
      else
        {
          isInPrefix = interest.getName().isPrefixOf((*startingPoint)->getName());
        }

      if (isInPrefix)
        {
          if (doesComplyWithSelectors(interest, *startingPoint, doesInterestContainDigest))
            {
              return &(*startingPoint)->getData();
            }
        }
    }

  //iterate to the right
  SkipListLayer::iterator rightmost = startingPoint;
  if (startingPoint != (*m_skipList.begin())->end())
    {
      SkipListLayer::iterator rightmostCandidate = startingPoint;
      Name currentChildPrefix("");

      while (true)
        {
          ++rightmostCandidate;

          bool isInBoundaries = (rightmostCandidate != (*m_skipList.begin())->end());
          bool isInPrefix = false;
          bool doesInterestContainDigest = false;
          if (isInBoundaries)
            {
              doesInterestContainDigest = recognizeInterestWithDigest(interest,
                                                                      *rightmostCandidate);

              if (doesInterestContainDigest)
                {
                  isInPrefix = interest.getName().getPrefix(-1)
                                 .isPrefixOf((*rightmostCandidate)->getName());
                }
              else
                {
                  isInPrefix = interest.getName().isPrefixOf((*rightmostCandidate)->getName());
                }
            }

          if (isInPrefix)
            {
              if (doesComplyWithSelectors(interest, *rightmostCandidate, doesInterestContainDigest))
                {
                  if (hasLeftmostSelector)
                    {
                      return &(*rightmostCandidate)->getData();
                    }

                  if (hasRightmostSelector)
                    {
                      if (doesInterestContainDigest)
                        {
                          // get prefix which is one component longer than Interest name
                          // (without digest)
                          const Name& childPrefix = (*rightmostCandidate)->getName()
                                                      .getPrefix(interest.getName().size());
                          
                          if (currentChildPrefix.empty() || (childPrefix != currentChildPrefix))
                            {
                              currentChildPrefix = childPrefix;
                              rightmost = rightmostCandidate;
                            }
                        }
                      else
                        {
                          // get prefix which is one component longer than Interest name
                          const Name& childPrefix = (*rightmostCandidate)->getName()
                                                      .getPrefix(interest.getName().size() + 1);
                          
                          if (currentChildPrefix.empty() || (childPrefix != currentChildPrefix))
                            {
                              currentChildPrefix = childPrefix;
                              rightmost = rightmostCandidate;
                            }
                        }
                    }
                }
            }
          else
            break;
        }
    }

  if (rightmost != startingPoint)
    {
      return &(*rightmost)->getData();
    }

  if (hasRightmostSelector) // if rightmost was not found, try starting point
    {
      bool doesInterestContainDigest = recognizeInterestWithDigest(interest, *startingPoint);
      bool isInPrefix = false;

      if (doesInterestContainDigest)
        {
          isInPrefix = interest.getName().getPrefix(-1).isPrefixOf((*startingPoint)->getName());
        }
      else
        {
          isInPrefix = interest.getName().isPrefixOf((*startingPoint)->getName());
        }

      if (isInPrefix)
        {
          if (doesComplyWithSelectors(interest, *startingPoint, doesInterestContainDigest))
            {
              return &(*startingPoint)->getData();
            }
        }
    }

  return 0;
}

bool
Cs::doesComplyWithSelectors(const Interest& interest,
                            cs::Entry* entry,
                            bool doesInterestContainDigest) const
{
  /// \todo The following detection is not correct
  ///       1. If data name ends with 32-octet component doesn't mean that this component is digest
  ///       2. Only min/max selectors (both 0) can be specified, all other selectors do not
  ///          make sense for interests with digest (though not sure if we need to enforce this)

  if (doesInterestContainDigest)
    {
      const ndn::name::Component& last = interest.getName().get(-1);
      const ndn::ConstBufferPtr& digest = entry->getDigest();

      BOOST_ASSERT(digest->size() == last.value_size());
      BOOST_ASSERT(digest->size() == ndn::crypto::SHA256_DIGEST_SIZE);

      if (std::memcmp(digest->buf(), last.value(), ndn::crypto::SHA256_DIGEST_SIZE) != 0)
        {
          return false;
        }
    }

  if (!doesInterestContainDigest)
    {
      if (interest.getMinSuffixComponents() >= 0)
        {
          size_t minDataNameLength = interest.getName().size() + interest.getMinSuffixComponents();

          bool isSatisfied = (minDataNameLength <= entry->getName().size());
          if (!isSatisfied)
            {
              return false;
            }
        }

      if (interest.getMaxSuffixComponents() >= 0)
        {
          size_t maxDataNameLength = interest.getName().size() + interest.getMaxSuffixComponents();

          bool isSatisfied = (maxDataNameLength >= entry->getName().size());
          if (!isSatisfied)
            {
              return false;
            }
        }
    }

  if (!interest.getPublisherPublicKeyLocator().empty())
    {
      if (entry->getData().getSignature().getType() == ndn::Signature::Sha256WithRsa)
        {
          ndn::SignatureSha256WithRsa rsaSignature(entry->getData().getSignature());
          if (rsaSignature.getKeyLocator() != interest.getPublisherPublicKeyLocator())
            {
              return false;
            }
        }
      else
        {
          return false;
        }
    }

  if (doesInterestContainDigest)
    {
      const ndn::name::Component& lastComponent = entry->getName().get(-1);

      if (!lastComponent.empty())
        {
          if (interest.getExclude().isExcluded(lastComponent))
            {
              return false;
            }
        }
    }
  else
    {
      if (entry->getName().size() >= interest.getName().size() + 1)
        {
          const ndn::name::Component& nextComponent = entry->getName()
                                                        .get(interest.getName().size());
          if (!nextComponent.empty())
            {
              if (interest.getExclude().isExcluded(nextComponent))
                {
                  return false;
                }
            }
        }
    }

  return true;
}

bool
Cs::recognizeInterestWithDigest(const Interest& interest, cs::Entry* entry) const
{
  // only when min selector is not specified or specified with value of 0
  // and Interest's name length is exactly the length of the name of CS entry
  if (interest.getMinSuffixComponents() <= 0 &&
      interest.getName().size() == (entry->getName().size()))
    {
      const ndn::name::Component& last = interest.getName().get(-1);
      if (last.value_size() == ndn::crypto::SHA256_DIGEST_SIZE)
        {
          return true;
        }
    }

  return false;
}

void
Cs::erase(const Name& exactName)
{
  m_mutex.lock();
  
  bool isIterated = false;
  SkipListLayer::iterator updateTable[SKIPLIST_MAX_LAYERS];
  SkipList::reverse_iterator topLayer = m_skipList.rbegin();
  SkipListLayer::iterator head = (*topLayer)->begin();

  if (!(*topLayer)->empty())
    {
      //start from the upper layer towards bottom
      int layer = m_skipList.size() - 1;
      for (SkipList::reverse_iterator rit = topLayer; rit != m_skipList.rend(); ++rit)
        {
          //if we didn't do any iterations on the higher layers, start from the begin() again
          if (!isIterated)
            head = (*rit)->begin();

          updateTable[layer] = head;

          if (head != (*rit)->end())
            {
              // it can happen when begin() contains the element we want to remove
              if (!isIterated && ((*head)->getName() == exactName))
                {
                  eraseFromSkipList(*head);
                  m_mutex.unlock();
                  return;
                }
              else
                {
                  SkipListLayer::iterator it = head;

                  while ((*it)->getName() < exactName)
                    {
                      head = it;
                      updateTable[layer] = it;
                      isIterated = true;

                      ++it;
                      if (it == (*rit)->end())
                        break;
                    }
                }
            }

          if (layer > 0)
            head = (*head)->getIterators().find(layer - 1)->second; // move HEAD to the lower layer

          layer--;
        }
    }
  else
    {
      m_mutex.unlock();
      return;
    }

  head = updateTable[0];
  ++head; // look at the next slot to check if it contains the item we want to remove

  bool isCsEmpty = (size() == 0);
  bool isInBoundaries = (head != (*m_skipList.begin())->end());
  bool isNameIdentical = false;
  if (!isCsEmpty && isInBoundaries)
    {
      isNameIdentical = (*head)->getName() == exactName;
    }

  if (isNameIdentical)
    {
      eraseFromSkipList(*head);
    }
}

void
Cs::printSkipList() const
{
  //start from the upper layer towards bottom
  int layer = m_skipList.size() - 1;
  for (SkipList::const_reverse_iterator rit = m_skipList.rbegin(); rit != m_skipList.rend(); ++rit)
    {
      for (SkipListLayer::iterator it = (*rit)->begin(); it != (*rit)->end(); ++it)
        {
          std::cout << "Layer " << layer << " " << (*it)->getName() << std::endl;
        }
      layer--;
    }
}

} //namespace nfd
