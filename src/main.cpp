#include <iostream>
#include <aris.h>
#include"rokae.h"

using namespace aris::dynamic;

int main(int argc, char *argv[])
{
	auto&cs = aris::server::ControlServer::instance();
	auto port = argc < 2 ? 5866 : std::stoi(argv[1]);

	cs.resetController(rokae::createControllerRokaeXB4().release());
	cs.resetModel(rokae::createModelRokaeXB4().release());
	cs.resetPlanRoot(rokae::createPlanRootRokaeXB4().release());
	cs.resetSensorRoot(new aris::sensor::SensorRoot);

	aris::core::WebSocket socket;
	socket.setOnReceivedMsg([&](aris::core::WebSocket *socket, aris::core::Msg &msg)->int
	{
		std::string msg_data = msg.toString();

		//std::cout << "recv:" << msg_data << std::endl;

		if (msg.header().msg_id_ == 0)
		{
			LOG_INFO << "the request is cmd:"
				<< msg.header().msg_size_ << "&"
				<< msg.header().msg_id_ << "&"
				<< msg.header().msg_type_ << "&"
				<< msg.header().reserved1_ << "&"
				<< msg.header().reserved2_ << "&"
				<< msg.header().reserved3_ << ":"
				<< msg_data << std::endl;

			try
			{
				auto id = cs.executeCmd(aris::core::Msg(msg_data));
				std::cout << "command id:" << id << std::endl;
				socket->sendMsg(aris::core::Msg());
			}
			catch (std::exception &e)
			{
				std::cout << e.what() << std::endl;
				LOG_ERROR << e.what() << std::endl;
				socket->sendMsg(aris::core::Msg(e.what()));
			}
		}
		else if (msg.header().msg_id_ == 1)
		{
			LOG_INFO_EVERY_N(10) << "socket receive request msg:"
				<< msg.header().msg_size_ << "&"
				<< msg.header().msg_id_ << "&"
				<< msg.header().msg_type_ << "&"
				<< msg.header().reserved1_ << "&"
				<< msg.header().reserved2_ << "&"
				<< msg.header().reserved3_ << ":"
				<< msg_data << std::endl;

			auto part_pm_vec = std::make_any<std::vector<double> >(cs.model().partPool().size() * 16);
			cs.getRtData([](aris::server::ControlServer& cs, std::any& data)
			{
				for (aris::Size i(-1); ++i < cs.model().partPool().size();)
					cs.model().partPool().at(i).getPm(std::any_cast<std::vector<double>&>(data).data() + i * 16);
			}, part_pm_vec);

			std::vector<double> part_pq(cs.model().partPool().size() * 7);
			for (aris::Size i(-1); ++i < cs.model().partPool().size();)
			{
				aris::dynamic::s_pm2pq(std::any_cast<std::vector<double>&>(part_pm_vec).data() + i * 16, part_pq.data() + i * 7);
			}

			//// return binary ////
			aris::core::Msg msg;
			msg.copy(part_pq.data(), part_pq.size() * 8);
			socket->sendMsg(msg);
		}

		return 0;
	});
	socket.setOnReceivedConnection([](aris::core::WebSocket *sock, const char *ip, int port)->int
	{
		std::cout << "socket receive connection" << std::endl;
		LOG_INFO << "socket receive connection:\n"
			<< std::setw(aris::core::LOG_SPACE_WIDTH) << "|" << "  ip:" << ip << "\n"
			<< std::setw(aris::core::LOG_SPACE_WIDTH) << "|" << "port:" << port << std::endl;
		return 0;
	});
	socket.setOnLoseConnection([](aris::core::WebSocket *socket)
	{
		std::cout << "socket lose connection" << std::endl;
		LOG_INFO << "socket lose connection" << std::endl;
		for (;;)
		{
			try
			{
				socket->startServer("5866");
				break;
			}
			catch (aris::core::Socket::StartServerError &e)
			{
				std::cout << e.what() << std::endl << "will try to restart server socket in 1s" << std::endl;
				LOG_ERROR << e.what() << std::endl << "will try to restart server socket in 1s" << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}
		std::cout << "socket restart successful" << std::endl;
		LOG_INFO << "socket restart successful" << std::endl;

		return 0;
	});
	
	cs.start();
	socket.startServer(std::to_string(port));
	//try 
	//{
	//	cs.executeCmd(aris::core::Msg("moveJS"));
	//}
	//catch (std::exception &e)
	//{
	//	std::cout << e.what() << std::endl;
	//	//1LOG_ERROR << e.what() << std::endl;
	//}
	
	// �������� //
	for (std::string command_in; std::getline(std::cin, command_in);)
	{
		try
		{
			if (command_in == "start")
			{
				cs.start();
				socket.startServer(std::to_string(port));
			}
			else if (command_in == "stop")
			{
				cs.stop();
				socket.stop();
			}
			else
			{
				auto id = cs.executeCmd(aris::core::Msg(command_in));
				std::cout << "command id:" << id << std::endl;
			}
		}
		catch (std::exception &e)
		{
			std::cout << e.what() << std::endl;
			LOG_ERROR << e.what() << std::endl;
		}
	}

	return 0;
}