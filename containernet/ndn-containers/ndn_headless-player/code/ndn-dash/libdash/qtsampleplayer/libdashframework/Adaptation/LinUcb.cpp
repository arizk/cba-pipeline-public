/*
 * LinUcb.cpp
 * By Trevor Ballard and Bastian Alt, <trevorcoleballard@gmail.com>, <bastian.alt@bcs.tu-darmstadt.de>
 *
 * Adapted from code by Michele Tortelli and Jacques Samain, whose copyright is
 * reproduced below.
 *
 *****************************************************************************
 * Copyright (C) 2016
 * Michele Tortelli and Jacques Samain, <michele.tortelli@telecom-paristech.fr>, <jsamain@cisco.com>
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "LinUcb.h"
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
#include <Python.h>
#include <chrono>
#include <thread>

const double MINIMUM_BUFFER_LEVEL_SPACING = 5.0;            // The minimum space required between buffer levels (in seconds).
const uint32_t THROUGHPUT_SAMPLES = 3;                      // Number of samples considered for throughput estimate.
const double SAFETY_FACTOR = 0.9;                           // Safety factor used with bandwidth estimate.

static PyObject *pName, *pModule, *pDict, *pFuncChoose, *pFuncUpdate;
static int gil_init = 0;

using namespace dash::mpd;
using namespace libdash::framework::adaptation;
using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;


using duration_in_seconds = std::chrono::duration<double, std::ratio<1, 1> >;

LinUcbAdaptation::LinUcbAdaptation (IMPD *mpd, IPeriod *period, IAdaptationSet *adaptationSet, bool isVid, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6) : AbstractAdaptationLogic (mpd, period, adaptationSet, isVid)
{
    // Set LinUcb init STATE
    this->initState = true;
    this->linUcbState = STARTUP;

    this->lastDownloadTimeInstant = 0.0;
    this->currentDownloadTimeInstant = 0.0;
    //this->lastSegmentDownloadTime = 0.0;
    this->currentQuality = 0;

    this->cumQoE = 0.0;


    this->bufferMaxSizeSeconds = 20.0;      // It is 'bufferMax' in dash.js implementation
                                            // Find a way to retrieve the value without hard coding it

    // Set QoE weights
    this->qoe_w1 = arg4;
    this->qoe_w2 = arg5;
    this->qoe_w3 = arg6;

    // Set alpha for the EWMA (bw estimate)
    this->alphaRate = 0.8;
    this->bufferTargetSeconds = 12.0;   // 12s (dash.js implementation) corresponds to 40% with a buffer of 30s

    /// Retrieve available bitrates
    std::vector<IRepresentation* > representations = this->adaptationSet->GetRepresentation();

    this->availableBitrates.clear();
    //L("LINUCB Available Bitrates...\n");
    for(size_t i = 0; i < representations.size(); i++)
    {
        this->availableBitrates.push_back((uint64_t)(representations.at(i)->GetBandwidth()));
        //L("%d  -  %I64u bps\n", i+1, this->availableBitrates[i]);
    }
    // Check if they are in increasing order (i.e., bitrate[0] <= bitrate[1], etc.)

    this->bitrateCount = this->availableBitrates.size();

    // We check if we have only one birate value or if the bitrate list is irregular (i.e., it is not strictly increasing)
    if (this->bitrateCount < 2 || this->availableBitrates[0] >= this->availableBitrates[1] || this->availableBitrates[this->bitrateCount - 2] >= this->availableBitrates[this->bitrateCount - 1]) {
        this->linUcbState = ONE_BITRATE;
        // return 0;   // Check if exit with a message is necessary
    }

    // Check if the following is correct
    this->totalDuration = TimeResolver::GetDurationInSec(this->mpd->GetMediaPresentationDuration());
    //this->segmentDuration = (double) (representations.at(0)->GetSegmentTemplate()->GetDuration() / representations.at(0)->GetSegmentTemplate()->GetTimescale() );
    this->segmentDuration = 2.0;
    //L("Total Duration - LINUCB:\t%f\nSegment Duration - LINUCB:\t%f\n",this->totalDuration, this->segmentDuration);
    // if not correct --> segmentDuration = 2.0;

    // Compute the LINUCB Buffer Target
    this->linUcbBufferTargetSeconds = this->bufferTargetSeconds;
    if (this->linUcbBufferTargetSeconds < this->segmentDuration + MINIMUM_BUFFER_LEVEL_SPACING)
    {
        this->linUcbBufferTargetSeconds = this->segmentDuration + MINIMUM_BUFFER_LEVEL_SPACING;
    }
    //L("LINUCB Buffer Target Seconds:\t%f\n",this->linUcbBufferTargetSeconds);

    // Compute UTILTY vector, Vp, and gp
    //L("LINUCB Utility Values...\n");
    double utility_tmp;
    for (uint32_t i = 0; i < this->bitrateCount; ++i) {
        utility_tmp = log(((double)this->availableBitrates[i] * (1./(double)this->availableBitrates[0])));
        if(utility_tmp < 0)
            utility_tmp = 0;
        this->utilityVector.push_back( log(((double)this->availableBitrates[i] * (1./(double)this->availableBitrates[0]))));
        //L("%d  -  %f\n", i+1, this->utilityVector[i]);
    }

    this->Vp = (this->linUcbBufferTargetSeconds - this->segmentDuration) / this->utilityVector[this->bitrateCount - 1];
    this->gp = 1.0 + this->utilityVector[this->bitrateCount - 1] / (this->linUcbBufferTargetSeconds / this->segmentDuration - 1.0);

    //L("LINUCB Parameters:\tVp:  %f\tgp:  %f\n",this->Vp, this->gp);
    /* If bufferTargetSeconds (not linUcbBufferTargetSecond) is large enough, we might guarantee that LinUcb will never rebuffer
     * unless the network bandwidth drops below the lowest encoded bitrate level. For this to work, LinUcb needs to use the real buffer
     * level without the additional virtualBuffer. Also, for this to work efficiently, we need to make sure that if the buffer level
     * drops to one fragment during a download, the current download does not have more bits remaining than the size of one fragment
     * at the lowest quality*/

    this->maxRtt = 0.2; // Is this reasonable?
    if(this->linUcbBufferTargetSeconds == this->bufferTargetSeconds) {
        this->safetyGuarantee = true;
    }
    if (this->safetyGuarantee) {
        //L("LINUCB SafetyGuarantee...\n");
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

    //L("LINUCB New Parameters:\tVp:  %f\tgp:  %f\n",this->Vp, this->gp);

    // Capping of the virtual buffer (when using it)
    this->linUcbBufferMaxSeconds = this->Vp * (this->utilityVector[this->bitrateCount - 1] + this->gp);
    //L("LINUCB Max Buffer Seconds:\t%f\n",this->linUcbBufferMaxSeconds);

    this->virtualBuffer = 0.0;          // Check if we should use either the virtualBuffer or the safetyGuarantee

    this->instantBw = 0;
    this->averageBw = 0;
    this->batchBw = 0;                  // Computed every THROUGHPUT_SAMPLES samples (average)
    this->batchBwCount = 0;

    this->multimediaManager = NULL;
    this->lastBufferFill = 0;       // (?)
    this->bufferEOS = false;
    this->shouldAbort = false;
    this->isCheckedForReceiver = false;

    this->representation = representations.at(0);
    this->currentBitrate = (uint64_t) this->representation->GetBandwidth();

    this->rebufferStart = 0;
    this->rebufferEnd = 0;

    Py_Initialize();
    PyRun_SimpleString("import sys; sys.path.append('/'); import run_bandits");

    pName = PyUnicode_FromString("run_bandits");
    pModule = PyImport_Import(pName);
    pDict = PyModule_GetDict(pModule);
    pFuncChoose = PyDict_GetItemString(pDict, "choose");
    pFuncUpdate = PyDict_GetItemString(pDict, "update");

    //L("LINUCB Init Params - \tAlpha: %f \t BufferTarget: %f\n",this->alphaRate, this->bufferTargetSeconds);
    //L("LINUCB Init Current BitRate - %I64u\n",this->currentBitrate);
    Debug("Buffer Adaptation LINUCB:    STARTED\n");
}
LinUcbAdaptation::~LinUcbAdaptation ()
{
    Py_DECREF(pModule);
    Py_DECREF(pName);
    Py_Finalize();
}
LogicType LinUcbAdaptation::GetType ()
{
    return adaptation::BufferBased;
}

bool LinUcbAdaptation::IsUserDependent ()
{
    return false;
}

bool LinUcbAdaptation::IsRateBased ()
{
    return true;
}
bool LinUcbAdaptation::IsBufferBased ()
{
    return true;
}

void LinUcbAdaptation::SetMultimediaManager (sampleplayer::managers::IMultimediaManagerBase *_mmManager)
{
    this->multimediaManager = _mmManager;
}

void LinUcbAdaptation::NotifyBitrateChange ()
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
        //L("Aborting! To avoid race condition.");
        //printf("Sending Abort request\n");
        this->multimediaManager->ShouldAbort(this->isVideo);
        //printf("Received Abort request\n");
    }
    this->shouldAbort = false;
}

uint64_t        LinUcbAdaptation::GetBitrate                ()
{
    return this->currentBitrate;
}

int LinUcbAdaptation::getQualityFromThroughput(uint64_t bps) {
    int q = 0;
    for (int i = 0; i < this->availableBitrates.size(); ++i) {
        if (this->availableBitrates[i] > bps) {
            break;
        }
        q = i;
    }
    return q;
}

int LinUcbAdaptation::getQualityFromBufferLevel(double bufferLevelSec) {
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

void            LinUcbAdaptation::SetBitrate                (uint32_t bufferFill)
{
    int result = 0;
    PyObject *pArgs, *pValue;
    std::vector<IRepresentation*> representations = this->adaptationSet->GetRepresentation();
    int numArms = (int)representations.size();
    pValue = PyList_New(0);
    for (int i=0; i<numArms; i++) {
        PyObject* pTemp = PyList_New(0);
        // for bufferFill
        PyList_Append(pTemp, PyFloat_FromDouble((double)bufferFill/(double)100));
        // for RTT
        PyList_Append(pTemp, PyUnicode_FromString("DELIM"));
        std::vector<int> rttVec = this->rttMap[i];
        for (int j=0; j<rttVec.size(); j++) {
            PyList_Append(pTemp, PyFloat_FromDouble((double)rttVec[j]));
        }
        // for num hops
        PyList_Append(pTemp, PyUnicode_FromString("DELIM"));
        std::vector<int> numHopsVec = this->numHopsMap[i];
        for (int j=0; j<numHopsVec.size(); j++) {
            PyList_Append(pTemp, PyFloat_FromDouble((double)numHopsVec[j]));
        }
        PyList_Append(pValue, pTemp);
        Py_XDECREF(pTemp);
    }
    pArgs = PyTuple_New(2);
    PyTuple_SetItem(pArgs, 0, PyUnicode_FromString("linucb"));
    PyTuple_SetItem(pArgs, 1, pValue);

    if (!gil_init) {
        gil_init = 1;
        PyEval_InitThreads();
        PyEval_SaveThread();
    }

    // 2: Execute
    PyGILState_STATE state = PyGILState_Ensure();
    unsigned long long int execStart = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    pValue = PyObject_CallObject(pFuncChoose, pArgs);
    unsigned long long int execEnd = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    result = (int)PyLong_AsLong(pValue);
    Py_XDECREF(pArgs);
    Py_XDECREF(pValue);
    PyGILState_Release(state);


    int duration = (int)(execEnd - execStart);
    std::cout << "stat-begin" << std::endl;
    std::cout << "stat-bandit-normal" << std::endl;
    std::cout << "stat-chexecms " << duration << std::endl;


    std::cout << "stat-chqual " << result << std::endl;
    if (result < 0 || result > numArms-1) {
        std::cout << "ERROR: Result was out of bounds. Using quality of 2." << std::endl;
        result = 2;
    }

    this->currentQuality = result;
    this->representation = this->adaptationSet->GetRepresentation().at(this->currentQuality);
    this->currentBitrate = (uint64_t) this->availableBitrates[this->currentQuality];
    //L("STEADY - Current Bitrate:\t%I64u\n", this->currentBitrate);
    //L("ADAPTATION_LOGIC:\tFor %s:\tlast_buffer: %f\tbuffer_level: %f, instantaneousBw: %lu, AverageBW: %lu, choice: %d\n",isVideo ? "video" : "audio",(double)lastBufferFill/100 , (double)bufferFill/100, this->instantBw, this->averageBw , this->currentQuality);
    this->lastBufferFill = bufferFill;
}

void            LinUcbAdaptation::BitrateUpdate(uint64_t bps)
{
}

void            LinUcbAdaptation::SegmentPositionUpdate         (uint32_t qualitybps)
{
    //L("Called SegmentPositionUpdate\n");
}

void            LinUcbAdaptation::DLTimeUpdate      (double time)
{
    auto m_now = std::chrono::system_clock::now();
    auto m_now_sec = std::chrono::time_point_cast<std::chrono::seconds>(m_now);

    auto now_value = m_now_sec.time_since_epoch();
    double dl_instant = now_value.count();
    //this->lastSegmentDownloadTime = time;
    //this->currentDownloadTimeInstant = std::chrono::duration_cast<duration_in_seconds>(system_clock::now()).count();
    this->currentDownloadTimeInstant = dl_instant;
}

void            LinUcbAdaptation::OnEOS             (bool value)
{
    this->bufferEOS = value;
}

void            LinUcbAdaptation::OnSegmentDownloaded (double bufferFillPct, int segNum, int quality, std::vector<int> rttVec, std::vector<int> numHopsVec)
{

    std::vector<IRepresentation*> representations = this->adaptationSet->GetRepresentation();
    int numArms = (int)representations.size();
    uint32_t bufferFill = (uint32_t)(bufferFillPct * (double)100);
    std::cout << "stat-qual " << quality << std::endl;
    //L("stat-buffpct %f\n", bufferFillPct);

    ////////////////
    // Update MAB //
    ////////////////
    // First calculate the real QoE for the segment:
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

    // If we're still in the first K iterations, we have to try all arms. Try the one
    // corresponding to this segment
    // TODO would be better to put in SetBitrate, but that'd take a bit of refactoring
	/*
    if (segNum < numArms) {
        //L("stat-initarms %d\n", segNum);
        std::cout << "stat-bandit-init" << std::endl;
        this->currentQuality = segNum;
        this->representation = this->adaptationSet->GetRepresentation().at(this->currentQuality);
        this->currentBitrate = (uint64_t) this->availableBitrates[this->currentQuality];
        this->lastBufferFill = bufferFill;
        //L("STEADY - Current Bitrate:\t%I64u\n", this->currentBitrate);
        this->qualityHistory[segNum] = quality;
        this->rttMap[quality] = rttVec;
        this->ecnMap[quality] = ecnVec;
        this->numHopsMap[quality] = numHopsVec;
        this->NotifyBitrateChange();
    }
    else {
	*/
	// 1: Build args
	int result = 0;
	PyObject *pArgs, *pValue;
	pValue = PyList_New(0);
	for (int i=0; i<numArms; i++) {
		PyObject* pTemp = PyList_New(0);
		PyList_Append(pTemp, PyFloat_FromDouble(bufferFillPct));
		// for RTT
		PyList_Append(pTemp, PyUnicode_FromString("DELIM"));
		std::vector<int> tempRttVec = this->rttMap[i];
		for (int j=0; j<tempRttVec.size(); j++) {
			PyList_Append(pTemp, PyFloat_FromDouble((double)tempRttVec[j]));
		}
		/*
		// for ECN
		PyList_Append(pTemp, PyUnicode_FromString("DELIM"));
		std::vector<int> tempEcnVec = this->ecnMap[i];
		for (int j=0; j<tempEcnVec.size(); j++) {
			PyList_Append(pTemp, PyFloat_FromDouble((double)tempEcnVec[j]));
		}
		*/
		// for num hops
		PyList_Append(pTemp, PyUnicode_FromString("DELIM"));
		std::vector<int> tempNumHopsVec = this->numHopsMap[i];
		for (int j=0; j<tempNumHopsVec.size(); j++) {
			PyList_Append(pTemp, PyFloat_FromDouble((double)tempNumHopsVec[j]));
		}
		PyList_Append(pValue, pTemp);
		Py_XDECREF(pTemp);
	}
	pArgs = PyTuple_New(5);
	PyTuple_SetItem(pArgs, 0, PyUnicode_FromString("linucb"));
	// context
	PyTuple_SetItem(pArgs, 1, pValue);
	// last quality (action)
	PyTuple_SetItem(pArgs, 2, PyLong_FromLong((long)quality));
	// reward
	PyTuple_SetItem(pArgs, 3, PyFloat_FromDouble(reward));
	// timestep
	PyTuple_SetItem(pArgs, 4, PyLong_FromLong((long)segNum));

	if (!gil_init) {
		std::cout << "WARNING: setting gil_init in BitrateUpdate" << std::endl;
		gil_init = 1;
		PyEval_InitThreads();
		PyEval_SaveThread();
	}

	// 2: Execute
	PyGILState_STATE state = PyGILState_Ensure();
	unsigned long long int execStart = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
	pValue = PyObject_CallObject(pFuncUpdate, pArgs);
	unsigned long long int execEnd = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
	result = (int)PyLong_AsLong(pValue);
	Py_XDECREF(pArgs);
	Py_XDECREF(pValue);
	PyGILState_Release(state);

	int duration = (int)(execEnd - execStart);
	std::cout << "stat-updexecms " << duration << std::endl;

	if (result != 1) {
		std::cout << "WARNING: Update MAB failed" << std::endl;
	}
	
	/////////////////////////////
	// Update internal context //
	/////////////////////////////
	this->rttMap[quality] = rttVec;
	//this->ecnMap[quality] = ecnVec;
	this->numHopsMap[quality] = numHopsVec;

	// Run MAB using the new context
	this->SetBitrate(bufferFill);
	this->NotifyBitrateChange();
    //} <--- the commented out else statement
}

void            LinUcbAdaptation::CheckedByDASHReceiver ()
{
    //L("CheckedByDASHReceiver called\n");
    this->isCheckedForReceiver = false;
}
void            LinUcbAdaptation::BufferUpdate          (uint32_t bufferFill)
{
    if (bufferFill == (uint32_t)0 && this->rebufferStart == 0) {
        this->rebufferStart = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        this->rebufferEnd = 0;
        //L("Buffer is at 0; rebuffering has begun and playback has stalled\n");
    }
    else if (this->rebufferEnd == 0 && this->rebufferStart != 0) {
        this->rebufferEnd = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        //L("Stopped rebuffering; resuming playback\n");
    }
}
