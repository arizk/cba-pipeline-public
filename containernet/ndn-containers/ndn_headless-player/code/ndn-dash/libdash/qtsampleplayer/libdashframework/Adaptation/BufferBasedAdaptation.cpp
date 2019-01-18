/*
 * BufferBasedAdaptation.cpp
 *****************************************************************************
 * Copyright (C) 2016
 * Jacques Samain <jsamain@cisco.com>
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "BufferBasedAdaptation.h"
#include<stdio.h>


using namespace dash::mpd;
using namespace libdash::framework::adaptation;
using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

BufferBasedAdaptation::BufferBasedAdaptation          (IMPD *mpd, IPeriod *period, IAdaptationSet *adaptationSet, bool isVid, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6) :
                  AbstractAdaptationLogic   (mpd, period, adaptationSet, isVid)
{
	std::vector<IRepresentation* > representations = this->adaptationSet->GetRepresentation();
	if(arg1)
	{
		if(arg1 != 0 && arg1 >= 100)
			this->reservoirThreshold = arg1;
		else
			this->reservoirThreshold = 25;
	}
	else
		this->reservoirThreshold = 25;

	if(arg2)
	{
		if(arg2 <=100 && arg2 >= arg1)
			this->maxThreshold = arg2;
		else
			this->maxThreshold = 75;
	}
	else
		this->maxThreshold = 75;

	this->representation = this->adaptationSet->GetRepresentation().at(0);
	this->multimediaManager = NULL;
	this->lastBufferFill = 0;
	this->bufferEOS = false;
	this->shouldAbort = false;
	L("BufferBasedParams:\t%f\t%f\n", (double)reservoirThreshold/100, (double)maxThreshold/100);
	Debug("Buffer Adaptation:	STARTED\n");
}
BufferBasedAdaptation::~BufferBasedAdaptation         ()
{
}

LogicType       BufferBasedAdaptation::GetType               ()
{
    return adaptation::BufferBased;
}

bool			BufferBasedAdaptation::IsUserDependent		()
{
	return false;
}

bool			BufferBasedAdaptation::IsRateBased		()
{
	return false;
}
bool			BufferBasedAdaptation::IsBufferBased		()
{
	return true;
}

void			BufferBasedAdaptation::SetMultimediaManager (sampleplayer::managers::IMultimediaManagerBase *_mmManager)
{
	this->multimediaManager = _mmManager;
}

void			BufferBasedAdaptation::NotifyBitrateChange	()
{
	if(this->multimediaManager)
		if(this->multimediaManager->IsStarted() && !this->multimediaManager->IsStopping())
			if(this->isVideo)
				this->multimediaManager->SetVideoQuality(this->period, this->adaptationSet, this->representation);
			else
				this->multimediaManager->SetAudioQuality(this->period, this->adaptationSet, this->representation);
	//Should Abort is done here to avoid race condition with DASHReceiver::DoBuffering()
	if(this->shouldAbort)
	{
		//printf("Sending Abort request\n");
		this->multimediaManager->ShouldAbort(this->isVideo);
		//printf("Received Abort request\n");
	}
	this->shouldAbort = false;
}

uint64_t		BufferBasedAdaptation::GetBitrate				()
{
	return this->currentBitrate;
}

void 			BufferBasedAdaptation::SetBitrate				(uint32_t bufferFill)
{
	std::vector<IRepresentation *> representations;
	representations = this->adaptationSet->GetRepresentation();
	size_t i = 0;

	if(representations.size() == 1)
	{
		i = 0;
	}
	else
	{
		while(bufferFill > this->reservoirThreshold + i * (this->maxThreshold - this->reservoirThreshold)/(representations.size()-1))
		{
			i++;
		}
	}
	if((size_t)i >= (size_t)(representations.size()))
		i = representations.size() - 1;
	this->representation = representations.at(i);
	if( 0 && !this->bufferEOS && this->lastBufferFill > this->reservoirThreshold && bufferFill <= this->reservoirThreshold)
	{
		this->shouldAbort = true;
	}
	L("ADAPTATION_LOGIC:\tFor %s:\tlast_buffer: %f\tbuffer_level: %f, choice: %lu, should_trigger_abort: %s\n",isVideo ? "video" : "audio",(double)lastBufferFill/100 , (double)bufferFill/100, i, this->shouldAbort ? "YES" : "NO");
	this->lastBufferFill = bufferFill;

}

void			BufferBasedAdaptation::BitrateUpdate		(uint64_t bps)
{
}

void			BufferBasedAdaptation::SegmentPositionUpdate			(uint32_t qualitybps)
{
}

void 			BufferBasedAdaptation::DLTimeUpdate		(double time)
{
}

void			BufferBasedAdaptation::OnEOS				(bool value)
{
	this->bufferEOS = value;
}
void            BufferBasedAdaptation::OnSegmentDownloaded (double bufferFillPct, int segNum, int quality, std::vector<int> rttVec, std::vector<int> numHopsVec)
{
}
void			BufferBasedAdaptation::CheckedByDASHReceiver()
{
}

void			BufferBasedAdaptation::BufferUpdate			(uint32_t bufferFill)
{
	this->SetBitrate(bufferFill);
	this->NotifyBitrateChange();
}
