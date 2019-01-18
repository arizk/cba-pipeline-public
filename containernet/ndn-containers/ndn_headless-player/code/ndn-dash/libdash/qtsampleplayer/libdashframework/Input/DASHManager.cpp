
/*
 * DASHManager.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include <immintrin.h>
#include <tmmintrin.h>
#include "DASHManager.h"

using namespace libdash::framework::input;
using namespace libdash::framework::buffer;
using namespace sampleplayer::decoder;

using namespace dash;
using namespace dash::network;
using namespace dash::mpd;

//To circumvent the stupid refactoring in libavcodec...
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

DASHManager::DASHManager        (sampleplayer::managers::StreamType type, uint32_t maxCapacity, IDASHManagerObserver* stream, IMPD* mpd, bool ndnEnabled, double ndnAlpha, bool nodecoding, bool chunkGranularityEnabled, int sample) :
            		 readSegmentCount	(0),
					 receiver			(NULL),
					 mediaObjectDecoder (NULL),
					 multimediaStream	(stream),
					 isRunning			(false),
					 isNDN				(ndnEnabled),
					 ndnAlpha			(ndnAlpha),
					 noDecoding			(nodecoding),
					 chunkGranularity	(chunkGranularityEnabled),
					 samplesize			(sample)
{
	this->buffer = new MediaObjectBuffer(type, maxCapacity);
	this->buffer->AttachObserver(this);

	this->receiver  = new DASHReceiver(mpd, this, this->buffer, maxCapacity, this->IsNDN(), this->ndnAlpha, this->chunkGranularity, this->samplesize);
}
DASHManager::~DASHManager       ()
{
	this->Stop();
	delete this->receiver;
	delete this->buffer;

	this->receiver = NULL;
	this->buffer   = NULL;
}

bool 		DASHManager::IsNDN					()
{
	return this->isNDN;
}
void		DASHManager::ShouldAbort			()
{
	Debug("DASH MANAGER: ABORT REQUEST\n");
	this->receiver->ShouldAbort();
}
bool        DASHManager::Start                  ()
{
	this->receiver->SetAdaptationLogic(this->adaptationLogic);
	if (!this->receiver->Start())
		return false;

	if (!this->CreateAVDecoder())
		return false;

	this->isRunning = true;
	return true;
}
void        DASHManager::Stop                   ()
{
	if (!this->isRunning)
		return;

	this->isRunning = false;

	this->receiver->Stop();
	this->mediaObjectDecoder->Stop();
	this->buffer->Clear();
}
uint32_t    DASHManager::GetPosition            ()
{
	return this->receiver->GetPosition();
}
void        DASHManager::SetPosition            (uint32_t segmentNumber)
{
	this->receiver->SetPosition(segmentNumber);
}
void        DASHManager::SetPositionInMsec      (uint32_t milliSecs)
{
	this->receiver->SetPositionInMsecs(milliSecs);
}
void 		DASHManager::SetAdaptationLogic		(libdash::framework::adaptation::IAdaptationLogic *_adaptationLogic)
{
	this->adaptationLogic = _adaptationLogic;
}
void        DASHManager::Clear                  ()
{
	this->buffer->Clear();
}
/*void        DASHManager::ClearTail              ()
{
	this->buffer->ClearTail();
}
*/
void        DASHManager::SetRepresentation      (IPeriod *period, IAdaptationSet *adaptationSet, IRepresentation *representation)
{
	this->receiver->SetRepresentation(period, adaptationSet, representation);
	//this->buffer->ClearTail();
}
void        DASHManager::EnqueueRepresentation  (IPeriod *period, IAdaptationSet *adaptationSet, IRepresentation *representation)
{
	this->receiver->SetRepresentation(period, adaptationSet, representation);
}
void        DASHManager::OnVideoFrameDecoded    (const uint8_t **data, videoFrameProperties* props)
{
	/* TODO: some error handling here */
	if (data == NULL || props->fireError)
	{
		Debug("data is %sNULL and %serror was fired\n", (data==NULL)? "" : "not ", (props->fireError)?"":"no ");
		return;
	}

	int w = props->width;
	int h = props->height;

	//AVFrame *rgbframe   = avcodec_alloc_frame();
	AVFrame *rgbframe   = av_frame_alloc();
	int     numBytes    = avpicture_get_size(AV_PIX_FMT_RGB24, w, h);
	uint8_t *buffer     = (uint8_t*)av_malloc(numBytes);

	avpicture_fill((AVPicture*)rgbframe, buffer, AV_PIX_FMT_RGB24, w, h);

	SwsContext *imgConvertCtx = sws_getContext(props->width, props->height, (AVPixelFormat)props->pxlFmt, w, h, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	sws_scale(imgConvertCtx, data, props->linesize, 0, h, rgbframe->data, rgbframe->linesize);

	uint8_t *src = (uint8_t *)rgbframe->data[0];
	QImage *image = new QImage(w, h, QImage::Format_RGB32);

	//TRYING to tackle efficiency issue by multithreading the rendering
///*
	int numberOfThreads = 3;
	Container* m_Containers[numberOfThreads];
	THREAD_HANDLE m_threads[numberOfThreads];
	for(int i=0; i < numberOfThreads; i++)
	{
		m_Containers[i] = (Container *)malloc(sizeof(struct Container));
		m_Containers[i]->number = i;
		m_Containers[i]->total = numberOfThreads;
		m_Containers[i]->height = h;
		m_Containers[i]->width = w;
		m_Containers[i]->src = src + i * (h/numberOfThreads - 1) *rgbframe->linesize[0];
		m_Containers[i]->image =  image;
		m_Containers[i]->linesize = rgbframe->linesize[0];

		m_threads[i] = CreateThreadPortable(AV2QImage, m_Containers[i]);
	}

	for(int i = 0; i < numberOfThreads; i++)
	{
		JoinThread(m_threads[i]);
	}

	for(int i=0; i < numberOfThreads; i++)
		free(m_Containers[i]);
//*/
/*	for (size_t y = 0; y < h; y++)
	{
		QRgb *scanLine = (QRgb *)image->scanLine(y);
		for(size_t x = 0; x < w; x ++)
		{
			scanLine[x] = qRgb(src[3*x],src[3*x + 1],src[3*x + 2]);
		}
		src += rgbframe->linesize[0];
	}*/

	this->multimediaStream->AddFrame(image);
	//delete image;
	//image = NULL;
	av_free(rgbframe);
	av_free(buffer);
	sws_freeContext(imgConvertCtx);
	imgConvertCtx = NULL;
}

/* in and out must be 16-byte aligned */
void DASHManager::rgb_to_bgrx_sse(unsigned w, const void *in, void *out)
{
    const __m128i *in_vec =  in;
    __m128i *out_vec = out;

    w /= 16;

    while (w-- > 0) {
        /*             0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
         * in_vec[0]   Ra Ga Ba Rb Gb Bb Rc Gc Bc Rd Gd Bd Re Ge Be Rf
         * in_vec[1]   Gf Bf Rg Gg Bg Rh Gh Bh Ri Gi Bi Rj Gj Bj Rk Gk
         * in_vec[2]   Bk Rl Gl Bl Rm Gm Bm Rn Gn Bn Ro Go Bo Rp Gp Bp
         */
        __m128i in1, in2, in3;
        __m128i out;

        in1 = in_vec[0];

        out = _mm_shuffle_epi8(in1,
            _mm_set_epi8(0xff, 9, 10, 11, 0xff, 6, 7, 8, 0xff, 3, 4, 5, 0xff, 0, 1, 2));
        out = _mm_or_si128(out,
            _mm_set_epi8(0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0));
        out_vec[0] = out;

        in2 = in_vec[1];

        in1 = _mm_and_si128(in1,
            _mm_set_epi8(0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0));
        out = _mm_and_si128(in2,
            _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff));
        out = _mm_or_si128(out, in1);
        out = _mm_shuffle_epi8(out,
            _mm_set_epi8(0xff, 5, 6, 7, 0xff, 2, 3, 4, 0xff, 15, 0, 1, 0xff, 12, 13, 14));
        out = _mm_or_si128(out,
            _mm_set_epi8(0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0));
        out_vec[1] = out;

        in3 = in_vec[2];
        in_vec += 3;

        in2 = _mm_and_si128(in2,
            _mm_set_epi8(0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0));
        out = _mm_and_si128(in3,
            _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff));
        out = _mm_or_si128(out, in2);
        out = _mm_shuffle_epi8(out,
            _mm_set_epi8(0xff, 1, 2, 3, 0xff, 14, 15, 0, 0xff, 11, 12, 13, 0xff, 8, 9, 10));
        out = _mm_or_si128(out,
            _mm_set_epi8(0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0));
        out_vec[2] = out;

        out = _mm_shuffle_epi8(in3,
            _mm_set_epi8(0xff, 13, 14, 15, 0xff, 10, 11, 12, 0xff, 7, 8, 9, 0xff, 4, 5, 6));
        out = _mm_or_si128(out,
            _mm_set_epi8(0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0));
        out_vec[3] = out;

        out_vec += 4;
    }
}

void*	    DASHManager::AV2QImage		(void* data)
{
	Container *m_container = (Container *) data;
	for(size_t y = (m_container->height / m_container->total)*m_container->number; y < ((m_container->number == m_container->total - 1) ? m_container->height : (m_container->height / m_container->total) * (m_container->number + 1) ); y++)
	{
		QRgb *scanLine = (QRgb *)m_container->image->scanLine(y);

		rgb_to_bgrx_sse(m_container->width, m_container->src, (void *) scanLine);
		/*for(size_t x = 0; x < m_container->width; x ++)
		{
			scanLine[x] = qRgb(m_container->src[3*x],m_container->src[3*x + 1],m_container->src[3*x + 2]);
		}*/
		m_container->src += m_container->linesize;
	}
}

void        DASHManager::OnAudioSampleDecoded   (const uint8_t **data, audioFrameProperties* props)
{

	/* TODO: some error handling here */
	if (data == NULL || props->fireError)
		return;

	QAudioFormat *format = new QAudioFormat();
	format->setSampleRate(props->sampleRate);
	format->setChannelCount(props->channels);
	format->setSampleSize(props->samples);
	format->setCodec("audio/pcm");
	format->setByteOrder(QAudioFormat::LittleEndian);
	format->setSampleType(QAudioFormat::Float);

	Debug("chunksize: %d, sampleRate: %d, samples: %d\n",props->chunkSize, props->sampleRate, props->samples);
	AudioChunk *samples = new AudioChunk(format, (char*)data[0], props->linesize);

	this->multimediaStream->AddSamples(samples);
}
void        DASHManager::OnBufferStateChanged   (uint32_t fillstateInPercent)
{
	this->multimediaStream->OnSegmentBufferStateChanged(fillstateInPercent);
	if(this->adaptationLogic->IsBufferBased())
		this->receiver->OnSegmentBufferStateChanged(fillstateInPercent);
}
void 		DASHManager::OnEOS					(bool value)
{
	if(this->adaptationLogic->IsBufferBased())
		this->receiver->OnEOS(value);
}
void        DASHManager::OnSegmentDownloaded    ()
{
	this->readSegmentCount++;

	// notify observers
}
void        DASHManager::OnDecodingFinished     ()
{
	if (this->isRunning)
		this->CreateAVDecoder();
}
bool        DASHManager::CreateAVDecoder        ()
{
	MediaObject *mediaObject            = this->buffer->GetFront();
	// initSegForMediaObject may be NULL => BaseUrls
	if (!mediaObject)
	{
		//No media Object here means that the stream is finished ? Feels like it is but need to check TODO
		this->multimediaStream->SetEOS(true);
		return false;
	}
	MediaObject *initSegForMediaObject  = this->receiver->FindInitSegment(mediaObject->GetRepresentation());

	if(this->mediaObjectDecoder)
	{
		delete this->mediaObjectDecoder;
	}
	this->mediaObjectDecoder = new MediaObjectDecoder(initSegForMediaObject, mediaObject, this, this->noDecoding);
	return this->mediaObjectDecoder->Start();
}
void		DASHManager::FastForward			()
{
	this->receiver->FastForward();
}
void		DASHManager::FastRewind			()
{
	this->receiver->FastRewind();
}

void		DASHManager::SetTargetDownloadingTime	(double target)
{
	this->receiver->SetTargetDownloadingTime(target);
}
