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

#ifndef NDNICPDOWNLOAD

#include "NDNConnectionConsumerApi.h"

#define DEFAULT_LIFETIME 250
#define RETRY_TIMEOUTS 5

using namespace dash;
using namespace dash::network;
using namespace dash::metrics;
using namespace ndn;

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

using duration_in_seconds = std::chrono::duration<double, std::ratio<1, 1> >;


namespace libdash {
	namespace framework {
		namespace input {

		NDNConnectionConsumerApi::NDNConnectionConsumerApi(double alpha) :
					m_recv_name(std::string()),
					m_first(1),
					m_isFinished(false),
					sizeDownloaded (0),
					cumulativeBytesReceived(0),
					ndnAlpha(alpha)
		{
		    drop_factor = 0.95;
		    beta = 0.05;
		    gamma = 1;

			this->speed = 0.0;
			this->dnltime = 0.0;
			this->deezData = NULL;
			this->datSize = 0;
			this->dataPos = 0;
		    InitializeConditionVariable (&this->contentRetrieved);
		    InitializeCriticalSection   (&this->monitorMutex);

			this->myConsumer = new Consumer(Name(""), RAAQMDR);
			this->myConsumer->setContextOption(INTEREST_LIFETIME, DEFAULT_LIFETIME);
			this->myConsumer->setContextOption(INTEREST_RETX, RETRY_TIMEOUTS);
			this->myConsumer->setContextOption(BETA_VALUE, beta);
			this->myConsumer->setContextOption(GAMMA_VALUE, (int)gamma);
			this->myConsumer->setContextOption(DROP_FACTOR, drop_factor);
			this->myConsumer->setContextOption(RATE_ESTIMATION_ALPHA, this->ndnAlpha);
			this->myConsumer->setContextOption(RATE_ESTIMATION_OBSERVER, this);

			this->myConsumer->setContextOption(CONTENT_RETRIEVED, (ConsumerContentCallback) bind(&NDNConnectionConsumerApi::processPayload, this, _1, _2, _3));
			this->myConsumer->setContextOption(DATA_TO_VERIFY, (ConsumerDataVerificationCallback)bind(&NDNConnectionConsumerApi::onPacket, this, _1, _2));
#ifdef NO_GUI
			if(this->ndnAlpha != 20)
				this->ndnRateBased = true;
#else
			this->ndnRateBased = true;
#endif
		    Debug("NDN class created\n");

		}

		NDNConnectionConsumerApi::~NDNConnectionConsumerApi() {
			delete this->myConsumer;
			if(this->deezData)
			{
				free(this->deezData);
				this->deezData = NULL;
			}
		    DeleteConditionVariable (&this->contentRetrieved);
		    DeleteCriticalSection   (&this->monitorMutex);
		}

		void NDNConnectionConsumerApi::Init(IChunk *chunk) {
			Debug("NDN Connection:	STARTING\n");
		    m_first = 1;
		    sizeDownloaded = 0;
		    m_name = "ndn:/" + chunk->Host() + chunk->Path();
		    m_isFinished = false;

		    res = false;
		    dataPos = 0;
		    datSize = 0;
		    if(this->deezData)
		    {
		    	free(this->deezData);
		    	deezData = NULL;
		    }

		    Debug("API_NDN_Connection:\tINTIATED_to_name %s\n", m_name.c_str());
		    L("API_NDN_Connection:\tSTARTING DOWNLOAD %s\n", m_name.c_str());
		}

		void NDNConnectionConsumerApi::InitForMPD(const std::string& url)
		{
			m_first = 1;
			sizeDownloaded = 0;

			if(url.find("//") != std::string::npos)
			{
				int pos = url.find("//");
				char* myName = (char*)malloc(strlen(url.c_str()) - 1);
				strncpy(myName, url.c_str(), pos + 1);
				strncpy(myName + pos + 1, url.c_str() + pos + 2, strlen(url.c_str()) - pos - 2);
				m_name = std::string(myName);
			}
			else
			{
				m_name = url;
			}
			m_isFinished = false;

		    res = false;
		    dataPos = 0;
		    datSize = 0;

		    if(this->deezData)
		    {
		      	free(this->deezData);
		      	deezData = NULL;
		    }

		    Debug("API_NDN_Connection:\tINTIATED_for_mpd %s\n", m_name.c_str());
			    L("API_NDN_Connection:\tSTARTING DOWNLOAD %s\n", m_name.c_str());
		}


		int				NDNConnectionConsumerApi::Read(uint8_t* data, size_t len, IChunk *chunk)
		{
			return this->Read(data, len);
		}

		int				NDNConnectionConsumerApi::Read(uint8_t *data, size_t len)
		{
			if(!res)
				m_start_time = std::chrono::system_clock::now();

			if(res)
			{
				if(this->dataPos == this->datSize)
				{
					this->dnltime = std::chrono::duration_cast<duration_in_seconds>(std::chrono::system_clock::now() - m_start_time).count();
					if(speed == 0 || !this->ndnRateBased)
						speed = (double) (sizeDownloaded * 8 / this->dnltime);
					cumulativeBytesReceived += sizeDownloaded;
					L("API_NDN_Connection:\tFINISHED DOWNLOADING %s Average_DL: %f size: %lu cumulative: %lu Throughput: %f\n", m_name.c_str(), speed, sizeDownloaded, cumulativeBytesReceived, (double) (sizeDownloaded * 8 / this->dnltime));
					free(this->deezData);
					this->deezData = NULL;
					return 0;
				}
				if((this->datSize - this->dataPos) > (int)len)
				{
					memcpy(data, this->deezData + this->dataPos, len);
					this->dataPos += len;
					sizeDownloaded += len;
					return len;
				}
				else
				{
					assert(this->datSize - this->dataPos > 0);
					memcpy(data, this->deezData + this->dataPos, this->datSize - this->dataPos);
					int temp = this->datSize - this->dataPos;
					this->dataPos += this->datSize - this->dataPos;
					sizeDownloaded += temp;
					return temp;
				}
			}

			this->myConsumer->consume(Name(m_name));

			EnterCriticalSection(&this->monitorMutex);
			while(this->m_isFinished == false)
				SleepConditionVariableCS(&this->contentRetrieved, &this->monitorMutex, INFINITE);
			assert(this->datSize != 0);
			this->res = true;
			LeaveCriticalSection(&this->monitorMutex);

			if(this->datSize > (int)len)
			{
				memcpy(data, this->deezData, len);
				this->dataPos += len;
				sizeDownloaded += len;
				return len;
			}
			else
			{
				memcpy(data, this->deezData, this->datSize);
				this->dataPos += this->datSize;
				sizeDownloaded += this->datSize;
				return this->datSize;
			}
		}

		int             NDNConnectionConsumerApi::Peek(uint8_t *data, size_t len, IChunk *chunk) {
		    return -1;
		}

		bool   NDNConnectionConsumerApi::onPacket(Consumer& c, const Data& data)
		{
			return true;
		}

		void   NDNConnectionConsumerApi::processPayload(Consumer& c, const uint8_t* buffer, size_t bufferSize)
		{
			EnterCriticalSection(&this->monitorMutex);
			this->deezData = (char *)malloc(bufferSize*sizeof(uint8_t));
			memcpy(this->deezData, buffer, bufferSize*sizeof(uint8_t));
			this->m_isFinished = true;
			this->datSize = (int) bufferSize;
			WakeAllConditionVariable(&this->contentRetrieved);
			LeaveCriticalSection(&this->monitorMutex);
		}

		double NDNConnectionConsumerApi::GetAverageDownloadingSpeed()
		{
			Debug("NDNConnection:	DL speed is %f\n", this->speed);
			return this->speed;
		}

		double NDNConnectionConsumerApi::GetDownloadingTime()
		{
			Debug("NDNConnection:	DL time is %f\n", this->dnltime);
			return this->dnltime;
		}

		const std::vector<ITCPConnection *> &NDNConnectionConsumerApi::GetTCPConnectionList() const {
		    return tcpConnections;
		}

		const std::vector<IHTTPTransaction *> &NDNConnectionConsumerApi::GetHTTPTransactionList() const {
		    return httpTransactions;
		}

		void NDNConnectionConsumerApi::notifyStats(double winSize)
		{
			this->speed = (winSize); // * 1000000 * 1400 * 8;
			L("API_NDNConnection:\tNotificationICPDL\t%f\t%f\n", winSize, speed);
		}

		void NDNConnectionConsumerApi::notifyChunkStats(std::string chunkName, double chunkThroughput) //for measurement on chunk-level
		{
			L("API_NDNConnection:\tChunk-Information(API)\t%s\t%f\n", chunkName.c_str(), chunkThroughput);
		}

		} /* namespace input */
	} /* namespace framework */
} /* namespace libdash */


#endif //NDEF NDNICPDOWNLOAD
