#!/bin/bash

VIDEO_URL="http://www-itec.uni-klu.ac.at/ftp/datasets/DASHDataset2014/BigBuckBunny/2sec/bunny_2133691bps/"
FOLDER_NDN_NAME="bbb-rates-ndn/"
FOLDER_TCP_NAME="videos/"
RATES="bunny_2500kbit bunny_3000kbit/ bunny_3500kbit/ bunny_4000kbit/ bunny_4500kbit/ bunny_5000kbit/"
#SERVER="pirl-ndn-5.cisco.com"
SERVER="10.60.17.171"
#RATES="bunny_3840360bps/"

####################################################
# ADD LOCAL NDN REPOSITORY                         #
# UPDATE ARCHIVE AND INSTALL REQUIRED NDN SOFTWARE #
####################################################

install_software() {

    # While loops are due to failures when creating the base container.

    echo "deb http://$SERVER/dists trusty/" > /etc/apt/sources.list.d/icn-repo.list
    echo "# deb-src http://$SERVER/dists trusty/" >> /etc/apt/sources.list.d/icn-repo.list
    apt-add-repository -y ppa:ubuntu-sdk-team/ppa
    apt-add-repository -y ppa:canonical-qt5-edgers/qt5-proper

    apt-get --force-yes -y update && \
    apt-get --force-yes -y install build-essential apache2 git libpcap-dev openssh-client openssh-server ifstat sysstat nano psmisc \
    ndn-cxx ndn-cxx-dev nfd iperf ndndump ndn-dissect ndnpeek ndnping ndn-tools nfd-autoreg ndn-tlv-ping ndn-pib \
    ndn-dissect-wireshark ndn-autoconfig-server cmake iperf3 ndn-virtual-repo ndn-icp-download libndn-icp  \
    repo-ng vlc-plugin-ndn qtmultimedia5-dev qtbase5-dev libqt5widgets5 libqt5core5a libqt5gui5 libqt5multimedia5 \
    libqt5multimediawidgets5 libqt5opengl5 libav-tools libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev \
    libavutil-dev libpostproc-dev libswscale-dev libavresample-dev libxml2-dev libcurl4-openssl-dev

    apt-get clean

}

#################################################################
# ASYNCHRONOUS DOWNLOAD OF THE VIDEO CHUNKS DURING INSTALLATION #
#################################################################

chunk_download() {

    pushd /tmp
    wget $SERVER/ndn-dash/bbb-rates-ndn.tar && tar -xf bbb-rates-ndn.tar
    wget $SERVER/ndn-dash/bbb-rates-tcp.tar && tar -xf bbb-rates-tcp.tar
    popd
}

##################################################
# SET PASSWORD AUTHENTICATION FOR SSH CONNECTION #
##################################################

set_ssh_password_auth() {

    sed -i "s/PasswordAuthentication no/PasswordAuthentication yes/" /etc/ssh/sshd_config
    while [ $? != "0" ]
    do
        sed -i "s/PasswordAuthentication no/PasswordAuthentication yes/" /etc/ssh/sshd_config
    done
}


###################
# SET DNS SERVERS #
###################

set_dns_servers() {

    echo "nameserver 144.254.71.184" >> /etc/resolvconf/resolv.conf.d/head
    echo "search cisco.com" >> /etc/resolvconf/resolv.conf.d/head

}

##############################################
# FILL NDN REPOSITORY WITH DASH VIDEO CHUNKS #
##############################################

fill_ndn_repo() {

    mv /tmp/$FOLDER_TCP_NAME /var/www/html/

    pushd /tmp/$FOLDER_NDN_NAME

    #wget http://$SERVER/ndn-dash/mpd.mpd
    #wget http://$SERVER/ndn-dash/bbb.mpd
    #sed -i '0,/prefix .*/  s/prefix .*$/prefix "ndn:\/n"/' /etc/ndn/repo-ng.conf
    #sed -i 's/max-packets 100000/max-packets 9999999999/' /etc/ndn/repo-ng.conf
    #service repo-ng stop
    #service repo-ng start

    #sleep 20

    #ndnputfile -c 1400 -D -u ndn:/localhost/repo-ng ndn:/n/mpd mpd.mpd
    #ndnputfile -c 1400 -D -u ndn:/localhost/repo-ng ndn:/n/mpd2 bbb.mpd
    #rm mpd.mpd
    #rm mpd2.mpd

    ndn_rate=8
    ndn_rates_char=(a b c d e f k l m n o p q)
    ndnputfile -v -x 3600000 ndn:/localhost/repo-ng ndn:/n/mpd $VIDEOS_FOLDER/bbb/mpd

    for rate in $RATES
    do
        cd $FOLDER_NDN_NAME$rate
        rm -f robots.txt
        chunk_number=1

        if [ "$ndn_rate" -gt 5 ]
        then
          echo "ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/init binit.mp4"
          ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/init binit.mp4


          for chunk in b{1..1270}.m4s
          do
            echo "ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/$chunk_number $chunk"
            ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/$chunk_number $chunk
            ((chunk_number++))
          done
        else

          echo "ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/init BigBuckBunny_2s_init.mp4"
          ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/init BigBuckBunny_2s_init.mp4

          echo "ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/noseg BigBuckBunny_2snonSeg.mp4"
          ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/noseg BigBuckBunny_2snonSeg.mp4

          for chunk in BigBuckBunny_2s{1..299}.m4s
          do
            echo "ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/$chunk_number $chunk"
            ndnputfile -c 1400 -u -D ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/$chunk_number $chunk
            ((chunk_number++))
          done
        fi
        ((ndn_rate++))
    done

    popd

}

######################################
# INSTALL LIBDASH AND QTSAMPLEPLAYER #
######################################

install_libdash() {

    cd /home/ubuntu

    wget http://$SERVER/ndn-dash/ndn-dash.tar
    tar -xf ndn-dash.tar && rm ndn-dash.tar
    cd ndn-dash/libdash && mkdir -p build && cd build/
    cmake .. && make

    cd ../qtsampleplayer/ && mkdir build && cd build/
    cmake .. && make

    cd /home/ubuntu

    chown -R ubuntu:ubuntu ndn-dash

}

sleep 5

#chunk_download
#install_software
#set_ssh_password_auth
#set_dns_servers
fill_ndn_repo
#install_libdash

exit 0
