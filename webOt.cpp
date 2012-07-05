#include "webOt.h"

	WebOt::WebOt()
	{
		workInfo.resize(50); //max number of worker threads is 50 (change to constant?)
		numOfActiveWorkers = 0;
		highestWorkerID = 0;
		haltSignal = false;
		srand(time(NULL));

				//generate a random name for this node - later, have a specific server-node that manages naming new nodes
		string GenName = "";
		for(int i = 0; i < 10; i++)
			GenName += ('a' + (rand() % 25));

		init();
	}

	int WebOt::init()
	{
		if (SDLNet_Init() < 0)
		{
			cout << "SDLNet_Init: " << SDLNet_GetError() << "\n";
			exit(0);
		}
		cout << "SDLnet init complete.\n";
		/* Resolving the host using NULL make network interface to listen */
		if (SDLNet_ResolveHost(&ip, NULL, 2000) < 0)
		{
			cout << "SDLNet_ResolveHost: " << SDLNet_GetError() << "\n";
			exit(0);
		}
		cout << "Hostname resolved.\n";
		/* Open a connection with the IP provided (listen on the host's port) */
		if (!(sd = SDLNet_TCP_Open(&ip)))
		{
			cout << "SDLNet_TCP_Open: " << SDLNet_GetError() << "\n";
			exit(0);
		}
		cout << "Socket opened.\n";

		return 0;
	}

	void WebOt::run()
	{
		thread wmgr(&WebOt::workManager, this);
		wmgr.detach();

		while(running)
		{
			if((csd = SDLNet_TCP_Accept(sd)))
			{
				thread t(&WebOt::handleConnection, this, csd, nullptr);
				t.detach();				
			}
			//potentially throttle this, or limit connections in some way
			if(SDL_GetTicks() % 1000 == 0)
			{
				std::lock_guard<std::mutex> lk(workInfo_tex);
				running = !haltSignal;
			}
		}
	}

		//finds the connection id related to the ip, and if its not in the list, adds it and returns a new id
	//defaults to returning -1 when the address is not found, when create is set to true, will assign the ip a new ID and return it instead.
	int WebOt::getConnectionId(IPaddress *ip, bool create=false)
	{
		bool found = false;
		int ID = 0;
		for(int i = 0; i < connections.size() && !found; i++)
		{
			if(connections[i].addr.host == ip->host)
			{
				ID = i;
				found = true;
			}
		}
		if(!found)
		{
			connection n = {100, *ip};
			ID = connections.size();
			connections.push_back(n);
			sockAssigns.resize(sockAssigns.size() + 1);
		}
		return ID;
	}

	void WebOt::handleConnection(TCPsocket sock, IPaddress *remoteIP=nullptr)
	{
		p("client connected.\n");
		char buffer[512] = {0};

		if(remoteIP == nullptr)
			remoteIP = SDLNet_TCP_GetPeerAddress(sock);
		int connectionID = getConnectionId(remoteIP);
		bool ConOpen = true;
		string a;
		int meslen = 0;
		while(ConOpen)
		{
			if((meslen = SDLNet_TCP_Recv(sock, buffer, 512)) > 1 && buffer[0] != -1 && buffer[0] != '\r' && buffer[0] != '\n')
			{
				buffer[meslen] = '\0';
				addReceivedMessage(buffer);
			}
			a = getAssignment(connectionID);
			if(a != "")
			{
				//i want to measure (in ms) the time it takes for each transmission and keep a running average of it.
				//this information will be used for rerouting through the fastest path(s)
				if(SDLNet_TCP_Send(sock, a.c_str(), a.length() + 1))
				{
					//do something to indicate that the message failed to send...
				}
			}
		}
	}

	void WebOt::halt()
	{
		std::lock_guard<std::mutex> lk(run_tex);
		haltSignal = true;
	}