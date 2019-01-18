/*
 * FOOBAR.cpp
 *
 *****************************************************************************/

#include "FOOBAR.h"
#include<stdio.h>


using namespace dash::mpd;
using namespace libdash::framework::adaptation;
using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

FOOBARAdaptation::FOOBARAdaptation(IMPD *mpd, IPeriod *period, IAdaptationSet *adaptationSet, bool isVid, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6) :
                          AbstractAdaptationLogic   (mpd, period, adaptationSet, isVid)
{
	this->param_Alpha = 0.2;
	this->param_Beta = 0.2;
	this->param_K = 0.14;
	this->param_W = 300000;
	this->param_Epsilon = 0.2;				//safety threshhold changed from 0.15 to 0.2(5)
	this->targetBw = 0;
	this->targetInterTime = 0.0;

	this->averageBw = 0;
	this->smoothBw = 0;
	this->instantBw = 0;
	this->targetBw = 0;

	this->targetInterTime = 0.0;
	this->interTime = 0.0;

	this->segmentDuration = 2.0;
	this->bufferMaxSizeSeconds = 20.0;

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

	// Set 'param_Bmin'
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

	this->availableBitrates.clear();
	L("FOOBAR Available Bitrates...\n");
	for(size_t i = 0; i < representations.size(); i++)
	{
		this->availableBitrates.push_back((uint64_t)(representations.at(i)->GetBandwidth()));
		L("%d  -  %I64u bps\n", i+1, this->availableBitrates[i]);
	}

	this->representation = this->adaptationSet->GetRepresentation().at(0);
	this->currentBitrate = (uint64_t) this->representation->GetBandwidth();

	L("FOOBAR parameters: K= %f, Bmin = %f, alpha = %f, beta = %f, W = %f\n", param_K, param_Bmin, param_Alpha, param_Beta, param_W);
}

FOOBARAdaptation::~FOOBARAdaptation() {
}

LogicType		FOOBARAdaptation::GetType             ()
{
	return adaptation::FOOBAR;
}

bool			FOOBARAdaptation::IsUserDependent		()
{
   	return false;
}

bool			FOOBARAdaptation::IsRateBased			()
{
   	return true;
}

bool			FOOBARAdaptation::IsBufferBased		()
{
  	return true;
}

void			FOOBARAdaptation::SetMultimediaManager (sampleplayer::managers::IMultimediaManagerBase *_mmManager)
{
   	this->multimediaManager = _mmManager;
}

void			FOOBARAdaptation::NotifyBitrateChange	()
{
	if(this->multimediaManager->IsStarted() && !this->multimediaManager->IsStopping())
	if(this->isVideo)
		this->multimediaManager->SetVideoQuality(this->period, this->adaptationSet, this->representation);
	else
		this->multimediaManager->SetAudioQuality(this->period, this->adaptationSet, this->representation);
}

uint64_t		FOOBARAdaptation::GetBitrate				()
{
	return this->currentBitrate;
}

void		FOOBARAdaptation::Quantizer				()
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
void 		FOOBARAdaptation::SetBitrate				(uint64_t bps)
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
	L("** FOOBAR TARGET BW:\t%d\n", this->targetBw);

	// 2. Calculating the smoothBW
	if(this->interTime)
		this->smoothBw = (uint64_t)((double)this->smoothBw - this->param_Alpha * this->interTime * ((double)this->smoothBw - (double)this->targetBw));
	else
		this->smoothBw = this->targetBw;

	L("** FOOBAR SMOOTH BW:\t%d\n", this->smoothBw);

	// 3. Quantization
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

void			FOOBARAdaptation::BitrateUpdate			(uint64_t bps)
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

void			FOOBARAdaptation::SegmentPositionUpdate			(uint32_t qualitybps)
{
}

void 			FOOBARAdaptation::DLTimeUpdate		(double time)
{
	this->downloadTime = time;
}

void			FOOBARAdaptation::BufferUpdate			(uint32_t bufferfill)
{
	this->bufferLevel = bufferfill;
}

void 			FOOBARAdaptation::OnEOS					(bool value)
{
}
void            FOOBARAdaptation::OnSegmentDownloaded (double bufferFillPct, int segNum, int quality, std::vector<int> rttVec, std::vector<int> numHopsVec)
{
}

void			FOOBARAdaptation::CheckedByDASHReceiver	()
{
}
