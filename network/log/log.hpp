#ifndef MQTT_UTILITY_LOG_H_
#define MQTT_UTILITY_LOG_H_

#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>

enum kMessageType {
	info = 0,
	debug,
	warning,
	error
};

struct {
	constexpr std::string_view operator[](kMessageType type) {
		switch (type)
		{
		case info:
			return "INFO";
		case debug:
			return "DEBUG";
		case warning:
			return "WARNING";
		case error:
			return "ERROR";
		}
		return "SOMETHING ELSE";
	}
} kEnumToString;

static void Log(std::string file_name, kMessageType type, unsigned int session_id, std::string_view what) {

	auto time_ = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(time_);
	auto th_id = std::this_thread::get_id();

	std::fstream outfile{ file_name, std::ios::app };

	outfile << '\n' << '[' << kEnumToString[type] << ']' << ' ' << '[' << (time) << ']' 
			<< ' ' << '[' << th_id << ']' 
			<< ' ' << '[' << session_id << ']' 
			<< ' ' << what << '\n';

	std::cout << '\n' << '[' << kEnumToString[type] << ']' 
			  << ' ' << '[' << time << ']' 
			  << ' ' << '[' << th_id << ']' 
			  << ' ' << '[' << session_id << ']' 
			  << ' ' << what << '\n';
}

#endif 


