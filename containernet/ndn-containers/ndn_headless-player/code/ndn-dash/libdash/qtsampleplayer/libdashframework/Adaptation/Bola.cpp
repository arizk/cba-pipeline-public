/*
 * Bola.cpp
 *****************************************************************************
 * Copyright (C) 2016
 * Michele Tortelli and Jacques Samain, <michele.tortelli@telecom-paristech.fr>, <jsamain@cisco.com>
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "Bola.h"
#include <stdio.h>
#include <math.h>
#include <chrono>
#include <string>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <inttypes.h>
#include <stdlib.h>
#include <stdarg.h>
#include <algorithm>
#include <inttypes.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

const double MINIMUM_BUFFER_LEVEL_SPACING = 5.0;			// The minimum space required between buffer levels (in seconds).
const uint32_t THROUGHPUT_SAMPLES = 3;						// Number of samples considered for throughput estimate.
const double SAFETY_FACTOR = 0.9;							// Safety factor used with bandwidth estimate.

using namespace dash::mpd;
using namespace libdash::framework::adaptation;
using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;


using duration_in_seconds = std::chrono::duration<double, std::ratio<1, 1> >;

BolaAdaptation::BolaAdaptation          (IMPD *mpd, IPeriod *period, IAdaptationSet *adaptationSet, bool isVid, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6) :
                  AbstractAdaptationLogic   (mpd, period, adaptationSet, isVid)
{
	// Set Bola init STATE
	this->initState = true;
	this->bolaState = STARTUP;

	this->lastDownloadTimeInstant = 0.0;
	this->currentDownloadTimeInstant = 0.0;
	//this->lastSegmentDownloadTime = 0.0;
	this->currentQuality = 0;


	this->bufferMaxSizeSeconds = 20.0;		// It is 'bufferMax' in dash.js implementation
											// Find a way to retrieve the value without hard coding it
    // Set QoE weights
    this->qoe_w1 = arg4;
    this->qoe_w2 = arg5;
    this->qoe_w3 = arg6;

    this->rebufferStart = 0;
    this->rebufferEnd = 0;

	// Set alpha for the EWMA (bw estimate)
	if(arg1) {
	    if(arg1 >= 0 && arg1 <= 1) {
			this->alphaRate = arg1;
		}
		else
			this->alphaRate = 0.8;
	}
	else {
		Debug("EWMA Alpha parameter NOT SPECIFIED!\n");
	}

	/// Set 'bufferTarget' (it separates STARTUP from STEADY STATE)
	if(arg2) {
		if(arg2 > 0 && arg2 < (int)bufferMaxSizeSeconds) {
			this->bufferTargetSeconds = (double)arg2;
		}
		else
			this->bufferTargetSeconds = 12.0;   // 12s (dash.js implementation) corresponds to 40% with a buffer of 30s

		this->bufferTargetPerc = (uint32_t) ( round(this->bufferTargetSeconds / this->bufferMaxSizeSeconds)*100 );
	}
	else {
		Debug("bufferTarget NOT SPECIFIED!\n");
	}

	/// Retrieve available bitrates
	std::vector<IRepresentation* > representations = this->adaptationSet->GetRepresentation();

	this->availableBitrates.clear();
	L("BOLA Available Bitrates...\n");
	for(size_t i = 0; i < representations.size(); i++)
	{
		this->availableBitrates.push_back((uint64_t)(representations.at(i)->GetBandwidth()));
		L("%d  -  %I64u bps\n", i+1, this->availableBitrates[i]);
	}
	// Check if they are in increasing order (i.e., bitrate[0] <= bitrate[1], etc.)

	this->bitrateCount = this->availableBitrates.size();

	// We check if we have only one birate value or if the bitrate list is irregular (i.e., it is not strictly increasing)
	if (this->bitrateCount < 2 || this->availableBitrates[0] >= this->availableBitrates[1] || this->availableBitrates[this->bitrateCount - 2] >= this->availableBitrates[this->bitrateCount - 1]) {
		this->bolaState = ONE_BITRATE;
	    // return 0;   // Check if exit with a message is necessary
	}

	// Check if the following is correct
	this->totalDuration = TimeResolver::GetDurationInSec(this->mpd->GetMediaPresentationDuration());
	//this->segmentDuration = (double) (representations.at(0)->GetSegmentTemplate()->GetDuration() / representations.at(0)->GetSegmentTemplate()->GetTimescale() );
	this->segmentDuration = 2.0;
	L("Total Duration - BOLA:\t%f\nSegment Duration - BOLA:\t%f\n",this->totalDuration, this->segmentDuration);
	// if not correct --> segmentDuration = 2.0;

	// Compute the BOLA Buffer Target
	this->bolaBufferTargetSeconds = this->bufferTargetSeconds;
	if (this->bolaBufferTargetSeconds < this->segmentDuration + MINIMUM_BUFFER_LEVEL_SPACING)
	{
		this->bolaBufferTargetSeconds = this->segmentDuration + MINIMUM_BUFFER_LEVEL_SPACING;
	}
	L("BOLA Buffer Target Seconds:\t%f\n",this->bolaBufferTargetSeconds);

	// Compute UTILTY vector, Vp, and gp
	L("BOLA Utility Values...\n");
	double utility_tmp;
	for (uint32_t i = 0; i < this->bitrateCount; ++i) {
		utility_tmp = log(((double)this->availableBitrates[i] * (1./(double)this->availableBitrates[0])));
		if(utility_tmp < 0)
			utility_tmp = 0;
		this->utilityVector.push_back( log(((double)this->availableBitrates[i] * (1./(double)this->availableBitrates[0]))));
		L("%d  -  %f\n", i+1, this->utilityVector[i]);
	}

	this->Vp = (this->bolaBufferTargetSeconds - this->segmentDuration) / this->utilityVector[this->bitrateCount - 1];
	this->gp = 1.0 + this->utilityVector[this->bitrateCount - 1] / (this->bolaBufferTargetSeconds / this->segmentDuration - 1.0);

	L("BOLA Parameters:\tVp:  %f\tgp:  %f\n",this->Vp, this->gp);
	/* If bufferTargetSeconds (not bolaBufferTargetSecond) is large enough, we might guarantee that Bola will never rebuffer
	 * unless the network bandwidth drops below the lowest encoded bitrate level. For this to work, Bola needs to use the real buffer
	 * level without the additional virtualBuffer. Also, for this to work efficiently, we need to make sure that if the buffer level
	 * drops to one fragment during a download, the current download does not have more bits remaining than the size of one fragment
	 * at the lowest quality*/

	this->maxRtt = 0.2; // Is this reasonable?
	if(this->bolaBufferTargetSeconds == this->bufferTargetSeconds) {
		this->safetyGuarantee = true;
	}
	if (this->safetyGuarantee) {
		L("BOLA SafetyGuarantee...\n");
		// we might need to adjust Vp and gp
		double VpNew = this->Vp;
		double gpNew = this->gp;
		for (uint32_t i = 1; i < this->bitrateCount; ++i) {
			double threshold = VpNew * (gpNew - this->availableBitrates[0] * this->utilityVector[i] / (this->availableBitrates[i] - this->availableBitrates[0]));
			double minThreshold = this->segmentDuration * (2.0 - this->availableBitrates[0] / this->availableBitrates[i]) + maxRtt;
			if (minThreshold >= this->bufferTargetSeconds) {
				safetyGuarantee = false;
				break;
			}
			if (threshold < minThreshold) {
				VpNew = VpNew * (this->bufferTargetSeconds - minThreshold) / (this->bufferTargetSeconds - threshold);
				gpNew = minThreshold / VpNew + this->utilityVector[i] * this->availableBitrates[0] / (this->availableBitrates[i] - this->availableBitrates[0]);
			}
		}
		if (safetyGuarantee && (this->bufferTargetSeconds - this->segmentDuration) * VpNew / this->Vp < MINIMUM_BUFFER_LEVEL_SPACING) {
			safetyGuarantee = false;
		}
		if (safetyGuarantee) {
			this->Vp = VpNew;
			this->gp = gpNew;
		}
	}

	L("BOLA New Parameters:\tVp:  %f\tgp:  %f\n",this->Vp, this->gp);

	// Capping of the virtual buffer (when using it)
	this->bolaBufferMaxSeconds = this->Vp * (this->utilityVector[this->bitrateCount - 1] + this->gp);
	L("BOLA Max Buffer Seconds:\t%f\n",this->bolaBufferMaxSeconds);

	this->virtualBuffer = 0.0;			// Check if we should use either the virtualBuffer or the safetyGuarantee

	this->instantBw = 0;
	this->averageBw = 0;
	this->batchBw = 0;					// Computed every THROUGHPUT_SAMPLES samples (average)
	this->batchBwCount = 0;

	this->multimediaManager = NULL;
	this->lastBufferFill = 0;		// (?)
	this->bufferEOS = false;
	this->shouldAbort = false;
	this->isCheckedForReceiver = false;

	this->representation = representations.at(0);
	this->currentBitrate = (uint64_t) this->representation->GetBandwidth();

	L("BOLA Init Params - \tAlpha: %f \t BufferTarget: %f\n",this->alphaRate, this->bufferTargetSeconds);
	L("BOLA Init Current BitRate - %I64u\n",this->currentBitrate);
	Debug("Buffer Adaptation BOLA:	STARTED\n");
}
BolaAdaptation::~BolaAdaptation         ()
{
}

LogicType       BolaAdaptation::GetType               ()
{
    return adaptation::BufferBased;
}

bool			BolaAdaptation::IsUserDependent		()
{
	return false;
}

bool			BolaAdaptation::IsRateBased		()
{
	return true;
}
bool			BolaAdaptation::IsBufferBased		()
{
	return true;
}

void			BolaAdaptation::SetMultimediaManager (sampleplayer::managers::IMultimediaManagerBase *_mmManager)
{
	this->multimediaManager = _mmManager;
}

void			BolaAdaptation::NotifyBitrateChange	()
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

uint64_t		BolaAdaptation::GetBitrate				()
{
	return this->currentBitrate;
}

int BolaAdaptation::getQualityFromThroughput(uint64_t bps) {
    int q = 0;
    for (int i = 0; i < this->availableBitrates.size(); ++i) {
        if (this->availableBitrates[i] > bps) {
            break;
        }
        q = i;
    }
    return q;
}

int BolaAdaptation::getQualityFromBufferLevel(double bufferLevelSec) {
	int quality = this->bitrateCount - 1;
	double score = 0.0;
	for (int i = 0; i < this->bitrateCount; ++i) {
		double s = (this->utilityVector[i] + this->gp - bufferLevelSec / this->Vp) / this->availableBitrates[i];
		if (s > score) {
			score = s;
			quality = i;
		}
	}
	return quality;
}

void 			BolaAdaptation::SetBitrate				(uint32_t bufferFill)
{
	// *** NB *** Insert Log messages

	if(this->initState)
	{
		this->initState = false;

		if(this->bolaState != ONE_BITRATE)
		{
		    if(this->batchBw != 0)		// Check the current estimated throughput (batch mean)
		    	this->currentQuality = getQualityFromThroughput(this->batchBw*SAFETY_FACTOR);
			//else --> quality unchanged
		}
		//this->representation = this->availableBitrates[this->currentQuality];
		//this->currentBitrate = (uint64_t) this->representation->GetBandwidth();
		this->representation = this->adaptationSet->GetRepresentation().at(this->currentQuality);
		this->currentBitrate = (uint64_t) this->availableBitrates[this->currentQuality];
		L("INIT - Current Bitrate:\t%I64u\n", this->currentBitrate);
		L("ADAPTATION_LOGIC:\tFor %s:\tlast_buffer: %f\tbuffer_level: %f, instantaneousBw: %lu, AverageBW: %lu, choice: %d\n",isVideo ? "video" : "audio",(double)lastBufferFill/100 , (double)bufferFill/100, this->instantBw, this->averageBw , this->currentQuality);
		this->lastBufferFill = bufferFill;
		return;
	}

	if(this->bolaState == ONE_BITRATE) {
		this->currentQuality = 0;
		//this->representation = this->availableBitrates[this->currentQuality];
		//this->currentBitrate = (uint64_t) this->representation->GetBandwidth();
		this->representation = this->adaptationSet->GetRepresentation().at(this->currentQuality);
		this->currentBitrate = (uint64_t) this->availableBitrates[this->currentQuality];
		L("ONE BITRATE - Current Bitrate:\t%I64u\n", this->currentBitrate);
		L("ADAPTATION_LOGIC:\tFor %s:\tlast_buffer: %f\tbuffer_level: %f, instantaneousBw: %lu, AverageBW: %lu, choice: %d\n",isVideo ? "video" : "audio",(double)lastBufferFill/100 , (double)bufferFill/100, this->instantBw, this->averageBw , this->currentQuality);
		this->lastBufferFill = bufferFill;
		return;
	}

	// Obtain bufferFill in seconds;
	double bufferLevelSeconds = (double)( (bufferFill * this->bufferMaxSizeSeconds) *1./100);
	int bolaQuality = getQualityFromBufferLevel(bufferLevelSeconds);

	L("REGULAR - Buffer Level Seconds:\t%f; Bola Quality:\t%d\n", bufferLevelSeconds, bolaQuality);


	if (bufferLevelSeconds <= 0.1) {
		// rebuffering occurred, reset virtual buffer
		this->virtualBuffer = 0.0;
	}

	// We check if the safetyGuarantee should be used. if not, we use the virtual buffer
	// STILL NOT COMPLETE; Find a way to retrieved time since the last download
	if (!this->safetyGuarantee) // we can use virtualBuffer
	{
		// find out if there was delay because of lack of availability or because bolaBufferTarget > bufferTarget
		// TODO
	    //double timeSinceLastDownload = getDelayFromLastFragmentInSeconds();		// Define function
	    double timeSinceLastDownload = this->currentDownloadTimeInstant - this->lastDownloadTimeInstant;

	    L("VirtualBuffer - Time Since Last Download:\t%f\n", timeSinceLastDownload);

	    if (timeSinceLastDownload > 0.0) {
	    	this->virtualBuffer += timeSinceLastDownload;
	    }
	    if ( (bufferLevelSeconds + this->virtualBuffer) > this->bolaBufferMaxSeconds) {
	    	this->virtualBuffer = this->bolaBufferMaxSeconds - bufferLevelSeconds;
	    }
	    if (this->virtualBuffer < 0.0) {
	    	this->virtualBuffer = 0.0;
	    }

	    L("VirtualBuffer - Virtual Buffer Value:\t%f\n", this->virtualBuffer);

	    // Update currentDownloadTimeInstant
	    this->lastDownloadTimeInstant = this->currentDownloadTimeInstant;

	    // Update bolaQuality using virtualBuffer: bufferLevel might be artificially low because of lack of availability

	    int bolaQualityVirtual = getQualityFromBufferLevel(bufferLevelSeconds + this->virtualBuffer);
	    L("VirtualBuffer - Bola Quality Virtual:\t%d\n", bolaQualityVirtual);
	    if (bolaQualityVirtual > bolaQuality) {
	    	// May use quality higher than that indicated by real buffer level.
	    	// In this case, make sure there is enough throughput to download a fragment before real buffer runs out.
	    	int maxQuality = bolaQuality;
	    	while (maxQuality < bolaQualityVirtual && (this->availableBitrates[maxQuality + 1] * this->segmentDuration) / (this->currentBitrate * SAFETY_FACTOR) < bufferLevelSeconds)
	    	{
	    		++maxQuality;
	    	}
	    	// TODO: maybe we can use a more conservative level here, but this should be OK
	    	L("VirtualBuffer - Bola Quality Virtual HIGHER than Bola Quality - Max Quality:\t%d\n", maxQuality);
	    	if (maxQuality > bolaQuality)
	    	{
	    		// We can (and will) download at a quality higher than that indicated by real buffer level.
	    		if (bolaQualityVirtual <= maxQuality) {
	    			// we can download fragment indicated by real+virtual buffer without rebuffering
	    			bolaQuality = bolaQualityVirtual;
	    		} else {
	    			// downloading fragment indicated by real+virtual rebuffers, use lower quality
	    			bolaQuality = maxQuality;
	    			// deflate virtual buffer to match quality
	    			double targetBufferLevel = this->Vp * (this->gp + this->utilityVector[bolaQuality]);
	    			if (bufferLevelSeconds + this->virtualBuffer > targetBufferLevel) {
	    				this->virtualBuffer = targetBufferLevel - bufferLevelSeconds;
	    				if (this->virtualBuffer < 0.0) { // should be false
	    					this->virtualBuffer = 0.0;
	    				}
	    			}
	    		}
	    	}
	    }
	}


	if (this->bolaState == STARTUP || this->bolaState == STARTUP_NO_INC) {
		// in startup phase, use some throughput estimation

	    int quality = getQualityFromThroughput(this->batchBw*SAFETY_FACTOR);

	    if (this->batchBw <= 0.0) {
	    	// something went wrong - go to steady state
	    	this->bolaState = STEADY;
	    }
	    if (this->bolaState == STARTUP && quality < this->currentQuality) {
	    	// Since the quality is decreasing during startup, it will not be allowed to increase again.
	    	this->bolaState = STARTUP_NO_INC;
	    }
	    if (this->bolaState == STARTUP_NO_INC && quality > this->currentQuality) {
	    	// In this state the quality is not allowed to increase until steady state.
	    	quality = this->currentQuality;
	    }
	    if (quality <= bolaQuality) {
	    	// Since the buffer is full enough for steady state operation to match startup operation, switch over to steady state.
	    	this->bolaState = STEADY;
	    }
	    if (this->bolaState != STEADY) {
	    	// still in startup mode
	    	this->currentQuality = quality;
	    	//this->representation = this->availableBitrates[this->currentQuality];
	    	//this->currentBitrate = (uint64_t) this->representation->GetBandwidth();
	    	this->representation = this->adaptationSet->GetRepresentation().at(this->currentQuality);
	    	this->currentBitrate = (uint64_t) this->availableBitrates[this->currentQuality];
	    	L("STILL IN STARTUP - Current Bitrate:\t%I64u\n", this->currentBitrate);
	    	L("ADAPTATION_LOGIC:\tFor %s:\tlast_buffer: %f\tbuffer_level: %f, instantaneousBw: %lu, AverageBW: %lu, choice: %d\n",isVideo ? "video" : "audio",(double)lastBufferFill/100 , (double)bufferFill/100, this->instantBw, this->averageBw , this->currentQuality);
	    	this->lastBufferFill = bufferFill;
	    	return;
	    }
	}

	// Steady State

	// In order to avoid oscillation, the "BOLA-O" variant is implemented.
	// When network bandwidth lies between two encoded bitrate levels, stick to the lowest one.
	double delaySeconds = 0.0;
	if (bolaQuality > this->currentQuality) {
		L("STEADY -- BOLA QUALITY:\t%d  - HIGHER than - CURRENT QUALITY:\t%I64u\n", bolaQuality, this->currentBitrate);
		// do not multiply throughput by bandwidthSafetyFactor here;
		// we are not using throughput estimation but capping bitrate to avoid oscillations
		int quality = getQualityFromThroughput(this->batchBw);
		if (bolaQuality > quality) {
			// only intervene if we are trying to *increase* quality to an *unsustainable* level
			if (quality < this->currentQuality) {
				// The aim is only to avoid oscillations - do not drop below current quality
				quality = this->currentQuality;
			} else {
				// We are dropping to an encoded bitrate which is a little less than the network bandwidth
				// since bitrate levels are discrete. Quality 'quality' might lead to buffer inflation,
				// so we deflate the buffer to the level that 'quality' gives positive utility.
				double targetBufferLevel = this->Vp * (this->utilityVector[quality] + this->gp);
				delaySeconds = bufferLevelSeconds - targetBufferLevel;
			}
			bolaQuality = quality;
		}
	}

	if (delaySeconds > 0.0) {
		// first reduce virtual buffer
		if (delaySeconds > this->virtualBuffer) {
			delaySeconds -= this->virtualBuffer;
			this->virtualBuffer = 0.0;
		} else {
			this->virtualBuffer -= delaySeconds;
			delaySeconds = 0.0;
		}
	}
	if (delaySeconds > 0.0) {
		// TODO  Check the scope of this function. Is it a delayed request?
		// streamProcessor.getScheduleController().setTimeToLoadDelay(1000.0 * delaySeconds);
		// NEED TO CHECK THIS
		L("STEADY -- DELAY DOWNLOAD OF:\t%f\n", delaySeconds);
		this->multimediaManager->SetTargetDownloadingTime(this->isVideo, delaySeconds);
	}

	this->currentQuality = bolaQuality;
	//this->representation = this->availableBitrates[this->currentQuality];
	this->representation = this->adaptationSet->GetRepresentation().at(this->currentQuality);
	this->currentBitrate = (uint64_t) this->availableBitrates[this->currentQuality];
	L("STEADY - Current Bitrate:\t%I64u\n", this->currentBitrate);
	L("ADAPTATION_LOGIC:\tFor %s:\tlast_buffer: %f\tbuffer_level: %f, instantaneousBw: %lu, AverageBW: %lu, choice: %d\n",isVideo ? "video" : "audio",(double)lastBufferFill/100 , (double)bufferFill/100, this->instantBw, this->averageBw , this->currentQuality);
	this->lastBufferFill = bufferFill;
}

void			BolaAdaptation::BitrateUpdate		(uint64_t bps)
{
	this->instantBw = bps;

	// Avg bandwidth estimate with EWMA
	if(this->averageBw == 0)
	{
		this->averageBw = bps;
	}
	else
	{
		this->averageBw = this->alphaRate*this->averageBw + (1 - this->alphaRate)*bps;
	}

	// Avg bandwidth estimate with batch mean of THROUGHPUT_SAMPLES sample
	this->batchBwCount++;
	this->batchBwSamples.push_back(bps);

	if(this->batchBwCount++ == THROUGHPUT_SAMPLES)
	{
		for(int i=0; i<THROUGHPUT_SAMPLES; i++)
			this->batchBw += this->batchBwSamples[i];

		this->batchBw /= THROUGHPUT_SAMPLES;

		L("BATCH BW:\t%I64u\n", this->batchBw);

		this->batchBwCount=0;
		this->batchBwSamples.clear();
	}
}

void			BolaAdaptation::SegmentPositionUpdate			(uint32_t qualitybps)
{
}

void 			BolaAdaptation::DLTimeUpdate		(double time)
{
	auto m_now = std::chrono::system_clock::now();
	auto m_now_sec = std::chrono::time_point_cast<std::chrono::seconds>(m_now);

	auto now_value = m_now_sec.time_since_epoch();
	double dl_instant = now_value.count();
	//this->lastSegmentDownloadTime = time;
	//this->currentDownloadTimeInstant = std::chrono::duration_cast<duration_in_seconds>(system_clock::now()).count();
	this->currentDownloadTimeInstant = dl_instant;
}

void			BolaAdaptation::OnEOS				(bool value)
{
	this->bufferEOS = value;
}
void            BolaAdaptation::OnSegmentDownloaded (double bufferFillPct, int segNum, int quality, std::vector<int> rttVec, std::vector<int> numHopsVec)
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

void 			BolaAdaptation::CheckedByDASHReceiver	()
{
	this->isCheckedForReceiver = false;
}
void			BolaAdaptation::BufferUpdate			(uint32_t bufferFill)
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
	this->SetBitrate(bufferFill);
	this->NotifyBitrateChange();
}
