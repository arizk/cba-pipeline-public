/*
 * FullScreenDialog.h
 *
 *  Created on: Oct 11, 2016
 *      Author: ndnops
 */

#ifndef QTSAMPLEPLAYER_UI_FULLSCREENDIALOG_H_
#define QTSAMPLEPLAYER_UI_FULLSCREENDIALOG_H_

//USELESS FILE

#include <QDialog>
#include <QtMultimedia/qmediaplayer.h>
#include <QtGui/QMovie>
#include <QtWidgets/QMainWindow>
#include "QtSamplePlayerGui.h"

namespace sampleplayer
{
//	class QtSamplePlayerGui;
//	void QtSamplePlayerGui::keyPressEvent(QKeyEvent*);

	class FullScreenDialog : public QDialog {
	public:
		FullScreenDialog(QtSamplePlayerGui *gui);
		~FullScreenDialog();

        void keyPressEvent (QKeyEvent* event);
	private:
		QtSamplePlayerGui *gui;
	};
}
#endif /* QTSAMPLEPLAYER_UI_FULLSCREENDIALOG_H_ */
