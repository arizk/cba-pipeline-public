/*
 * main.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/
#include <QApplication>

#include "UI/DASHPlayerNoGUI.h"
#include "UI/DASHPlayer.h"

#include "log/log.h"


using namespace sampleplayer;

int main(int argc, char *argv[])
{
	bool no_gui = false;
	for(int i = 0; i < argc; i++)
	{
		if(!strcmp(argv[i],"-nohead"))
		{
			no_gui = true;
			break;
		}
	}
#ifdef LOG_BUILD
    //    sampleplayer::log::Start((argc > 1) ? argv[1] : NULL);
    sampleplayer::log::Start(NULL);
#endif //LOG_BUILD

    if(no_gui)
	{
    	pthread_mutex_t mainMutex;
    	pthread_cond_t mainCond;

    	pthread_mutex_init(&mainMutex,NULL);
    	pthread_cond_init(&mainCond, NULL);

    	L("STARTING NO GUI\n");
    	DASHPlayerNoGUI p(argc,argv, &mainCond, true);


    	pthread_mutex_lock(&mainMutex);
    	while(p.IsRunning())
    	{
    		pthread_cond_wait(&mainCond, &mainMutex);
    	}
    	pthread_mutex_unlock(&mainMutex);

    	return 0;
	}
    else
    {
    	QApplication a(argc, argv);
    	QtSamplePlayerGui w;

    	DASHPlayer p(w, argc, argv);

    	w.show();
    	L("Start\n");
    	return a.exec();
    }
}
