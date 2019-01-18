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

#include "ndn-icp-download.hpp"
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/chrono/include.hpp>
#include <math.h>
#include "log/log.h"
#include <map>      //for mapping the starttime to the name
#define BOOST_THREAD_PROVIDES_FUTURE
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>






namespace ndn {

    NdnIcpDownload::NdnIcpDownload(unsigned int pipe_size,
                                   unsigned int initial_window,
                                   unsigned int gamma,
                                   double beta,
                                   bool allowStale,
                                   unsigned int lifetime_ms)
            : m_unversioned(false)
	        , m_maxWindow(pipe_size)
            , m_pMin(P_MIN)
            , m_finalChunk(UINT_MAX)
            , m_samples(SAMPLES)
            , m_gamma(gamma)
            , m_beta(beta)
            , m_allowStale(allowStale)
            , m_defaulInterestLifeTime(time::milliseconds(lifetime_ms))
            , m_isOutputEnabled(false)      
            , m_isPathReportEnabled(false)
            , m_winPending(0)
            , m_winCurrent(initial_window)
            , m_initialWindow(initial_window)
            , m_firstInFlight(0)
            , m_outOfOrderCount(0)
            , m_nextPendingInterest(0)
            , m_finalSlot(UINT_MAX)
            , m_interestsSent(0)
            , m_packetsDelivered(0)
            , m_rawBytesDataDelivered(0)
            , m_bytesDataDelivered(0)
            , m_isAbortModeEnabled(false)
            , m_winRetransmissions(0)
            , m_downloadCompleted(false)
            , m_retxLimit(0)
            , m_isSending(false)
            , m_nTimeouts(0)
            , m_curPath(nullptr)
            , m_statistics(true)
            , m_observed(false)
            , c_observed(false)
            , m_averageWin(initial_window)
            , m_set_interest_filter(false)
            , m_observer(NULL)
            , c_observer(NULL)
	        , maxPacketSize(0)   {

            this->m_variables = new Variables();
            m_variables->alpha = ALPHA;
            m_variables->avgWin = 0.0;
	        m_variables->avgRtt = 0.0;
	        m_variables->rtt = 0.0;
	        m_variables->instantaneousThroughput = 0.0;
	        m_variables->winChange = 0;
	        m_variables->batchingStatus = 0;
	        m_variables->isRunning = true;
            m_variables->isBusy = false;
            m_variables->maxPacketSize;
            windowFile = fopen ("/cwindow","w");

	    //this->ndnIcpDownloadRateEstimator = new InterRttEstimator(this->m_variables);
            this->ndnIcpDownloadRateEstimator = new SimpleEstimator(this->m_variables, BATCH);
            //this->ndnIcpDownloadRateEstimator = new BatchingPacketsEstimator(this->m_variables, BATCH);
            m_dataName = Name();
            srand(std::time(NULL));
            std::cout << "reception buffer size #" << RECEPTION_BUFFER << std::endl;
    }

    void NdnIcpDownload::addObserver(NdnIcpDownloadObserver* obs)
    {
        this->m_observer = obs;
        this->m_observed = true;
    }

    void NdnIcpDownload::addObserver(ChunktimeObserver* obs)
    {
        this->c_observer = obs;
        this->c_observed = true;
    }

    void NdnIcpDownload::setAlpha(double alpha)
    {
        if(alpha >= 0 && alpha <= 1)
            m_variables->alpha = alpha;
    }

    void
    NdnIcpDownload::setCurrentPath(shared_ptr<DataPath> path)
    {
        m_curPath = path;
        m_savedPath = path;
    }

    void
    NdnIcpDownload::setStatistics(bool value)
    {
        m_statistics = value;
    }

    void
    NdnIcpDownload::setUnversioned(bool value)
    {
        m_unversioned = value;
    }

    void
    NdnIcpDownload::setMaxRetries(unsigned max_retries)
    {
        m_retxLimit = max_retries;
    }

    void
    NdnIcpDownload::insertToPathTable(std::string key, shared_ptr<DataPath> path)
    {
        if (m_pathTable.find(key) == m_pathTable.end()) {
            m_pathTable[key] = path;
        }
        else {
            std::cerr << "ERROR: failed to insert path to path table, the path entry already exists" << std::endl;
        }
    }

    void
    NdnIcpDownload::controlStatsReporter()
    {
        struct timeval now;
        gettimeofday(&now, 0);
        fprintf(stdout,
                "[Control Stats Report]: "
                "Time %ld.%06d [usec] ndn-icp-download: "
                "Interests Sent: %ld, Received: %ld, Timeouts: %ld, "
                "Current Window: %f, Interest pending (inside the window): %u, Interest Received (inside the window): %u, "
                "Interest received out of order: %u\n",
                (long) now.tv_sec,
                (unsigned) now.tv_usec,
                m_interestsSent,
                m_packetsDelivered,
                m_nTimeouts,
                m_winCurrent,
                m_winPending,
                m_outOfOrderCount);
    }

    void
    NdnIcpDownload::printSummary()
    {
        const char *expid;
        const char *dlm = " ";
        expid = getenv("NDN_EXPERIMENT_ID");
        if (expid == NULL)
            expid = dlm = "";
        double elapsed = 0.0;
        double rate = 0.0;
        double goodput = 0.0;
        gettimeofday(&m_stopTimeValue, 0);

        elapsed = (double) (long) (m_stopTimeValue.tv_sec - m_startTimeValue.tv_sec);
        elapsed += ((int) m_stopTimeValue.tv_usec - (int) m_startTimeValue.tv_usec) / 1000000.0;

        if (elapsed > 0.00001) {
            rate = m_bytesDataDelivered * 8 / elapsed / 1000000;
            goodput = m_rawBytesDataDelivered * 8 / elapsed / 1000000;
        }

        fprintf(stdout,
                "%ld.%06u ndn-icp-download[%d]: %s%s "
                "%ld bytes transferred (filesize: %ld [bytes]) in %.6f seconds. "
                "Rate: %6f [Mbps] Goodput: %6f [Mbps] Timeouts: %ld \n",
                (long) m_stopTimeValue.tv_sec,
                (unsigned) m_stopTimeValue.tv_usec,
                (int) getpid(),
                expid,
                dlm,
                m_bytesDataDelivered,
                m_rawBytesDataDelivered,
                elapsed,
                rate,
                goodput,
                m_nTimeouts
        );

        if (m_isPathReportEnabled) {

            // Print number of paths in the transmission process, excluding the default path

            std::cout << "Number of paths in the path table: " <<
            (m_pathTable.size() - 1) << std::endl;

            int i = 0;
            BOOST_FOREACH(HashTableForPath::value_type kv, m_pathTable) {

                if (kv.first.length() <= 1) {
                    i++;
                    std::cout << "[Path " << i << "]\n" << "ID : " <<
                    (int) *(reinterpret_cast<const unsigned char *>(kv.first.data())) << "\n";
                    kv.second->pathReporter();

                }
            }
        }
    }

    void
    NdnIcpDownload::setLastTimeOutToNow()
    {
        gettimeofday(&m_lastTimeout, 0);
    }

    void
    NdnIcpDownload::setStartTimeToNow()
    {
        gettimeofday(&m_startTimeValue, 0);
    }

    void
    NdnIcpDownload::enableOutput()
    {
        m_isOutputEnabled = true;
    }

    void
    NdnIcpDownload::enablePathReport()
    {
        m_isPathReportEnabled = true;
    }

    void
    NdnIcpDownload::updateRTT(uint64_t slot)
    {
        if (!m_curPath)
            throw std::runtime_error("ERROR: no current path found, exit");
        else {
            double rtt;
            struct timeval now;
            gettimeofday(&now, 0);

            const timeval &sendTime = m_outOfOrderInterests[slot].getSendTime();

            rtt = DataPath::getMicroSeconds(now) - DataPath::getMicroSeconds(sendTime);
            ndnIcpDownloadRateEstimator->onRttUpdate(rtt);
            m_curPath->insertNewRtt(rtt, m_winCurrent, &m_averageWin);
            m_curPath->smoothTimer();
        }
    }

    void
    NdnIcpDownload::increaseWindow()
    {
	if ((unsigned) m_winCurrent < m_maxWindow)
            m_winCurrent += (double) m_gamma/m_winCurrent;
        
        //fprintf(pFile, "%6f\n", m_winCurrent);
        ndnIcpDownloadRateEstimator->onWindowIncrease(m_winCurrent);
    }

    void
    NdnIcpDownload::decreaseWindow()
    {
        m_winCurrent = m_winCurrent * m_beta;
        if (m_winCurrent < m_initialWindow)
            m_winCurrent = m_initialWindow;

        //fprintf(pFile, "%6f\n", m_winCurrent);
        ndnIcpDownloadRateEstimator->onWindowDecrease(m_winCurrent);
    }

    void
    NdnIcpDownload::RAQM()
    {
        if (!m_curPath) {
            std::cerr << "ERROR: no current path found, exit" << std::endl;
            exit(EXIT_FAILURE);
        }
        else {
            // Change drop probability according to RTT statistics
            m_curPath->updateDropProb();

            if (rand() % 10000 <= m_curPath->getDropProb() * 10000) //TODO, INFO this seems to cause low bw in docker setting, investigate
                decreaseWindow();
        }
    }

    void
    NdnIcpDownload::afterDataReception(uint64_t slot)
    {
        // Update win counters
        m_winPending--;
        increaseWindow();
        updateRTT(slot);

        // Set drop probablility and window size accordingly
        RAQM();
    }

    void
    NdnIcpDownload::onData(const Data &data)
    {
        sec seconds = boost::chrono::nanoseconds(timer.elapsed().wall);
        fprintf(windowFile, "%6f,%6f\n", seconds, m_winCurrent);
        const ndn::name::Component &finalBlockId = data.getMetaInfo().getFinalBlockId();

        if (finalBlockId.isSegment() && finalBlockId.toSegment() < this->m_finalChunk) {
            this->m_finalChunk = finalBlockId.toSegment();
            this->m_finalChunk++;
        }

        const Block &content = data.getContent();
        const Name &name = data.getName();
        std::string nameAsString = name.toUri();
        int seq = name[-1].toSegment();
        uint64_t slot = seq % RECEPTION_BUFFER;
        size_t dataSize = content.value_size(); //= max. 1400 Bytes = 1.4 kB = 11200 Bits = 11.2 kBits

       


	if(seq == (m_finalChunk -1))
		this->m_downloadCompleted = true;

        //check if we got a M-flaged interest for this data
        NackSet::iterator it = m_nackSet.find(seq);
        if(it != m_nackSet.end()){
            m_nackSet.erase(it);
        }

        size_t packetSize = data.wireEncode().size();

        ndnIcpDownloadRateEstimator->onDataReceived((int)packetSize);

	if((int) packetSize > this->maxPacketSize)
	{
	    this->maxPacketSize = (int)packetSize;
	    while(this->m_variables->isBusy)
	    {}
	    this->m_variables->isBusy = true;
	    this->m_variables->maxPacketSize = this->maxPacketSize;
	    this->m_variables->isBusy = false;
	}

        if (m_isAbortModeEnabled) {
            if (m_winRetransmissions > 0 && m_outOfOrderInterests[slot].getRetxCount() >= 2) // ??
                m_winRetransmissions--;
        }

        if (m_isOutputEnabled) {
            std::cout << "data received, seq number #" << seq << std::endl;
            std::cout.write(reinterpret_cast<const char *>(content.value()), content.value_size());
        }

        GOT_HERE();
#ifdef PATH_LABELLING

        ndn::Block pathIdBlock = data.getPathId();

        if(pathIdBlock.empty())
        {
          std::cerr<< "[ERROR]: Path ID lost in the transmission.";
          exit(EXIT_FAILURE);
        }

        unsigned char pathId = *(data.getPathId().value());
#else
        unsigned char pathId = 0;
#endif

        std::string pathIdString(1, pathId);

        if (m_pathTable.find(pathIdString) == m_pathTable.end()) {
            if (m_curPath) {
                // Create a new path with some default param
                if (m_pathTable.empty()) {
                    std::cerr << "No path initialized for path table, error could be in default path initialization." <<
                    std::endl;
                    exit(EXIT_FAILURE);
                }
                else {
                    // Initiate the new path default param
                    shared_ptr<DataPath> newPath = make_shared<DataPath>(*(m_pathTable.at(DEFAULT_PATH_ID)));
                    // Insert the new path into hash table
                    m_pathTable[pathIdString] = newPath;
                }
            }
            else {
                std::cerr << "UNEXPECTED ERROR: when running,current path not found." << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        m_curPath = m_pathTable[pathIdString];

        // Update measurements for path
        m_curPath->updateReceivedStats(packetSize, dataSize);

        unsigned int nextSlot = m_nextPendingInterest % RECEPTION_BUFFER;

        if (nextSlot > m_firstInFlight && (slot > nextSlot || slot < m_firstInFlight)) {
            std::cout << "out of window data received at # " << slot << std::endl;
            return;
        }
        if (nextSlot < m_firstInFlight && (slot > nextSlot && slot < m_firstInFlight)) {
            std::cout << "out of window data received at # " << slot << std::endl;
            return;
        }

        if (seq == (m_finalChunk - 1)) {
            m_finalSlot = slot;
            GOT_HERE();
        }

        if (slot != m_firstInFlight) {
            // Out of order data received, save it for later.
            //std::cout << "out of order count " << m_outOfOrderCount << std::endl;
            if (!m_outOfOrderInterests[slot].checkIfDataIsReceived()) {
                GOT_HERE();
                m_outOfOrderCount++; // todo Rename it
                m_outOfOrderInterests[slot].addRawData(data);
                m_outOfOrderInterests[slot].markAsReceived(true);
                afterDataReception(slot);
                if(c_observed)
                    this->speed_chunk = c_observer->onDataReceived(data);

                if(m_observed)
                {
                    if(this->speed_chunk > 0)
                        m_observer->notifyChunkStats(nameAsString, this->speed_chunk); // logging the throughput on chunk level
                } else 
                    std::cout<<"NOT OBSERVED! (M) \t";
                }
        }
        else {
            // In order data arrived

            assert(!m_outOfOrderInterests[slot].checkIfDataIsReceived());
            m_packetsDelivered++;
            m_rawBytesDataDelivered += dataSize;
            m_bytesDataDelivered += packetSize;

            // Save data to the reception buffer

            this->m_recBuffer->insert(m_recBuffer->end(),
                                      reinterpret_cast<const char *>(content.value()),
                                      reinterpret_cast<const char *>(content.value()) + content.value_size());

             if(c_observed)
                    this->speed_chunk = c_observer->onDataReceived(data);

            if(m_observed)
            {
                if(this->speed_chunk > 0)
                    m_observer->notifyChunkStats(nameAsString, this->speed_chunk); // logging the throughput on chunk level
            } else 
                        std::cout<<"NOT OBSERVED! (M) \t";


            afterDataReception(slot);
            slot = (slot + 1) % RECEPTION_BUFFER;
            m_firstInFlight = slot;

            if (slot >= m_finalSlot && m_outOfOrderCount == 0) {
                m_face.removeAllPendingInterests();
                m_winPending = 0;
                return;
            }

            /*
             * Consume out-of-order pkts already received until there is a hole
             */
            while (m_outOfOrderCount > 0 && m_outOfOrderInterests[slot].checkIfDataIsReceived()) {
                m_packetsDelivered++;
                m_rawBytesDataDelivered += m_outOfOrderInterests[slot].getRawDataSize();
                m_bytesDataDelivered += m_outOfOrderInterests[slot].getPacketSize();

               
                this->m_recBuffer->insert(m_recBuffer->end(),
                                           m_outOfOrderInterests[slot].getRawData(),
                                           m_outOfOrderInterests[slot].getRawData() +
                                           m_outOfOrderInterests[slot].getRawDataSize());
                        

                if (slot >= m_finalSlot) {
                    GOT_HERE();
                    m_face.removeAllPendingInterests();
                    m_winPending = 0;
                    m_outOfOrderInterests[slot].removeRawData();
                    m_outOfOrderInterests[slot].markAsReceived(false);
                    m_firstInFlight = (slot + 1) % RECEPTION_BUFFER;
                    m_outOfOrderCount--;
                    return;
                }

                m_outOfOrderInterests[slot].removeRawData();
                m_outOfOrderInterests[slot].markAsReceived(false);
                slot = (slot + 1) % RECEPTION_BUFFER;
                m_firstInFlight = slot;
                m_outOfOrderCount--;
            }
        }

        boost::async(boost::bind(&ndn::NdnIcpDownload::scheduleNextPacket,this));
    }

    void
    NdnIcpDownload::onInterest(const Interest &interest) {
	   bool mobility = false; //interest.get_MobilityLossFlag();
	    if(mobility){ //MLDR M-flaged interest
            const Name &name = interest.getName();
            uint64_t segment = name[-1].toSegment();
            timeval now;
            gettimeofday(&now, 0);
            std::cout << (long) now.tv_sec << "." << (unsigned) now.tv_usec << " ndn-icp-download: M-Interest " <<
                        segment << " " << interest.getName() << "\n";
            NackSet::iterator it = m_nackSet.find(segment);
            if(it == m_nackSet.end()){
                m_nackSet.insert(segment);
            }
	    }
    }

    void
    NdnIcpDownload::onNack(const Interest &nack){
        timeval now;
        gettimeofday(&now, 0);
        std::cout << (long) now.tv_sec << "." << (unsigned) now.tv_usec << " ndn-icp-download: NACK " << nack.getName() << "\n";
	    boost::asio::io_service m_io;
        boost::asio::deadline_timer t(m_io, boost::posix_time::seconds(1));
        t.async_wait([=] (const boost::system::error_code e) {if (e != boost::asio::error::operation_aborted) onTimeout(nack);});
        m_io.run();
    }

    void
    NdnIcpDownload::scheduleNextPacket()
    {
        if (!m_dataName.empty() && m_winPending < (unsigned) m_winCurrent && m_nextPendingInterest < m_finalChunk && !m_isSending)
            sendPacket();
        //else(scheduleNextPacket());
    }

    void
    NdnIcpDownload::sendPacket()
    {
        m_isSending = true;

        uint64_t seq;
        uint64_t slot;
        seq = m_nextPendingInterest++;
        slot = seq % RECEPTION_BUFFER;
        assert(!m_outOfOrderInterests[slot].checkIfDataIsReceived());
        struct timeval now;
        gettimeofday(&now, 0);
        m_outOfOrderInterests[slot].setSendTime(now).markAsReceived(false);

        // Make a proper interest
        Interest interest;
        interest.setName(Name(m_dataName).appendSegment(seq));

        interest.setInterestLifetime(m_defaulInterestLifeTime);
        interest.setMustBeFresh(m_allowStale);
        interest.setChildSelector(1);

        m_face.expressInterest(interest,
                               bind(&NdnIcpDownload::onData, this, _2),
                               bind(&NdnIcpDownload::onNack, this, _1),
                               bind(&NdnIcpDownload::onTimeout, this, _1));

        if(c_observed)
            c_observer->onInterestSent(interest);
        
        m_interestsSent++;
        m_winPending++;

        assert(m_outOfOrderCount < RECEPTION_BUFFER);

        m_isSending = false;

        scheduleNextPacket();
    }

    void
    NdnIcpDownload::reInitialize()
    {
        m_finalChunk = UINT_MAX;
        m_firstDataName.clear();
        m_dataName.clear();
        m_firstInFlight = 0;
        m_outOfOrderCount = 0;
        m_nextPendingInterest = 0;
        m_finalSlot = UINT_MAX;
        m_interestsSent = 0;
        m_packetsDelivered = 0;
        m_rawBytesDataDelivered = 0;
        m_bytesDataDelivered = 0;
        m_isAbortModeEnabled = false;
        m_winRetransmissions = 0;
        m_downloadCompleted = false;
        m_retxLimit = 0;
        m_isSending = false;
        m_nTimeouts = 0;
        m_variables->batchingStatus = 0;
        m_variables->avgWin = 0;
    }

    void
    NdnIcpDownload::resetRAAQM()
    {
        m_winPending = 0;
        m_winCurrent = m_initialWindow;
        m_averageWin = m_winCurrent;

        m_curPath = m_savedPath;
        m_pathTable.clear();
        this->insertToPathTable(DEFAULT_PATH_ID, m_savedPath);
    }

    void
    NdnIcpDownload::onTimeout(const Interest &interest)
    {
        if (!m_curPath) {
            throw std::runtime_error("ERROR: when timed-out no current path found, exit");
        }

        const Name &name = interest.getName();
        uint64_t timedOutSegment;

        if (name[-1].isSegment())
            timedOutSegment = name[-1].toSegment();
        else
            timedOutSegment = 0;

        uint64_t slot = timedOutSegment % RECEPTION_BUFFER;

        // Check if it the asked data exist
        if (timedOutSegment >= m_finalChunk)
            return;

        // Check if data is received
        if (m_outOfOrderInterests[slot].checkIfDataIsReceived())
            return;

        // Check whether we reached the retransmission limit
        if (m_retxLimit != 0 && m_outOfOrderInterests[slot].getRetxCount() >= m_retxLimit)
            throw std::runtime_error("ERROR: Download failed.");

        NackSet::iterator it = m_nackSet.find(timedOutSegment);
        if(it != m_nackSet.end()){
            std::cout << "erase the nack from the list, do not decrease the window, seq: " << timedOutSegment << std::endl;
            m_nackSet.erase(it);
        }else{
            //std::cout << "Decrease the window because the timeout happened" << timedOutSegment << std::endl;
            decreaseWindow();
        }

        //m_outOfOrderInterests[slot].markAsReceived(false);
        timeval now;
        gettimeofday(&now, 0);
        m_outOfOrderInterests[slot].setSendTime(now);
        m_outOfOrderInterests[slot].markAsReceived(false);

        if (m_isAbortModeEnabled) {
            if (m_outOfOrderInterests[slot].getRetxCount() == 1)
                m_winRetransmissions++;

            if (m_winRetransmissions >= m_winCurrent)
                throw std::runtime_error("Error: full window of interests timed out, application aborted");
        }

        // Retransmit the interest
        Interest newInterest = interest;

        // Since we made a copy, we need to refresh interest nonce otherwise it could be considered as a looped interest by NFD.
        newInterest.refreshNonce();
        newInterest.setInterestLifetime(m_defaulInterestLifeTime);

        // Re-express interest
        m_face.expressInterest(newInterest,
                               bind(&NdnIcpDownload::onData, this, _2),
                               bind(&NdnIcpDownload::onNack, this, _1),
                               bind(&NdnIcpDownload::onTimeout, this, _1));

        m_outOfOrderInterests[slot].increaseRetxCount();
        m_interestsSent++;
        m_nTimeouts++;

        gettimeofday(&m_lastTimeout, 0);
        
        std::cout << (long) now.tv_sec << "." << (unsigned) now.tv_usec << " ndn-icp-download: timeout on " <<
        timedOutSegment << " " << newInterest.getName()  << "\n";
    }

    void
    NdnIcpDownload::onFirstData(const Data &data)
    {
        const Name &name = data.getName();
        Name first_copy = m_firstDataName;

        if (name.size() == m_firstDataName.size() + 1)
            m_dataName = Name(first_copy.append(name[-1]));
        else if (name.size() == m_firstDataName.size() + 2)
            m_dataName = Name(first_copy.append(name[-2]));
        else {
            std::cerr << "ERROR: Wrong number of components." << std::endl;
            return;
        }
        //boost::async(boost::bind(&ndn::NdnIcpDownload::askFirst,this));
        //scheduleNextPacket();
    }

    void
    NdnIcpDownload::askFirst()
    {
        Interest interest;
        interest.setName(Name(m_firstDataName));
        interest.setInterestLifetime(m_defaulInterestLifeTime);
        interest.setMustBeFresh(true);
        interest.setChildSelector(1);

        m_face.expressInterest(interest,
                               bind(&NdnIcpDownload::onFirstData, this, _2),
                               bind(&NdnIcpDownload::onNack, this, _1),
                               bind(&NdnIcpDownload::onTimeout, this, _1));
        //scheduleNextPacket();
        boost::async(boost::bind(&ndn::NdnIcpDownload::scheduleNextPacket,this));
    }


    bool
    NdnIcpDownload::download(std::string name, std::vector<char> *rec_buffer, int initial_chunk, int n_chunk)
    {
    	gettimeofday(&(m_variables->begin),0);
        if ((Name(name) != Name(m_firstDataName) && !m_firstDataName.empty()) || initial_chunk > -1)
            this->reInitialize();

        if(!m_set_interest_filter){
            InterestFilter intFilter(name);
            m_face.setInterestFilter(intFilter, bind(&NdnIcpDownload::onInterest, this, _2));
            m_set_interest_filter = true;
        }

        if (initial_chunk > -1) {
            this->m_nextPendingInterest = (unsigned int)initial_chunk;
            this->m_firstInFlight = (unsigned int)initial_chunk;
        }

        if (m_unversioned)
            m_dataName = name;
        else
            this->m_firstDataName = name;

        this->m_recBuffer = rec_buffer;
        this->m_finalSlot = UINT_MAX;

        if (n_chunk != -1 && m_finalChunk != UINT_MAX) {
            m_finalChunk += n_chunk;
            this->sendPacket();
        }
        else if (n_chunk != -1 && m_finalChunk == UINT_MAX) {
            m_finalChunk = n_chunk + this->m_nextPendingInterest;
            if (m_unversioned)
                this->sendPacket();
            else
                this->askFirst();
        }
        else {
            if (m_unversioned)
                this->sendPacket();
            else
                this->askFirst();
        }

        m_face.processEvents();

        if (m_packetsDelivered == 0) {
            std::cerr << "NDN-ICP-DOWNLOAD: no data found: " << std::endl << m_dataName.toUri();
            throw std::runtime_error("No data found");
        }

        if (m_nextPendingInterest <= m_finalChunk)
            m_face.processEvents();

        if (m_downloadCompleted) {
            if (m_statistics)
                //controlStatsReporter();
                printSummary();
            if(m_observed)
               m_observer->notifyStats(m_variables->instantaneousThroughput, m_curPath->getAverageRtt());
            this->reInitialize();
            return true;
        }
        else
            return false;
    }
}
