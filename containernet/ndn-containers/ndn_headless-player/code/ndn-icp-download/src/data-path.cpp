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

#include "data-path.hpp"

namespace ndn {

DataPath::DataPath(double drop_factor,
                   double p_min,
                   unsigned new_timer,
                   unsigned int samples,
                   uint64_t new_rtt,
                   uint64_t new_rtt_min,
                   uint64_t new_rtt_max)

: m_dropFactor(drop_factor)
  , m_pMin(p_min)
  , m_timer(new_timer)
  , m_samples(samples)
  , m_rtt(new_rtt)
  , m_rttMin(new_rtt_min)
  , m_rttMax(new_rtt_max)

  , m_dropProb(0)
  , m_packetsReceived(0)
  , m_lastPacketsReceived(0)
  , m_packetsBytesReceived(0)
  , m_lastPacketsBytesReceived(0)
  , m_rawDataBytesReceived(0)
  , m_lastRawDataBytesReceived(0)
  , m_averageRtt(0)
  , m_alpha(ALPHA)
{
    gettimeofday(&m_previousCallOfPathReporter, 0);
}

void
DataPath::pathReporter()
{
    struct timeval now;
    gettimeofday(&now, 0);

    double rate, delta_t;

    delta_t = getMicroSeconds(now) - getMicroSeconds(m_previousCallOfPathReporter);
    rate = (m_packetsBytesReceived - m_lastPacketsBytesReceived) * 8 / delta_t; // MB/s
    std::cout << "DataPath status report: "
    << "at time " << (long) now.tv_sec << "." << (unsigned) now.tv_usec << " sec:\n"
    << (void *) this << " path\n"
    << "Packets Received: " << (m_packetsReceived - m_lastPacketsReceived) << "\n"
    << "delta_t " << delta_t << " [us]\n"
    << "rate " << rate << " [Mbps]\n"
    << "Last RTT " << m_rtt << " [us]\n"
    << "RTT_max " << m_rttMax << " [us]\n"
    << "RTT_min " << m_rttMin << " [us]\n" << std::endl;

    m_lastPacketsReceived = m_packetsReceived;
    m_lastPacketsBytesReceived = m_packetsBytesReceived;
    gettimeofday(&m_previousCallOfPathReporter, 0);
}

void
DataPath::insertNewRtt(double new_rtt, double winSize, double *averageThroughput)
{

    if(winSize)
    {
	if(*averageThroughput == 0)
	    *averageThroughput = winSize / new_rtt;
	*averageThroughput = m_alpha * *averageThroughput + (1 - m_alpha) * (winSize / new_rtt);
    }
    //if(m_averageRtt == 0)                                           
    //    m_averageRtt = new_rtt;                                     
    //m_averageRtt = m_alpha * m_averageRtt + (1 - m_alpha) * new_rtt; // not really promising measurement
    m_rtt = new_rtt;
    m_rttSamples.get<byArrival>().push_back(new_rtt);

    if (m_rttSamples.get<byArrival>().size() > m_samples)
        m_rttSamples.get<byArrival>().pop_front();

    m_rttMax = *(m_rttSamples.get<byOrder>().rbegin());
    m_rttMin = *(m_rttSamples.get<byOrder>().begin());
}

void
DataPath::updateReceivedStats(std::size_t packet_size, std::size_t data_size)
{
    m_packetsReceived++;
    m_packetsBytesReceived += packet_size;
    m_rawDataBytesReceived += data_size;
}

double
DataPath::getDropFactor()
{
    return m_dropFactor;
}

double
DataPath::getDropProb()
{
    return m_dropProb;
}

void
DataPath::setDropProb(double dropProb)
{
    m_dropProb = dropProb;
}

double
DataPath::getPMin()
{
    return m_pMin;
}

double
DataPath::getTimer()
{
    return m_timer;
}

void
DataPath::smoothTimer()
{
    m_timer = (1 - TIMEOUT_SMOOTHER) * m_timer + (TIMEOUT_SMOOTHER) * m_rtt * (TIMEOUT_RATIO);
}

double
DataPath::getRtt()
{
    return m_rtt;
}

double
DataPath::getAverageRtt()
{
    return m_averageRtt;
}

double
DataPath::getRttMax()
{
    return m_rttMax;
}

double
DataPath::getRttMin()
{
    return m_rttMin;
}

unsigned
DataPath::getSampleValue()
{
    return m_samples;
}

unsigned
DataPath::getRttQueueSize()
{
    return m_rttSamples.get<byArrival>().size();
}

void
DataPath::updateDropProb()
{

    m_dropProb = 0.0;

    if (getSampleValue() == getRttQueueSize()) {
        if (m_rttMax == m_rttMin)
            m_dropProb = m_pMin;
        else
            m_dropProb = m_pMin + m_dropFactor * (m_rtt - m_rttMin) / (m_rttMax - m_rttMin);
    }
}

double
DataPath::getMicroSeconds(struct timeval time)
{
    return (double) (time.tv_sec) * 1000000 + (double) (time.tv_usec);
}

void
DataPath::setAlpha(double alpha)
{
    if(alpha >= 0 && alpha <= 1)
        m_alpha = alpha;
}

} // namespace ndn
