#include "mqtt.hpp"



int mqtt::EncodeLength(uint8_t* buffer, size_t len) {
	int bytes = 0;
	int d;
	buffer[1] = 0x00;

	do {
		d = len % 128;
		len /= 128;
		if (len > 0)
			d |= 128;
		buffer[bytes++] = d;

	} while (len > 0);
	return bytes;
}

long long mqtt::DecodeLength(const uint8_t* buffer) {
	char byte = 0;
	int multiplier = 1;
	unsigned long long value = 0;

	do
	{
		byte = *buffer;
		value += (byte & 127) * multiplier;
		multiplier *= 128;
		buffer++;
	} while ((byte & 128) != 0);
	return value;
}

size_t mqtt::UnpackConnect(const uint8_t* buffer, mqtt::Header* head, mqtt::Connect* pkt) {

	pkt->header = *head;

	size_t size = mqtt::DecodeLength(buffer);

	buffer += 8;

	pkt->variable_header.level = *buffer;

	buffer++;

	pkt->variable_header.connect_flags = *buffer;

	buffer++;

	pkt->variable_header.keepalive = (*buffer << 8u) | (*(buffer + 1));

	buffer += 2;

	uint16_t len;
	len = (*buffer << 8u) | (*(buffer + 1));
	buffer += 2;

	if (len > 0) {
		pkt->payload.cliend_id = std::string((char*)buffer, len);
	}
	buffer += len;
	if ((pkt->variable_header.connect_flags & 0x4)) {
		len = (*buffer << 8u) | (*(buffer + 1));
		buffer += 2;
		pkt->payload.will_topic = std::string((char*)buffer, len);
		buffer += len;
		len = (*buffer << 8u) | (*(buffer + 1));
		buffer += 2;
		pkt->payload.will_message = std::string((char*)buffer, len);
		buffer += len;
	}

	if ((pkt->variable_header.connect_flags & 0x80)) {
		len = (*buffer << 8u) | (*(buffer + 1));
		buffer += 2;
		pkt->payload.username = std::string((char*)buffer, len);
		buffer += len;
	}

	if ((pkt->variable_header.connect_flags & 0x40)) {
		len = (*buffer << 8u) | (*(buffer + 1));
		buffer += 2;
		pkt->payload.password = std::string((char*)buffer, len);
		buffer += len - 1;
	}

	return size;
}

size_t mqtt::UnpackPublish(const uint8_t* buffer, Header* head, Publish* pkt)
{
	mqtt::Publish* pub = pkt;

	pub->header = *head;

	size_t len = mqtt::DecodeLength(buffer + 1);
	buffer += 2;

	uint16_t topic_len = (*buffer << 8u) | (*(buffer + 1));
	
	buffer += 2;

	pub->topic = std::string((char*)buffer, topic_len);
	buffer += topic_len;

	if (((pub->header.bits & 0x6) >> 1u) > 0) {
		
		pub->pkt_id = (*buffer << 8u) | (*(buffer + 1));
		buffer += 2;
		topic_len += 2;
	}
	
	pkt->payload = std::string((char*)buffer, len - topic_len - 2);

	return len;
}

long long mqtt::UnpackSubscribe(const uint8_t* buffer, Header* head, Subscribe* pkt)
{
	
	long long len = mqtt::DecodeLength(buffer + 1);
	size_t remaining_len = len;

	Subscribe* sub = pkt;
	sub->header = *head;

	buffer += 2;

	sub->pkt_id = (*buffer << 8u) | (*(buffer + 1));
	buffer += 2;
	remaining_len -= sizeof(uint16_t);

	while(remaining_len > 0) { //can work alg op with remaining_len ?
		remaining_len -= sizeof(uint16_t);
		uint16_t topic_len = (*buffer << 8u) | (*(buffer + 1));
		buffer += 2;
		sub->topic_and_qos.insert(
			std::make_pair(std::string((char*)buffer, topic_len), *(buffer + topic_len)));

		if((*(buffer + topic_len) & 0xFC) != 0) {
			return -1;
		}

		buffer += topic_len + 1;
		remaining_len -= (topic_len + sizeof(uint8_t));
	}
	
	return len;
}

size_t mqtt::UnpackUnsubscribe(const uint8_t* buffer, Header* head, Unsubscribe* pkt) {
	size_t len = DecodeLength(buffer + 1);
	size_t remaining_bytes = len;
	Unsubscribe* unsub = pkt;
	unsub->header = *head;

	buffer += 2;

	unsub->pkt_id = (*buffer << 8u) | (*(buffer + 1));
	buffer += 2;
	remaining_bytes -= sizeof(uint16_t);

	while(remaining_bytes > 0) {
		uint16_t topic_len = (*buffer << 8u) | (*(buffer + 1));
		buffer += 2;
		unsub->topics.push_back(std::string((char*)buffer, topic_len));
		buffer += topic_len;
		remaining_bytes -= topic_len;
		remaining_bytes -= 2;
	}

	return len;
}

size_t mqtt::UnpackAck(const uint8_t* buffer, Header* head, AckPacket* pkt) {
	size_t len = DecodeLength(buffer + 1);
	AckPacket* paket = pkt;

	paket->header = *head;

	buffer += 2;

	paket->pkt_id = (*buffer << 8u) | (*(buffer + 1));
	buffer += 2;

	return len;
}

mqtt::AckPacket mqtt::PacketAck(const uint8_t byte, const uint16_t pkt_id) {
	AckPacket ack;
	ack.header.bits = byte;
	ack.header.remaining_length = 0x02;
	ack.pkt_id = pkt_id;

	return ack;
}

mqtt::Connack mqtt::PacketConnack
	(const uint8_t byte, const uint8_t flags, const uint8_t rc) {

	Connack conk;
	conk.header.bits = byte;
	conk.header.remaining_length = 0x02;
	conk.flags = flags;
	conk.rc = rc;
	return conk;
}

mqtt::Suback mqtt::PacketSuback
	(const uint8_t byte, const uint16_t pkt_id, const uint16_t rc_len, const uint8_t* rcs) {
	mqtt::Suback sub;
	sub.header.bits = byte;
	sub.pkt_id = pkt_id;
	sub.rcs.reserve(rc_len);
	std::copy(rcs, rcs + rc_len, std::back_inserter(sub.rcs));
	return sub;
}

mqtt::Publish mqtt::PacketPublish
	(const uint8_t byte, const uint16_t pkt_id, const std::string topic, const std::string payload) {

	Publish pub;
	pub.header.bits = byte;
	pub.pkt_id = pkt_id;
	pub.topic = topic;
	pub.payload = payload;

	return pub;
}


uint8_ptr mqtt::PackHeader(mqtt::Header* hdr) {

	uint8_ptr ptr{ new uint8_t[2] };
	ptr.get()[0] = hdr->bits;
	ptr.get()[1] = 0x00;

	return ptr;
}

uint8_ptr mqtt::PackAck(mqtt::AckPacket* ack) {
	uint8_ptr ptr{ new uint8_t[4] };
	auto pack = ptr.get();

	pack[0] = ack->header.bits;
	pack[1] = 0x02;
	pack[2] = uint8_t(ack->pkt_id >> 8u);
	pack[3] = uint8_t(ack->pkt_id);

	return ptr;
}

uint8_ptr mqtt::PackConnack(mqtt::Connack* con) {
	uint8_ptr ptr{ new uint8_t[4] };
	ptr.get()[0] = con->header.bits;
	ptr.get()[1] = 0x02;
	ptr.get()[2] = con->flags;
	ptr.get()[3] = con->rc;

	return ptr;
}

uint8_ptr mqtt::PackSuback(mqtt::Suback* sub) {
	uint8_ptr ptr{ new uint8_t[2 + sizeof(uint16_t) + sub->rcs.size()] };
	ptr.get()[0] = sub->header.bits;
	mqtt::EncodeLength(ptr.get() + 1, sizeof(uint16_t) + sub->rcs.size());
	ptr.get()[2] = uint8_t(sub->pkt_id >> 8u);
	ptr.get()[3] = uint8_t(sub->pkt_id);
	
	for(int i = 0; i < sub->rcs.size(); i++) {
		ptr.get()[4 + i] = sub->rcs[i];
	}
	return ptr;
}

uint8_ptr mqtt::PackPublish(mqtt::Publish* pub) {
	size_t packet_len = 2; // bits and remaining length
	packet_len += sizeof(uint16_t); // Length MSB and Length LSB
	packet_len += pub->topic.size(); // topic len
	packet_len += pub->payload.size(); // payload len

	if (((pub->header.bits & 0x6) >> 1u) > 0) {
		packet_len += 2;
	}


	uint8_ptr ptr{ new uint8_t[packet_len] };
	auto pack = ptr.get();

	pack[0] = pub->header.bits;
	packet_len -= 2;
	mqtt::EncodeLength(pack + 1, packet_len);
	pack[2] = uint8_t(pub->topic.size() >> 8u); // MSB
	pack[3] = uint8_t(pub->topic.size()); // LSB

	size_t pos = 4;

	size_t end = 4;
	end += pub->topic.size();

	for (; pos < end; ++pos) {
		pack[pos] = pub->topic[pos - 4];
	}

	pos = end;

	if (((pub->header.bits & 0x6) >> 1u) > 0) {
		end++;

		if(pub->pkt_id != 0) {
			pack[pos] = uint8_t(pub->pkt_id >> 8u);
			pack[end] = uint8_t(pub->pkt_id);
		} else {
			pack[pos] = 0;
			pack[end] = 1;
		}
		pos += 2;
	}



	for (auto& w : pub->payload) {
		pack[pos] = w;
		pos++;
	}

	return ptr;
}

uint8_ptr mqtt::PackPingreq(mqtt::Pingreq* ping) {
	return PackHeader(ping);
}

uint8_ptr mqtt::PackPingresp(mqtt::Pingresp* ping) {
	return PackHeader(ping);
}