/*
 * DASHPlayer.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "DASHPlayer.h"
#include <iostream>

using namespace libdash::framework::adaptation;
using namespace libdash::framework::mpd;
using namespace libdash::framework::buffer;
using namespace sampleplayer;
using namespace sampleplayer::renderer;
using namespace sampleplayer::managers;
using namespace dash::mpd;
using namespace std;

DASHPlayer::DASHPlayer  (QtSamplePlayerGui &gui,int argc, char ** argv) :
            gui         (&gui)
{
    InitializeCriticalSection(&this->monitorMutex);

    this->url 				= NULL;
	this->isNDN				= false;
	this->adaptLogic = LogicType::RateBased;
//    this->parseArgs(argc, argv);
	if(argc == 2)
	{
		if(strstr(argv[1],"ndn:/"))
			this->isNDN = true;
		const char* startUrl = this->isNDN ? strstr(argv[1], "ndn:/") : strstr(argv[1], "http://");
		this->url = startUrl;
		const char* pos = strstr(this->url,"GNOME_KEYRING");
		if(pos)
			strncpy(pos, "\0", 1);
	}
    this->SetSettings(0, 0, 0, 0, 0);
    this->videoElement      = gui.GetVideoElement();
    this->audioElement      = new QTAudioRenderer(&gui);
    this->multimediaManager = new MultimediaManager(this->videoElement, this->audioElement);
    this->multimediaManager->SetFrameRate(25);
    this->multimediaManager->AttachManagerObserver(this);
    this->gui->AddWidgetObserver(this);

    QObject::connect(this, SIGNAL(VideoSegmentBufferFillStateChanged(int)), &gui, SLOT(SetVideoSegmentBufferFillState(int)));
    QObject::connect(this, SIGNAL(VideoBufferFillStateChanged(int)), &gui, SLOT(SetVideoBufferFillState(int)));
    QObject::connect(this, SIGNAL(AudioSegmentBufferFillStateChanged(int)), &gui, SLOT(SetAudioSegmentBufferFillState(int)));
    QObject::connect(this, SIGNAL(AudioBufferFillStateChanged(int)), &gui, SLOT(SetAudioBufferFillState(int)));

    bool shouldStart = false;

    if(this->isNDN)
    {
    	this->gui->SetNDNStatus(this->isNDN);
    }
    if(this->url != NULL)
    {
    	shouldStart = true;
    	this->gui->SetUrl(this->url);
    }
    this->gui->SetAdaptationLogic(this->adaptLogic);

    if(shouldStart)
    {
    	if(this->OnDownloadMPDPressed(string(this->url)))
    			{
    				this->gui->LockStartAndDownloadButton();
    				this->OnStartButtonPressed(0,0,0,0,0);
    				this->videoElement->grabKeyboard();
    			}
    }
}
DASHPlayer::~DASHPlayer ()
{
    this->multimediaManager->Stop();
    this->audioElement->StopPlayback();
    this->audioElement = NULL;

    delete(this->multimediaManager);
    delete(this->audioElement);

    DeleteCriticalSection(&this->monitorMutex);
}

void DASHPlayer::OnStartButtonPressed               (int period, int videoAdaptationSet, int videoRepresentation, int audioAdaptationSet, int audioRepresentation)
{
	   if(!((this->multimediaManager->SetVideoAdaptationLogic((LogicType)this->gui->GetAdaptationLogic(), 0, 0, 0) && (this->multimediaManager->SetAudioAdaptationLogic((LogicType)this->gui->GetAdaptationLogic(), 0, 0, 0)))))
	   {
	    	this->gui->SetStatusBar("Error setting Video/Audio adaptation logic...");
	    	return;
	    }

	   if(!this->multimediaManager->IsUserDependent())
	   {
		   this->gui->DisableUserActions();
	   }
	   this->gui->isRunning = true;
	   L("DASH PLAYER:	STARTING VIDEO\n");
	   this->multimediaManager->Start(this->gui->GetNDNStatus(), 20, false, 100);	//only segment-based adaptation when gui, therefore also samplesize not important
}
void DASHPlayer::OnStopButtonPressed                ()
{
	this->gui->EnableUserActions();
    this->multimediaManager->Stop();
}
void DASHPlayer::OnPauseButtonPressed				()
{
	this->multimediaManager->OnPausePressed();
}
void DASHPlayer::OnFastForward						()
{
	this->multimediaManager->OnFastForward();
}
void DASHPlayer::OnFastRewind						()
{
	this->multimediaManager->OnFastRewind();
}
void DASHPlayer::OnSettingsChanged                  (int period, int videoAdaptationSet, int videoRepresentation, int audioAdaptationSet, int audioRepresentation)
{
    if(this->multimediaManager->GetMPD() == NULL)
        return; // TODO dialog or symbol that indicates that error

    if (!this->SettingsChanged(period, videoAdaptationSet, videoRepresentation, audioAdaptationSet, audioRepresentation))
        return;

    IPeriod                         *currentPeriod      = this->multimediaManager->GetMPD()->GetPeriods().at(period);
    std::vector<IAdaptationSet *>   videoAdaptationSets = AdaptationSetHelper::GetVideoAdaptationSets(currentPeriod);
    std::vector<IAdaptationSet *>   audioAdaptationSets = AdaptationSetHelper::GetAudioAdaptationSets(currentPeriod);

    if (videoAdaptationSet >= 0 && videoRepresentation >= 0)
    {
        this->multimediaManager->SetVideoQuality(currentPeriod,
                                                 videoAdaptationSets.at(videoAdaptationSet),
                                                 videoAdaptationSets.at(videoAdaptationSet)->GetRepresentation().at(videoRepresentation));
    }
    else
    {
        this->multimediaManager->SetVideoQuality(currentPeriod, NULL, NULL);
    }

    if (audioAdaptationSet >= 0 && audioRepresentation >= 0)
    {
        this->multimediaManager->SetAudioQuality(currentPeriod,
                                                 audioAdaptationSets.at(audioAdaptationSet),
                                                 audioAdaptationSets.at(audioAdaptationSet)->GetRepresentation().at(audioRepresentation));
    }
    else
    {
        this->multimediaManager->SetAudioQuality(currentPeriod, NULL, NULL);
    }

}
void DASHPlayer::OnVideoBufferStateChanged          (uint32_t fillstateInPercent)
{
    emit VideoBufferFillStateChanged(fillstateInPercent);
}
void DASHPlayer::OnVideoSegmentBufferStateChanged   (uint32_t fillstateInPercent)
{
    emit VideoSegmentBufferFillStateChanged(fillstateInPercent);
}
void DASHPlayer::OnAudioBufferStateChanged          (uint32_t fillstateInPercent)
{
    emit AudioBufferFillStateChanged(fillstateInPercent);
}
void DASHPlayer::OnAudioSegmentBufferStateChanged   (uint32_t fillstateInPercent)
{
    emit AudioSegmentBufferFillStateChanged(fillstateInPercent);
}
void DASHPlayer::OnEOS								()
{
	this->OnStopButtonPressed();
}
bool DASHPlayer::OnDownloadMPDPressed               (const std::string &url)
{
	if(this->gui->GetNDNStatus())
	{
		if(!this->multimediaManager->InitNDN(url))
		{
			this->gui->SetStatusBar("Error parsing mpd at: " + url);
			return false;
		}
	}
	else
	{
		if(!this->multimediaManager->Init(url))
		{
			this->gui->SetStatusBar("Error parsing mpd at: " + url);
			return false; // TODO dialog or symbol that indicates that error
		}
	}
	this->SetSettings(-1, -1, -1, -1, -1);
	this->gui->SetGuiFields(this->multimediaManager->GetMPD());

    this->gui->SetStatusBar("Successfully parsed MPD at: " + url);
    return true;
}
bool DASHPlayer::SettingsChanged                    (int period, int videoAdaptationSet, int videoRepresentation, int audioAdaptationSet, int audioRepresentation)
{
    EnterCriticalSection(&this->monitorMutex);

    bool settingsChanged = false;

    if (this->currentSettings.videoRepresentation != videoRepresentation ||
        this->currentSettings.audioRepresentation != audioRepresentation ||
        this->currentSettings.videoAdaptationSet != videoAdaptationSet ||
        this->currentSettings.audioAdaptationSet != audioAdaptationSet ||
        this->currentSettings.period != period)
        settingsChanged = true;

    if (settingsChanged)
        this->SetSettings(period, videoAdaptationSet, videoRepresentation, audioAdaptationSet, audioRepresentation);

    LeaveCriticalSection(&this->monitorMutex);

    return settingsChanged;
}
void DASHPlayer::SetSettings                        (int period, int videoAdaptationSet, int videoRepresentation, int audioAdaptationSet, int audioRepresentation)
{
    this->currentSettings.period                = period;
    this->currentSettings.videoAdaptationSet    = videoAdaptationSet;
    this->currentSettings.videoRepresentation   = videoRepresentation;
    this->currentSettings.audioAdaptationSet    = audioAdaptationSet;
    this->currentSettings.audioRepresentation   = audioRepresentation;
}

void DASHPlayer::parseArgs(int argc, char ** argv)
{
	if(argc == 1)
		{
			return;
		}
		else
		{
			int i = 0;
			while(i < argc)
			{
				if(!strcmp(argv[i],"-u"))
				{
					this->url = argv[i+1];
					i++;
					goto end;
				}
				if(!strcmp(argv[i],"-n"))
				{
					this->isNDN = true;
					goto end;
				}
				if(!strcmp(argv[i],"-a"))
				{
					int j =0;
					for(j = 0; j < LogicType::Count; j++)
					{
						if(!strcmp(LogicType_string[j],argv[i+1]))
						{
							this->adaptLogic = (LogicType)j;
							break;
						}
					}
					if(j == LogicType::Count)
					{
						printf("the different adaptation logics implemented are:\n");
						for(j = 0;j < LogicType::Count; j++)
						{
							printf("*%s\n",LogicType_string[j]);
						}
						printf("By default, the %s logic is selected.\n", LogicType_string[this->adaptLogic]);
					}
					i++;
					goto end;
				}
		end:	i++;
			}
		}
}
