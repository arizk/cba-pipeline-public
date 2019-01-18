/*
 * BufferBasedAdaptationWithRateBased.cpp
 *****************************************************************************
 * Copyright (C) 2016
 * Jacques Samain <jsamain@cisco.com>
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "BufferBasedThreeThresholdAdaptation.h"
#include<stdio.h>


using namespace dash::mpd;
using namespace libdash::framework::adaptation;
using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

BufferBasedThreeThresholdAdaptation::BufferBasedThreeThresholdAdaptation          (IMPD *mpd, IPeriod *period, IAdaptationSet *adaptationSet, bool isVid, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6) :
                  AbstractAdaptationLogic   (mpd, period, adaptationSet, isVid)
{
	this->slackParam = 0.8;
	std::vector<IRepresentation* > representations = this->adaptationSet->GetRepresentation();
	if(arg1 >= 0 && arg1 <= 100)
	{
		this->firstThreshold = arg1;
	}
	else
		this->firstThreshold = 25;
	if(arg2)
	{
		if(arg2 <= 100)
			this->secondThreshold = arg2;
		else
			this->secondThreshold = 50;
	}
	if(arg3)
	{
		if(arg3 <= 100 && arg3)
			this->thirdThreshold = arg3;
		else
			this->thirdThreshold = 75;
	}
	this->representation = this->adaptationSet->GetRepresentation().at(0);
	this->multimediaManager = NULL;
	this->lastBufferFill = 0;
	this->bufferEOS = false;
	this->shouldAbort = false;
	this->isCheckedForReceiver = false;
	L("BufferRateBasedParams:\t%f\t%f\t%f\n",(double)this->firstThreshold/100, (double)secondThreshold/100, (double)thirdThreshold/100);
	Debug("Buffer Adaptation:	STARTED\n");
}
BufferBasedThreeThresholdAdaptation::~BufferBasedThreeThresholdAdaptation         ()
{
}

LogicType       BufferBasedThreeThresholdAdaptation::GetType               ()
{
    return adaptation::BufferBasedThreeThreshold;
}

bool			BufferBasedThreeThresholdAdaptation::IsUserDependent		()
{
	return false;
}

bool			BufferBasedThreeThresholdAdaptation::IsRateBased		()
{
	return true;
}
bool			BufferBasedThreeThresholdAdaptation::IsBufferBased		()
{
	return true;
}

void			BufferBasedThreeThresholdAdaptation::SetMultimediaManager (sampleplayer::managers::IMultimediaManagerBase *_mmManager)
{
	this->multimediaManager = _mmManager;
}

void			BufferBasedThreeThresholdAdaptation::NotifyBitrateChange	()
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

uint64_t		BufferBasedThreeThresholdAdaptation::GetBitrate				()
{
	return this->currentBitrate;
}

void 			BufferBasedThreeThresholdAdaptation::SetBitrate				(uint32_t bufferFill)
{
	uint32_t phi1, phi2;
	std::vector<IRepresentation *> representations;
	representations = this->adaptationSet->GetRepresentation();
	size_t i = 0;

	if(this->isCheckedForReceiver)
	{
		return;
	}
	this->isCheckedForReceiver = true;

//	phi1 = 0;

//	while(i < representations.size())
//	{
//		if(phi1 == 0 && representations.at(i)->GetBandwidth() > slackParam * this->instantBw)
//		{
//			phi1 = representations.at((i == 0) ? i : i -1)->GetBandwidth();
//		}
//		i++;
//	}

//	if(!phi1)
//		phi1 = representations.at(representations.size() - 1)->GetBandwidth();


	if(bufferFill < this->firstThreshold)
	{
		this->myQuality = 0;
	}
	else
	{
		if(bufferFill < this->secondThreshold)
		{
			if(this->currentBitrate >= this->instantBw)
			{
				if(this->myQuality > 0)
				{
					this->myQuality--;
				}
			}
		}
		else
		{
			if(bufferFill < this->thirdThreshold)
			{
			}
			else
			{// bufferLevel > thirdThreshold
				if(this->currentBitrate <= this->instantBw)
				{
					if(this->myQuality < representations.size() - 1)
						this->myQuality++;
				}
			}
		}
	}
	this->representation = representations.at(this->myQuality);
	this->currentBitrate = (uint64_t) this->representation->GetBandwidth();
	L("ADAPTATION_LOGIC:\tFor %s:\tlast_buffer: %f\tbuffer_level: %f, instantaneousBw: %lu, choice: %d\n",isVideo ? "video" : "audio",(double)lastBufferFill/100 , (double)bufferFill/100, this->instantBw, this->myQuality);
}

void			BufferBasedThreeThresholdAdaptation::BitrateUpdate		(uint64_t bps)
{
	this->instantBw = bps;
}

void			BufferBasedThreeThresholdAdaptation::SegmentPositionUpdate			(uint32_t qualitybps)
{
}

void 			BufferBasedThreeThresholdAdaptation::DLTimeUpdate		(double time)
{
}

void			BufferBasedThreeThresholdAdaptation::OnEOS				(bool value)
{
	this->bufferEOS = value;
}
void            BufferBasedThreeThresholdAdaptation::OnSegmentDownloaded (double bufferFillPct, int segNum, int quality, std::vector<int> rttVec, std::vector<int> numHopsVec)
{
}

void 			BufferBasedThreeThresholdAdaptation::CheckedByDASHReceiver	()
{
	this->isCheckedForReceiver = false;
}
void			BufferBasedThreeThresholdAdaptation::BufferUpdate			(uint32_t bufferFill)
{
	this->SetBitrate(bufferFill);
	this->NotifyBitrateChange();
}
