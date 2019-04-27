FROM buildpack-deps:trusty
ENV NDNVERSION 0.4.1 
ENV REPONG 959c5b9740cec055def1347be8cb845eec1118af

MAINTAINER Denny Stohr

EXPOSE 80 9696 6363

RUN apt-get update && apt-get install -y \
    gstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-good \
    gstreamer1.0-libav multitail \
    build-essential libsqlite3-dev libcrypto++-dev \
    libboost-all-dev libssl-dev \
    doxygen graphviz python-sphinx python-pip \
    git software-properties-common \
    pkg-config libpcap-dev telnet htop\
    build-essential iperf \
    libssl-dev \
    libsqlite3-dev \
    libprotobuf-dev \
    fuse libfuse-dev sqlite3 protobuf-compiler cmake \
    qtmultimedia5-dev qtbase5-dev libqt5widgets5 libqt5core5a libqt5gui5 libqt5multimedia5 \
    libqt5multimediawidgets5 libqt5opengl5 libav-tools libavcodec-dev libavdevice-dev libavfilter-dev \ 
    libavformat-dev libavutil-dev libpostproc-dev libswscale-dev libavresample-dev libxml2-dev libcurl4-openssl-dev \
    python3 python3-pip python3-numpy python3-scipy python3-matplotlib python3-setuptools python3-dev libblas-dev \
    liblapack-dev libatlas-base-dev libatlas-dev libatlas3gf-base gfortran \
    python3-tk python3-lxml python3-six

#RUN add-apt-repository ppa:named-data/ppa && apt-get update && apt-get install -y \
#    ndn-tools nfd


# #Install NDN-CXX
RUN git clone -b trevor --depth 1 https://github.com/ballardt/ndn-cxx.git && \
     cd ndn-cxx &&\
    ./waf configure &&\
     ./waf &&\
     ./waf install &&\
     ldconfig

# RUN git clone -b v0.7 https://github.com/named-data/ndn-cpp.git && \
#      cd ndn-cpp && \
#      ./configure &&\
#      make &&\
#      make install &&\
#      ldconfig

#install NFD
RUN git clone -b trevor --depth 1 --recursive https://github.com/ballardt/NFD &&\
     cd NFD &&\
    ./waf configure &&\
    ./waf &&\
    ./waf install &&\
    cp build/nfd.conf.sample /usr/local/etc/ndn/nfd.conf


RUN git clone https://github.com/named-data/ndn-tools &&\
     cd ndn-tools &&\
     git checkout tags/ndn-tools-0.4 &&\
    ./waf configure --disable-pib --disable-chunks &&\
    ./waf &&\
    ./waf install    

# Install and set up Python 3.4, and the bandits
RUN pip3 install sklearn
RUN sed 's/set(_PYTHON3_VERSIONS 3.3/set(_PYTHON3_VERSIONS 3.4 3.3/' -i /usr/share/cmake-2.8/Modules/FindPython*.cmake
ADD run_bandits.py /run_bandits.py
ADD bandits /bandits

# For testing
RUN apt-get install -y gdb valgrind vim
ADD restart.sh /restart.sh
ADD rebuild-ndn-icp.sh /rebuild-ndn-icp.sh
ADD rebuild-player.sh /rebuild-player.sh
RUN chmod +x /*.sh
ADD .vimrc /root/.vimrc

ADD code /code
ADD nfd.conf /usr/local/etc/ndn/nfd.conf
RUN /code/createClient.sh
ADD ndn-entrypoint.sh /ndn-entrypoint.sh
RUN chmod +x /usr/local/bin/nfd
RUN chmod +x /*.sh
ENTRYPOINT /ndn-entrypoint.sh
