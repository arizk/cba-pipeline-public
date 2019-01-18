/*
 * MultimediaStream.cpp
 *****************************************************************************
 * Copyright (C) 2013, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "MultimediaStream.h"

using namespace sampleplayer::managers;
using namespace sampleplayer::decoder;
using namespace libdash::framework::adaptation;
using namespace libdash::framework::input;
using namespace libdash::framework::buffer;
using namespace dash::mpd;

MultimediaStream::MultimediaStream  (StreamType type, IMPD *mpd, uint32_t bufferSize, uint32_t frameBufferSize, uint32_t sampleBufferSize, bool ndnEnabled, double ndnAlpha, bool nodecoding, bool chunkGranularityEnabled, int sample) :
                		  type              (type),
						  segmentBufferSize (bufferSize),
						  frameBufferSize   (frameBufferSize),
						  sampleBufferSize  (sampleBufferSize),
						  dashManager       (NULL),
						  mpd               (mpd),
						  isNDN				(ndnEnabled),
						  ndnAlpha			(ndnAlpha),
						  noDecoding		(nodecoding),
						  chunkGranularity	(chunkGranularityEnabled),
						  samplesize		(sample)
{
	this->frameBuffer = NULL;
	this->sampleBuffer = NULL;
	this->Init();
}
MultimediaStream::~MultimediaStream ()
{
	this->Stop();
	delete this->dashManager;

	if(this->frameBuffer)
	{
		this->frameBuffer->Clear();
		delete this->frameBuffer;
	}
	if(this->sampleBuffer)
	{
		this->sampleBuffer->Clear();
		delete this->sampleBuffer;
	}
}

bool MultimediaStream::IsNDN							()
{
	return this->isNDN;
}
void		MultimediaStream::ShouldAbort				()
{
	this->dashManager->ShouldAbort();
}
uint32_t    MultimediaStream::GetPosition               ()
{
	return this->dashManager->GetPosition();
}
void        MultimediaStream::SetPosition               (uint32_t segmentNumber)
{
	this->dashManager->SetPosition(segmentNumber);
}
void        MultimediaStream::SetPositionInMsec         (uint32_t milliSecs)
{
	this->dashManager->SetPositionInMsec(milliSecs);
}
void        MultimediaStream::Init                      ()
{
	this->dashManager   = new DASHManager(this->type, this->segmentBufferSize, this, this->mpd, this->IsNDN(), this->ndnAlpha, this->noDecoding, this->chunkGranularity, this->samplesize);
	if(this->frameBufferSize != 0)
		this->frameBuffer   = new Buffer<QImage>(this->frameBufferSize, libdash::framework::buffer::VIDEO);
	if(this->sampleBufferSize != 0)
		this->sampleBuffer  = new Buffer<AudioChunk>(this->sampleBufferSize, libdash::framework::buffer::AUDIO);

	if(this->frameBuffer)
		this->frameBuffer->AttachObserver(this);
	if(this->sampleBuffer)
		this->sampleBuffer->AttachObserver(this);
}
bool        MultimediaStream::Start                     ()
{
	if(!this->StartDownload())
		return false;

	return true;
}
bool        MultimediaStream::StartDownload             ()
{
	dashManager->SetAdaptationLogic(this->logic);
	if(!dashManager->Start())
		return false;

	return true;
}
void        MultimediaStream::Stop                      ()
{

	this->StopDownload();

	if(this->frameBuffer)
	{
		this->frameBuffer->SetEOS(true);
		this->frameBuffer->Clear();
	}
	if(this->sampleBuffer)
	{
		this->sampleBuffer->SetEOS(true);
		this->sampleBuffer->Clear();
	}
}
void        MultimediaStream::StopDownload              ()
{
	this->dashManager->Stop();
}
void        MultimediaStream::Clear                     ()
{
	this->dashManager->Clear();
}
void        MultimediaStream::AddFrame                  (QImage *frame)
{
	this->frameBuffer->PushBack(frame);
}
QImage*     MultimediaStream::GetFrame                  ()
{
	//return NULL;
	return this->frameBuffer->GetFront();
}
void        MultimediaStream::AddSamples                (AudioChunk *samples)
{
	this->sampleBuffer->PushBack(samples);
}
AudioChunk* MultimediaStream::GetSamples                ()
{
	return this->sampleBuffer->GetFront();
}
void        MultimediaStream::AttachStreamObserver      (IStreamObserver *observer)
{
	this->observers.push_back(observer);
}
void        MultimediaStream::SetRepresentation         (IPeriod *period, IAdaptationSet *adaptationSet, IRepresentation *representation)
{
	this->dashManager->SetRepresentation(period, adaptationSet, representation);
}
void        MultimediaStream::EnqueueRepresentation     (IPeriod *period, IAdaptationSet *adaptationSet, IRepresentation *representation)
{
	this->dashManager->EnqueueRepresentation(period, adaptationSet, representation);
}
void        MultimediaStream::SetAdaptationLogic        (libdash::framework::adaptation::IAdaptationLogic *logic)
{
	this->logic = logic;
}
void        MultimediaStream::OnSegmentBufferStateChanged   (uint32_t fillstateInPercent)
{
	for (size_t i = 0; i < observers.size(); i++)
		this->observers.at(i)->OnSegmentBufferStateChanged(this->type, fillstateInPercent);
}
void        MultimediaStream::OnBufferStateChanged          (BufferType type, uint32_t fillstateInPercent)
{
	switch(type)
	{
	case libdash::framework::buffer::AUDIO:
		for (size_t i = 0; i < observers.size(); i++)
			this->observers.at(i)->OnAudioBufferStateChanged(fillstateInPercent);
		break;
	case libdash::framework::buffer::VIDEO:
		for (size_t i = 0; i < observers.size(); i++)
			this->observers.at(i)->OnVideoBufferStateChanged(fillstateInPercent);
	default:
		break;
	}
}

void MultimediaStream::SetEOS (bool value)
{
	if(this->sampleBuffer)
		this->sampleBuffer->SetEOS(value);
	if(this->frameBuffer)
		this->frameBuffer->SetEOS(value);

	for(size_t i = 0; i < observers.size(); i++)
		this->observers.at(i)->SetEOS(value);
}

void MultimediaStream::SetTargetDownloadingTime (double target)
{
	this->dashManager->SetTargetDownloadingTime(target);
}

void MultimediaStream::FastForward ()
{
	this->dashManager->FastForward();
}

void MultimediaStream::FastRewind ()
{
	this->dashManager->FastRewind();
}
