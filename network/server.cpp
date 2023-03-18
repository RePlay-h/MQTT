
#include "server.hpp"

asio::awaitable<void> network::Server::Listen(tcp::acceptor acceptor, std::string filename) {

	unsigned int id_of_session = 1;
	server.filename_ = std::move(filename);
	for(;;) {
		Log(server.filename_, info, 0, "Start Listen");
		bool b = true;

		auto sock = co_await acceptor.async_accept(asio::use_awaitable);

		// Checking if there is a free session
		for(auto session : server.sessions_) {
			if(session->SessionIsFree()) { //Checking
				session->TransferControl(std::move(sock));
				b = false; // Setting a special variable to false
				break;
			}
		}

		// If the special variable is true, then create a new session
		if(b) {
			server.sessions_.push_back(std::make_shared<Session>(std::move(sock), id_of_session));
			server.sessions_.back()->Start();
		}

		id_of_session++;
	}
}

// Using this function, you can send a message to the user with the specified id
void network::Server::SendMessageTo(std::string id, uint8_t* msg, size_t len_of_msg) {
	auto user = sessions_.begin();

	//looking for a session with the specified id
	for(; user != end(sessions_); user++) {
		if (user->get()->GetId() == id) {
			break;
		}
	}
	
	if (user != end(sessions_)) {
		user->get()->RewriteBuffer(msg, len_of_msg);
	}
}

// This function returns a smart pointer to the session
std::shared_ptr<network::Session> network::Server::GetSession(std::string& client_id) {
	for(auto session : sessions_) {
		if(session->GetId() == client_id) {
			return session;
		}
	}
	return nullptr;
}


/*
*  This is one of the most important functions.
*  A packet with the received data is sent to her 
*  and she processes it by calling special functions and handlers
*/
int network::Session::PacketHandler(uint8_t* packet)
{
	mqtt::packet pack;
	mqtt::Header head;
	head.bits = packet[0];
	head.remaining_length = packet[1];

	int rc = -SHOULD_SEND;

	try {
		switch (packet[0] >> 4) // Checking the package type
			/*
			*  First, in each case, the pack variable is initialized by the type of package we received. 
			*  Next, a function of the Unpack type is called, which moves data to the package object. 
			*  At the end, the corresponding handler is called for the package
			*/
		{
		case CONNECT:
		{

			pack = mqtt::Connect();
			mqtt::Connect con = std::move(std::get<2>(pack));

			mqtt::UnpackConnect(packet, &head, &con);

			rc = ConnectHandler(&con);
			break;
		}
		case PUBLISH: {
			pack = mqtt::Publish();
			mqtt::Publish pub = std::move(std::get<5>(pack));

			mqtt::UnpackPublish(packet, &head, &pub);
			rc = PublishHandler(&pub);
			break;
		}

		case SUBSCRIBE: {

			pack = mqtt::Subscribe();
			mqtt::Subscribe sub = std::move(std::get<6>(pack));

			long error = mqtt::UnpackSubscribe(packet, &head, &sub);

			if(error == -1) {
				Log(server.GetFilename(), debug, id_of_session_, "The value of the reserved bit is incorrect");
				Stop();
				break;
			}

			rc = SubscribeHandler(&sub);
			break;
		}

		case UNSUBSCRIBE: {
			pack = mqtt::Unsubscribe();

			mqtt::Unsubscribe unsub = std::move(std::get<7>(pack));
			mqtt::UnpackUnsubscribe(packet, &head, &unsub);
			rc = UnsubscribeHandler(&unsub);
			break;
		}

		case DISCONNECT: {
			rc = DisconnectHandler();
			break;
		}
		case PINGREQ: {
			if((head.bits & 0x0F) != 0) {
				Log(server.GetFilename(), debug, id_of_session_, "The value of the reserved bit is incorrect");
				Stop();
			}
			rc = PingreqHandler();
			break;
		}
		default:
			pack = mqtt::AckPacket();
			mqtt::AckPacket ack_packet = std::move(std::get<0>(pack));
			mqtt::UnpackAck(packet, &head, &ack_packet);

			switch (packet[0] >> 4)
			{
			case PUBACK:
				rc = -SHOULD_SEND;
				break;
			case PUBREC:

				rc = PubrecHandler(&ack_packet);
				break;
			case PUBREL:
				rc = PubrelHandler(&ack_packet);
				break;
			}
		}
	}
	catch (std::exception& ex) {
		Log(server.GetFilename(), error, id_of_session_,
			"An error occurred while processing the package: " + std::string(ex.what()));
	}

	return rc;
}

network::Session::Session(tcp::socket sock, unsigned int id_of_session)
	: sock_(std::move(sock)), timer_for_send(sock_.get_executor()), 
	  timer_for_ping(sock_.get_executor()), id_of_session_(id_of_session), session_is_available(false) 
{
	timer_for_send.expires_at(std::chrono::steady_clock::time_point::max());
	std::fill(begin(buf_), end(buf_), 0);
}

// Coroutines are initialized in this function
void network::Session::Start() {
	asio::co_spawn(sock_.get_executor(), ReadBytes(), asio::detached);
	asio::co_spawn(sock_.get_executor(), SendBytes(), asio::detached);
}

/*
* This function overwrites the data in the buffer
* This is only used in the SendMessageTo function
*/
void network::Session::RewriteBuffer(uint8_t* buf, size_t len_of_msg)
{
	std::fill(begin(buf_), end(buf_), 0);
	std::vector<uint8_t> buf1(len_of_msg);

	for(int i = 0; i < len_of_msg; ++i) {
		buf1[i] = buf[i];
	}

	packets_.push(buf1);


	timer_for_send.cancel_one();
}

std::string network::Session::GetId() {
	return cl.client_id_;
}

unsigned int network::Session::GetSessionId() {
	return id_of_session_;
}

// This function gives control to some session
void network::Session::TransferControl(tcp::socket sock) {
	sock_ = std::move(sock);
	session_is_available = false;
	this->Start();

	Log(server.GetFilename(), info, id_of_session_, "We have transferred control to this session");
}


// This is a special handler that is called only if the Clear Session bit in the CONNECT packet is 0
void network::Session::CleanSessionHandler() {

	//before the session is fully restored, you need to send a CONNACK package
	mqtt::Connack answer;

	answer = std::move(mqtt::PacketConnack(CONNACK_BYTE, 1, 0));

	uint8_ptr ptr = std::move(mqtt::PackConnack(&answer));

	len_of_packet_ = 4;
	should_send_ = true;

	std::vector<uint8_t> buf(len_of_packet_);

	for (int i = 0; i < 4; i++) {
		buf[i] = ptr.get()[i];
	}

	
	//sock_.write_some(asio::buffer(buf, len_of_packet_));  //TODO

	//Now we restore the contents of the variablesand the buffer

	packets_.push(buf);

	timer_for_send.cancel_one();
}

bool network::Session::SessionIsFree() {
	return session_is_available;
}


/* 
*  This function sends WillMessage when the user disconnects from the server. 
*  First, it generates publish for each WillTopic subscriber. 
*  At the end, it sends the packet to the subscriber.
*/
void network::Session::SendWillMessage() {
	if (cl.connect_flags_ & 0x4) {
		mqtt::Publish pub;
		mqtt::Header header;

		header.bits = (((cl.connect_flags_ & 0x18) >> 2u) | 0x30);
		size_t packet_len = 4;
		packet_len += cl.will_msg_.size();
		packet_len += cl.will_topic_.size();
		
		if((cl.connect_flags_ & 0x18) > 0) {
			packet_len += 2;
		}

		std::vector<uint8_t> buf(packet_len);
		size_t step = 0;

		buf[step] = header.bits;
		step += 2;

		packet_len -= 2;
		mqtt::EncodeLength(buf.data() + 1, packet_len);
		header.remaining_length = buf[1];

		buf[step] = (cl.will_topic_.size() >> 8u);
		step++;

		buf[step] = (cl.will_topic_.size());
		step++;
		
		for(char ch : cl.will_topic_) {
			buf[step] = ch;
			step++;
		}

		if(cl.connect_flags_ & 0x18) {
			buf[step] = 0x00;
			step++;
			buf[step] = 0x01;
			step++;
		}

		for(char ch : cl.will_msg_) {
			buf[step] = ch;
			step++;
		}

		mqtt::UnpackPublish(buf.data(), &header, &pub);
		PublishHandler(&pub);

		Log(server.GetFilename(), info, id_of_session_,
			"Sent WillMessage for subscribers of " + pub.topic);
	}
}

/*
*  This coroutine is responsible for reading the data. 
*  It also analyzes the package for errors
*/
asio::awaitable<void> network::Session::ReadBytes() {

	try{
		for (;;) {

			co_await sock_.async_read_some(asio::buffer(buf_, 2), asio::use_awaitable);

			int pack_type = buf_[0] >> 4u;
			if (pack_type < CONNECT || pack_type > DISCONNECT) {
				std::stringstream ss;
				ss << std::hex << buf_[0];
				Log(server.GetFilename(), error, id_of_session_, "There is no package with this type: 0x" + ss.str());
				Stop();
				continue;
			}

			size_t tlen = mqtt::DecodeLength(buf_.data() + 1);

			if (tlen > MAX_PACKET_LEN) {
				Log(server.GetFilename(), error, id_of_session_, "The package is too big");
				std::fill(begin(buf_), end(buf_), 0);
				continue;
			}

			if (co_await sock_.async_read_some(asio::buffer(buf_.data() + 2, tlen), asio::use_awaitable) < 0) {
				Log(server.GetFilename(), error, id_of_session_, "Variable header not received");
				Stop();
				continue;
			}

			Log(server.GetFilename(), info, id_of_session_, 
				"The package was successfully received. PACKET TYPE: " + std::to_string(int(pack_type)));

			int rc = PacketHandler(buf_.data());

			if (rc == SHOULD_SEND) {
				should_send_ = true;
				timer_for_send.cancel_one();
			}
			
		}
	}
	catch (std::exception&) {
		Log(server.GetFilename(), error, id_of_session_, "error during reading");
	}
	
}

/*
*  This coroutine sends data. 
*  These are all packages that have not been sent before
*/
asio::awaitable<void> network::Session::SendBytes() {
	try {
		for (;;) {

			if (!should_send_) {
				boost::system::error_code ec;
				co_await timer_for_send.async_wait(asio::redirect_error(asio::use_awaitable, ec));
			}
			else {
				while(!packets_.empty()) {
					if (co_await sock_.async_write_some(asio::buffer(packets_.front()), asio::use_awaitable) <= 0) {
	
						Log(server.GetFilename(), error, id_of_session_, "The package was not sent");
					}
					packets_.pop();
				}
				len_of_packet_ = 0;
				should_send_ = false;
			}
		}
	}
	catch (std::exception&) {
		Log(server.GetFilename(), error, id_of_session_, "error during sending");
	}
}

void network::Session::Stop(bool delete_session) {
	if (sock_.is_open()) {

		sock_.shutdown(tcp::socket::shutdown_both);
		sock_.close();
		Log(server.GetFilename(), info, id_of_session_, "The session was over");
		timer_for_send.cancel();
		curiosity.clients_.erase(cl.client_id_); //delete user from database

		SendWillMessage();


		//delete all user subscriptions
		for (const std::string& topic : curiosity.clients_[cl.client_id_]) {
			std::list<std::shared_ptr<Subscriber>> subs = curiosity.topics_.get(topic);

			for (std::shared_ptr<Subscriber> sub : subs) {
				if (sub->client_id == cl.client_id_) {
					subs.remove(sub);
					break;
				}
			}
		}
		
		auto it = curiosity.clients_.find(cl.client_id_);
		curiosity.clients_.erase(it);

		for(auto& [a, b] : curiosity.clients_) {
			std::cout << a << '\n';
		}

		cl.client_id_.clear();
		cl.connect_flags_ = 0x0;
		cl.password_.clear();
		cl.username_.clear();
		cl.will_msg_.clear();
		cl.will_topic_.clear();
	}
}

network::Session::~Session() {
} 

int network::Session::ConnectHandler(mqtt::Connect* pkt) {

	if((pkt->header.bits & 0x0F) != 0) {
		Log(server.GetFilename(), debug, id_of_session_, "The value of the reserved bit is incorrect");
		Stop();
		return -SHOULD_SEND;
	}

	if(pkt->payload.cliend_id.empty()) {
		Log(server.GetFilename(), debug, id_of_session_, "Client Id is empty");
		Stop();
		return -SHOULD_SEND;
	}

	if((pkt->variable_header.connect_flags & 0x1) != 0) {
		Log(server.GetFilename(), debug, id_of_session_, "The value of the reserved bit is incorrect");
		Stop();
		return -SHOULD_SEND;
	}

	
	if ((pkt->variable_header.connect_flags & 0x2) == 0) {
		auto session = server.GetSession(pkt->payload.cliend_id);

		//We must restore the connection
		if (session != nullptr) {
			std::cout << sock_.is_open() << '\n';
			session->TransferControl(std::move(sock_));
			session->CleanSessionHandler();
			session_is_available = true;
			Stop();
			return -SHOULD_SEND;
		}
	}

	if (curiosity.clients_.find(pkt->payload.cliend_id) != curiosity.clients_.end()
		 && (pkt->variable_header.connect_flags & 0x2) == 1) {
		Log(server.GetFilename(), debug, id_of_session_, "Double connection: " + pkt->payload.cliend_id);
		Stop();
		return -SHOULD_SEND;
	}
	

	std::fill(begin(buf_), end(buf_), 0);
	std::vector<uint8_t> buf;

	cl.client_id_ = pkt->payload.cliend_id;
	cl.connect_flags_ = pkt->variable_header.connect_flags;
	cl.password_ = pkt->payload.password;
	cl.username_ = pkt->payload.username;
	cl.will_msg_ = pkt->payload.will_message;
	cl.will_topic_ = pkt->payload.will_topic;
	cl.keepalive_ = pkt->variable_header.keepalive;

	timer_for_ping.expires_after(std::chrono::seconds(cl.keepalive_ * 2));
	timer_for_ping.async_wait([&](const boost::system::error_code &ec) { 
		if (!(ec == asio::error::operation_aborted)) {
			this->Stop();
		}
		});
	timer_for_ping.cancel();
	curiosity.clients_[cl.client_id_] = {};

	//make CONNACK packet
	mqtt::Connack answer;
	
	answer = std::move(mqtt::PacketConnack(CONNACK_BYTE, 0, 0));

	uint8_ptr ptr = std::move(mqtt::PackConnack(&answer));

	for(int i = 0; i < 4; i++) {
		buf.push_back(ptr.get()[i]);
	}
	buf.resize(4);
	len_of_packet_ = 4;
	packets_.push(buf);
	
	return SHOULD_SEND;
}

int network::Session::DisconnectHandler() {
	Stop();
	session_is_available = true;
	return -SHOULD_SEND; 
}

int network::Session::SubscribeHandler(mqtt::Subscribe* ptr) {
	
	if ((ptr->header.bits & 0xF) != 2) {
		Log(server.GetFilename(), debug, id_of_session_, "The value of the reserved bit is incorrect");
		Stop();
	}

	std::vector<uint8_t> rcs;

	for (auto& [topic, qos] : ptr->topic_and_qos) {
		Log(server.GetFilename(), info, id_of_session_,
			"The user (" + cl.client_id_ + ") subscribed " + "[ Topic: " + topic + " Qos: " + std::to_string(qos) + "]");
		rcs.push_back(qos);

		size_t topic_len = topic.length();
		topic_len--;

		if ((topic[topic_len] == '#') && (topic[--topic_len] == '/')) {

			//get a topic without a special sign
			const std::string& top = topic.substr(0, topic_len);


			// the first template parameter is a topic, and the second is a subtree that is below this topic
			std::map<std::string, subscriptions_tree>* tops = curiosity.topics_.get_node(top);

			
			for (const auto& [piece_of_topic, tree] : *tops) {

				curiosity.topics_.get(top + "/" + piece_of_topic).push_back(std::make_shared<Subscriber>(qos, cl.client_id_));
				curiosity.clients_[cl.client_id_].push_back(top + "/" + piece_of_topic);
			}

		}
		else {
			curiosity.topics_.get(topic).push_back(std::make_shared<Subscriber>(qos, cl.client_id_));
			curiosity.clients_[cl.client_id_].push_back(topic);
		}
	}

	//create SUBACK
	mqtt::Suback sub = std::move(mqtt::PacketSuback(SUBACK_BYTE, ptr->pkt_id, rcs.size(), rcs.data()));

	uint8_ptr pkt = std::move(mqtt::PackSuback(&sub));

	std::fill(begin(buf_), end(buf_), 0);

	len_of_packet_ = 4;
	len_of_packet_ += ptr->topic_and_qos.size();

	std::vector<uint8_t> buf(len_of_packet_);

	for (int i = 0; i < len_of_packet_; i++) {
		buf[i] = pkt.get()[i];
	}
	packets_.push(buf);

	return SHOULD_SEND;
}

int network::Session::UnsubscribeHandler(mqtt::Unsubscribe* ptr) {

	if((ptr->header.bits & 0x0F) != 2) {
		Log(server.GetFilename(), debug, id_of_session_, "The value of the reserved bit is incorrect");
		Stop();
	}

	//unsubscribe from the specified topics
	for (auto topic : ptr->topics) {

		auto &subs = curiosity.topics_.get(topic);

		auto it = std::remove_if(begin(subs), end(subs), [&](std::shared_ptr<Subscriber> user) {
			return user->client_id == cl.client_id_; 
			});

		if(it != subs.end()) 
			subs.erase(it);
	}

	//create UNSUBACK
	mqtt::Unsuback unsub = std::move(mqtt::PacketAck(UNSUBACK_BYTE, ptr->pkt_id));
	uint8_ptr pkt = std::move(mqtt::PackAck(&unsub));

	std::vector<uint8_t> buf(4);

	for (int i = 0; i < 4; i++) {
		buf[i] = pkt.get()[i];
	}
	packets_.push(buf);

	return SHOULD_SEND;
}

int network::Session::PublishHandler(mqtt::Publish* ptr) {

	uint16_t qos = ptr->header.bits & 0xF9;


	for(auto &subscriber : curiosity.topics_.get(ptr->topic)) {

		//create PUBLISH
		ptr->header.bits &= 0xF9;
		ptr->header.bits |= (subscriber->qos << 1u);
		uint8_ptr pkt = mqtt::PackPublish(ptr);

		size_t packet_len = subscriber->qos == 0 ? 4 : 6;
		packet_len += ptr->payload.length();
		packet_len += ptr->topic.size();

		for (int i = 0; i < packet_len; ++i) {
			buf_[i] = pkt.get()[i];
		}

		//send PUBLISH to subscriber
		server.SendMessageTo(subscriber->client_id, buf_.data(), packet_len);
	}
	
	return -SHOULD_SEND;
}

int network::Session::PubrecHandler(mqtt::Pubrec* ptr) {
	
	if((ptr->header.bits & 0x0F) != 0) {
		Log(server.GetFilename(), debug, id_of_session_, "The value of the reserved bit is incorrect");
		Stop();
	}

	//create PUBREL
	mqtt::Pubrel pub = std::move(mqtt::PacketAck(PUBREL_BYTE, ptr->pkt_id));
	uint8_ptr pkt = std::move(mqtt::PackAck(&pub));

	std::fill(begin(buf_), end(buf_), 0);

	std::vector<uint8_t> buf(4);

	for(int i = 0; i < 4; i++) {
		buf[i] = pkt.get()[i];
	}

	packets_.push(buf);

	should_send_ = true;
	len_of_packet_ = 4;
	
	return SHOULD_SEND;
}

int network::Session::PubrelHandler(mqtt::Pubrel* ptr) {
	
	//create PUBCOMP
	if ((ptr->header.bits & 0x0F) != 2) {
		Log(server.GetFilename(), debug, id_of_session_, "The value of the reserved bit is incorrect");
		Stop();
	}

	mqtt::Pubcomp pub = std::move(mqtt::PacketAck(PUBCOMP_BYTE, ptr->pkt_id));
	uint8_ptr pkt = std::move(mqtt::PackAck(&pub));

	std::vector<uint8_t> buf(4);

	for(int i = 0; i < 4; i++) {
		buf[i] = pkt.get()[i];
	}
	packets_.push(buf);

	return SHOULD_SEND;
}

int network::Session::PingreqHandler() {

	timer_for_ping.cancel();
	timer_for_ping.expires_after(std::chrono::seconds(cl.keepalive_ * 2));

	timer_for_ping.async_wait([&](const boost::system::error_code& ec) {
		if (!(ec == asio::error::operation_aborted)) {
		this->Stop();
	}});

	std::fill(begin(buf_), end(buf_), 0);
	std::vector<uint8_t> buf(2);
	buf[0] = PINGRESP_BYTE;
	buf[1] = 0;
	len_of_packet_ = 2;
	packets_.push(buf);
	return SHOULD_SEND;
}
