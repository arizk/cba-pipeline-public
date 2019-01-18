/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2015   Regents of Cisco Systems,
 *                      IRT Systemx
 *
 * This project is a library implementing an Interest Control Protocol. It can be used by
 * applications that aims to get content through a reliable transport protocol.
 *
 * libndn-icp is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * libndn-icp is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Luca Muscariello <lumuscar@cisco.com>
 * @author Zeng Xuan <xuan.zeng@irt-systemx.fr>
 * @author Mauro Sardara <mauro.sardara@irt-systemx.fr>
 * @author Michele Papalini <micpapal@cisco.com>
 *
 */

#include "pending-interest.hpp"

namespace ndn {

PendingInterest::PendingInterest()
: m_rawData(NULL)
  , m_rawDataSize(0)
  , m_packetSize(0)
  , m_isReceived(false)
  , m_retxCount(0)
{
}

const timeval &
PendingInterest::getSendTime()
{
    return m_sendTime;
}

PendingInterest
PendingInterest::setSendTime(const timeval &now)
{
    m_sendTime.tv_sec = now.tv_sec;
    m_sendTime.tv_usec = now.tv_usec;

    return *this;
}

std::size_t
PendingInterest::getRawDataSize()
{
    return m_rawDataSize;
}

std::size_t
PendingInterest::getPacketSize()
{
    return m_packetSize;
}

unsigned int
PendingInterest::getRetxCount()
{
    return m_retxCount;
}

PendingInterest
PendingInterest::increaseRetxCount()
{
    m_retxCount++;
    return *this;
}

PendingInterest
PendingInterest::markAsReceived(bool val)
{
    m_isReceived = val;

    return *this;
}

bool
PendingInterest::checkIfDataIsReceived()
{
    return m_isReceived;
}

PendingInterest
PendingInterest::addRawData(const Data &data)
{
    if (m_rawData != NULL)
        removeRawData();

    const Block &content = data.getContent();
    m_rawDataSize = content.value_size();
    m_packetSize = data.wireEncode().size();
    m_rawData = (unsigned char *) malloc(m_rawDataSize);

    memcpy(m_rawData,
           reinterpret_cast<const char *>(content.value()),
           content.value_size());

    return *this;
}

const char *
PendingInterest::getRawData()
{
    if (m_rawData != NULL)
        return reinterpret_cast<const char *>(m_rawData);
    else
        return nullptr;
}

PendingInterest
PendingInterest::removeRawData()
{
    if (m_rawData != NULL)
    {
        free(m_rawData);
        m_rawData = NULL;
        m_rawDataSize = 0;
        m_packetSize = 0;
    }

    return *this;
}

} // namespace ndn