/*
 * FullScreenDialog.cpp
 *
 *  Created on: Oct 11, 2016
 *      Author: ndnops
 */

#include "QtSamplePlayerGui.h"

using namespace sampleplayer;

FullScreenDialog::FullScreenDialog(QtSamplePlayerGui * gui) :
			QDialog((QMainWindow*)gui)
{
	this->gui = gui;
}

FullScreenDialog::~FullScreenDialog()
{
	delete this;
}

void FullScreenDialog::keyPressEvent (QKeyEvent* event)
{
	if(event->key() ==  Qt::Key_F)
		this->accept();
	else
		this->gui->keyPressEvent(event);
}
