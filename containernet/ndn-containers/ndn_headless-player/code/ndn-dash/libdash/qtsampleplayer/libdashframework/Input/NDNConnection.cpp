/*
 * NDNConnection.cpp
 *****************************************************************************
 * NDNx input module for libdash
 *
 * Author: Qian Li <qian.li@irt-systemx.fr>
 *		   Jacques Samain <jsamain@cisco.com>
 *
 * Copyright (C) 2016 IRT SystemX, Cisco
 *
 * Based on NDNx input module for VLC by Jordan Augé
 * Portions Copyright (C) 2015 Cisco Systems
 * Based on CCNx input module for libdash by
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 * Based on the NDNx input module by UCLA
 * Portions Copyright (C) 2013 Regents of the University of California.
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 * This work is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *****************************************************************************/

#ifdef NDNICPDOWNLOAD

#include "NDNConnection.h"
#include <iostream>
#define BOOST_THREAD_PROVIDES_FUTURE
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <iostream>
#include <boost/bind.hpp>


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

using namespace libdash::framework::input;

using namespace dash;
using namespace dash::network;
using namespace dash::metrics;

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;

using duration_in_seconds = std::chrono::duration<double, std::ratio<1, 1> >;

NDNConnection::NDNConnection(double alpha, ChunktimeObserver* c_obs) :
			m_recv_name(std::string()),
			m_first(1),
			m_isFinished(false),
			cumulativeBytesReceived(0),
			ndnAlpha(alpha)
{
    i_chunksize = -1;
    i_missed_co = 0;
    i_lifetime = DEFAULT_LIFETIME;
    allow_stale = false;
    InitialMaxwindow = RECEPTION_BUFFER - 1; // Constants declared inside ndn-icp-download.hpp
    double InitialWindow = 50;
    timer = DEFAULT_INTEREST_TIMEOUT;
    drop_factor = 0.96;
    p_min = P_MIN;
    beta = 0.97; //Packet loss occurs burst so e.g. 250*0,95^10 was 0.05 decrease factor
    gamma = 2.5; //growth factor, was 1 increase to 5
    samples = SAMPLES;
    output = false;
    report_path = false;
    this->firstChunk = -1;
    output = false;
    report_path = false;
	


    this->ndnicpdownload = new ndn::NdnIcpDownload(this->InitialMaxwindow,InitialWindow, this->gamma, this->beta, this->allow_stale, 1000);
    this->ndnicpdownload->setAlpha(ndnAlpha);

	ndnicpdownload->addObserver(this); //ALWAYS OBSERVE
	if(c_obs != NULL)
		ndnicpdownload->addObserver(c_obs);

    std::shared_ptr<ndn::DataPath> initPath = std::make_shared<ndn::DataPath>(drop_factor, p_min, timer, samples);
    initPath->setAlpha(ndnAlpha);
    ndnicpdownload->insertToPathTable(DEFAULT_PATH_ID, initPath);
    ndnicpdownload->setCurrentPath(initPath);
    ndnicpdownload->setStartTimeToNow();
    ndnicpdownload->setLastTimeOutToNow();
    ndnicpdownload->setUnversioned(true);
    if (output)
    	ndnicpdownload->enableOutput();
    if (report_path)
        ndnicpdownload->enablePathReport();

    Debug("NDN class created\n");
}


NDNConnection::~NDNConnection() {
	delete ndnicpdownload;
}

void NDNConnection::Init(IChunk *chunk) {
	Debug("NDN Connection:	STARTING\n");
    m_first = 1;
    m_buffer.size = 0;
    sizeDownloaded = 0;
    m_name = "ndn:/" + chunk->Host() + chunk->Path();
   	m_nextSeg = 0;
    m_isFinished = false;

    res = false;
    dataPos = 0;
    datSize = 0;
    deezData = NULL;

    Debug("NDN_Connection:\tINTIATED_to_name %s\n", m_name.c_str());
    L("NDN_Connection:\tSTARTING DOWNLOAD %s\n", m_name.c_str());
}

void NDNConnection::InitForMPD(const std::string& url)
{
	m_first = 1;
	m_buffer.size = 0;
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
	m_nextSeg = 0;
	m_isFinished = false;

    res = false;
    dataPos = 0;
    datSize = 0;
    deezData = NULL;

    Debug("NDN_Connection:\tINTIATED_for_mpd %s\n", m_name.c_str());
	    L("NDN_Connection:\tSTARTING DOWNLOAD %s\n", m_name.c_str());
}

int             NDNConnection::Peek(uint8_t *data, size_t len, IChunk *chunk) {
    return -1;
}

void NDNConnection::doFetch() {

    ndn::Name name(m_name);

    if (m_first == 0)
    {
        name.append(m_recv_name[-2]).appendSegment(m_nextSeg);
    }

    ndn::Interest interest(name, ndn::time::milliseconds(i_lifetime));
    Debug("fetching %s\n", m_name.c_str());
    if (m_first == 1)
    {
    	m_start_time = std::chrono::system_clock::now();
		m_last_fetch_chunk = std::chrono::system_clock::now();
        interest.setMustBeFresh(true);
#ifdef FRESH
        } else {
    interest.setMustBeFresh(true);
#endif // FRESH
    }

	Debug("NDN_Init_CHUNK:\n"  );
    L("NDN_Init_CHUNK:\n"  );
    m_face.expressInterest(interest, bind(&NDNConnection::onData, this, _1, _2),
                           bind(&NDNConnection::onTimeout, this, _1));

}


/*****************************************************************************
 * CALLBACKS
 *****************************************************************************/

void NDNConnection::onData(const ndn::Interest &interest, const ndn::Data &data) {

	m_nextSeg++;
    if (m_isFinished)
    {
    	return;
    }

    if (m_first == 1) {
       m_recv_name = data.getName();
        i_chunksize = data.getContent().value_size();

        m_first = 0;
    }

	if (true) {
		dnltime_chunk = std::chrono::duration_cast<duration_in_seconds>(std::chrono::system_clock::now() - m_last_fetch_chunk).count();
        speed_chunk = (double) (i_chunksize * 8/ dnltime_chunk);
		L("NDNConnection::onData\n");
		Debug("NDN_Connection_SEGMENT:\tonData %s, Average_DL: %f\n", m_name.c_str(), speed_chunk);
        L("NDN_Connection_SEGMENT:\tonData %s, Average_DL: %f\n",  m_name.c_str(), speed_chunk);
		m_last_fetch_chunk = std::chrono::system_clock::now();
	}

    const ndn::Block &content = data.getContent();

    memcpy(m_buffer.data, reinterpret_cast<const char *>(content.value()), content.value_size());
    m_buffer.size = content.value_size();
    m_buffer.start_offset = 0;

    sizeDownloaded += m_buffer.size;
    const ndn::name::Component& finalBlockId = data.getMetaInfo().getFinalBlockId();
      if (finalBlockId == data.getName()[-1])
      {
		//Stopping ReadStopAndWait when last segment was fetched in this method
        m_isFinished = true;
      }
}


void NDNConnection::onTimeout(const ndn::Interest &interest) {

	//TODO What do we do on timeouts?
/*    m_buffer.size = 0;
    m_buffer.start_offset = 0;


    if (!m_first && interest.getName()[-1].toSegment() == m_cid_timing_out) {
        m_timeout_counter = (m_timeout_counter + 1) % (i_retrytimeout + 2);
        if (m_timeout_counter > NDN_MAX_NDN_GET_TRYS) {
            b_last = true;
        }
    }
    else {
        m_cid_timing_out = m_first ? 0 : interest.getName()[-1].toSegment();
        m_timeout_counter = 1;
        if (m_timeout_counter > NDN_MAX_NDN_GET_TRYS) {
        	Debug("Abort, abort\n\n");
            b_last = true;
        }
    }
*/
}

int NDNConnection::ReadStopAndWait(uint8_t *data, size_t len, IChunk *chunk) {

    while (true) {
        if (m_isFinished)
        {
        	this->dnltime = std::chrono::duration_cast<duration_in_seconds>(std::chrono::system_clock::now() - m_start_time).count();
        	speed = (double) (sizeDownloaded * 8/ this->dnltime);
        	cumulativeBytesReceived += sizeDownloaded;
        	L("NDN_Connection:\tFINISHED DOWNLOADING %s, Average_DL: %f, size: %lu, cumulative: %lu\n", m_name.c_str(), speed, sizeDownloaded, cumulativeBytesReceived);
            return 0;
        }

        doFetch();

        try {
            m_face.processEvents();
        } catch (...) {
            std::cerr << "Unexpected exception during the fetching, diagnostic informations:\n" <<
            boost::current_exception_diagnostic_information();

            return 0;
        }

        if (m_buffer.size <= 0) {
            i_missed_co++;
            //i_missed_co is the number of timeouts we have during the DL of one chunk
            continue;
        }

        memcpy(data, m_buffer.data, m_buffer.size);
        if (m_buffer.size > 0) {
            return m_buffer.size;
        }
    }
}

int NDNConnection::ReadBetter(uint8_t *data, size_t len, IChunk *chunk)  {

	if(this->firstChunk == -1) //this means that we start the DL, thus the timer is launched.
		m_start_time = std::chrono::system_clock::now();

	if(res)
	{
    	this->dnltime = std::chrono::duration_cast<duration_in_seconds>(std::chrono::system_clock::now() - m_start_time).count();
    	speed = (double) (sizeDownloaded * 8 / this->dnltime);
    	cumulativeBytesReceived += sizeDownloaded;
    	L("NDN_Connection:\tFINISHED DOWNLOADING %s Average_DL: %f size: %lu cumulative: %lu \n", m_name.c_str(), speed, sizeDownloaded, cumulativeBytesReceived);
		return 0;
	}
	this->mdata.clear();
	nchunks = len / 1000; //TODO how to know in advance the chunksize, ie: 1000?
	this->res = this->ndnicpdownload->download(this->m_name, &(this->mdata), this->firstChunk, nchunks);
	int size = this->mdata.size();
	int i = 0;
	for(std::vector<char>::const_iterator it = this->mdata.begin(); it != this->mdata.end(); it++)
	{
		data[i] = *it;
		i++;
	}
	if(this->firstChunk == -1)
		this->firstChunk = 0;
	this->firstChunk += nchunks;
	sizeDownloaded += size;
	return size;
}

int NDNConnection::ReadFull(uint8_t *data, size_t len, IChunk *chunk)  {

	if(!res)
	{
		L("NDNConnection::ReadFull-Init\n");
		m_start_time = std::chrono::system_clock::now();
	}
	if(res)
	{
		if(this->dataPos == this->datSize)
		{
			this->dnltime = std::chrono::duration_cast<duration_in_seconds>(std::chrono::system_clock::now() - m_start_time).count();
			if(!this->ndnRateBased)
				speed = (double) (sizeDownloaded * 8 / this->dnltime);
			cumulativeBytesReceived += sizeDownloaded;
			L("NDN_Connection:\tFINISHED DOWNLOADING %s Average_DL: %f size: %lu cumulative: %lu Throughput: %f\n", m_name.c_str(), speed, sizeDownloaded, cumulativeBytesReceived, (double) (sizeDownloaded * 8 / this->dnltime));
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
	this->mdata.clear();

	L("NDN_Connection:\tSTARTINF ICP DOWNLOAD\n");
	boost::future<bool> f = boost::async(boost::bind(&ndn::NdnIcpDownload::download, this->ndnicpdownload, this->m_name, &(this->mdata), this->firstChunk, -1));
	this->res = f.get();
	this->datSize = this->mdata.size();
	this->deezData = (char *)malloc(this->datSize);
	int i = 0;
	std::copy(this->mdata.begin(), this->mdata.end(), this->deezData);

	/*	for(std::vector<char>::const_iterator it = this->mdata.begin(); it != this->mdata.end(); it++)
		{
		deezData[i] = *it;
		i++;
		}
 	*/	

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

int NDNConnection::Read(uint8_t *data, size_t len, IChunk *chunk) {

	//return ReadStopAndWait(data, len, chunk);
	//return ReadBetter(data, len, chunk);
	return ReadFull(data, len, chunk);
}

int NDNConnection::Read(uint8_t *data, size_t len) {			//method to read the mpd file

	if(!res)
		m_start_time = std::chrono::system_clock::now();
	
	if(res)
	{
		if(this->dataPos == this->datSize)
		{
			this->dnltime = std::chrono::duration_cast<duration_in_seconds>(std::chrono::system_clock::now() - m_start_time).count();
			speed = (double) (sizeDownloaded * 8 / this->dnltime);
			cumulativeBytesReceived += sizeDownloaded;
			L("NDN_Connection:\tFINISHED DOWNLOADING %s, Average_DL: %f, size: %lu, cumulative: %lu\n", m_name.c_str(), speed, sizeDownloaded, cumulativeBytesReceived);
			return 0;
		}
		if((this->datSize - this->dataPos) > len)
		{
			memcpy(data, this->deezData + this->dataPos, len);
			this->dataPos += len;
			sizeDownloaded += len;
			return len;
		}
		else
		{
			memcpy(data, this->deezData + this->dataPos, this->datSize - this->dataPos);
			int temp = this->datSize - this->dataPos;
			this->dataPos += this->datSize - this->dataPos;
			sizeDownloaded += temp;
			return temp;
		}
	}
	this->mdata.clear();
	this->res = this->ndnicpdownload->download(this->m_name, &(this->mdata), this->firstChunk, -1);
	this->datSize = this->mdata.size();
	this->deezData = (char *)malloc(this->datSize);
	int i = 0;
	for(std::vector<char>::const_iterator it = this->mdata.begin(); it != this->mdata.end(); it++)
	{
		deezData[i] = *it;
		i++;
	}
	if(this->datSize > len)
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

double NDNConnection::GetAverageDownloadingSpeed()
{
	Debug("NDNConnection:	DL speed is %f\n", this->speed);
	return this->speed;
}

double NDNConnection::GetDownloadingTime()
{
	L("NDNConnection:	DL time is %f\n", this->dnltime);
	return this->dnltime;
}

const std::vector<ITCPConnection *> &NDNConnection::GetTCPConnectionList() const {
    return tcpConnections;
}

const std::vector<IHTTPTransaction *> &NDNConnection::GetHTTPTransactionList() const {
    return httpTransactions;
}

void NDNConnection::notifyStats(double winSize, double RTT)
{
	this->speed = (winSize); // * 1000000 * 1400 * 8;
	L("NDNConnection:\tNotificationICPDL\t%f\t%f\t%f\n", winSize, RTT, speed);
}

void NDNConnection::notifyChunkStats(std::string chunkName, double chunkThroughput)
{
	L("NDNConnection:\tChunk-Download\t Chunk-Name: %s\t single Chunk-Throughput: %f\n", chunkName.c_str(), chunkThroughput);
}

#endif
