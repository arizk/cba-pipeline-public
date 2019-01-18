/*
 * FOOBAR.h
 *
 *  Created on: Oct 17, 2016
 *      Author: ndnops
 */

#ifndef LIBDASH_FRAMEWORK_ADAPTATION_FOOBAR_H_
#define LIBDASH_FRAMEWORK_ADAPTATION_FOOBAR_H_

#include "AbstractAdaptationLogic.h"
#include "../MPD/AdaptationSetStream.h"
#include "../Input/IDASHReceiverObserver.h"

namespace libdash
{
	namespace framework
	{
		namespace adaptation
		{
			class FOOBARAdaptation : public AbstractAdaptationLogic
			{
				public:
					FOOBARAdaptation(dash::mpd::IMPD *mpd, dash::mpd::IPeriod *period, dash::mpd::IAdaptationSet *adaptationSet, bool isVid, double arg1, double arg2, double arg3, double arg4, double arg5, double arg6);
					virtual ~FOOBARAdaptation();

					virtual LogicType               GetType             ();
			        virtual bool					IsUserDependent		();
		            virtual bool 					IsRateBased			();
	                virtual bool 					IsBufferBased		();
                    virtual void 					BitrateUpdate		(uint64_t bps);
                    virtual void	                SegmentPositionUpdate(uint32_t qualitybps);
                    virtual void 					DLTimeUpdate		(double time);
                    virtual void 					BufferUpdate		(uint32_t bufferFill);
                    void 							SetBitrate			(uint64_t bufferFill);
                    uint64_t						GetBitrate			();
                    virtual void 					SetMultimediaManager	(sampleplayer::managers::IMultimediaManagerBase *_mmManager);
                    void							NotifyBitrateChange	();
                    void							OnEOS				(bool value);
                    virtual void                    OnSegmentDownloaded (double bufferFillPct, int segNum, int quality, std::vector<int> rttVec, std::vector<int> numHopsVec);
                    void 							CheckedByDASHReceiver();

		    void						Quantizer			();
                private:
                    uint64_t						currentBitrate;

                    std::vector<uint64_t>			availableBitrates;
                    sampleplayer::managers::IMultimediaManagerBase	*multimediaManager;
                    dash::mpd::IRepresentation		*representation;

                    uint64_t						averageBw;			// Classic EWMA
                    uint64_t						instantBw;
                    uint64_t						smoothBw;			// FOOBAR paper smoothed y[n]
                    uint64_t						targetBw;			// FOOBAR paper x[n] bw estimation

                    double						param_Alpha;
                    double 						alpha_ewma;
                    double						param_Epsilon;
                    double						param_K;
                    double						param_W;
                    double						param_Beta;
                    double						param_Bmin;

                    double						interTime;				// Actual inter time
                    double						targetInterTime;		// Target inter time
                    double						downloadTime;

                    uint32_t						bufferLevel;
                    uint32_t						lastBufferLevel;
                    double 							bufferMaxSizeSeconds;		// Usually set to 60s
                    double 							bufferLevelSeconds;			// Current buffer level [s]

                    double						segmentDuration;
		    double						deltaUp;
		    double						deltaDown;
		    size_t						current;
			};

		} /* namespace adaptation */
	} /* namespace framework */
} /* namespace libdash */

#endif /* LIBDASH_FRAMEWORK_ADAPTATION_FOOBAR_H_ */
