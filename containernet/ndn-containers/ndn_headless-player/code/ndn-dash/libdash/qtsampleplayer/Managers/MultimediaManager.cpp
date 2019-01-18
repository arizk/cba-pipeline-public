/*
 * MultimediaManager.cpp
 *****************************************************************************
 * Copyright (C) 2013, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *
 * Modifications:
 *
 * Copyright (C) 2016
 *
 * Jacques Samain <jsamain@cisco.com>
 *
 *****************************************************************************/

#include "MultimediaManager.h"

#include<fstream>
#include<iostream>

using namespace libdash::framework::adaptation;
using namespace libdash::framework::buffer;
using namespace sampleplayer::managers;
using namespace sampleplayer::renderer;
using namespace dash::mpd;

#define SEGMENTBUFFER_SIZE 10       //Buffersize changed HERE?

MultimediaManager::MultimediaManager            (QTGLRenderer *videoElement, QTAudioRenderer *audioElement, bool nodecoding) :
                   videoElement                 (videoElement),
                   audioElement                 (audioElement),
                   mpd                          (NULL),
                   period                       (NULL),
                   videoAdaptationSet           (NULL),
                   videoRepresentation          (NULL),
                   videoLogic                   (NULL),
                   videoStream                  (NULL),
                   audioAdaptationSet           (NULL),
                   audioRepresentation          (NULL),
                   audioLogic                   (NULL),
				   videoRendererHandle			(NULL),
				   audioRendererHandle			(NULL),
				   audioStream                  (NULL),
                   isStarted                    (false),
				   isStopping					(false),
                   framesDisplayed              (0),
                   segmentsDownloaded           (0),
                   isVideoRendering             (false),
                   isAudioRendering             (false),
				   eos							(false),
				   playing						(false),
                   noDecoding					(nodecoding),
                   localmpdset                  (false),
                   localmpdpath                 (NULL)
{
    InitializeCriticalSection (&this->monitorMutex);
    InitializeCriticalSection (&this->monitor_playing_video_mutex);
    InitializeConditionVariable (&this->playingVideoStatusChanged);
    InitializeCriticalSection (&this->monitor_playing_audio_mutex);
    InitializeConditionVariable (&this->playingAudioStatusChanged);

    this->manager = CreateDashManager();
   // av_register_all();
}
MultimediaManager::~MultimediaManager           ()
{
    this->Stop();
    this->manager->Delete();

    DeleteCriticalSection (&this->monitorMutex);
    DeleteCriticalSection (&this->monitor_playing_video_mutex);
    DeleteConditionVariable (&this->playingVideoStatusChanged);
    DeleteCriticalSection (&this->monitor_playing_audio_mutex);
    DeleteConditionVariable (&this->playingAudioStatusChanged);
}

IMPD*   MultimediaManager::GetMPD                           ()
{
    return this->mpd;
}
bool    MultimediaManager::Init                             (const std::string& url)
{
    EnterCriticalSection(&this->monitorMutex);

    this->mpd = this->manager->Open((char *)url.c_str());

    if(this->mpd == NULL)
    {
        LeaveCriticalSection(&this->monitorMutex);
        return false;
    }

    LeaveCriticalSection(&this->monitorMutex);
    return true;
}

void    MultimediaManager::SetLocalMPD                      (char* path)
{
    localmpdpath = path;
    localmpdset = true;
}

 bool    MultimediaManager::InitNDN                          (const std::string& url)
{
    EnterCriticalSection(&this->monitorMutex);

#ifdef NDNICPDOWNLOAD
    libdash::framework::input::INDNConnection* ndnConn = new libdash::framework::input::NDNConnection(20, NULL);
#else
    libdash::framework::input::INDNConnection* ndnConn = new libdash::framework::input::NDNConnectionConsumerApi(20);
#endif
    
    ndnConn->InitForMPD(url);
    int ret = 0;
    char * data = (char *)malloc(4096);
    int pos = url.find_last_of("/");
    if(pos == std::string::npos)
    {
    	pos = strlen(url.c_str());
    }
    else
    {
    	pos = pos + 1;
    }
    char *name = (char*)malloc(2 + pos);
    0[name] = '.';
    1[name] = '/';
    strncpy(name +2, url.substr(pos).c_str(), pos);

    FILE *fp;
    fp = fopen(name, "w");
    if(fp == NULL)
    {
    	free(data);
    	free(name);
    	delete ndnConn;
    	LeaveCriticalSection(&this->monitorMutex);
    	return false;
    }

    ret = ndnConn->Read((uint8_t*)data, 4096);
    while(ret)
    {
    	fwrite(data, sizeof(char), ret, fp);
    	ret = ndnConn->Read((uint8_t*)data,4096);
    }
    fclose(fp);


    if(localmpdset)
    {
        printf("Start the extended MPD parsing.\n");
        this->mpd = this->manager->Open(localmpdpath);
        printf("Extended MPD is ready.\n");
    } else
        this->mpd = this->manager->Open(name);

    if(this->mpd == NULL)
    {
    	remove(name);
    	free(name);
    	free(data);
    	delete ndnConn;
        LeaveCriticalSection(&this->monitorMutex);
        return false;
    }
	remove(name);
	free(name);
	free(data);
	delete ndnConn;
    LeaveCriticalSection(&this->monitorMutex);
    return true;
}

bool	MultimediaManager::IsStarted						()
{
	return this->isStarted;
}
bool 	MultimediaManager::IsStopping						()
{
	return this->isStopping;
}
bool	MultimediaManager::IsNDN							()
{
	return this->isNDN;
}
void    MultimediaManager::Start                            (bool ndnEnabled, double ndnAlpha, bool chunkGranularityEnabled, int sample)
{
	this->isNDN = ndnEnabled;
    this->ndnAlpha = ndnAlpha;
    this->chunkGranularity = chunkGranularityEnabled;
    this->samplesize = sample;
    /* Global Start button for start must be added to interface*/
    if (this->isStarted)
        this->Stop();

    L("Starting MultimediaManager...\n");
    L("Chunk Granularity Enabled:\t%s\twith sample size\t%d\n", chunkGranularity ? "true" : "false", samplesize);
    L("Is_NDN:\t%s\n", isNDN ? "true" : "false");
    if(ndnAlpha <= 1 && ndnAlpha >=0)
    	L("ICN-enhanced rate estimation: alpha = %f\n",ndnAlpha);
    else
    	L("normal rate estimation\n");

    L("Segment_Buffer_size:\t%d\n", SEGMENTBUFFER_SIZE);
    L("Adaptation_rate:\t%s\n", this->logicName);
    EnterCriticalSection(&this->monitorMutex);

    if (this->videoAdaptationSet && this->videoRepresentation)
    {
        this->InitVideoRendering(0);
        this->videoStream->SetAdaptationLogic(this->videoLogic);
        this->videoLogic->SetMultimediaManager(this);
        this->videoStream->Start();
        if(this->videoElement)
        	this->StartVideoRenderingThread();
    }

    if (this->audioAdaptationSet && this->audioRepresentation)
    {
        this->InitAudioPlayback(0);
        if(this->audioElement)
        	this->audioElement->StartPlayback();
        this->audioStream->SetAdaptationLogic(this->audioLogic);
        this->audioLogic->SetMultimediaManager(this);
        this->audioStream->Start();
        if(this->audioElement)
        	this->StartAudioRenderingThread();

    }

    this->isStarted = true;
    this->playing = true;
    LeaveCriticalSection(&this->monitorMutex);
}
void    MultimediaManager::Stop                             ()
{
    if (!this->isStarted)
        return;

    this->isStopping = true;
    EnterCriticalSection(&this->monitorMutex);

    this->StopVideo();
    this->StopAudio();

    this->isStopping = false;
    this->isStarted = false;

    LeaveCriticalSection(&this->monitorMutex);
    L("VIDEO STOPPED\n");
    FlushLog();
}
void    MultimediaManager::StopVideo                        ()
{
    if(this->isStarted && this->videoStream)
    {
        this->videoStream->Stop();
        this->StopVideoRenderingThread();

        delete this->videoStream;
        delete this->videoLogic;

        this->videoStream = NULL;
        this->videoLogic = NULL;
    }
}
void    MultimediaManager::StopAudio                        ()
{
    if (this->isStarted && this->audioStream)
    {
        this->audioStream->Stop();
        this->StopAudioRenderingThread();

        if(this->audioElement)
        	this->audioElement->StopPlayback();

        delete this->audioStream;
        delete this->audioLogic;

        this->audioStream = NULL;
        this->audioLogic = NULL;
    }
}
bool    MultimediaManager::SetVideoQuality                  (IPeriod* period, IAdaptationSet *adaptationSet, IRepresentation *representation)
{
    EnterCriticalSection(&this->monitorMutex);

    this->period                = period;
    this->videoAdaptationSet    = adaptationSet;
    this->videoRepresentation   = representation;

    if (this->videoStream)
        this->videoStream->SetRepresentation(this->period, this->videoAdaptationSet, this->videoRepresentation);

    LeaveCriticalSection(&this->monitorMutex);
    return true;
}
bool    MultimediaManager::SetAudioQuality                  (IPeriod* period, IAdaptationSet *adaptationSet, IRepresentation *representation)
{
    EnterCriticalSection(&this->monitorMutex);

    this->period                = period;
    this->audioAdaptationSet    = adaptationSet;
    this->audioRepresentation   = representation;

    if (this->audioStream)
        this->audioStream->SetRepresentation(this->period, this->audioAdaptationSet, this->audioRepresentation);

    LeaveCriticalSection(&this->monitorMutex);
    return true;
}
bool	MultimediaManager::IsUserDependent					()
{
	if(this->videoLogic)
		return this->videoLogic->IsUserDependent();
	else
		return true;
}
bool    MultimediaManager::SetVideoAdaptationLogic          (libdash::framework::adaptation::LogicType type, double arg1, double arg2, double arg3, 
                                                                                                             double arg4, double arg5, double arg6)
{
	if(this->videoAdaptationSet)
	{
		this->videoLogic = AdaptationLogicFactory::Create(type, this->mpd, this->period, this->videoAdaptationSet, 1, arg1, arg2, arg3, arg4, arg5, arg6);
		this->logicName = LogicType_string[type];
	}
	else
	{
		this->videoLogic = NULL;
		return true;
	}
	if(this->videoLogic)
		return true;
	else
		return false;
}
void 	MultimediaManager::ShouldAbort							(bool isVideo)
{
	if(isVideo)
	{
		this->videoStream->ShouldAbort();
		return;
	}
	else
	{
		this->audioStream->ShouldAbort();
	}
}

void	MultimediaManager::SetTargetDownloadingTime			(bool isVideo, double target)
{
	if(isVideo)
		this->videoStream->SetTargetDownloadingTime(target);
	else
		this->audioStream->SetTargetDownloadingTime(target);
}

bool    MultimediaManager::SetAudioAdaptationLogic          (libdash::framework::adaptation::LogicType type, double arg1, double arg2, double arg3,
                                                                                                             double arg4, double arg5, double arg6)
{
	if(this->audioAdaptationSet)
	{
		this->audioLogic = AdaptationLogicFactory::Create(type, this->mpd, this->period, this->audioAdaptationSet, 0, arg1, arg2, arg3);
		this->logicName = LogicType_string[type];
	}
	else
	{
		this->audioLogic = NULL;
		return true;
	}
	if(this->audioLogic)
		return true;
	else
		return false;
}
void    MultimediaManager::AttachManagerObserver            (IMultimediaManagerObserver *observer)
{
    this->managerObservers.push_back(observer);
}
void    MultimediaManager::NotifyVideoBufferObservers       (uint32_t fillstateInPercent)
{
    for (size_t i = 0; i < this->managerObservers.size(); i++)
        this->managerObservers.at(i)->OnVideoBufferStateChanged(fillstateInPercent);
}
void    MultimediaManager::NotifyVideoSegmentBufferObservers(uint32_t fillstateInPercent)
{
    for (size_t i = 0; i < this->managerObservers.size(); i++)
        this->managerObservers.at(i)->OnVideoSegmentBufferStateChanged(fillstateInPercent);
}
void    MultimediaManager::NotifyAudioSegmentBufferObservers(uint32_t fillstateInPercent)
{
    for (size_t i = 0; i < this->managerObservers.size(); i++)
        this->managerObservers.at(i)->OnAudioSegmentBufferStateChanged(fillstateInPercent);
}
void    MultimediaManager::NotifyAudioBufferObservers       (uint32_t fillstateInPercent)
{
    for (size_t i = 0; i < this->managerObservers.size(); i++)
        this->managerObservers.at(i)->OnAudioBufferStateChanged(fillstateInPercent);
}
void    MultimediaManager::InitVideoRendering               (uint32_t offset)
{
//    this->videoLogic = AdaptationLogicFactory::Create(libdash::framework::adaptation::Manual, this->mpd, this->period, this->videoAdaptationSet);

    this->videoStream = new MultimediaStream(sampleplayer::managers::VIDEO, this->mpd, SEGMENTBUFFER_SIZE, 20, 0, this->IsNDN(), this->ndnAlpha, this->noDecoding, this->chunkGranularity, this->samplesize);
    this->videoStream->AttachStreamObserver(this);
    this->videoStream->SetRepresentation(this->period, this->videoAdaptationSet, this->videoRepresentation);
    this->videoStream->SetPosition(offset);
}
void    MultimediaManager::InitAudioPlayback                (uint32_t offset)
{
 //   this->audioLogic = AdaptationLogicFactory::Create(libdash::framework::adaptation::Manual, this->mpd, this->period, this->audioAdaptationSet);

    this->audioStream = new MultimediaStream(sampleplayer::managers::AUDIO, this->mpd, SEGMENTBUFFER_SIZE, 0, 10, this->IsNDN(), this->ndnAlpha, this->noDecoding, this->chunkGranularity, this->samplesize);
    this->audioStream->AttachStreamObserver(this);
    this->audioStream->SetRepresentation(this->period, this->audioAdaptationSet, this->audioRepresentation);
    this->audioStream->SetPosition(offset);
}
void    MultimediaManager::OnSegmentDownloaded              ()
{
    this->segmentsDownloaded++;
}
void    MultimediaManager::OnSegmentBufferStateChanged    (StreamType type, uint32_t fillstateInPercent)
{
    switch (type)
    {
        case AUDIO:
            this->NotifyAudioSegmentBufferObservers(fillstateInPercent);
            break;
        case VIDEO:
            this->NotifyVideoSegmentBufferObservers(fillstateInPercent);
            break;
        default:
            break;
    }
}
void    MultimediaManager::OnVideoBufferStateChanged      (uint32_t fillstateInPercent)
{
    this->NotifyVideoBufferObservers(fillstateInPercent);
}
void    MultimediaManager::OnAudioBufferStateChanged      (uint32_t fillstateInPercent)
{
    this->NotifyAudioBufferObservers(fillstateInPercent);
}
void    MultimediaManager::SetFrameRate                     (double framerate)
{
    this->frameRate = framerate;
}

void 	MultimediaManager::SetEOS							(bool value)
{
	this->eos = value;
	if(value) //ie: End of Stream so the rendering thread(s) will finish
	{
		this->isStopping = true;
		if(this->videoRendererHandle != NULL)
		{
			JoinThread(this->videoRendererHandle);
			DestroyThreadPortable(this->videoRendererHandle);
			this->videoRendererHandle = NULL;
		}
		if(this->audioRendererHandle != NULL)
		{
			JoinThread(this->audioRendererHandle);
			DestroyThreadPortable(this->audioRendererHandle);
			this->audioRendererHandle = NULL;
		}
		this->isStopping = false;
		for(size_t i = 0; i < this->managerObservers.size(); i++)
			this->managerObservers.at(0)->OnEOS();
	}
}

bool    MultimediaManager::StartVideoRenderingThread        ()
{
    this->isVideoRendering = true;

    this->videoRendererHandle = CreateThreadPortable (RenderVideo, this);

    if(this->videoRendererHandle == NULL)
        return false;

    return true;
}
void    MultimediaManager::StopVideoRenderingThread         ()
{
    this->isVideoRendering = false;

    if (this->videoRendererHandle != NULL)
    {
        JoinThread(this->videoRendererHandle);
        DestroyThreadPortable(this->videoRendererHandle);
    }
}
bool    MultimediaManager::StartAudioRenderingThread        ()
{
    this->isAudioRendering = true;

    this->audioRendererHandle = CreateThreadPortable (RenderAudio, this);

    if(this->audioRendererHandle == NULL)
        return false;

    return true;
}
void    MultimediaManager::StopAudioRenderingThread         ()
{
    this->isAudioRendering = false;

    if (this->audioRendererHandle != NULL)
    {
        JoinThread(this->audioRendererHandle);
        DestroyThreadPortable(this->audioRendererHandle);
    }
}
void*   MultimediaManager::RenderVideo        (void *data)
{
    MultimediaManager *manager = (MultimediaManager*) data;
    QImage *frame = manager->videoStream->GetFrame();

    while(manager->isVideoRendering)
    {
    	EnterCriticalSection(&manager->monitor_playing_video_mutex);
    	while(manager->isPlaying() == false)
    		SleepConditionVariableCS(&manager->playingVideoStatusChanged, &manager->monitor_playing_video_mutex, INFINITE);
    	LeaveCriticalSection(&manager->monitor_playing_video_mutex);

        if (frame)
        {
        	if(manager->videoElement)
        	{
        		manager->videoElement->SetImage(frame);
        		manager->videoElement->update();
        	}
            manager->framesDisplayed++;
            PortableSleep(1 / manager->frameRate);
            delete(frame);
        }
        else
        {
        	if(manager->eos)
        		break;
        }
        frame = manager->videoStream->GetFrame();
    }

    return NULL;
}
void*   MultimediaManager::RenderAudio        (void *data)
{
	MultimediaManager *manager = (MultimediaManager*) data;

    AudioChunk *samples = manager->audioStream->GetSamples();

    while(manager->isAudioRendering)
    {
    	EnterCriticalSection(&manager->monitor_playing_audio_mutex);
    	while(manager->isPlaying() == false)
    		SleepConditionVariableCS(&manager->playingAudioStatusChanged, &manager->monitor_playing_audio_mutex, INFINITE);
    	LeaveCriticalSection(&manager->monitor_playing_audio_mutex);
        if (samples)
        {
        	if(manager->audioElement)
        	{
        		manager->audioElement->WriteToBuffer(samples->Data(), samples->Length());
        	}

            PortableSleep(1 / (double)48000);
            delete samples;
        }
        else
        	if(manager->eos)
        		break;

        samples = manager->audioStream->GetSamples();
    }

    return NULL;
}
bool	MultimediaManager::isPlaying		()
{
	return this->playing;
}

void	MultimediaManager::OnPausePressed	()
{
	EnterCriticalSection(&this->monitor_playing_video_mutex);
	EnterCriticalSection(&this->monitor_playing_audio_mutex);

	this->playing = !this->playing;
	WakeAllConditionVariable(&this->playingVideoStatusChanged);
	WakeAllConditionVariable(&this->playingAudioStatusChanged);

	LeaveCriticalSection(&this->monitor_playing_video_mutex);
	LeaveCriticalSection(&this->monitor_playing_audio_mutex);
}

void	MultimediaManager::OnFastForward	()
{
	if(this->videoStream)
	{
		this->videoStream->FastForward();
	}
	if(this->audioStream)
	{
		this->audioStream->FastForward();
	}
}

void	MultimediaManager::OnFastRewind	()
{
	if(this->videoStream)
	{
		this->videoStream->FastRewind();
	}
	if(this->audioStream)
	{
		this->audioStream->FastRewind();
	}
}
