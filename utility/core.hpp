#pragma once

#ifndef MQTT_UTILITY_CORE_H_
#define MQTT_UTILITY_CORE_H_

#include <cstdint>
#include <string>
#include <list>

struct Subscriber {
	Subscriber(uint8_t qos, std::string client_id) : client_id{ client_id }, qos{ qos } {}

	uint8_t qos;
	std::string client_id;
};

static bool operator==(const Subscriber& s1, const Subscriber& s2) {
	return s1.client_id == s2.client_id;
}

struct Client {	
	uint8_t connect_flags_;
	std::string client_id_;
	std::string will_topic_;
	std::string will_msg_;
	std::string username_;
	std::string password_;
	uint16_t keepalive_;
};

#endif