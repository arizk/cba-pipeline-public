/*
 * SegmentSize.h
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef SEGMENTSIZE_H_
#define SEGMENTSIZE_H_

#include "config.h"

#include "../helpers/String.h"

namespace dash
{
    namespace mpd
    {
        class SegmentSize
        {
            public:
                SegmentSize             ();
                virtual ~SegmentSize    ();

                const std::string&  GetID                ()  const;
                const std::string&  GetScale             ()  const;
                const int           GetSize              ()  const;

                void    SetID                   (const std::string& id);
                void    SetScale                (const std::string& scale);

                //ATTENTION: Maximum value is 2147483647 Bits (2147483.6 Kbits, 2147.4 Mbits, 2.14 Gbits)
                void    SetSize                 (const int size);

            private:
                std::string identifier;
                std::string scale;
                int size;
        };
    }
}

#endif /* SEGMENTSIZE_H_ */
