Interest Control Protocol For NDN (ndn-icp-download)
======================================

This is a library implementing an Interest control protocol that applications can use for downloading data.
The protocol implements:
		- reliable data fetching
		- Interest window based flow control based on delay measurements;
		- congestion control using remote active queue management;
		- multipath management by using explicit path labeling if NFD
		  supports path labeling;
		- NFD patch to enable path labeling is also available and 
		  provided in this package.
A complete description and analysis of the protocol is available in
the following publication:

Carofiglio, G.; Gallo, M.; Muscariello, L.; Papalini, M.; Sen Wang, 
"Optimal multipath congestion control and request forwarding in 
Information-Centric Networks," Network Protocols (ICNP), 
21st IEEE International Conference on , vol., no., pp.1,10, 7-10 Oct. 2013
http://dx.doi.org/10.1109/ICNP.2013.6733576
  
The application requires installation of ndn-cxx and boost libraries. 

## Prerequisites ##

Compiling and running ndn-icp-download requires the following dependencies:

1. C++ Boost Libraries version >= 1.48

    On Ubuntu 14.04 and later

        sudo apt-get install libboost-all-dev

    On OSX with macports

        sudo port install boost

    On OSX with brew

        brew install boost

    On other platforms Boost Libraries can be installed from the packaged version for the
    distribution, if the version matches requirements, or compiled from source
    (http://www.boost.org)

2. ndn-cxx library (https://github.com/named-data/ndn-cxx)

    For detailed installation instructions refer to
    https://github.com/named-data/ndn-cxx

3. NDN forwarding daemon (https://github.com/named-data/NFD)

-----------------------------------------------------

## 1. Compile And Installation Instructions: ##

    ./waf configure --prefix=/usr
    ./waf
    sudo ./waf install

## 2. Usage

This library can be used by including the header file <ndn-icp-download.hpp>.
If you correctly installed the library you should have this file under /usr/include.

The application can exploit the library in this way:

```cpp
#include <ndn-icp-download.hpp>
#include <fstream>

namespace ndn
{

int
main(int argc, char** argv)
{
  bool allow_stale = false;
  int sysTimeout = -1;
  unsigned InitialMaxwindow = RECEPTION_BUFFER - 1; // Constants declared inside ndn-icp-download.hpp
  unsigned int m_timer = DEFAULT_INTEREST_TIMEOUT;
  double drop_factor = DROP_FACTOR;
  double p_min = P_MIN;
  double beta = BETA;
  unsigned int gamma = GAMMA;
  unsigned int samples = SAMPLES;
  unsigned int nchunks = -1; // XXX chunks=-1 means: download the whole file!
  bool output = false;
  bool report_path = false;

  std::string name = argv[1];

  NdnIcpDownload ndnicpdownload(InitialMaxwindow, gamma, beta, allow_stale, m_timer/1000);

  shared_ptr<DataPath> initPath = make_shared<DataPath>(drop_factor, p_min, m_timer, samples);
  ndnicpdownload.insertToPathTable(DEFAULT_PATH_ID, initPath);

  ndnicpdownload.setCurrentPath(initPath);
  ndnicpdownload.setStartTimetoNow();
  ndnicpdownload.setLastTimeOutoNow();

  if (output)
    ndnicpdownload.enableOutput();
  if (report_path)
    ndnicpdownload.enablePathReport();

  if (sysTimeout >= 0)
    ndnicpdownload.setSystemTimeout(sysTimeout);

  std::ofstream output_file;
  output_file.open("book.pdf", std::ios::out | std::ios::binary);

  std::vector<char> * data = new std::vector<char>;
  bool res;

  while(true)
  {

    res = ndnicpdownload.download(name, data, -1, 1000);

    std::cout << data->empty() << std::endl;

    for (std::vector<char>::const_iterator i = data->begin(); i != data->end(); ++i)
      output_file << *i;

    if (res)
      break;
  }

  output_file.close();

  return 0;
}
}

int
main(int argc, char** argv)
{
  return ndn::main(argc, argv);
}
```
    
   To ensure compatibity with all the ICN repositories, the support for PATH_LABELLING has been
   disabled. You can re-enabled it by adding the configuration option --with-path-labelling.

   ```
        ./waf configure --prefix=/usr --with-path-labelling
   ```

   There are basically 2 ways to download a file using this library:

   - The first consists in downloading the whole file, storing it in RAM. It is useful for small files,
       that can be stored into a buffer.
       You have to create a std::vector<char> and then pass a pointer to it to the download method if the class NdnIcpDownload,
       in this way:

          ```cpp
          std::vector<char> * data = new std::vector<char>;
          bool res = ndnicpdownload.download(name, data);
          ```
   - The second way works as the data transfer implemented by the system call recv. Basically you create a buffer and then
     you download a certain number of chunk, as the example above. The function will return after the download of the number of chunk specified.
     It is so possible to download a certain amount of data, store it in the hard disk and then continue the download,
     without wasting memory with buffer of Gigabytes of data.
         
         ```cpp
         std::vector<char> * data = new std::vector<char>;
         bool res;

         while(true)
         {

           res = ndnicpdownload.download(name, data, -1, 1000);

           std::cout << data->empty() << std::endl;
           
           // XXX It would be better to store the data through blocks, not char by char.
           for (std::vector<char>::const_iterator i = data->begin(); i != data->end(); ++i)
             output_file << *i;

           if (res)
            break;
         }
      ```
   If there is any error during the download, the download  method will throw an exception.
      
   If you change the name in the middle of another download, the download will restart from zero.
