FROM ubuntu:14.04
MAINTAINER Denny Stohr

ENV DEBIAN_FRONTEND=noninteractive
#dpkg --print-architecture
RUN export MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"
RUN export CFLAGS="-march=native -O2 -pipe"
#For Raspberry Pi 3

RUN export CXXFLAGS="${CFLAGS}"

#959c5b9740cec055def1347be8cb845eec1118af

#0.4.1 ,repo-ng: 959c5b9740cec055def1347be8cb845eec1118af
#0.5, repo-ng e1801314be82d1212193b7b77f3c422d4e7f3553
#EXPOSE 80 9696 6363

RUN apt-get update && apt-get install -y wget \
    #gstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    #gstreamer1.0-plugins-bad-videoparsers gstreamer1.0-plugins-good \
    #gstreamer1.0-libav multitail \
    build-essential libsqlite3-dev libcrypto++-dev \
    libssl-dev \
    doxygen graphviz python-sphinx python-pip \
    git parallel iperf telnet libcap2-bin htop tcpdump \
    pkg-config libpcap-dev \
    build-essential \
    libboost-all-dev \
    libsqlite3-dev \
    libprotobuf-dev \
    fuse libfuse-dev sqlite3 protobuf-compiler sudo zsh

#ENV boost_version 1.63.0
#ENV boost_dir boost_1_63_0

#RUN wget https://dl.bintray.com/boostorg/release/${boost_version}/source/${boost_dir}.tar.gz \
#    && tar xfz ${boost_dir}.tar.gz \
#    && rm ${boost_dir}.tar.gz \
#    && cd ${boost_dir} \
#    && ./bootstrap.sh \
#    && ./b2 --without-python --prefix=/usr -j $(grep -c ^processor /proc/cpuinfo) link=shared runtime-link=shared install \
#    && cd .. && rm -rf ${boost_dir} && ldconfig


ENV NDNCPPVERSION v0.10
RUN git clone https://github.com/named-data/ndn-cpp.git && \
    cd ndn-cpp && git checkout $NDNCPPVERSION && \
    ./configure --disable-doxygen-doc &&\
    make &&\
    make install &&\
    /sbin/ldconfig

ENV NDNVERSION 0.4.1
#Install NDN-CXX
RUN git clone -b trevor --depth 1 https://github.com/ballardt/ndn-cxx.git && \
    cd ndn-cxx &&\
    ./waf configure  &&\
    ./waf &&\
    ./waf install &&\
    /sbin/ldconfig

#install NFD
RUN git clone -b trevor --depth 1 --recursive https://github.com/ballardt/NFD &&\
     cd NFD &&\
    ./waf configure --boost-mt  &&\
    ./waf &&\
    ./waf install &&\
    cp build/nfd.conf.sample /usr/local/etc/ndn/nfd.conf


RUN git clone https://github.com/named-data/ndn-tools &&\
     cd ndn-tools &&\
     git checkout tags/ndn-tools-0.4 &&\
    ./waf configure --disable-pib --disable-chunks &&\
    ./waf &&\
    ./waf install

#RUN mkdir /videos

#Install NDN-FS (ported)
#RUN git clone https://github.com/remap/ndnfs-port.git
#RUN export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig
#RUN export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
#RUN cd ndnfs-port &&\
#    ./waf configure --boost-lib=/usr/lib/x86_64-linux-gnu/ &&\
#    ./waf

ADD dash /videos

ENV REPONG ndn-dash 
#Install REPO-NG
RUN echo 'Build Repo NG 2.2'
RUN git clone https://github.com/dstohr/repo-ng.git &&\
    cd repo-ng &&\
    git checkout $REPONG &&\
    ./waf configure  &&\
    ./waf &&\
    ./waf install &&\
    mkdir -p /var/db/ndn-repo-ng &&\
    cp repo-ng.conf.sample /usr/local/etc/ndn/repo-ng.conf

ADD repo-ng.conf /usr/local/etc/ndn/

ADD ndn-entrypoint.sh /ndn-entrypoint.sh
ADD copy_files_to_ndnfs.sh /copy_files_to_ndnfs.sh
ADD copy_files_to_repong.sh /copy_files_to_repong.sh
ADD copy_bbb_repong.sh /copy_bbb_repong.sh
ADD nfd.conf /usr/local/etc/ndn/nfd.conf
ADD repo-ng.sh /repo-ng.sh
RUN mv /usr/sbin/tcpdump /usr/bin/tcpdump
 
ARG STORAGE_TYPE
ENV STORAGE_TYPE ${STORAGE_TYPE}
CMD /bin/bash /ndn-entrypoint.sh $STORAGE_TYPE
