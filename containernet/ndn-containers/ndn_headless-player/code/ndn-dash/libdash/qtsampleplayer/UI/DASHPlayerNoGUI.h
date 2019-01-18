/*
 * DASHPlayerNoGUI.h
 *****************************************************************************
 * Copyright (C) 2016
 *
 * Jacques Samain <jsamain@cisco.com>
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef DASHPLAYERNOGUI_H_
#define DASHPLAYERNOGUI_H_

#include <iostream>
#include <sstream>
#include "libdash.h"
#include "IDASHPlayerNoGuiObserver.h"
#include "../Managers/IMultimediaManagerObserver.h"
#include "../Renderer/QTGLRenderer.h"
#include "../Renderer/QTAudioRenderer.h"
#include "../Managers/MultimediaManager.h"
#include "../libdashframework/Adaptation/AlwaysLowestLogic.h"
#include "../libdashframework/Adaptation/IAdaptationLogic.h"
#include "../libdashframework/Adaptation/ManualAdaptation.h"
#include "../libdashframework/Buffer/IBufferObserver.h"
#include "../libdashframework/MPD/AdaptationSetHelper.h"
#include "DASHPlayer.h"

namespace sampleplayer
{
    class DASHPlayerNoGUI : public IDASHPlayerNoGuiObserver, public managers::IMultimediaManagerObserver

    {
        public:
            DASHPlayerNoGUI          (int argc, char** argv, pthread_cond_t *mainCond, bool nodecoding);
            virtual ~DASHPlayerNoGUI ();

            void parseArgs(int argc, char ** argv);
            void helpMessage(char *name);
            virtual void OnStartButtonPressed   (int period, int videoAdaptationSet, int videoRepresentation, int audioAdaptationSet, int audioRepresentation);
            virtual void OnStopButtonPressed    ();

            virtual void OnSettingsChanged		(int period, int videoAdaptationSet, int videoRepresentation, int audioAdaptationSet, int audioRepresentation);

            /* IMultimediaManagerObserver */
            virtual void OnVideoBufferStateChanged          (uint32_t fillstateInPercent);
            virtual void OnVideoSegmentBufferStateChanged   (uint32_t fillstateInPercent);
            virtual void OnAudioBufferStateChanged          (uint32_t fillstateInPercent);
            virtual void OnAudioSegmentBufferStateChanged   (uint32_t fillstateInPercent);
            virtual void OnEOS								();

            virtual bool OnDownloadMPDPressed   (const std::string &url);

            bool IsRunning						();

        private:
            dash::mpd::IMPD                             *mpd;
            sampleplayer::renderer::QTGLRenderer        *videoElement;
            sampleplayer::renderer::QTAudioRenderer     *audioElement;
            sampleplayer::managers::MultimediaManager   *multimediaManager;
            settings_t                                  currentSettings;
            CRITICAL_SECTION                            monitorMutex;
            char										*url;
            bool										isNDN;
            libdash::framework::adaptation::LogicType 	adaptLogic;
            pthread_cond_t								*mainCond;
            bool										isRunning;
            int											reservoirThreshold;
            int											maxThreshold;
            double										bufferTargetSeconds;
            double										alphaRate;
            bool										isBuff;
            double										ndnAlpha;
            bool                                        chunkGranularity;
            int                                         samplesize;
            bool    SettingsChanged (int period, int videoAdaptationSet, int videoRepresentation, int audioAdaptationSet, int audioRepresentation);
            void    SetSettings     (int period, int videoAdaptationSet, int videoRepresentation, int audioAdaptationSet, int audioRepresentation);
            bool										noDecoding;
            double                                      qoe_w1;
            double                                      qoe_w2;
            double                                      qoe_w3;
    };
}
#endif /* DASHPLAYER_H_ */
