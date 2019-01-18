/*
 * Panda.cpp
 *****************************************************************************
 * Based on paper "Probe and Adapt: Adaptation for HTTP Video Streaming At Scale", Zhi Li et al.
 *
 * Copyright (C) 2016,
 * Jacques Samain <jsamain@cisco.com>
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "Panda.h"
#include <stdio.h>
#include <iostream>


using namespace dash::mpd;
using namespace libdash::framework::adaptation;
using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

PandaAdaptation::PandaAdaptation(IMPD *mpd, IPeriod *period, IAdaptationSet *adaptationSet, bool isVid, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6) :
                          AbstractAdaptationLogic   (mpd, period, adaptationSet, isVid)
{
//	this->param_Alpha = ((arg1 < 1) ? ((arg1 >= 0) ? arg1 : 0.8) : 0.8);
	this->param_Alpha = 0.2;
	this->param_Beta = 0.2;
	this->param_K = 0.14;
	this->param_W = 300000;
	this->param_Epsilon = 0.15;				//safety threshhold (changed from 0.15 to 0.2(5)) without MPD extension

	//Extended MPD additions + parameter
	this->segPos = 0;
	this->historylength = 10;				//parameter for the Instability over the last x segments
	this->param_instability = 0.03;			//instability threshold	(turn off by value = 1, good value around 0.05)
	this->localSegVariance = 3;				//parameter on how many segments into the future have to be feasible by the current bw (turn off by value = 0)

    // Set QoE weights
    this->qoe_w1 = arg4;
    this->qoe_w2 = arg5;
    this->qoe_w3 = arg6;

	this->targetBw = 0;
	this->targetInterTime = 0.0;

	this->averageBw = 0;
	this->smoothBw = 0;
	this->instantBw = 0;
	this->targetBw = 0;

	this->targetInterTime = 0.0;
	this->interTime = 0.0;

	this->segmentDuration = 2.0;
	this->bufferMaxSizeSeconds = 20.0;		//changed from 60s to 20s	(HAS TO BE CHANGED IN MultimediaManager.cpp !)

	if(arg1) {
		if(arg1 >= 0 && arg1 <= 1) {
			this->alpha_ewma = arg1;
		}
		else
			this->alpha_ewma = 0.8;
	}
	else {
		Debug("EWMA Alpha parameter NOT SPECIFIED!\n");
	}

	/// Set 'param_Bmin'
	if(arg2) {
		if(arg2 > 0 && arg2 < (int)bufferMaxSizeSeconds) {
			this->param_Bmin = (double)arg2;
		}
		else
			this->param_Bmin = 44;
	}
	else {
		Debug("bufferTarget NOT SPECIFIED!\n");
	}

	this->bufferLevel = 0;
	this->bufferLevelSeconds = 0.0;
	//this->bufferMaxSizeSeconds = 60.0;

    this->rebufferStart = 0;
    this->rebufferEnd = 0;

	this->downloadTime = 0.0;

	this->isVideo = isVid;
	this->mpd = mpd;
	this->adaptationSet = adaptationSet;
	this->period = period;
	this->multimediaManager = NULL;
	this->representation = NULL;
	this->currentBitrate = 0;
	this->current = 0;

	// Retrieve the available bitrates
	std::vector<IRepresentation* > representations = this->adaptationSet->GetRepresentation();

	this->extMPDuse = representations.at(0)->IsExtended();
	L("Extended MPD file used: %s\n", extMPDuse? "yes": "no");

	this->availableBitrates.clear();
	L("PANDA Available Bitrates...\n");
	for(size_t i = 0; i < representations.size(); i++)
	{
		this->availableBitrates.push_back((uint64_t)(representations.at(i)->GetBandwidth()));
		L("%d  -  %I64u bps\n", i, this->availableBitrates[i]);

		if(false)
		{
			for(int j=1; j <= representations.at(i)->GetSegmentSizes().size(); j++)
			{
				if(representations.at(i)->GetSegmentSizes().count(std::to_string(j)) == 1)
					L("\tSegment No. %d  -  %d bits per second\n", j, representations.at(i)->GetSpecificSegmentSize(std::to_string(j)));
				else
					L("Failed to load size from Segment %d", j);
			}
		}
	}

	this->representation = this->adaptationSet->GetRepresentation().at(0);
	this->currentBitrate = (uint64_t) this->representation->GetBandwidth();
	this->currentBitrateVirtual = (uint64_t) this->representation->GetBandwidth();

	L("Panda parameters: K= %f, Bmin = %f, alpha = %f, beta = %f, W = %f\n", param_K, param_Bmin, param_Alpha, param_Beta, param_W);
}

PandaAdaptation::~PandaAdaptation() {
}

LogicType		PandaAdaptation::GetType             ()
{
	return adaptation::Panda;
}

bool			PandaAdaptation::IsUserDependent		()
{
   	return false;
}

bool			PandaAdaptation::IsRateBased			()
{
   	return true;
}

bool			PandaAdaptation::IsBufferBased		()
{
  	return true;
}

void			PandaAdaptation::SetMultimediaManager (sampleplayer::managers::IMultimediaManagerBase *_mmManager)
{
   	this->multimediaManager = _mmManager;
}

void			PandaAdaptation::NotifyBitrateChange	()
{
	if(this->multimediaManager->IsStarted() && !this->multimediaManager->IsStopping())
	if(this->isVideo)
		this->multimediaManager->SetVideoQuality(this->period, this->adaptationSet, this->representation);
	else
		this->multimediaManager->SetAudioQuality(this->period, this->adaptationSet, this->representation);
}

uint64_t		PandaAdaptation::GetBitrate				()
{
	return this->currentBitrate;
}

void		PandaAdaptation::Quantizer				()
{
	this->deltaUp = this->param_W + this->param_Epsilon * (double)this->smoothBw;
	this->deltaDown = this->param_W;

	L("** DELTA UP:\t%f\n", this->deltaUp);

	uint64_t smoothBw_UP = this->smoothBw - this->deltaUp;
	uint64_t smoothBw_DOWN = this->smoothBw - this->deltaDown;

	L("** Smooth-BW UP:\t%d\t Smooth-BW DOWN:\t%d\n", smoothBw_UP, smoothBw_DOWN);

	std::vector<IRepresentation *> representations;
	representations = this->adaptationSet->GetRepresentation();
	uint32_t numQualLevels = representations.size();

	// We have to find bitrateMin and bitrateMax
	uint64_t bitrateDown, bitrateUp;

    // DOWN
	uint32_t iDown = 0;
    uint32_t i_d,i_u;
	for (i_d = 0; i_d < this->availableBitrates.size(); ++i_d) {
        if (this->availableBitrates[i_d] > smoothBw_DOWN) {

            break;
        }
	}
	if(i_d > 0)
		iDown = i_d-1;
	else
		iDown = 0;

    bitrateDown = (uint64_t) representations.at(iDown)->GetBandwidth();
    L("** Bitrate DOWN:\t%d\t at Quality:\t%d\n", bitrateDown, iDown);

    // UP
    uint32_t iUp = 0;
    for (i_u = 0; i_u < this->availableBitrates.size(); ++i_u) {
    	if (this->availableBitrates[i_u] > smoothBw_UP) {
    		break;
    	}
    }
	if(i_u > 0)
		iUp = i_u-1;
	else
		iUp = 0;

    bitrateUp = (uint64_t) representations.at(iUp)->GetBandwidth();
    L("** Bitrate UP:\t%d\t at Quality:\t%d\n", bitrateUp, iUp);

    L("** Current RATE:\t%d\n Current QUALITY:\t%d\n", this->currentBitrate, this->current);


    // Next bitrate computation
	if(this->currentBitrate < bitrateUp)
	{
		this->currentBitrate = bitrateUp;
		this->current = iUp;
	}
	else if(this->currentBitrate <= bitrateDown && this->currentBitrate >= bitrateUp)
	{
		L("** CURRENT UNCHANGED **\n");
	}
	else
	{
		this->currentBitrate = bitrateDown;
		this->current = iDown;
	}
	this->representation = this->adaptationSet->GetRepresentation().at(this->current);
}

void		PandaAdaptation::QuantizerExtended				()
{
	
	this->deltaUp = this->param_W + this->param_Epsilon * (double)this->smoothBw;	// window of acceptable video rates (high impact on stability when extd MPD used)
	this->deltaDown = this->param_W;
	L("** DELTA UP:\t%f\n", this->deltaUp);

	uint64_t smoothBw_UP = this->smoothBw - this->deltaUp;
	uint64_t smoothBw_DOWN = this->smoothBw - this->deltaDown;

	L("** Smooth-BW UP:\t%d\t Smooth-BW DOWN:\t%d\n", smoothBw_UP, smoothBw_DOWN);

	std::vector<IRepresentation *> representations;
	representations = this->adaptationSet->GetRepresentation();
	uint32_t numQualLevels = representations.size();

	// We have to find bitrateMin and bitrateMax
	uint64_t bitrateDown, bitrateUp, bitrateDownSegment, bitrateUpSegment;

    // DOWN
	uint32_t iDown = 0;
	uint32_t i_d;
	uint64_t realbps;
	for (i_d = 0; i_d < this->availableBitrates.size(); ++i_d) {
		realbps = representations.at(i_d)->GetSpecificSegmentSize(std::to_string(segPos+1));
        if (realbps > smoothBw_DOWN) {
            break;
        }
	}
	if(i_d > 0)
		iDown = i_d-1;
	else
		iDown = 0;

	bitrateDown = (uint64_t) representations.at(iDown)->GetBandwidth();
	bitrateDownSegment = representations.at(iDown)->GetSpecificSegmentSize(std::to_string(segPos+1));
    L("** Bitrate DOWN: (avg)\t%d\t (specific)\t%d\t at Quality:\t%d\n", bitrateDown, bitrateDownSegment, iDown);

    // UP
    uint32_t iUp = 0;
	uint32_t i_u;
    for (i_u = 0; i_u < this->availableBitrates.size(); ++i_u) {
		realbps = representations.at(i_u)->GetSpecificSegmentSize(std::to_string(segPos+1));
    	if (realbps > smoothBw_UP) {
    		break;
    	}
    }
	if(i_u > 0)
		iUp = i_u-1;
	else
		iUp = 0;

	bitrateUp = (uint64_t) representations.at(iUp)->GetBandwidth();
	bitrateUpSegment = representations.at(iUp)->GetSpecificSegmentSize(std::to_string(segPos+1));
    L("** Bitrate UP: (avg)\t%d\t (specific)\t%d\t at Quality:\t%d\n", bitrateUp, bitrateUpSegment, iUp);

    L("** Current RATE:\t%d\n Current QUALITY:\t%d\n", this->currentBitrate, this->current);


	//Looking into the future 
	bool singleOutlier = false;
	for(int i = 2; i < this->localSegVariance + 2; i++)
	{
		uint64_t bitrateUpSegmentNext = representations.at(iUp)->GetSpecificSegmentSize(std::to_string(segPos+i));	//real bps of i'th segment after current segment
		if (bitrateUpSegmentNext > smoothBw_UP)
		{
			singleOutlier = true;
			break;
		}
	}

	// Next bitrate computation
	if(this->currentBitrate < bitrateUp)
	{
		if(this->currentInstability < this->param_instability && !singleOutlier)
		{
			this->currentBitrateVirtual = bitrateUp;
			this->current = iUp;
			std::cout << "[UP] " << bitrateUp << " /real :" << bitrateUpSegment << " Instability: " << this->currentInstability << "\n";
		} else
		{
			std::cout << "[NOT UP]" << bitrateUp << " /real: " << bitrateUpSegment << " Instability: " << this->currentInstability << "\n";
			L("** CURRENT UNCHANGED because of INSTABILITY **\n");
		}
	}
	else if(this->currentBitrate <= bitrateDown && this->currentBitrate >= bitrateUp)
	{
		L("** CURRENT UNCHANGED **\n");
		std::cout << "[UNCHANGED]" << bitrateDown << " >= " << this->currentBitrate << " >= " << bitrateUp << " Instability: " << this->currentInstability << "\n";
	}
	else if(this->currentBitrate > bitrateDown)
	{
		this->currentBitrateVirtual = bitrateDown;
		this->current = iDown;
		std::cout << "[DOWN]" << bitrateDown << " /real: " << bitrateDownSegment << " Instability: " << this->currentInstability << "\n";
	}
	
	this->representation = this->adaptationSet->GetRepresentation().at(this->current);
}

void 		PandaAdaptation::SetBitrate				(uint64_t bps)
{

	// 1. Calculating the targetBW
	if(this->targetBw)
		{
			//this->targetBw = this->targetBw + param_K * this->interTime * (param_W - ((this->targetBw - bps + this->param_W) > 0 ? this->targetBw - bps + this->param_W: 0));
			if ((double)this->targetBw - (double)bps + this->param_W > 0)
				this->targetBw = this->targetBw + (uint64_t)(param_K * this->interTime * (param_W - ((double)this->targetBw - (double)bps + this->param_W)));
			else
				this->targetBw = this->targetBw + (uint64_t)(param_K * this->interTime * param_W);
		}
		else
			this->targetBw = bps;

	L("** INSTANTANEOUS BW:\t%d\n", bps);
	L("** CLASSIC EWMA BW:\t%d\n", this->averageBw);
	L("** PANDA TARGET BW:\t%d\n", this->targetBw);

	// 2. Calculating the smoothBW
	if(this->interTime)
		this->smoothBw = (uint64_t)((double)this->smoothBw - this->param_Alpha * this->interTime * ((double)this->smoothBw - (double)this->targetBw));
	else
		this->smoothBw = this->targetBw;

	L("** PANDA SMOOTH BW:\t%d\n", this->smoothBw);

	// 3. Quantization
	if(extMPDuse)
		this->QuantizerExtended();
	else
		this->Quantizer();

	L("ADAPTATION_LOGIC:\tFor %s:\tlast_buffer: %f\tbuffer_level: %f, instantaneousBw: %lu, AverageBW: %lu, choice: %d\n",isVideo ? "video" : "audio",(double)lastBufferLevel/100 , (double)bufferLevel/100, this->instantBw, this->averageBw , this->current);
	this->lastBufferLevel = this->bufferLevel;

	// 4. Computing the "actual inter time"
	this->bufferLevelSeconds = (double)( (this->bufferLevel * this->bufferMaxSizeSeconds) *1./100);
	this->targetInterTime = ((double)this->currentBitrate * segmentDuration) * 1./this->smoothBw + param_Beta * (this->bufferLevelSeconds - param_Bmin);
	L("** TARGET INTER TIME:\t%f\n", this->targetInterTime);
	L("** DOWNLOAD TIME:\t%f\n", this->downloadTime);
	this->targetInterTime = (this->targetInterTime > 0) ? this->targetInterTime : 0.0;
	this->interTime = this->targetInterTime > this->downloadTime ? this->targetInterTime : this->downloadTime;
	L("** ACTUAL INTER TIME:\t%f\n", this->interTime);
	this->multimediaManager->SetTargetDownloadingTime(this->isVideo, interTime);
}

void			PandaAdaptation::BitrateUpdate			(uint64_t bps)
{
	this->instantBw = bps;

	// Avg bandwidth estimate with EWMA
	if(this->averageBw == 0)
	{
		this->averageBw = bps;
	}
	else
	{
		this->averageBw = this->alpha_ewma*this->averageBw + (1 - this->alpha_ewma)*bps;
	}

	this->SetBitrate(bps);
	this->NotifyBitrateChange();								//HERE is the update for the Multimedia Manager (triggers "setVideoQuality")
}

void			PandaAdaptation::SegmentPositionUpdate			(uint32_t qualitybps)
{
	this->segPos++;
	this->currentBitrate = (uint64_t)qualitybps;
	std::cout << "Pos: " << segPos << " with " << this->currentBitrate << "\n";
	
	history.push_back(this->currentBitrate);		//build up the history

	if(history.size() > historylength)				//only history over last k segments (k=historylength)
		history.pop_front();

	this->currentInstability = this->ComputeInstability();
}

double 		PandaAdaptation::ComputeInstability			()
{
	int64_t numerator = 0;
	int64_t denominator = 0;
	int64_t last_rate = history.front();
	double weight = 1.0;

	for(uint64_t rate : history)
	{
		numerator = numerator + abs(((int64_t)rate - last_rate) * weight);
		denominator = denominator + ((int64_t)rate * weight);
		last_rate = (int64_t)rate;
	}
	
	double instability = numerator / (double)denominator;

	return instability;
}

void 			PandaAdaptation::DLTimeUpdate		(double time)
{
	this->downloadTime = time;
}

void			PandaAdaptation::BufferUpdate			(uint32_t bufferFill)
{
    if (bufferFill == (uint32_t)0 && this->rebufferStart == 0) {
        this->rebufferStart = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        this->rebufferEnd = 0;
    }
    else if (this->rebufferEnd == 0 && this->rebufferStart != 0) {
        this->rebufferEnd = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
	this->bufferLevel = bufferFill;
}

void 			PandaAdaptation::OnEOS					(bool value)
{
}

void            PandaAdaptation::OnSegmentDownloaded (double bufferFillPct, int segNum, int quality, std::vector<int> rttVec, std::vector<int> numHopsVec)
{
    std::vector<IRepresentation*> representations = this->adaptationSet->GetRepresentation();
    int numArms = (int)representations.size();
    uint32_t bufferFill = (uint32_t)(bufferFillPct * (double)100);
    std::cout << "stat-begin" << std::endl;
    std::cout << "stat-nonbandit" << std::endl;
    std::cout << "stat-qual " << quality << std::endl;

    ///////////////////
    // Calculate QoE //
    ///////////////////
    // 1. Base quality (kbps)
    // 2. Decline in quality (kbps)
    // 3. Time spent rebuffering (ms)

    // 1. Base quality
    int baseQuality = (int)(this->availableBitrates[quality] / (uint64_t)1000);

    std::cout << "stat-qoebase " << baseQuality << std::endl;

    // 2. Decline in quality
    this->qualityHistory[segNum] = quality;
    int qualityDecline = 0;
    if (this->qualityHistory[segNum] < this->qualityHistory[segNum-1]) {
        int prevQuality = (int)(this->availableBitrates[this->qualityHistory[segNum-1]]/(uint64_t)1000);
        int currQuality = (int)(this->availableBitrates[quality]/(uint64_t)1000);
        qualityDecline = prevQuality - currQuality;
    }
    std::cout << "stat-qoedecl " << qualityDecline << std::endl;

    // 3. Time spent rebuffering
    // We just got a new segment so rebuffering has stopped
    // If it occurred, we need to know for how long
    int rebufferTime = 0;
    if (this->rebufferStart != 0) {
        if (this->rebufferEnd == 0) {
            this->rebufferEnd = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
        rebufferTime = (int)(this->rebufferEnd - this->rebufferStart);
        this->rebufferStart = 0;
    }
    std::cout << "stat-qoerebuffms " << rebufferTime << std::endl;

    // Put it all together for the reward
    // The weight of 3 comes from the SIGCOMM paper; they use 3000 because their 
    // rebufferTime is in seconds, but ours is milliseconds
    double reward = (this->qoe_w1*(double)baseQuality) - (this->qoe_w2*(double)qualityDecline) - (this->qoe_w3*(double)rebufferTime*(double)rebufferTime);
    this->cumQoE += reward;
    std::cout << "stat-qoeseg " << reward << std::endl;
    std::cout << "stat-qoecum " << this->cumQoE << std::endl;
}

void			PandaAdaptation::CheckedByDASHReceiver	()
{
}
