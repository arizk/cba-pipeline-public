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


#include <sys/time.h>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/foreach.hpp>
#include <iostream>

#define TIMEOUT_SMOOTHER 0.1
#define TIMEOUT_RATIO 10
#define ALPHA 0.8

namespace ndn {

class DataPath
{
public:

    DataPath(double drop_factor,
             double p_min,
             unsigned new_timer,
             unsigned int samples,
             uint64_t new_rtt = 1000,
             uint64_t new_rtt_min = 1000,
             uint64_t new_rtt_max = 1000);

public:

    /*
     * @brief Print Path Status
     */
    void
    pathReporter();


    /*
     * @brief Add a new RTT to the RTT queue of the path, check if RTT queue is full, and thus need overwrite.
     *        Also it maintains the validity of min and max of RTT.
     * @param new_rtt is the value of the new RTT
     */
    void
    insertNewRtt(double new_rtt, double winSize, double *averageThroughput);

    /**
     * @brief Update the path statistics
     * @param packet_size the size of the packet received, including the ICN header
     * @param data_size the size of the data received, without the ICN header
     */
    void
    updateReceivedStats(std::size_t packet_size, std::size_t data_size);

    /**
     * @brief Get the value of the drop factor parameter
     */
    double
    getDropFactor();

    /**
     * @brief Get the value of the drop probability
     */
    double
    getDropProb();

    /**
     * @brief Set the value pf the drop probability
     * @param drop_prob is the value of the drop probability
     */
    void
    setDropProb(double drop_prob);

    /**
     * @brief Get the minimum drop probability
     */
    double
    getPMin();

    /**
     * @brief Get last RTT
     */
    double
    getRtt();

    /**
     * @brief Get average RTT
     */
    double
    getAverageRtt();

    /**
     * @brief Get the current m_timer value
     */
    double
    getTimer();

    /**
     * @brief Smooth he value of the m_timer accordingly with the last RTT measured
     */
    void
    smoothTimer();

    /**
     * @brief Get the maximum RTT among the last samples
     */
    double
    getRttMax();

    /**
     * @brief Get the minimum RTT among the last samples
     */
    double
    getRttMin();

    /**
     * @brief Get the number of saved samples
     */
    unsigned
    getSampleValue();

    /**
     * @brief Get the size og the RTT queue
     */
    unsigned
    getRttQueueSize();

    /*
     * @brief Change drop probability according to RTT statistics
     *        Invoked in RAAQM(), before control window size update.
     */
    void
    updateDropProb();

    /**
     * @brief This function convert the time from struct timeval to its value in microseconds
     */
    static double getMicroSeconds(struct timeval time);

    void setAlpha(double alpha);

private:

    /**
     * The value of the drop factor
     */
    double m_dropFactor;

    /**
     * The minumum drop probability
     */
    double m_pMin;

    /**
     * The timer, expressed in milliseconds
     */
    double m_timer;

    /**
     * The number of samples to store for computing the protocol measurements
     */
    const unsigned int m_samples;

    /**
     * The last, the minimum and the maximum value of the RTT (among the last m_samples samples)
     */
    double m_rtt, m_rttMin, m_rttMax;

    /**
     * The current drop probability
     */
    double m_dropProb;

    /**
     * The number of packets received in this path
     */
    intmax_t m_packetsReceived;

    /**
     * The first packet received after the statistics print
     */
    intmax_t m_lastPacketsReceived;

    /**
     * Total number of bytes received including the ICN header
     */
    intmax_t m_packetsBytesReceived;

    /**
     * The amount of packet bytes received at the last path summary computation
     */
    intmax_t m_lastPacketsBytesReceived;

    /**
     * Total number of bytes received without including the ICN header
     */
    intmax_t m_rawDataBytesReceived;

    /**
     * The amount of raw dat bytes received at the last path summary computation
     */
    intmax_t m_lastRawDataBytesReceived;

    class byArrival;

    class byOrder;

    /**
     * Double ended queue for the RTTs
     */
    typedef boost::multi_index_container
    <
    unsigned,
    boost::multi_index::indexed_by<
    // by arrival (FIFO)
    boost::multi_index::sequenced<boost::multi_index::tag<byArrival> >,
    // index by ascending order
    boost::multi_index::ordered_non_unique<
    boost::multi_index::tag<byOrder>,
    boost::multi_index::identity<unsigned>
    >
    >
    > RTTQueue;
    RTTQueue m_rttSamples;

    /**
     * Time of the last call to the path reporter method
     */
    struct timeval m_previousCallOfPathReporter;

    double m_averageRtt;
    double m_alpha;
};

} // namespace ndn
