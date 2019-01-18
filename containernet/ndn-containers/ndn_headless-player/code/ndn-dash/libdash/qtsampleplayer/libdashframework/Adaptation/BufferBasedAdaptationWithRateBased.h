/*
 * RateBasedAdaptation.h
 *****************************************************************************
 * Copyright (C) 2016,
 * Jacques Samain <jsamain@cisco.com>
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef LIBDASH_FRAMEWORK_ADAPTATION_BUFFERBASEDADAPTATIONRATE_H_
#define LIBDASH_FRAMEWORK_ADAPTATION_BUFFERBASEDADAPTATIONRATE_H_

#include "AbstractAdaptationLogic.h"
#include "../MPD/AdaptationSetStream.h"
#include "../Input/IDASHReceiverObserver.h"

namespace libdash
{
    namespace framework
    {
        namespace adaptation
        {
            class BufferBasedAdaptationWithRateBased : public AbstractAdaptationLogic
            {
                public:
            	BufferBasedAdaptationWithRateBased            (dash::mpd::IMPD *mpd, dash::mpd::IPeriod *period, dash::mpd::IAdaptationSet *adaptationSet, bool isVid, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6);
                    virtual ~BufferBasedAdaptationWithRateBased   ();

                    virtual LogicType               GetType             ();
                    virtual bool					IsUserDependent		();
                    virtual bool 					IsRateBased			();
                    virtual bool 					IsBufferBased		();
                    virtual void 					BitrateUpdate		(uint64_t bps);
                    virtual void	                SegmentPositionUpdate(uint32_t qualitybps);
                    virtual void 					DLTimeUpdate		(double time);
                    virtual void 					BufferUpdate		(uint32_t bufferFill);
                    void 							SetBitrate			(uint32_t bufferFill);
                    uint64_t						GetBitrate			();
                    virtual void 					SetMultimediaManager	(sampleplayer::managers::IMultimediaManagerBase *_mmManager);
                    void							NotifyBitrateChange	();
                    void							OnEOS				(bool value);
                    virtual void                    OnSegmentDownloaded (double bufferFillPct, int segNum, int quality, std::vector<int> rttVec, std::vector<int> numHopsVec);
                    void							CheckedByDASHReceiver();

                private:
                    uint64_t						currentBitrate;
                    std::vector<uint64_t>			availableBitrates;
                    sampleplayer::managers::IMultimediaManagerBase	*multimediaManager;
                    dash::mpd::IRepresentation		*representation;
                    uint32_t						reservoirThreshold;
                    uint32_t						maxThreshold;
                    uint32_t						lastBufferFill;
                    int								m_count;
                    int								switchUpThreshold;
                    bool							bufferEOS;
                    bool							shouldAbort;
                    double							alphaRate;
                    uint64_t						averageBw;
                    uint64_t						instantBw;
                    int								myQuality;
                    double							slackParam;
                    bool							isCheckedForReceiver;
            };
        }
    }
}

#endif /* LIBDASH_FRAMEWORK_ADAPTATION_BUFFERBASEDADAPTATIONRATE_H_ */
