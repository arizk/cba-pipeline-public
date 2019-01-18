/*
 *  INDNConnection.h
 *
 * Author:Jack Samain <jsamain@cisco.com>
 *
 */

#ifndef QTSAMPLEPLAYER_LIBDASHFRAMEWORK_INPUT_INDNCONNECTION_H_
#define QTSAMPLEPLAYER_LIBDASHFRAMEWORK_INPUT_INDNCONNECTION_H_

#include "IConnection.h"
#include "IChunk.h"


namespace libdash {
	namespace framework {

		namespace input {

			class INDNConnection : public dash::network::IConnection {

			public:
				virtual ~INDNConnection(){};

				virtual int Read(uint8_t* data, size_t len) = 0;
				virtual void Init(dash::network::IChunk *chunk) = 0;
				virtual void InitForMPD(const std::string&) = 0;
			};
		}
	}
}


#endif // QTSAMPLEPLAYER_LIBDASHFRAMEWORK_INPUT_INDNCONNECTION_H_
