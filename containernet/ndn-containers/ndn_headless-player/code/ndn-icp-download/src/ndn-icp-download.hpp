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
#ifndef NDN_ICP_DOWNLOAD_H
#define NDN_ICP_DOWNLOAD_H

#include <unistd.h>
#include <chrono>
#include <boost/timer/timer.hpp>
#include <stdio.h>
#include "chunktimeObserver.hpp"
#include "rate-estimation.hpp"

/**
 * Default values for the protocol. They could be overwritten by the values provided by the application.
 */

#define RECEPTION_BUFFER 128000
#define DEFAULT_PATH_ID "default_path"
#define MAX_WINDOW (RECEPTION_BUFFER - 1)
#define MAX_SEQUENCE_NUMBER 1000
#define P_MIN 0.00001
#define P_MIN_STRING "0.00001"
#define DROP_FACTOR 0.02
#define DROP_FACTOR_STRING "0.02"
#define BETA 0.5
#define BETA_STRING "0.5"
#define GAMMA 1

/*
 * Size of RTT sample queue. RAQM Window adaptation starts only if this queue is full.
 * Until it is full, Interests are sent one per Data Packet
 */
#define SAMPLES 30

#define ALLOW_STALE false
#define DEFAULT_INTEREST_TIMEOUT 1000000 // microsecond (us)
#define MAX_INTEREST_LIFETIME_MS 10000 // millisecond (ms)
#define CHUNK_SIZE 1024
#define RATE_SAMPLES 4096  //number of packets on which rate is calculated
#define PATH_REP_INTVL 2000000
#define QUEUE_CAP 200
#define THRESHOLD 0.9

#define ALPHA 0.8


#define GOT_HERE() ((void)(__LINE__))
using duration_in_seconds = std::chrono::duration<double, std::ratio<1,1> >;

namespace ndn {

	//Allow to have an observer of the downloading and can be notified of the DL speed 
	class NdnIcpDownloadObserver
	{
	    public:
		virtual ~NdnIcpDownloadObserver(){};
		virtual void notifyStats(double WinSize, double RTT) = 0;
        virtual void notifyChunkStats(std::string chunkName, double chunkThroughput) = 0; //for measurement on chunk-level logging
	};

class NdnIcpDownload
{
public:
    NdnIcpDownload(unsigned int pipe_size,
                   unsigned int initial_window,
                   unsigned int gamma,
                   double beta,
                   bool allowStale,
                   unsigned int lifetime_ms);
       
    ~NdnIcpDownload() {m_variables->isRunning = false;};
    /**
     * @brief Set the start time of the download to now.
     */
    void
    setStartTimeToNow();

    /**
     * @brief Set the time of the last timeout to now.
     */
    void
    setLastTimeOutToNow();

    /**
     * @brief Enable output during the download
     */
    void
    enableOutput();

    /**
     * @brief Enable the software to print per-path statistics at the end of the download
     */
    void
    enablePathReport();

    /*
     * @brief Set the default path
     * @param path the current DataPath
     */
    void
    setCurrentPath(shared_ptr<DataPath> path);

    /**
     * @brief Report statistics regarding the congestion control algorithm
     */
    void
    controlStatsReporter();

    /**
     * Print a summary regarding the statistics of the download
     */
    void
    printSummary();

    /**
     * @brief Reset the congestion window to the initial values
     */
    void
    resetRAAQM();

    /**
     * @brief Enable the automatic print of al the statistics at the end of the download
     * @param value true for enabling statistics, false otherwise
     */
    void
    setStatistics(bool value);

    /**
     * @brief Enable the download of unversioned data
     * @param value true for enabling unversioned data, false otherwise
     */
    void 
    setUnversioned(bool value);

    /**
     * @brief Set the number of tries before exiting from the program due to data unavailability
     * @param max_retries is the max number of retries
     */
    void
    setMaxRetries(unsigned max_retries);

    /**
     * @brief Function for starting the download
     * @param name is the ICN name of the content
     * @param rec_buffer is a pointer to the buffer that is going to be filled with the downloaded data
     * @param initial_chunk is the first chunk of the download. initial_chunk = -1 means start from the beginning
     * @param n_chunk indicates how many chunk retrieve after initial_chunk. Default behavior: download the whole file.
     */
    bool
    download(std::string name, std::vector<char> *rec_buffer, int initial_chunk = -1, int n_chunk = -1);

    /**
     * @brief Insert new data path to the path table
     * @param key The identifier of the path
     * @param path The DataPath itself
     */
    void
    insertToPathTable(std::string key, shared_ptr<DataPath> path);

    /**
     * @brief To add an observer on the downloader
     * @param obs an instance of NdnIcpDownloadObserver
     */
    void
    addObserver(NdnIcpDownloadObserver* obs);

    /**
     * @brief To add an observer on the downloader
     * @param obs an instance of ChunktimeObserver
     */
     void
     addObserver(ChunktimeObserver* obs);


    /**
     */
    void
    setAlpha(double alpha);

private:

    /**
     * @brief Update the RTT status after a data packet at slot is received. Also update the m_timer for the current path.
     * @param slot  the corresponding slot of the received data packet.
     */
    void
    updateRTT(uint64_t slot);

    /**
     *  @brief Update the window size
     */
    void
    increaseWindow();

    /**
     * @brief Decrease the window size
     */
    void
    decreaseWindow();

    /**
     * @brief Re-initialize the download indexes, but keep the current RAAQM parameters such as samples and window size
     */
    void
    reInitialize();

    /**
     * @brief Send an interest with the name specified in download
     */
    void
    sendPacket();

    /**
     * @brief Schedule the sending of a new packet
     */
    void
    scheduleNextPacket();

    /*
     * @brief Update drop probability and modify curwindow accordingly.
     */
    void
    RAQM();

    /**
     * @brief Update statistics and congestion control protocol parameters
     * @param slot is the received data slot
     */
    void
    afterDataReception(uint64_t slot);

    /**
     * @brief Ask first interest. Used just for versioned data.
     */
    void
    askFirst();

    /**
     * @brief Callback for process the incoming data
     * @param data is the data block received
     */
    void
    onData(const Data &data);

    /**
     * @brief Callback for process incoming interest (used by MLDR for M-flaged interest)
     * @param interest is the interes block received
     */
    
    void
    onInterest(const Interest &interest);

    /*
     * @brief Callback for process incoming nacks 
     * @param nack is the nack block received
     */

    void
    onNack(const Interest &nack);

    /**
     * @brief Callback for fist content received. Used just for versioned data.
     * @param data is the data block received
     */
    void
    onFirstData(const Data &data);

    /**
     * @brief Callback that re-express the interest after a timeout
     */
    void
    onTimeout(const Interest &interest);
 
    /**
     * The name of the content
     */
    Name m_dataName;

    /**
     * The name of the first content (just for versioned data)
     */
    Name m_firstDataName;

    /*
     * Flag specifying if downloading unversioned data or not
     */
    bool m_unversioned;

    /**
     * Max value of the window of interests
     */
    unsigned m_maxWindow;

    /**
     * Minimum drop probability
     */
    double m_pMin;

    /**
     * Final chunk number
     */
    uint64_t m_finalChunk;

    /**
     * Number of samples to be considered for the protocol computations
     */
    unsigned int m_samples;

    /**
     * Additive window increment value
     */
    const unsigned int m_gamma;

    /**
     * Multiplicative window decrement value
     */
    const double m_beta;

    /**
     * Allow the download of unfresh contents
     */
    bool m_allowStale;

    /**
     * Default lifetime for the interests
     */
    const time::milliseconds m_defaulInterestLifeTime;

    /**
     * Enable output for debugging purposes
     */
    bool m_isOutputEnabled;

    /**
     * Enable path report at the end of the download
     */
    bool m_isPathReportEnabled; //set to false by default

    /**
     * Number of pending interests inside the current window
     */
    unsigned m_winPending; // Number of pending interest

    /**
     * Current size of the windows
     */
    double m_winCurrent;

    /**
     * Initial value of the window, to be used if the application wants to reset the RAAQM parameters
     */
    double m_initialWindow;

    /**
     * First pending interest
     */
    uint64_t m_firstInFlight;

    /**
     * Total count of pending interests
     */
    unsigned m_outOfOrderCount;

    /**
     * Next interest to be sent
     */
    unsigned m_nextPendingInterest;

    /**
     * Position in the reception buffer of the final chunk
     */
    uint64_t m_finalSlot;

    /**
     * Total number of interests sent. Kept just for statistic purposes.
     */
    intmax_t m_interestsSent;

    /**
     * Total number of packets sent. Kept just for statistic purposes.
     */
    intmax_t m_packetsDelivered;

    /**
     * Total number of bytes received, including headers. Kept just for statistic purposes.
     */
    intmax_t m_rawBytesDataDelivered;

    /**
     * Total number of bytes received, without considering headers. Kept just for statistic purposes.
     */
    intmax_t m_bytesDataDelivered;

    /**
     *
     */
    bool m_isAbortModeEnabled;

    /**
     * Number of retransmitted interests in the window
     */
    unsigned m_winRetransmissions;

    /**
     * Flag indicating whether the download terminated or not
     */
    bool m_downloadCompleted;

    /**
     * Number of times an interest can be retransmitted
     */
    unsigned m_retxLimit;

    /**
     * Flag indicating whether the library is downloading or not
     */
    bool m_isSending;

    /**
     * Total number of interest timeouts. Kept just for statistic purposes.
     */
    intmax_t m_nTimeouts;

    /**
     * Current download path
     */
    shared_ptr<DataPath> m_curPath;

    /**
     * Initial path. Stored for resetting RAAQM
     */
    shared_ptr<DataPath> m_savedPath;

    /**
     * Flag indicating whether print statistics at he end or not
     */
    bool m_statistics;

    /**
     * Flag indicating if there is an observer or not.
     */

    bool m_observed;
    bool c_observed;

    /**
     * Start time of the download. Kept just for measurements.
     */
    struct timeval m_startTimeValue;

    /**
     * End time of the download. Kept just for measurements.
     */
    struct timeval m_stopTimeValue;

    /**
     * Timestamp of the last interest timeout
     */
    struct timeval m_lastTimeout;

    double m_averageWin;

    /**
     * Hash table for path: each entry is a pair path ID(key) - path object
     *
     */
    typedef boost::unordered_map<std::string, shared_ptr<DataPath>> HashTableForPath;
    HashTableForPath m_pathTable;

    /**
     * Buffer storing the data received out of order
     */
    PendingInterest m_outOfOrderInterests[RECEPTION_BUFFER];

    /**
     * The buffer provided by the application where storing the downloaded data
     */
    std::vector<char> *m_recBuffer;

    /**
     * The local face used for forwarding interests to the local forwarder and viceversa
     */
    Face m_face;

    /**
     * Set for M-flaged interests (used by MLDR)
     */
    typedef std::set<uint64_t> NackSet;
    NackSet m_nackSet;

    bool m_set_interest_filter;

   /**
    * Observer, NULL if none
    */
    NdnIcpDownloadObserver * m_observer;
    ChunktimeObserver * c_observer;

    NdnIcpDownloadRateEstimator *ndnIcpDownloadRateEstimator;
    Variables *m_variables;
    int maxPacketSize;
    uint64_t cumulativeBytesReceived;
    double dnltime_chunk;
    double speed_chunk;
    FILE * windowFile;
    typedef boost::chrono::duration<double> sec; // seconds, stored with a double
    boost::timer::cpu_timer timer;



};//end class NdnIcpDownload


} // namespace ndn

#endif //NDN_ICP_DOWNLOAD_H

