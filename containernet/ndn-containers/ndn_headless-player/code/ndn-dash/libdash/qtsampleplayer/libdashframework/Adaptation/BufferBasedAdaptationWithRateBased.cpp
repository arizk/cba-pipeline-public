/*
 * BufferBasedAdaptationWithRateBased.cpp
 *****************************************************************************
 * Copyright (C) 2016
 * Jacques Samain <jsamain@cisco.com>
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "BufferBasedAdaptationWithRateBased.h"
#include<stdio.h>


using namespace dash::mpd;
using namespace libdash::framework::adaptation;
using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

BufferBasedAdaptationWithRateBased::BufferBasedAdaptationWithRateBased          (IMPD *mpd, IPeriod *period, IAdaptationSet *adaptationSet, bool isVid, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6) :
                  AbstractAdaptationLogic   (mpd, period, adaptationSet, isVid)
{
	this->slackParam = 0.8;
	std::vector<IRepresentation* > representations = this->adaptationSet->GetRepresentation();
	if(arg1 >= 0 && arg1 <= 1)
	{
		this->alphaRate = arg1;
	}
	else
		this->alphaRate = 0.8;
	if(arg2)
	{
		if(arg2 <= 100)
			this->reservoirThreshold = arg2;
		else
			this->reservoirThreshold = 25;
	}
	if(arg3)
	{
		if(arg3 <= 100 && arg3)
			this->maxThreshold = arg3;
		else
			this->maxThreshold = 100;
	}
	this->m_count = 0;
	//this->switchUpThreshold = 15;
	this->switchUpThreshold = 5;
	this->instantBw = 0;
	this->averageBw = 0;
	this->representation = this->adaptationSet->GetRepresentation().at(0);
	this->multimediaManager = NULL;
	this->lastBufferFill = 0;
	this->bufferEOS = false;
	this->shouldAbort = false;
	this->isCheckedForReceiver = false;
	L("BufferRateBasedParams:\talpha:%f\t%f\t%f\n",this->alphaRate, (double)reservoirThreshold/100, (double)maxThreshold/100);
	Debug("Buffer Adaptation:	STARTED\n");
}
BufferBasedAdaptationWithRateBased::~BufferBasedAdaptationWithRateBased         ()
{
}

LogicType       BufferBasedAdaptationWithRateBased::GetType               ()
{
    return adaptation::BufferBased;
}

bool			BufferBasedAdaptationWithRateBased::IsUserDependent		()
{
	return false;
}

bool			BufferBasedAdaptationWithRateBased::IsRateBased		()
{
	return true;
}
bool			BufferBasedAdaptationWithRateBased::IsBufferBased		()
{
	return true;
}

void			BufferBasedAdaptationWithRateBased::SetMultimediaManager (sampleplayer::managers::IMultimediaManagerBase *_mmManager)
{
	this->multimediaManager = _mmManager;
}

void			BufferBasedAdaptationWithRateBased::NotifyBitrateChange	()
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

uint64_t		BufferBasedAdaptationWithRateBased::GetBitrate				()
{
	return this->currentBitrate;
}

void 			BufferBasedAdaptationWithRateBased::SetBitrate				(uint32_t bufferFill)
{
	uint32_t phi1, phi2;
	std::vector<IRepresentation *> representations;
	representations = this->adaptationSet->GetRepresentation();
	size_t i = 0;

//	if(this->isCheckedForReceiver)
//	{
//		return;
//	}
//	this->isCheckedForReceiver = true;

	phi1 = 0;
	phi2 = 0;
	while(i < representations.size())
	{
		if(phi1 == 0 && representations.at(i)->GetBandwidth() > slackParam * this->instantBw)
		{
			phi1 = representations.at((i == 0) ? i : i -1)->GetBandwidth();
		}
		if(phi2 == 0 && representations.at(i)->GetBandwidth() > slackParam * this->averageBw)
		{
			phi2 = representations.at((i == 0) ? i : i -1)->GetBandwidth();
		}
		i++;
	}

	if(!phi1)
		phi1 = representations.at(representations.size() - 1)->GetBandwidth();

	if(!phi2)
		phi2 = representations.at(representations.size() - 1)->GetBandwidth();

	if(bufferFill < this->reservoirThreshold)
	{
		this->m_count = 0;
		this->myQuality = 0;
	}
	else
	{
		if(bufferFill < this->maxThreshold)
		{
			this->m_count = 0;
			if(this->currentBitrate > phi1)
			{
				if(this->myQuality > 0)
				{
					this->myQuality--;
				}
			}
			else
			{
				if(this->currentBitrate < phi1)
				{
					if(this->myQuality < representations.size() - 1)
					{
						this->myQuality++;
					}
				}
			}
		}
		else
		{ // bufferFill > this->maxThreshold
			if(this->currentBitrate < phi2)
			{
				m_count++;

				if(m_count >= switchUpThreshold && this->myQuality < representations.size() - 1)
				{
					this->m_count = 0;
					this->myQuality++;
				}
			}
		}
	}
	this->representation = representations.at(this->myQuality);
	this->currentBitrate = (uint64_t) this->representation->GetBandwidth();
	L("ADAPTATION_LOGIC:\tFor %s:\tlast_buffer: %f\tbuffer_level: %f, instantaneousBw: %lu, AverageBW: %lu, choice: %d\n",isVideo ? "video" : "audio",(double)lastBufferFill/100 , (double)bufferFill/100, this->instantBw, this->averageBw , this->myQuality);
}

void			BufferBasedAdaptationWithRateBased::BitrateUpdate		(uint64_t bps)
{
	this->instantBw = bps;
	if(this->averageBw == 0)
	{
		this->averageBw = bps;
	}
	else
	{
		this->averageBw = this->alphaRate*this->averageBw + (1 - this->alphaRate)*bps;
	}
}

void			BufferBasedAdaptationWithRateBased::SegmentPositionUpdate			(uint32_t qualitybps)
{
}

void 			BufferBasedAdaptationWithRateBased::DLTimeUpdate		(double time)
{
}

void			BufferBasedAdaptationWithRateBased::OnEOS				(bool value)
{
	this->bufferEOS = value;
}
void            BufferBasedAdaptationWithRateBased::OnSegmentDownloaded (double bufferFillPct, int segNum, int quality, std::vector<int> rttVec, std::vector<int> numHopsVec)
{
}

void 			BufferBasedAdaptationWithRateBased::CheckedByDASHReceiver	()
{
	this->isCheckedForReceiver = false;
}
void			BufferBasedAdaptationWithRateBased::BufferUpdate			(uint32_t bufferFill)
{
	this->SetBitrate(bufferFill);
	this->NotifyBitrateChange();
}
