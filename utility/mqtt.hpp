#ifndef MQTT_UTILITY_MQTT_H_
#define MQTT_UTILITY_MQTT_H_

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <list>

#define CONNACK_BYTE  0x20
#define PUBLISH_BYTE  0x30
#define PUBACK_BYTE   0x40
#define PUBREC_BYTE   0x50
#define PUBREL_BYTE   0x62
#define PUBCOMP_BYTE  0x70
#define SUBACK_BYTE   0x90
#define UNSUBACK_BYTE 0xB0
#define PINGRESP_BYTE 0xD0

typedef std::unique_ptr<uint8_t> uint8_ptr;

enum kControlPacketType {
	CONNECT = 1,
	CONNACT,
	PUBLISH,
	PUBACK,
	PUBREC,
	PUBREL,
	PUBCOMP,
	SUBSCRIBE,
	SUBACK,
	UNSUBSCRIBE,
	UNSUBACK,
	PINGREQ,
	PINGRESP,
	DISCONNECT
};

namespace mqtt {
	struct Header {
		uint8_t bits;
		uint8_t remaining_length;
	};

	struct Connect {
		Header header;

		struct {
			uint8_t connect_flags;
			uint16_t keepalive;
			uint8_t level;
		} variable_header;

		struct {
			std::string cliend_id;
			std::string username;
			std::string password;
			std::string will_topic;
			std::string will_message;
		} payload;
	};

	struct Connack {
		Header header;
		uint8_t flags;
		uint8_t rc;
	};

	struct Publish {
		Header header;
		std::string topic;
		uint16_t pkt_id;
		std::string payload;
	};

	struct Subscribe {
		Header header;
		uint16_t pkt_id;
		std::multimap<std::string, uint8_t> topic_and_qos;
	};

	struct Unsubscribe {
		Header header;
		uint16_t pkt_id;
		std::vector<std::string> topics;
	};

	struct Suback {
		Header header;
		uint16_t pkt_id;
		std::vector<uint8_t> rcs;
	};

	struct AckPacket {
		Header header;
		uint16_t pkt_id;
	};

	typedef struct AckPacket Puback;
	typedef struct AckPacket Pubrec;
	typedef struct AckPacket Pubrel;
	typedef struct AckPacket Pubcomp;
	typedef struct AckPacket Unsuback;
	typedef struct Header Pingreq;
	typedef struct Header Pingresp;
	typedef struct Header Disconnect;

	typedef std::variant<AckPacket, Header, Connect, Connack, Suback, Publish, Subscribe, Unsubscribe, Disconnect> packet;

	int EncodeLength(uint8_t *buffer, size_t len);
	long long DecodeLength(const uint8_t *buffer);

	//from buffer to packet object
	size_t UnpackConnect(const uint8_t *buffer, Header *head, Connect *pkt);
	size_t UnpackPublish(const uint8_t* buffer, Header* head, Publish* pkt);
	long long UnpackSubscribe(const uint8_t* buffer, Header* head, Subscribe* pkt);
	size_t UnpackUnsubscribe(const uint8_t* buffer, Header* head, Unsubscribe* pkt);
	size_t UnpackAck(const uint8_t* buffer, Header* head, AckPacket* pkt);

	//create a package from function parameters
	AckPacket PacketAck(const uint8_t byte, const uint16_t pkt_id);
	Connack PacketConnack(const uint8_t byte, const uint8_t flags, const uint8_t rc);
	Suback PacketSuback(const uint8_t byte, const uint16_t pkt_id, const uint16_t rc_len, const uint8_t* rcs);
	Publish PacketPublish(const uint8_t byte, const uint16_t pkt_id, const std::string topic, const std::string payload);

	//fill buffer with packet
	uint8_ptr PackHeader(Header* hdr);
	uint8_ptr PackAck(AckPacket* ack);
	uint8_ptr PackConnack(Connack* con);
	uint8_ptr PackSuback(Suback* sub);
	uint8_ptr PackPublish(Publish* pub);
	uint8_ptr PackPingreq(Pingreq* ping);
	uint8_ptr PackPingresp(Pingresp* ping);

}	// namespace mqtt

#endif