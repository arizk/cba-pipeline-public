/*
 * SegmentSize.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "SegmentSize.h"

using namespace dash::mpd;

SegmentSize::SegmentSize    () :
                    identifier(""),
                    scale(""),
                    size(0)
{
}
SegmentSize::~SegmentSize   ()
{
}

const std::string&  SegmentSize::GetID                          ()  const
{
    return this->identifier;
}
void                SegmentSize::SetID                          (const std::string& id)
{
    this->identifier = id;
}
const std::string&  SegmentSize::GetScale                       ()  const
{
    return this->scale;
}
void                SegmentSize::SetScale                       (const std::string& scale)
{
    this->scale = scale;
}
const int           SegmentSize::GetSize                        ()  const
{
    return this->size;
}
void                SegmentSize::SetSize                        (const int size)    //directly converted to bits
{
    if((this->scale).compare("Kbits") == 0)
        this->size = size * 1000;
    else if((this->scale).compare("Mbits") == 0)
            this->size = size * 1000000;
        else if((this->scale).compare("Gbits") == 0)
                this->size = size * 1000000000;
            else
                this->size = size;  //default: Bits
}
