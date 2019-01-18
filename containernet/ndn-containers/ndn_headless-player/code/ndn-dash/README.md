# libdash


libdash is the **official reference software of the ISO/IEC MPEG-DASH standard** and is an open-source library that provides an object orient (OO) interface to the MPEG-DASH standard, developed by [bitmovin](http://www.bitmovin.com).

## by bitmovin
<a href="https://www.bitmovin.com"><img src="https://cloudfront.bitmovin.com/wp-content/uploads/2014/11/Logo-bitmovin.jpg" width="400px"/></a>

## Documentation

The doxygen documentation availalbe in the repo.

## Sources and Binaries

You can find the latest sources and binaries on github.

## How to use

### Ubuntu 16.04
1. sudo apt-get install git-core build-essential cmake libxml2-dev libcurl4-openssl-dev
3. cd ndn-dash/libdash
4. mkdir build
5. cd build
6. cmake ../
7. make

#### QTSamplePlayer
Prerequisite: libdash must be built as described in the previous section.

1. sudo apt-get install qtmultimedia5-dev qtbase5-dev libqt5widgets5 libqt5core5a libqt5gui5 libqt5multimedia5 libqt5multimediawidgets5 libqt5opengl5 libav-tools libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavutil-dev libpostproc-dev libswscale-dev libqt5multimedia5-plugins libavresample-dev
2bis. sudo apt-get install ndn-icp-download
3. cd ndn-dash/libdash/qtsampleplayer
4. mkdir build
5. cd build
6. cmake ../ (if this doesn't succeed, try with cmake ../ -DLIBAV_ROOT_DIR=/usr/lib)
7. make
8. ./qtsampleplayer


##Usage with headless mode

./qtsampleplayer -nohead -u url [-nr alpha] [-n] [-r alpha] [-b B1 Bmax]

-u: to indicate the url to download the MPD file.
-n: indicate NDN download.
-nr: indicate NDN download and usage of NDN-based rate estimation with an EWMA of parameter alpha, 0 <= alpha < 1.
-r: indicate Rate-based adaptation with a parameter of alpha. 0 <= alpha < 1.
-b: indicate Buffer-based adaptation (BBA-0) with parameters B1 and Bmax.


## License

libdash is open source available and licensed under LGPL:

“This library is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA“

As libdash is licensed under LGPL, changes to the library have to be published again to the open-source project. As many user and companies do not want to publish their specific changes, libdash can be also relicensed to a commercial license on request. Please contact sales@bitmovin.net to provide you an offer.

## Acknowledgements

We specially want to thank our passionate developers at [bitmovin](http://www.bitmovin.com) as well as the researchers at the [Institute of Information Technology](http://www-itec.aau.at/dash/) (ITEC) from the Alpen Adria Universitaet Klagenfurt (AAU)!

Furthermore we want to thank the [Netidee](http://www.netidee.at) initiative from the [Internet Foundation Austria](http://www.nic.at/ipa) for partially funding the open source development of libdash.

![netidee logo](http://www.bitmovin.com/files/bitmovin/img/logos/netidee.png "netidee")

## Citation of libdash
We kindly ask you to refer the following paper in any publication mentioning libdash:

Christopher Mueller, Stefan Lederer, Joerg Poecher, and Christian Timmerer, “libdash – An Open Source Software Library for the MPEG-DASH Standard”, in Proceedings of the IEEE International Conference on Multimedia and Expo 2013, San Jose, USA, July, 2013
