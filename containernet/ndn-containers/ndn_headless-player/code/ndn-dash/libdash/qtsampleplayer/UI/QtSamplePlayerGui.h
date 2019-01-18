/*
 * qtsampleplayer.h
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef QTSAMPLEPLAYER_H
#define QTSAMPLEPLAYER_H

#include <QtMultimedia/qmediaplayer.h>
#include <QtMultimediaWidgets/qvideowidget.h>
#include <QtGui/QMovie>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStackedWidget>
#include <QMouseEvent>
#include "ui_qtsampleplayer.h"
#include "libdash.h"
#include "../libdashframework/MPD/AdaptationSetHelper.h"
#include "../libdashframework/Adaptation/IAdaptationLogic.h"
#include "../libdashframework/Portable/MultiThreading.h"
#include <QDialog>
//#include "FullScreenDialog.h"

namespace sampleplayer
{
    class IDASHPlayerGuiObserver;

    class QtSamplePlayerGui : public QMainWindow
    {
        Q_OBJECT

        public:
            QtSamplePlayerGui           (QWidget *parent = 0);
            virtual ~QtSamplePlayerGui  ();

            void                                    SetGuiFields            (dash::mpd::IMPD* mpd);
            virtual void                            AddWidgetObserver       (IDASHPlayerGuiObserver* observer);
            virtual void                            SetStatusBar            (const std::string& text);
            virtual std::string                     GetUrl                  ();
            sampleplayer::renderer::QTGLRenderer*   GetVideoElement         ();
            void									DisableUserActions		(); //On Bitrate selection
            void									EnableUserActions		(); //On Bitrate selection
            void									Reset					();
            void									mouseDoubleClickEvent	(QMouseEvent* event);
            void									keyPressEvent			(QKeyEvent* event);
            QStackedWidget*							myStackedWidget;
            bool									isRunning;

            void 									LockStartAndDownloadButton ();

        private slots:
            void on_cb_mpd_currentTextChanged                   (const QString &arg1);
            void on_cb_period_currentIndexChanged               (int index);
            void on_cb_video_adaptationset_currentIndexChanged  (int index);
            void on_cb_video_representation_currentIndexChanged (int index);
            void on_cb_audio_adaptationset_currentIndexChanged  (int index);
            void on_cb_audio_representation_currentIndexChanged (int index);
            void on_button_mpd_clicked                          ();
            void on_button_start_clicked                        ();
            void on_button_stop_clicked                         ();


        public slots:
            virtual void    SetVideoSegmentBufferFillState  (int percentage);
            virtual void    SetVideoBufferFillState         (int percentage);
            virtual void    SetAudioSegmentBufferFillState  (int percentage);
            virtual void    SetAudioBufferFillState         (int percentage);
            int 			GetAdaptationLogic				();
            bool			GetNDNStatus					(); //check if NDN is enabled or classical TCP
            void			SetNDNStatus					(bool value);
            void			SetUrl							(const char* text);
            void 			SetAdaptationLogic				(libdash::framework::adaptation::LogicType type);
            void			showNormal						();

        private:
            std::map<std::string, std::string>                  keyValues;
            std::map<std::string, int>                          keyIndices;
            std::map<std::string, std::vector<std::string> >    video;
            std::map<std::string, std::vector<std::string> >    audio;

            CRITICAL_SECTION						monitorMutex;
            Ui::QtSamplePlayerClass                 *ui;
            std::vector<IDASHPlayerGuiObserver *>   observers;
            dash::mpd::IMPD                         *mpd;
            bool									fullscreen;
            bool									isPlaying;

            void LockUI                     ();
            void UnLockUI                   ();

            void SetPeriodComboBox              (dash::mpd::IMPD *mpd, QComboBox *cb);
            void SetAdaptationLogicComboBox     (dash::mpd::IMPD *mpd, QComboBox *cb);
            void SetAdaptationSetComboBox       (dash::mpd::IPeriod *period, QComboBox *cb);
            void SetVideoAdaptationSetComboBox  (dash::mpd::IPeriod *period, QComboBox *cb);
            void SetAudioAdaptationSetComboBox  (dash::mpd::IPeriod *period, QComboBox *cb);
            void SetRepresentationComoboBox     (dash::mpd::IAdaptationSet *adaptationSet, QComboBox *cb);

            void ClearComboBoxes            ();

            void OnDoubleClick				();

            void NotifySettingsChanged      ();
            void NotifyMPDDownloadPressed   (const std::string &url);
            void NotifyStartButtonPressed   ();
            void NotifyStopButtonPressed    ();
            void NotifyPauseButtonPressed   ();
            void NotifyFastForward		    ();
            void NotifyFastRewind		    ();

    };

	class FullScreenDialog : public QDialog {
	public:
		FullScreenDialog(QtSamplePlayerGui *gui);
		~FullScreenDialog();

        void keyPressEvent (QKeyEvent* event);
	private:
		QtSamplePlayerGui *gui;
	};
}

#endif // QTSAMPLEPLAYER_H
