#ifndef MQTT_NETWORK_SERVER_H_
#define MQTT_NETWORK_SERVER_H_

#include <memory>
#include <string>
#include <array>
#include <queue>
#include <algorithm>
#include <chrono>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/redirect_error.hpp>

#include "../utility/mqtt.hpp"
#include "../utility/core.hpp"
#include "log/log.hpp"
#include "../utility/trie.hpp"

#define SHOULD_SEND 1
#define MAX_PACKET_LEN 268435456

using namespace boost;
using asio::ip::tcp;

typedef tree::trie<std::list<std::shared_ptr<Subscriber>>> subscriptions_tree;
using namespace std::chrono_literals;

namespace network {

	class Session;

	struct {
		subscriptions_tree topics_;
		std::map<std::string, std::list<std::string>> clients_;

	} curiosity;


	static class Server {
	public:

		asio::awaitable<void> Listen(tcp::acceptor acceptor, std::string filename);
		
		void SendMessageTo(std::string id, uint8_t *msg, size_t len_of_msg);

		std::shared_ptr<Session> GetSession(std::string& client_id);

		size_t SessionSize() { return sessions_.size(); }

		std::string GetFilename() { return filename_; }

	private:
		std::list<std::shared_ptr<Session>> sessions_;
		std::string filename_;
		
	} server;

	class Session {
	public:
		Session(tcp::socket sock, unsigned int id_of_session);
		void Start();
		void RewriteBuffer(uint8_t *buf, size_t len_of_msg);
		std::string GetId();
		unsigned int GetSessionId();
		void TransferControl(tcp::socket sock);
		void CleanSessionHandler();
		bool SessionIsFree();
		void SendWillMessage();

		asio::awaitable<void> ReadBytes();
		asio::awaitable<void> SendBytes();
		
		int PacketHandler(uint8_t* packet);

		int ConnectHandler(mqtt::Connect*);
		int DisconnectHandler();
		int SubscribeHandler(mqtt::Subscribe*);
		int UnsubscribeHandler(mqtt::Unsubscribe*);
		int PublishHandler(mqtt::Publish*);
		int PubrecHandler(mqtt::Pubrec*);
		int PubrelHandler(mqtt::Pubrel*);
		int PingreqHandler();

		void Stop(bool delete_session = false);

		~Session();
	private:
		tcp::socket sock_;
		asio::steady_timer timer_for_send;
		asio::steady_timer timer_for_ping;

		std::array<uint8_t, 268'435'456> buf_;
		std::queue<std::vector<uint8_t>> packets_;
		Client cl;

		bool should_send_ = false;
		bool session_is_available;
		size_t len_of_packet_ = 0;

		unsigned int id_of_session_;
	};
	
} //namespace network

#endif 

