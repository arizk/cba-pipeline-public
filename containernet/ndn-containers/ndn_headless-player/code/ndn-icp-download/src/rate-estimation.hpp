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
 * @author Jacques Samain <jsamain@cisco.com>
 *
 */

#ifndef RATE_ESTIMATOR_H
#define RATE_ESTIMATOR_H

#include <unistd.h>

#include <ndn-cxx/face.hpp>
#include <boost/unordered_map.hpp>

#include "data-path.hpp"
#include "pending-interest.hpp"

#define BATCH 50
#define KV 20

namespace ndn {
	class Variables
	{
		public:
		Variables();

		struct timeval begin;
		volatile double alpha;
		volatile double avgWin;
		volatile double avgRtt;
		volatile double rtt;
		volatile double instantaneousThroughput;
		volatile double winChange;
		volatile int batchingStatus;
		volatile bool isRunning;
		volatile double winCurrent;
		volatile bool isBusy;
		volatile int maxPacketSize;
	}; //end class Variables

	class NdnIcpDownloadRateEstimator
	{
		public:
		NdnIcpDownloadRateEstimator(){};
		~NdnIcpDownloadRateEstimator(){};

		virtual void onRttUpdate(double rtt){};
		virtual void onDataReceived(int packetSize){};
		virtual void onWindowIncrease(double winCurrent){};
		virtual void onWindowDecrease(double winCurrent){};

		Variables *m_variables;
	};

	//A rate estimator based on the RTT: upon the reception 
	class InterRttEstimator : public NdnIcpDownloadRateEstimator
	{
		public:
		InterRttEstimator(Variables *var);

		void onRttUpdate(double rtt);
		void onDataReceived(int packetSize) {};
		void onWindowIncrease(double winCurrent);
		void onWindowDecrease(double winCurrent);

		private:
		pthread_t * myTh;
		bool ThreadisRunning;
	};

	//A rate estimator, this one 
	class BatchingPacketsEstimator : public NdnIcpDownloadRateEstimator
	{
		public:
		BatchingPacketsEstimator(Variables *var,int batchingParam);

		void onRttUpdate(double rtt);
		void onDataReceived(int packetSize) {};
		void onWindowIncrease(double winCurrent);
		void onWindowDecrease(double winCurrent);

		private:
		int batchingParam;
		pthread_t * myTh;
		bool ThreadisRunning;
	};

	//A Rate estimator, this one is the simplest: counting batchingParam packets and then divide the sum of the size of these packets by the time taken to DL them.
	class SimpleEstimator : public NdnIcpDownloadRateEstimator
	{
		public:
		SimpleEstimator(Variables *var, int batchingParam);

		void onRttUpdate(double rtt);
		void onDataReceived(int packetSize);
		void onWindowIncrease(double winCurrent){};
		void onWindowDecrease(double winCurrent){};

		private:
		int batchingParam;
	};

	void *Timer(void * data);

}//end of namespace ndn
#endif //RATE_ESTIMATOR_H

