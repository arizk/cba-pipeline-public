/*
 * DASHReceiver.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "DASHReceiver.h"
#include <stdio.h>
#include <boost/bind.hpp>
#include <iostream>

using namespace libdash::framework::input;
using namespace libdash::framework::buffer;
using namespace libdash::framework::mpd;
using namespace dash::mpd;

using duration_in_seconds = std::chrono::duration<double, std::ratio<1, 1> >;

DASHReceiver::DASHReceiver          (IMPD *mpd, IDASHReceiverObserver *obs, MediaObjectBuffer *buffer, uint32_t bufferSize, bool ndnEnabled, double ndnAlpha, bool chunkGranularityEnabled, int sample) :
              mpd                   (mpd),
              period                (NULL),
              adaptationSet         (NULL),
              representation        (NULL),
              adaptationSetStream   (NULL),
              representationStream  (NULL),
              segmentNumber         (0),
              observer              (obs),
              buffer                (buffer),
              bufferSize            (bufferSize),
              isBuffering           (false),
			  withFeedBack			(false),
			  isNDN					(ndnEnabled),
			  ndnAlpha				(ndnAlpha),
			  previousQuality		(0),
			  isPaused				(false),
			  threadComplete		(false),
			  isScheduledPaced		(false),
			  targetDownload		(0.0),
              downloadingTime		(0.0),
              chunkGranularity      (chunkGranularityEnabled),
              samplesize            (sample)
{
    this->period                = this->mpd->GetPeriods().at(0);
    this->adaptationSet         = this->period->GetAdaptationSets().at(0);
    this->representation        = this->adaptationSet->GetRepresentation().at(0);

    this->adaptationSetStream   = new AdaptationSetStream(mpd, period, adaptationSet);
    this->representationStream  = adaptationSetStream->GetRepresentationStream(this->representation);
    this->segmentOffset         = CalculateSegmentOffset();

    this->conn = NULL;
    this->initConn = NULL;
    if(isNDN)
    {
#ifdef NDNICPDOWNLOAD
        this->chunktimeObs = new ChunktimeObserver(this->samplesize);
        this->chunktimeObs->addThroughputSignal(boost::bind(&DASHReceiver::NotifybpsChunk, this, _1));  //pass the function to register as a Slot at ChunktimeObserver-Signal
        this->chunktimeObs->addContextSignal(boost::bind(&DASHReceiver::NotifyContextChunk, this, _1, _2));
    	this->conn = new NDNConnection(this->ndnAlpha, this->chunktimeObs);
        this->initConn = new NDNConnection(this->ndnAlpha, this->chunktimeObs);
#else
    	this->conn = new NDNConnectionConsumerApi(this->ndnAlpha);
    	this->initConn = new NDNConnectionConsumerApi(this->ndnAlpha);
#endif
    }
    InitializeCriticalSection(&this->monitorMutex);
    InitializeCriticalSection(&this->monitorPausedMutex);
    InitializeConditionVariable(&this->paused);
}
DASHReceiver::~DASHReceiver		()
{
    delete this->adaptationSetStream;
    DeleteCriticalSection(&this->monitorMutex);
    DeleteCriticalSection(&this->monitorPausedMutex);
    DeleteConditionVariable(&this->paused);
}

void			DASHReceiver::SetAdaptationLogic(adaptation::IAdaptationLogic *_adaptationLogic)
{
	this->adaptationLogic = _adaptationLogic;
	this->withFeedBack = this->adaptationLogic->IsRateBased();
}
bool			DASHReceiver::Start						()
{
    if(this->isBuffering)
        return false;

    this->isBuffering       = true;
    this->bufferingThread   = CreateThreadPortable (DoBuffering, this);

    if(this->bufferingThread == NULL)
    {
        this->isBuffering = false;
        return false;
    }

    return true;
}
void			DASHReceiver::Stop						()
{
    if(!this->isBuffering)
        return;

    this->isBuffering = false;
    this->buffer->SetEOS(true);

    if(this->bufferingThread != NULL)
    {
        JoinThread(this->bufferingThread);
        DestroyThreadPortable(this->bufferingThread);
    }
}
MediaObject*	DASHReceiver::GetNextSegment	()
{
    ISegment *seg = NULL;

    EnterCriticalSection(&this->monitorPausedMutex);
    	while(this->isPaused)
    		SleepConditionVariableCS(&this->paused, &this->monitorPausedMutex, INFINITE);

    if(this->segmentNumber >= this->representationStream->GetSize())
    {
    	LeaveCriticalSection(&this->monitorPausedMutex);
    	return NULL;
    }
    seg = this->representationStream->GetMediaSegment(this->segmentNumber + this->segmentOffset);

    if (seg != NULL)
    {
    	std::vector<IRepresentation *> rep = this->adaptationSet->GetRepresentation();
    	int quality = 0;
    	while(quality != rep.size() - 1)
    	{
    		if(rep.at(quality) == this->representation)
    			break;
    		quality++;
    	}
        Debug("DASH receiver: Next segment is: %s / %s\n",((dash::network::IChunk*)seg)->Host().c_str(),((dash::network::IChunk*)seg)->Path().c_str());
        int realBPS = rep.at(quality)->GetSpecificSegmentSize(std::to_string(segmentNumber+1));
    	L("DASH_Receiver:\t%s\t%d\t%u\tRealBPS: %u\n", ((dash::network::IChunk*)seg)->Path().c_str() ,quality, rep.at(quality)->GetBandwidth(), realBPS);

    	if(quality != previousQuality)
    	{
    		L("DASH_Receiver:\tQUALITY_SWITCH\t%s\t%d\t%d\n", (quality > previousQuality) ?"UP" : "DOWN", previousQuality, quality);
    		previousQuality = quality;
    	}
        //TODO: Somewhere after this point there is an delay 
        MediaObject *media = new MediaObject(seg, this->representation,this->withFeedBack);
        this->segmentNumber++;
        this->adaptationLogic->SegmentPositionUpdate(rep.at(quality)->GetBandwidth());
       	LeaveCriticalSection(&this->monitorPausedMutex);
        return media;
    }
   	LeaveCriticalSection(&this->monitorPausedMutex);
    return NULL;
}
MediaObject*	DASHReceiver::GetSegment		(uint32_t segNum)
{
    ISegment *seg = NULL;

    if(segNum >= this->representationStream->GetSize())
        return NULL;

    seg = this->representationStream->GetMediaSegment(segNum + segmentOffset);

    if (seg != NULL)
    {
        MediaObject *media = new MediaObject(seg, this->representation);
        return media;
    }

    return NULL;
}
MediaObject*	DASHReceiver::GetInitSegment	()
{
    ISegment *seg = NULL;

    seg = this->representationStream->GetInitializationSegment();

    if (seg != NULL)
    {
        MediaObject *media = new MediaObject(seg, this->representation);
        return media;
    }

    return NULL;
}
MediaObject*	DASHReceiver::FindInitSegment	(dash::mpd::IRepresentation *representation)
{
    if (!this->InitSegmentExists(representation))
        return NULL;

    return this->initSegments[representation];
}
uint32_t                    DASHReceiver::GetPosition               ()
{
    return this->segmentNumber;
}
void                        DASHReceiver::SetPosition               (uint32_t segmentNumber)
{
    // some logic here

    this->segmentNumber = segmentNumber;
}
void                        DASHReceiver::SetPositionInMsecs        (uint32_t milliSecs)
{
    // some logic here

    this->positionInMsecs = milliSecs;
}
void                        DASHReceiver::SetRepresentation         (IPeriod *period, IAdaptationSet *adaptationSet, IRepresentation *representation)
{
    EnterCriticalSection(&this->monitorMutex);

    bool periodChanged = false;

    if (this->representation == representation)
    {
        LeaveCriticalSection(&this->monitorMutex);
        return;
    }

    this->representation = representation;

    if (this->adaptationSet != adaptationSet)
    {
        this->adaptationSet = adaptationSet;

        if (this->period != period)
        {
            this->period = period;
            periodChanged = true;
        }

        delete this->adaptationSetStream;
        this->adaptationSetStream = NULL;

        this->adaptationSetStream = new AdaptationSetStream(this->mpd, this->period, this->adaptationSet);
    }

    this->representationStream  = this->adaptationSetStream->GetRepresentationStream(this->representation);
    this->DownloadInitSegment(this->representation);

    if (periodChanged)
    {
        this->segmentNumber = 0;
        this->CalculateSegmentOffset();
    }

    LeaveCriticalSection(&this->monitorMutex);
}

libdash::framework::adaptation::IAdaptationLogic* DASHReceiver::GetAdaptationLogic	()
{
	return this->adaptationLogic;
}
dash::mpd::IRepresentation* DASHReceiver::GetRepresentation         ()
{
    return this->representation;
}
uint32_t                    DASHReceiver::CalculateSegmentOffset    ()
{
    if (mpd->GetType() == "static")
        return 0;

    uint32_t firstSegNum = this->representationStream->GetFirstSegmentNumber();
    uint32_t currSegNum  = this->representationStream->GetCurrentSegmentNumber();
    uint32_t startSegNum = currSegNum - 2*bufferSize;

    return (startSegNum > firstSegNum) ? startSegNum : firstSegNum;
}
void                        DASHReceiver::NotifySegmentDownloaded   (double bufferFillPct, int segNum, int quality)
{
    this->observer->OnSegmentDownloaded();
	std::vector<int> rttVec = this->rttMap[quality];
    std::vector<int> numHopsVec = this->numHopsMap[quality];
    this->adaptationLogic->OnSegmentDownloaded(bufferFillPct, segNum, quality, rttVec, numHopsVec);
    this->rttMap[quality].clear();
    this->numHopsMap[quality].clear();
}
void						DASHReceiver::NotifyBitrateChange(dash::mpd::IRepresentation *representation)
{
	if(this->representation != representation)
	{
		this->representation = representation;
		this->SetRepresentation(this->period,this->adaptationSet,this->representation);
	}
}
void                        DASHReceiver::DownloadInitSegment    (IRepresentation* rep)
{
    if (this->InitSegmentExists(rep))
        return;

    MediaObject *initSeg = NULL;
    initSeg = this->GetInitSegment();

    if (initSeg)
    {
        initSeg->StartDownload(this->initConn);
        this->initSegments[rep] = initSeg;
        initSeg->WaitFinished(true);
    }
}
bool                        DASHReceiver::InitSegmentExists      (IRepresentation* rep)
{
    if (this->initSegments.find(rep) != this->initSegments.end())
        return true;

    return false;
}

void					DASHReceiver::Notifybps					(uint64_t bps)
{
	if(this)
	{
		if(this->adaptationLogic)
		{
			if(this->withFeedBack)
			{
				this->adaptationLogic->BitrateUpdate(bps);
			}
		}
	}
}
void                    DASHReceiver::NotifybpsSegment        (uint64_t bps)    //segment-based feedback enabled
{
    if(!chunkGranularity)
        this->Notifybps(bps);
}
void                    DASHReceiver::NotifybpsChunk          (uint64_t bps)    //chunk-based feedback enabled
{
    if(chunkGranularity)
        this->Notifybps(bps);
}
void					DASHReceiver::NotifyContextChunk	  (uint64_t rtt, uint64_t numHops)
{
	int rttSeconds = (int)(rtt / (uint64_t)1000); // actually milliseconds?
    std::vector<IRepresentation *> rep = this->adaptationSet->GetRepresentation();
    int quality = 0;
    while(quality != rep.size() - 1)
    {
        if(rep.at(quality) == this->representation)
            break;
        quality++;
    }
    this->rttMap[quality].push_back(rttSeconds);
    this->numHopsMap[quality].push_back((int)numHops);
}

void					DASHReceiver::NotifyDlTime				(double time)
{
	if(this)
	{
		if(this->adaptationLogic)
		{
			if(this->withFeedBack)
			{
                //std::cout << "DASHReceiver::NotifyDlTime call " << time << "seconds \n";
				this->adaptationLogic->DLTimeUpdate(time);
			}
		}
	}
}
void					DASHReceiver::NotifyCheckedAdaptationLogic()
{
	this->adaptationLogic->CheckedByDASHReceiver();
}
//Is only called when this->adaptationLogic->IsBufferBased
void 					DASHReceiver::OnSegmentBufferStateChanged(uint32_t fillstateInPercent)
{
	this->adaptationLogic->BufferUpdate(fillstateInPercent);
}
void					DASHReceiver::OnEOS(bool value)
{
	this->adaptationLogic->OnEOS(value);
}
/* Thread that does the buffering of segments */
void*                       DASHReceiver::DoBuffering               (void *receiver)
{
    DASHReceiver *dashReceiver = (DASHReceiver *) receiver;

    dashReceiver->DownloadInitSegment(dashReceiver->GetRepresentation());

    MediaObject *media = dashReceiver->GetNextSegment();
    dashReceiver->NotifyCheckedAdaptationLogic();
    media->SetDASHReceiver(dashReceiver);
    std::chrono::time_point<std::chrono::system_clock> m_start_time = std::chrono::system_clock::now();
    while(media != NULL && dashReceiver->isBuffering)
    {
    	if(dashReceiver->isScheduledPaced)
    	{
    		double delay = std::chrono::duration_cast<duration_in_seconds>(std::chrono::system_clock::now() - m_start_time).count();
            L("-DASH-Reciever: Waiting for next Download in DoBufferung %f, targetdownload: %f, delay: %f\n", dashReceiver->targetDownload - delay, dashReceiver->targetDownload, delay);
            //Only wait when buffer if more than 80 percent full
    		if(delay < dashReceiver->targetDownload && ((double) dashReceiver->buffer->Length()/(double) dashReceiver->buffer->Capacity()) > 0.9)
    		{
    			sleep(dashReceiver->targetDownload - delay);
    		}
    	}
    	m_start_time = std::chrono::system_clock::now();
        media->StartDownload(dashReceiver->conn);
        media->WaitFinished(false);
        if (!dashReceiver->buffer->PushBack(media))
        {
        	dashReceiver->threadComplete = true;
        	return NULL;
        }
		
		// Get the quality so we can notify the adaptation logic
    	std::vector<IRepresentation *> rep = dashReceiver->adaptationSet->GetRepresentation();
    	int quality = 0;
    	while(quality != rep.size() - 1)
    	{
    		if(rep.at(quality) == dashReceiver->representation)
    			break;
    		quality++;
    	}

        double bufferFillPct = (double) dashReceiver->buffer->Length()/(double) dashReceiver->buffer->Capacity();
        dashReceiver->NotifySegmentDownloaded(bufferFillPct, dashReceiver->segmentNumber, quality);

        media = dashReceiver->GetNextSegment();
        dashReceiver->NotifyCheckedAdaptationLogic();
        if(media)
        	media->SetDASHReceiver(dashReceiver);
    }

    dashReceiver->buffer->SetEOS(true);
    dashReceiver->threadComplete = true;
    return NULL;
}

void					DASHReceiver::ShouldAbort				()
{
	Debug("DASH RECEIVER SEGMENT --\n");
	this->segmentNumber--;
	Debug("DASH RECEIVER ABORT REQUEST\n");
	this->buffer->ShouldAbort();
}
void					DASHReceiver::Seeking					(int offset)
{
	EnterCriticalSection(&this->monitorPausedMutex);
	this->isPaused = true;
	this->ShouldAbort();
	this->buffer->Clear();
	this->segmentNumber = this->segmentNumber + offset;
	if((int)this->segmentNumber < 0)
		this->segmentNumber = 0;
	this->isPaused = false;
	WakeAllConditionVariable(&this->paused);
	LeaveCriticalSection(&this->monitorPausedMutex);
}

void					DASHReceiver::FastForward				()
{
	//Seeking 30s in the future (15 segments = 2 x 15 seconds)
	this->Seeking(15);
}

void					DASHReceiver::FastRewind				()
{
	this->Seeking(-15);
    if(this->threadComplete)
    {
    	this->buffer->SetEOS(false);
        this->isBuffering       = true;
        this->bufferingThread   = CreateThreadPortable (DoBuffering, this);
        this->threadComplete = false;
    }
}

void					DASHReceiver::SetTargetDownloadingTime	(double target)
{
	this->isScheduledPaced = true;
	this->targetDownload = target;
}
