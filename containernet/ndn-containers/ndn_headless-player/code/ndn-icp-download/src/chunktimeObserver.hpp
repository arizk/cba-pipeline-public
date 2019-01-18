/*
 *  Timing of NDN Interest packets and NDN Data packets while downloading.
 */

#ifndef CHUNKTIME_OBSERVER_H
#define CHUNKTIME_OBSERVER_H

#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>
#include <map>
#include <vector>
#include <boost/chrono/include.hpp>
#include <string>
#include <boost/signals2.hpp>

class ChunktimeObserver
{
public:
    ChunktimeObserver(int samples);
       
    ~ChunktimeObserver();

    /**
     *  Used signal to trigger DASHReceiver-Function (notifybpsChunk and context) from ChunktimeObserver
     */
    typedef boost::signals2::signal<void (uint64_t)>::slot_type ThroughputNotificationSlot;
    typedef boost::signals2::signal<void (uint64_t, uint64_t)>::slot_type ContextNotificationSlot;
    void addThroughputSignal(ThroughputNotificationSlot slot);
    void addContextSignal(ContextNotificationSlot slot);

    /**
     *  Used to log the timing measurements seperately.
     */
    void writeLogFile(const std::string szString);
    

    /**
     *  Insert the timestamps of the sent Interest into the map.
     */
    void    onInterestSent(const ndn::Interest &interest);

    /**
     *  Insert the timestamps of the received Data packet into the map.
        @return     (double) the measured throughput OR "0.0" when only 1 Data packet is received yet (-> no Data Delay computation possible)   in MBit/s !
     */
    double    onDataReceived(const ndn::Data &data);

     

private:
    //Signal triggering the NotifybpsChunk-Function in DASHReceiver
    boost::signals2::signal<void(uint64_t)> signalThroughputToDASHReceiver;
    boost::signals2::signal<void(uint64_t, uint64_t)> signalContextToDASHReceiver;

    //   map<   key,           vector<  timeI1, timeI2, timeD1, timeD2  >   >
    std::map<std::string, std::vector<boost::chrono::time_point<boost::chrono::system_clock>>> timingMap;  

    //  Some magic to get the key from the last element if we can not compute it.
    std::string lastInterestKey;
    std::string lastDataKey;

    //for sampling over chunk throughput measurements
    int sampleSize;
    int sampleNumber;
    double cumulativeTp;

    //for logging
    FILE* pFile;
    boost::chrono::time_point<boost::chrono::system_clock> startTime;


    /**
     *  @param samplesize, throughput
     *  @eturn (double) avg throughput over samplesize OR 0.0 (if not enough samples gathered yet)
     */
    double getSampledThroughput(double throughput);


    /**
     *  @param  the string build from the last data packet
     *  @eturn (int) delay between two consecutive Interests (in microseconds µs)
     */
    int computeInterestDelay(std::string key);

    /**
     *  @param  (string) the key built from the last data packet
     *  @eturn (int)  delay between two consecutive Data packets (in microseconds µs)
     */
    int computeDataDelay(std::string key);

    /**
     *  @param  (string) the key built from the last data packet,   (bool) if the first Interest-Data pair is wanted (true) or the second pair (false)
     *  @eturn (int)  delay between two consecutive Data packets (in microseconds µs)
     */
    int computeRTT(std::string key, bool first);

    /**
     *  @param  (string) the rtt in µs,   (int) the datasize in Byte
     *  @eturn (double)  computed BW with the formula [datasize/(rount-trip time/2)] (in Bit/µs = MBit/s)
     */
    double computeClassicBW(int rtt, int size);

    /**
     *  @param  (int) the gap between two consecutive Data packets in µs,   (int) the datasize in Byte
     *  @eturn (double)  computed throughput with the formula [datasize/gap] (in Bit/µs = MBit/s)
     */
    double throughputEstimation(int gap, int size);

    /**
     *  @param (int) gap between two consecutive Data packets (incoming), (int) gap between two consecutive Interest packets (outgoing)
     *  @return (double)
     */
    double interestDataQuotient(double gIn, int gOut);

    bool isValidMeasurement(double throughput, int firstRTT, int dataDelay);


    //implement when needed
    int interestDataGap(int gIn, int gOut);
    double gapResponseCurve(int gIn, int gOut);

};//end class ChunktimeObserver

#endif //CHUNKTIME_OBSERVER_H

