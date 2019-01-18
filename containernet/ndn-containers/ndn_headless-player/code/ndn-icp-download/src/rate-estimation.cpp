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
 */


#include "rate-estimation.hpp"

namespace ndn {

	Variables::Variables () :
	alpha(0.0),
	avgWin(0.0),
	avgRtt(0.0),
	rtt(0.0),
	instantaneousThroughput(0.0),
	winChange(0.0),
	batchingStatus(0),
	isRunning(false),
	winCurrent(0.0),
	isBusy(false)
	{}

        void* Timer(void* data)
	{
                Variables * m_variables = (Variables*) data;
		double datRtt, myAvgWin, myAvgRtt;
		int myWinChange, myBatchingParam, maxPacketSize;
                timeval now;

                while(m_variables->isBusy)
                {}
                m_variables->isBusy = true;
		datRtt = m_variables->rtt;
		m_variables->isBusy = false;

	        while(m_variables->isRunning)
		{
			usleep(KV * datRtt);
			while(m_variables->isBusy)
			{}
			m_variables->isBusy = true;
			datRtt = m_variables->rtt;
			myAvgWin = m_variables->avgWin;
			myAvgRtt = m_variables->avgRtt;
			myWinChange = m_variables->winChange;
			myBatchingParam = m_variables->batchingStatus;
			maxPacketSize = m_variables->maxPacketSize;
			m_variables->avgRtt = m_variables->rtt;
			m_variables->avgWin = 0;
			m_variables->winChange = 0;
			m_variables->batchingStatus = 1;

			m_variables->isBusy = false;
			if(myBatchingParam == 0 || myWinChange == 0)
				continue;
			if(m_variables->instantaneousThroughput == 0)
				m_variables->instantaneousThroughput = (myAvgWin * 8.0 * maxPacketSize * 1000000.0 / (1.0 * myWinChange)) / (myAvgRtt / (1.0 * myBatchingParam));

	                m_variables->instantaneousThroughput = m_variables->alpha * m_variables->instantaneousThroughput + (1 - m_variables->alpha) * ( (myAvgWin * 8.0 * maxPacketSize * 1000000.0 / (1.0 * myWinChange)) / (myAvgRtt / (1.0 * myBatchingParam) ) );
                        gettimeofday(&now, 0);

                       // printf("time: %f instantaneous: %f\n", DataPath::getMicroSeconds(now), m_variables->instantaneousThroughput);
		       // fflush(stdout);
	      	}
	}

        InterRttEstimator::InterRttEstimator(Variables *var)
        {
		this->m_variables = var;
                this->ThreadisRunning = false;
                this->myTh = NULL;
		gettimeofday(&(this->m_variables->begin), 0);
        }

void    InterRttEstimator::onRttUpdate(double rtt) {

            while(m_variables->isBusy)
            {}
            m_variables->isBusy = true;
            m_variables->rtt = rtt;
	    m_variables->batchingStatus++;
	    m_variables->avgRtt += rtt;
            m_variables->isBusy = false;

            if(!ThreadisRunning)
            {
                myTh = (pthread_t*)malloc(sizeof(pthread_t));
	        if (!myTh)
		{
		        std::cerr << "Error allocating thread." << std::endl;
			myTh = NULL;
		}
                if(int err = pthread_create(myTh, NULL, ndn::Timer, (void*)this->m_variables))
		{
			std::cerr << "Error creating the thread" << std::endl;
			myTh = NULL;
		}
                ThreadisRunning = true;
            }
        }

void    InterRttEstimator::onWindowIncrease(double winCurrent) {

            timeval end;
	    gettimeofday(&end, 0);
	    double delay = DataPath::getMicroSeconds(end) - DataPath::getMicroSeconds(this->m_variables->begin);

	while(m_variables->isBusy)
        {}
        m_variables->isBusy = true;
        m_variables->avgWin += this->m_variables->winCurrent * delay;
        m_variables->winCurrent = winCurrent;
	m_variables->winChange += delay;
        m_variables->isBusy = false;

	gettimeofday(&(this->m_variables->begin), 0);
        }

void    InterRttEstimator::onWindowDecrease(double winCurrent) {

            timeval end;
	    gettimeofday(&end, 0);
	    double delay = DataPath::getMicroSeconds(end) - DataPath::getMicroSeconds(this->m_variables->begin);
	    while(m_variables->isBusy)
            {}
            m_variables->isBusy = true;
            m_variables->avgWin += this->m_variables->winCurrent * delay;
            m_variables->winCurrent = winCurrent;
	    m_variables->winChange += delay;
            m_variables->isBusy = false;

	    gettimeofday(&(this->m_variables->begin), 0);
        }


        SimpleEstimator::SimpleEstimator(Variables *var, int param)
        {
		this->m_variables = var;
		this->batchingParam = param;
		gettimeofday(&(this->m_variables->begin), 0);
        }

void 	SimpleEstimator::onDataReceived(int packetSize)
{
	while(this->m_variables->isBusy)
	{}
	this->m_variables->isBusy = true;
	this->m_variables->avgWin += packetSize;
	this->m_variables->isBusy = false;
}

void    SimpleEstimator::onRttUpdate(double rtt)
	{
		int nbrOfPackets = 0;
		while(this->m_variables->isBusy)
		{}
		this->m_variables->isBusy = true;
		this->m_variables->batchingStatus++;
		nbrOfPackets = this->m_variables->batchingStatus;
		this->m_variables->isBusy = false;

		if(nbrOfPackets == this->batchingParam)
		{
			timeval end;
			gettimeofday(&end, 0);
			double delay = DataPath::getMicroSeconds(end) - DataPath::getMicroSeconds(this->m_variables->begin);
			while(this->m_variables->isBusy)
			{}
			this->m_variables->isBusy = true;
			float inst = this->m_variables->instantaneousThroughput;
			double alpha = this->m_variables->alpha;
			int maxPacketSize = this->m_variables->maxPacketSize;
			double sizeTotal = this->m_variables->avgWin; //Here we use avgWin to store the total size downloaded during the time span of nbrOfPackets
			this->m_variables->isBusy = false;

			//Assuming all packets carry maxPacketSize bytes of data (8*maxPacketSize bits); 1000000 factor to convert us to seconds
			if(inst)
			{
				inst = alpha * inst + (1 - alpha) * (sizeTotal * 8 * 1000000.0 / (delay));
			}
			else
				inst = sizeTotal * 8 * 1000000.0 / (delay);
						timeval now;
			gettimeofday(&now, 0);
			//printf("time: %f, instantaneous: %f\n", DataPath::getMicroSeconds(now), inst);
			//fflush(stdout);

			while(this->m_variables->isBusy)
			{}
			this->m_variables->isBusy = true;
			this->m_variables->batchingStatus = 0;
			this->m_variables->instantaneousThroughput = inst;
			this->m_variables->avgWin = 0.0;
			this->m_variables->isBusy = false;

			gettimeofday(&(this->m_variables->begin),0);
		}
	}

        BatchingPacketsEstimator::BatchingPacketsEstimator(Variables *var, int param)
        {
		this->m_variables = var;
		this->batchingParam = param;
		gettimeofday(&(this->m_variables->begin), 0);
	}

void    BatchingPacketsEstimator::onRttUpdate(double rtt)
	{
                int nbrOfPackets = 0;
		while(this->m_variables->isBusy)
		{}
		this->m_variables->isBusy = true;
		this->m_variables->batchingStatus++;
		this->m_variables->avgRtt += rtt;
		nbrOfPackets = this->m_variables->batchingStatus;
		this->m_variables->isBusy = false;

                if(nbrOfPackets == this->batchingParam)
                {
			while(this->m_variables->isBusy)
			{}
			this->m_variables->isBusy = true;
			double inst = this->m_variables->instantaneousThroughput;
			double alpha = this->m_variables->alpha;
                        double myAvgWin = m_variables->avgWin;
			double myAvgRtt = m_variables->avgRtt;
			double myWinChange = m_variables->winChange;
			int maxPacketSize = m_variables->maxPacketSize;
			this->m_variables->isBusy = false;
			if(inst == 0)
				inst = (myAvgWin * 8.0 * maxPacketSize * 1000000.0 / (1.0 * myWinChange)) / (myAvgRtt / (1.0 * nbrOfPackets));
			else
				inst = alpha * inst + (1 - alpha) * ((myAvgWin * 8.0 * maxPacketSize * 1000000.0 / (1.0 * myWinChange)) / (myAvgRtt / (1.0 * nbrOfPackets)));
			timeval now;
			gettimeofday(&now, 0);
			//printf("time: %f, instantaneous: %f\n", DataPath::getMicroSeconds(now), inst);
			//fflush(stdout);

			while(this->m_variables->isBusy)
			{}
			this->m_variables->isBusy = true;
			this->m_variables->batchingStatus = 0;
			this->m_variables->avgWin = 0;
			this->m_variables->avgRtt = 0;
			this->m_variables->winChange = 0;
			this->m_variables->instantaneousThroughput = inst;
			this->m_variables->isBusy = false;
                }
        }

void   BatchingPacketsEstimator::onWindowIncrease(double winCurrent) {

            timeval end;
	    gettimeofday(&end, 0);
	    double delay = DataPath::getMicroSeconds(end) - DataPath::getMicroSeconds(this->m_variables->begin);

	while(m_variables->isBusy)
        {}
        m_variables->isBusy = true;
        m_variables->avgWin += this->m_variables->winCurrent * delay;
        m_variables->winCurrent = winCurrent;
	m_variables->winChange += delay;
        m_variables->isBusy = false;

	gettimeofday(&(this->m_variables->begin), 0);
        }

void    BatchingPacketsEstimator::onWindowDecrease(double winCurrent) {

            timeval end;
	    gettimeofday(&end, 0);
	    double delay = DataPath::getMicroSeconds(end) - DataPath::getMicroSeconds(this->m_variables->begin);
	    while(m_variables->isBusy)
            {}
            m_variables->isBusy = true;
            m_variables->avgWin += this->m_variables->winCurrent * delay;
            m_variables->winCurrent = winCurrent;
	    m_variables->winChange += delay;
            m_variables->isBusy = false;

	    gettimeofday(&(this->m_variables->begin), 0);
        }
}

