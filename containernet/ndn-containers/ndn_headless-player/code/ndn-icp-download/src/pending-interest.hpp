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

#include <ctime>
#include <ndn-cxx/data.hpp>


namespace ndn {

class PendingInterest
{

public:
    PendingInterest();

    /**
     * @brief Get the send time of the interest
     */
    const timeval &
    getSendTime();

    /**
     * @brief Set the send time of the interest
     */
    PendingInterest
    setSendTime(const timeval &now);

    /**
     * @brief Get the size of the data without considering the ICN header
     */
    std::size_t
    getRawDataSize();

    /**
     * @brief Get the size of the data packet considering the ICN header
     */
    std::size_t
    getPacketSize();

    /**
     * @brief True means that the data addressed by this pending interest has been received
     */
    PendingInterest
    markAsReceived(bool val);

    /**
     * @brief If the data has been received returns true, otherwise returns false
     */
    bool
    checkIfDataIsReceived();

    /**
     * @brief Temporarily store here the out of order data received
     * @param data The data corresponding to this pending interest
     */
    PendingInterest
    addRawData(const Data &data);

    /**
     * @brieg Get the raw data block
     */
    const char *
    getRawData();

    /**
     * @brief Free the memory occupied by the raw data block
     */
    PendingInterest
    removeRawData();

    /**
     * @brief Get the number of times this interest has been retransmitted
     */
    unsigned int
    getRetxCount();

    /**
     * @brief Increase the number of retransmission of this packet
     */
    PendingInterest
    increaseRetxCount();

private:
    /**
     * The buffer storing the raw data received out of order
     */
    unsigned char *m_rawData;

    /**
     * The size of the raw data buffer
     */
    std::size_t m_rawDataSize;

    /**
     * The size of the whole packet, including the ICN header
     */
    std::size_t m_packetSize;

    /**
     * True means that the data has been received.
     */
    bool m_isReceived;

    /**
     * Delivering time of this interest
     */
    struct timeval m_sendTime;


private:
    /**
     * Counter of the number of retransmissions of this interest
     */
    unsigned int m_retxCount;

}; // end class PendingInterests

} // end namespace ndn