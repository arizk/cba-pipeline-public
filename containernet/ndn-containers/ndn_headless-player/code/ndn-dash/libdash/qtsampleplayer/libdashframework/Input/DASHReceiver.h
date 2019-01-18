/*
 * DASHReceiver.h
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef LIBDASH_FRAMEWORK_INPUT_DASHRECEIVER_H_
#define LIBDASH_FRAMEWORK_INPUT_DASHRECEIVER_H_

#include "libdash.h"
#include "IMPD.h"

#include "../Input/MediaObject.h"
#include "IDASHReceiverObserver.h"
#include "../Buffer/MediaObjectBuffer.h"
#include "../MPD/AdaptationSetStream.h"
#include "../MPD/IRepresentationStream.h"
#include "../Portable/MultiThreading.h"
#include <chrono>

#ifdef NDNICPDOWNLOAD
	#include "NDNConnection.h"
#else
	#include "NDNConnectionConsumerApi.h"
#endif

namespace libdash
{
    namespace framework
    {
    	namespace adaptation
		{
    		class IAdaptationLogic;
		}
    	namespace buffer
		{
    		class MediaObjectBuffer;
		}
        namespace input
        {
        	class MediaObject;
            class DASHReceiver
            {
                public:
                    DASHReceiver            (dash::mpd::IMPD *mpd, IDASHReceiverObserver *obs, buffer::MediaObjectBuffer *buffer, uint32_t bufferSize, bool ndnEnabled, double ndnAlpha, bool chunkGranularityEnabled, int sample);
                    virtual ~DASHReceiver   ();

                    bool                        Start                   ();
                    void                        Stop                    ();
                    input::MediaObject*         GetNextSegment          ();
                    input::MediaObject*         GetSegment              (uint32_t segmentNumber);
                    input::MediaObject*         GetInitSegment          ();
                    input::MediaObject*         FindInitSegment         (dash::mpd::IRepresentation *representation);
                    uint32_t                    GetPosition             ();
                    void                        SetPosition             (uint32_t segmentNumber);
                    void                        SetPositionInMsecs      (uint32_t milliSecs);
                    dash::mpd::IRepresentation* GetRepresentation       ();
                    void                        SetRepresentation       (dash::mpd::IPeriod *period,
                                                                         dash::mpd::IAdaptationSet *adaptationSet,
                                                                         dash::mpd::IRepresentation *representation);
                    void						SetAdaptationLogic		(adaptation::IAdaptationLogic *_adaptationLogic);
                    libdash::framework::adaptation::IAdaptationLogic* GetAdaptationLogic		();

                    void						Notifybps				(uint64_t bps);
                    void						NotifybpsSegment		(uint64_t bps);
                    void						NotifybpsChunk		    (uint64_t bps);
					void						NotifyContextChunk		(uint64_t rtt, uint64_t numHops);
                    void						NotifyDlTime			(double dnltime);
                    void						NotifyBitrateChange		(dash::mpd::IRepresentation *representation);
                    void 						OnSegmentBufferStateChanged(uint32_t fillstateInPercent);
                    bool						IsNDN					();
                    void						ShouldAbort				();
                    void						Seeking					(int);
                    void						FastForward				();
                    void						FastRewind				();
                    void						OnEOS					(bool value);
                    void						SetTargetDownloadingTime(double);

                    void						NotifyCheckedAdaptationLogic();
                    bool						threadComplete;

                private:
                    uint32_t        CalculateSegmentOffset  ();
                    void            NotifySegmentDownloaded (double bufferFillPct, int segNum, int quality);
                    void            DownloadInitSegment     (dash::mpd::IRepresentation* rep);
                    bool            InitSegmentExists       (dash::mpd::IRepresentation* rep);
                    bool			withFeedBack;

                    static void*    DoBuffering             (void *receiver);

                    std::map<dash::mpd::IRepresentation*, MediaObject*> initSegments;
                    buffer::MediaObjectBuffer                           *buffer;
                    IDASHReceiverObserver                               *observer;
                    dash::mpd::IMPD                                     *mpd;
                    dash::mpd::IPeriod                                  *period;
                    dash::mpd::IAdaptationSet                           *adaptationSet;
                    dash::mpd::IRepresentation                          *representation;
                    mpd::AdaptationSetStream                            *adaptationSetStream;
                    mpd::IRepresentationStream                          *representationStream;
                    uint32_t                                            segmentNumber;
                    uint32_t                                            positionInMsecs;
                    uint32_t                                            segmentOffset;
                    uint32_t                                            bufferSize;
                    mutable CRITICAL_SECTION                            monitorMutex;
                    mutable CRITICAL_SECTION                            monitorPausedMutex;
                    mutable CONDITION_VARIABLE							paused;
                    bool												isPaused;
                    bool												isScheduledPaced;
                    double												targetDownload;
                    double												downloadingTime;

                    adaptation::IAdaptationLogic			*adaptationLogic;
                    INDNConnection* conn;
                    INDNConnection* initConn;
                    THREAD_HANDLE   bufferingThread;
                    bool            isBuffering;
                    bool			isNDN;
                    double			ndnAlpha;
                    int				previousQuality;
                    bool            chunkGranularity;
                    int             samplesize;
                    ChunktimeObserver* chunktimeObs;
					std::map<int, std::vector<int>> rttMap;
                    std::map<int, std::vector<int>> numHopsMap;
            };
        }
    }
}

#endif /* LIBDASH_FRAMEWORK_INPUT_DASHRECEIVER_H_ */
