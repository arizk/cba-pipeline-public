/*
 *  Timing of NDN Interest packets and NDN Data packets while downloading.
 */

#include "chunktimeObserver.hpp"


//for logging/terminal output
#include "stdio.h"
#include <sstream>
#include <iostream>

// To catch a potential OOR map issue
#include <stdexcept>

// For bandit context
#include <chrono>
#include <thread>

ChunktimeObserver::ChunktimeObserver(int sample)
{
    this->sampleSize = sample;
    this->lastInterestKey = "Dummy";
    this->lastDataKey = "Dummy";
    this->sampleNumber = 0;
    this->cumulativeTp = 0.0;
    pFile = fopen("log_ChunktimeObserver", "a");
    startTime = boost::chrono::system_clock::now();
}

ChunktimeObserver::~ChunktimeObserver() 
{
    fclose(pFile);
}


void ChunktimeObserver::addThroughputSignal(ThroughputNotificationSlot slot) {
	signalThroughputToDASHReceiver.connect(slot);
}

void ChunktimeObserver::addContextSignal(ContextNotificationSlot slot) {
	signalContextToDASHReceiver.connect(slot);
}

//logging method
void ChunktimeObserver::writeLogFile(const std::string szString)
{
  double timePoint = boost::chrono::duration_cast<boost::chrono::microseconds>(boost::chrono::system_clock::now() - startTime).count();
  timePoint = timePoint * 0.000001; //from microseconds to seconds
  fprintf(pFile, "%f",timePoint);
  fprintf(pFile, "\t%s",szString.c_str());
}

void ChunktimeObserver::onInterestSent(const ndn::Interest &interest)
{
    boost::chrono::time_point<boost::chrono::system_clock> timestamp = boost::chrono::system_clock::now();

    //Build the key and key-1
    int chunkNo = interest.getName()[-1].toSegment();
    int chunkNoMinusOne = chunkNo - 1;
    std::string chunkID = interest.getName().toUri();

    std::string key = chunkID.substr(0, 9).append(std::to_string(chunkNo));
    std::string keyMinusOne = chunkID.substr(0, 9).append(std::to_string(chunkNoMinusOne));

    //Insert timestamp into the Map
    if (timingMap.count(keyMinusOne) == 0) //no key-1 in map -> first Interest of Segment
    {
        if(this->lastInterestKey.compare("Dummy") == 0)   //no lastInterestKey set -> first Interest overall
        {
            //logging start
            std::stringstream stringstreamI;
            stringstreamI << "IKey: " << key << "\t" << "(no) IKey-1: " << keyMinusOne << "\n";
            this->writeLogFile(stringstreamI.str());
            //logging end

            std::vector<boost::chrono::time_point<boost::chrono::system_clock>> timeVector(4);
            timeVector.at(0) = timestamp;
            timeVector.at(1) = timestamp; timeVector.at(2) = timestamp; timeVector.at(3) = timestamp; //TODOTIMO: initialise new vector (with dummy values for yet unknown spots)

            timingMap.emplace(key, timeVector); //as Interest1
            this->lastInterestKey = key;
        } else                              //last Key set -> NOT first Interest overall
        {
            //logging start
            std::stringstream stringstreamI;
            stringstreamI << "IKey: " << key << "\t" << "LastIKey: " << this->lastInterestKey << "\n";
            this->writeLogFile(stringstreamI.str());
            //logging end

            timingMap.at(this->lastInterestKey).at(1) = timestamp; //as Interest2
            
            std::vector<boost::chrono::time_point<boost::chrono::system_clock>> timeVector(4);
            timeVector.at(0) = timestamp;
            timingMap.emplace(key, timeVector); //as Interest1

            this->lastInterestKey = key;
        }
    }
    else //NOT first Interest
    {
        //logging start
        std::stringstream stringstreamI;
        stringstreamI << "IKey: " << key << "\t" << "IKey-1: " << keyMinusOne << "\n";
        this->writeLogFile(stringstreamI.str());
        //logging end

        timingMap.at(keyMinusOne).at(1) = timestamp; //as Interest2

        std::vector<boost::chrono::time_point<boost::chrono::system_clock>> timeVector(4);
        timeVector.at(0) = timestamp;
        timingMap.emplace(key, timeVector); //as Interest1

        this->lastInterestKey = key;
    }
}

double ChunktimeObserver::onDataReceived(const ndn::Data &data)
{
    try {
        boost::chrono::time_point<boost::chrono::system_clock> timestamp = boost::chrono::system_clock::now();

        //Build key and key-1
        int chunkNo = data.getName()[-1].toSegment();
        int chunkNoMinusOne = chunkNo - 1;
        std::string chunkID = data.getName().toUri();
        std::string tail = chunkID.substr(std::max<int>(chunkID.size()-6,0));
        bool initChunk = (tail.compare("%00%00") == 0); //true, if it is the first chunk of a segment (no viable measurement)

        std::string key = chunkID.substr(0, 9).append(std::to_string(chunkNo));
        std::string keyMinusOne = chunkID.substr(0, 9).append(std::to_string(chunkNoMinusOne));

		int thisRTT = boost::chrono::duration_cast<boost::chrono::microseconds>(timestamp - timingMap.at(key).at(0)).count();
		int numHops = 0;
		auto numHopsTag = data.getTag<ndn::lp::NumHopsTag>();
		if (numHopsTag != nullptr) {
			numHops = (uint64_t)numHopsTag->get(); // Comes out as a unsigned long int
		}
		signalContextToDASHReceiver((uint64_t)thisRTT, (uint64_t)numHops);

        //Insert timestamp into the map
        if (timingMap.count(keyMinusOne) == 0) //no key-1 in map -> first Data packet of the Segment
        {
            if(this->lastDataKey.compare("Dummy") == 0)   //no lastDataKey set -> first Data packet overall)
            {
                std::stringstream stringstreamD;
                stringstreamD << "DKey: " << key << "\t" << "(no) DKey-1: " << keyMinusOne << "\n";
                this->writeLogFile(stringstreamD.str());

                timingMap.at(key).at(2) = timestamp; //as Data1

                this->lastDataKey = key;
                return 0;   //what to return when there is no gap b.c. of first Data packet of Segment?
            } else
            {
                std::stringstream stringstreamD;
                stringstreamD << "DKey: " << key << "\t" << "LastDKey: " << this->lastDataKey << "\n";
                this->writeLogFile(stringstreamD.str());
        

                timingMap.at(this->lastDataKey).at(3) = timestamp; //as Data2
                timingMap.at(key).at(2) = timestamp;         //as Data1
        
        
                //timingVector is complete for map[lastDataKey]     ->  measurement computations can be triggered now!
                int dataPacketSize = data.wireEncode().size();
                int interestDelay = this->computeInterestDelay(this->lastDataKey);
                int dataDelay = this->computeDataDelay(this->lastDataKey);
                int firstRTT = this->computeRTT(this->lastDataKey, true);
                int secondRTT = this->computeRTT(this->lastDataKey, false);
                double quotient = this->interestDataQuotient((double) dataDelay, interestDelay);
                double classicBW = this->computeClassicBW(firstRTT, dataPacketSize);
                double throughput = this->throughputEstimation(dataDelay, dataPacketSize);
                double sampledTp = 0.0;

                //SEPERATION: chunks at the beginning of a segment download may not have viable values 
                // AND out of order reception data delays also no viable values 
                // AND firstRTT must not be 0 because then, the dummy value in the timestamp vector is used (= the data delay is not viable)
                if(isValidMeasurement(throughput, firstRTT, dataDelay) && !initChunk)
                    sampledTp = this->getSampledThroughput(throughput);
                else
                    this->writeLogFile("Invalid computation.\n");

                //logging start
                std::stringstream loggingString;
                loggingString << "I-Delay: " << interestDelay << "\t" << "D-Delay: " << dataDelay << "\t" 
                            << "First RTT: " << firstRTT << "\t" << "Second RTT: " << secondRTT << "\t" 
                            << "Classic BW: " << classicBW << "\t" << "gOut/gIn: " << quotient << "\t" << "Throughput: " << throughput << "\n";
                this->writeLogFile(loggingString.str());     
                if(sampleNumber==0 && sampledTp != 0.0) //TODO Does this work?
                {
                    std::stringstream sampleString;
                    sampleString << sampleSize << "-Sampled Throughput: " << sampledTp << "\n";
                    this->writeLogFile(sampleString.str());
                }
                //logging end
                
                this->lastDataKey = key;

                if(isValidMeasurement(throughput, firstRTT, dataDelay) && !initChunk)
                    return throughput;    //unsampled throughput
                else
                    return 0;             //invalid throughput
            }
        }
        else                                    //NOT first Data packet
        {
            std::stringstream stringstreamD;
            stringstreamD << "DKey: " << key << "\t" << "DKey-1: " << keyMinusOne << "\n";
            this->writeLogFile(stringstreamD.str());


            timingMap.at(keyMinusOne).at(3) = timestamp; //as Data2
            timingMap.at(key).at(2) = timestamp;         //as Data1

            //timingVector is complete for map[keyMinusOne]     ->  measurement computations can be triggered now!
            int dataPacketSize = data.wireEncode().size();
            int interestDelay = this->computeInterestDelay(keyMinusOne);
            int dataDelay = this->computeDataDelay(keyMinusOne);
            int firstRTT = this->computeRTT(keyMinusOne, true);
            int secondRTT = this->computeRTT(keyMinusOne, false);
            double quotient = this->interestDataQuotient((double) dataDelay, interestDelay);
            double classicBW = this->computeClassicBW(secondRTT, dataPacketSize);
            double throughput = this->throughputEstimation(dataDelay, dataPacketSize);
            double sampledTp = 0.0;

            //SEPERATION: chunks at the beginning of a segment download may not have viable values 
            // AND out of order reception data delays also no viable values 
            // AND firstRTT must not be 0 because then, the dummy value in the timestamp vector is used (= the data delay is not viable)
            //only consider values bigger 0.1
                if(isValidMeasurement(throughput, firstRTT, dataDelay) && !initChunk)
                    sampledTp = this->getSampledThroughput(throughput);
                else
                    this->writeLogFile("Invalid computation.\n");
            

            //logging start
            std::stringstream loggingString;
            loggingString << "I-Delay: " << interestDelay << "\t" << "D-Delay: " << dataDelay << "\t" 
                        << "First RTT: " << firstRTT << "\t" << "Second RTT: " << secondRTT << "\t" 
                        << "Classic BW: " << classicBW << "\t" << "gOut/gIn: " << quotient << "\t" << "Throughput: " << throughput << "\n";       
            this->writeLogFile(loggingString.str());     
            if(sampleNumber==0 && sampledTp != 0.0)
            {
                std::stringstream sampleString;
                sampleString << sampleSize << "-Sampled Throughput: " << sampledTp << "\n";
                this->writeLogFile(sampleString.str());
            }
            //logging end


            this->lastDataKey = key;

            return throughput;    //unsampled
        }
    }
    catch (const std::out_of_range& oor) {
        std::cout << "Out of range error: " << oor.what() << std::endl;
    }
}
    bool ChunktimeObserver::isValidMeasurement(double throughput, int firstRTT, int dataDelay)
    {
        return dataDelay > 0 && firstRTT > 0 && throughput > 0.1;
    }


    double ChunktimeObserver::getSampledThroughput(double throughput)       //sampling: harmonic avg over n throughput computations (n is set in the contructor)
    {
        this->cumulativeTp = this->cumulativeTp + (1/throughput);
        sampleNumber++;

        if(this->sampleNumber == this->sampleSize)
        {
            double sampledThroughput = this->sampleNumber / this->cumulativeTp; // harmonic avg = n/(sum of inverses)
            this->sampleNumber = 0;
            this->cumulativeTp = 0.0;
            double convertedThroughput = sampledThroughput * 1000000;    //convertation from Mbit/s to Bit/s
            signalThroughputToDASHReceiver((uint64_t)convertedThroughput);         //trigger notification-function in DASHReceiver
            return sampledThroughput;
        }
        else
            return 0;                           //not enough samples yet
    }


    //******************************************************************************************
    //**********************************COMPUTATION FORMULAS************************************
    //******************************************************************************************

    int ChunktimeObserver::computeInterestDelay(std::string key)    //in µs (timeI2 - timeI1)
    {
        return boost::chrono::duration_cast<boost::chrono::microseconds>(timingMap.at(key).at(1) - timingMap.at(key).at(0)).count();
    }

    int ChunktimeObserver::computeDataDelay(std::string key)    //in µs (timeD2 - timeD1)
    {
        return boost::chrono::duration_cast<boost::chrono::microseconds>(timingMap.at(key).at(3) - timingMap.at(key).at(2)).count();
    }

    int ChunktimeObserver::computeRTT(std::string key, bool first)  //in µs  (first = timeD1 - timeI1,  !first = timeD2 - timeI2)
    {
        if(first)
            return boost::chrono::duration_cast<boost::chrono::microseconds>(timingMap.at(key).at(2) - timingMap.at(key).at(0)).count();
        else
            return boost::chrono::duration_cast<boost::chrono::microseconds>(timingMap.at(key).at(3) - timingMap.at(key).at(1)).count();
    }

    double ChunktimeObserver::computeClassicBW(int rtt, int size) //in MBit/s
    {
        return (double) (size*8)/(rtt/2);
    }

    double ChunktimeObserver::throughputEstimation(int gap, int size) //in Bit/µs = Mbit/s
    {
        if(gap != 0)
            return (double) (size*8)/gap;
        else
        {
            //this->writeLogFile("### Throughput Estimation: \t Data packet delay is 0 µs. DIV by ZERO! -> Throughput=0 ### \n");
            //return 0.0;
            this->writeLogFile("### Throughput Estimation: \t Data packet delay is 0 µs. DIV by ZERO! -> Data delay set to min=1 ### \n");
            return (double) (size*8)/(gap+1);
        }
    }

    
    double ChunktimeObserver::interestDataQuotient(double gIn, int gOut)
    {
        if(gIn != 0)
        return gOut/gIn;
    else
    {
        //this->writeLogFile("### Interest-Data-Quotient: \t Data packet delay is 0 µs. DIV by ZERO! -> Quotient=0 ### \n");
        //return 0.0;
        this->writeLogFile("### Interest-Data-Quotient: \t Data packet delay is 0 µs. DIV by ZERO! -> Data delay set to min=1 ### \n");
        return gOut/gIn;
    }
    }

    /*
    int ChunktimeObserver::interestDataGap(int gIn, int gOut)
    {
        return gIn - gOut;
    }

    double ChunktimeObserver::gapResponseCurve(int gIn, int gOut)
    {
        double div = gOut / gIn;

        if(div <= 1.0)
            return 1.0;
        else
            return 0.0; //dummy
    }
    */
