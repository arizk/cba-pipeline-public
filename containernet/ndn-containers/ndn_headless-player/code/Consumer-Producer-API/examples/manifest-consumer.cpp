/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2016 Regents of the University of California.
 *
 * This file is part of Consumer/Producer API library.
 *
 * Consumer/Producer API library library is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * Consumer/Producer API library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with Consumer/Producer API, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of Consumer/Producer API authors and contributors.
 */

// correct way to include Consumer/Producer API headers
//#include <Consumer-Producer-API/consumer-context.hpp>
#include "consumer-context.hpp"

// Enclosing code in ndn simplifies coding (can also use `using namespace ndn`)
namespace ndn {
// Additional nested namespace could be used to prevent/limit name contentions
namespace examples {

class CallbackContainer
{
public:
  CallbackContainer()
  : m_seenManifestSegments(0)
  , m_seenDataSegments(0)
  , m_byteCounter(0)
  {
  }
  
  void
  processPayload(Consumer& c, const uint8_t* buffer, size_t bufferSize)
  {
    std::string content((char*)buffer, bufferSize);
    m_byteCounter+=bufferSize;
    std::cout << "REASSEMBLED " << content << std::endl;
    
    std::cout << "**************************************************" << std::endl;
    std::cout << m_byteCounter << std::endl;
    std::cout << "**************************************************" << std::endl;
  }
  
  void
  countData(Consumer& c, const Data& data)
  {
    std::cout << "DATA IN CNTX" << std::endl;
    
    if (data.getContentType() == CONTENT_DATA_TYPE)
    {
      m_seenDataSegments++;
      std::cout << "Saw Content segment" << data.getName().get(-1) << std::endl;
    }
    else if (data.getContentType() == MANIFEST_DATA_TYPE)
    {
      m_seenManifestSegments++;
      std::cout << "Saw Manifest segment" << data.getName().get(-1) << std::endl;
    }
  }
  
  bool
  verifyData(Consumer& c, const Data& data)
  {
    if (data.getContentType() == CONTENT_DATA_TYPE) // this should never be true
      std::cout << "VERIFY CONTENT" << std::endl;
    else if (data.getContentType() == MANIFEST_DATA_TYPE)
      std::cout << "VERIFY MANIFEST" << std::endl;
    
    return true;
  }
  
  void
  processLeavingInterest(Consumer& c, Interest& interest)
  {
    std::cout << "LEAVES " << interest.toUri() << std::endl;
  }  

private:
  int m_seenManifestSegments;
  int m_seenDataSegments;
  int m_byteCounter;
};

int
main(int argc, char** argv)
{
  Name sampleName("/a/b/c");
      
  CallbackContainer stubs;

  Consumer c(sampleName, RDR);
  c.setContextOption(MUST_BE_FRESH_S, true);
    
  c.setContextOption(INTEREST_LEAVE_CNTX, 
        (ConsumerInterestCallback)bind(&CallbackContainer::processLeavingInterest, &stubs, _1, _2));
  
  c.setContextOption(DATA_TO_VERIFY, 
        (ConsumerDataVerificationCallback)bind(&CallbackContainer::verifyData, &stubs, _1, _2));
  
  c.setContextOption(DATA_ENTER_CNTX, 
        (ConsumerDataCallback)bind(&CallbackContainer::countData, &stubs, _1, _2));
  
  c.setContextOption(CONTENT_RETRIEVED, 
        (ConsumerContentCallback)bind(&CallbackContainer::processPayload, &stubs, _1, _2, _3));
  
  c.consume(Name());
  
  return 0;
}

} // namespace examples
} // namespace ndn

int
main(int argc, char** argv)
{
  return ndn::examples::main(argc, argv);
}
