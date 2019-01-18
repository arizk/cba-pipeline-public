/*
 * NDNConnectionConsumerApi.h
 *
 ****************************************
 *
 *  Copyright (c) Cisco Systems 2016
 *      Author: Jacques Samain <jsamain@cisco.com>
 *
 *	NDN Connection class based on the Consumer/Producer API.
 *
 ****************************************
 */

#ifndef QTSAMPLEPLAYER_LIBDASHFRAMEWORK_INPUT_NDNCONNECTIONCONSUMERAPI_H_
#define QTSAMPLEPLAYER_LIBDASHFRAMEWORK_INPUT_NDNCONNECTIONCONSUMERAPI_H_


#include "../libdash/source/portable/Networking.h"
#include "INDNConnection.h"
#include "../../debug.h"
#include "log/log.h"

#include <sys/types.h>
#include <string>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <inttypes.h>
#include <stdlib.h>
#include <stdarg.h>
#include <algorithm>
#include <ndn-cxx/security/validator.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include "Consumer-Producer-API/consumer-context.hpp"
#include "Consumer-Producer-API/manifest.hpp"
#include "Consumer-Producer-API/download-observer.hpp"
#include <future>
#include <inttypes.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

#include "../libdashframework/Portable/MultiThreading.h"
#include <boost/exception/diagnostic_information.hpp>

//logging purpose
#include <chrono>
#include <stdarg.h>


namespace libdash {
	namespace framework {
		namespace input {

		class NDNConnectionConsumerApi : public INDNConnection, public ndn::NdnObserver {
			public:
				NDNConnectionConsumerApi(double alpha);
				virtual ~NDNConnectionConsumerApi();

                virtual void Init(dash::network::IChunk *chunk);

                void InitForMPD(const std::string& url);

                virtual int Read(uint8_t *data, size_t len, dash::network::IChunk *chunk);

                int Read(uint8_t *data, size_t len);

                virtual int Peek(uint8_t *data, size_t len, dash::network::IChunk *chunk);

                virtual double GetAverageDownloadingSpeed();

                virtual double GetDownloadingTime();

                void processPayload(ndn::Consumer& , const uint8_t*, size_t);

                bool onPacket(ndn::Consumer& , const ndn::Data&);

//              virtual void onData(const ndn::Interest &interest, const ndn::Data &data);

 //             virtual void onTimeout(const ndn::Interest &interest);


                /*
                 *  IDASHMetrics
                 *  Can't be deleted...
                 */
                const std::vector<dash::metrics::ITCPConnection *> &GetTCPConnectionList() const;

                const std::vector<dash::metrics::IHTTPTransaction *> &GetHTTPTransactionList() const;

                /*
                 *	NdnObserver
                 */
                virtual void notifyStats(double throughput);

                virtual void notifyChunkStats(std::string chunkName, double chunkThroughput); //for measurement on chunk-level

            private:
                uint64_t i_chunksize;
                int i_lifetime;
                int i_missed_co;
                /**< number of content objects we missed in NDNBlock */

                std::string m_name;
                ndn::Name m_recv_name;
                ndn::Face m_face;
                int m_first;
                bool m_isFinished;
                uint64_t m_nextSeg;

                double ndnAlpha;
                bool ndnRateBased;

                bool allow_stale;
                int sysTimeout;
                unsigned InitialMaxwindow;
                unsigned int timer;
                double drop_factor;
                double p_min;
               	double beta;
               	unsigned int gamma;
               	unsigned int samples;
               	unsigned int nchunks; // XXX chunks=-1 means: download the whole file!
               	bool output;
               	bool report_path;
               	ndn::Consumer* myConsumer;
                bool res;
            	std::vector<char> mdata;
            	char* deezData;
            	int datSize;
            	int dataPos;
            	int firstChunk;
                double  speed; // in bps
                double dnltime; //in seconds
                uint64_t sizeDownloaded;
                std::chrono::time_point<std::chrono::system_clock> m_start_time;

                std::vector<dash::metrics::ITCPConnection *> tcpConnections;
                std::vector<dash::metrics::IHTTPTransaction *> httpTransactions;
                uint64_t cumulativeBytesReceived;

                mutable CRITICAL_SECTION monitorMutex;
                mutable CONDITION_VARIABLE contentRetrieved;
		};

		} /* namespace input */
	} /* namespace framework */
} /* namespace libdash */

#endif /* QTSAMPLEPLAYER_LIBDASHFRAMEWORK_INPUT_NDNCONNECTIONCONSUMERAPI_H_ */
