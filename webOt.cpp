#include "webOt.h"

WebOt::WebOt()
{
	workInfo.resize(50); //max number of worker threads is 50 (change to constant?)
	numOfActiveWorkers = 0;
	highestWorkerID = 0;
	haltSignal = false;
	srand(time(NULL));
	running = true;
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
			thread t(&WebOt::handleConnection, this, csd);
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

void WebOt::handleConnection(TCPsocket sock)
{
	SDLNet_SocketSet ss;
	ss = SDLNet_AllocSocketSet(1);
	SDLNet_TCP_AddSocket(ss, csd);

	p("client connected.\n");
	char buffer[512] = {0};

	IPaddress *remoteIP;
	remoteIP = SDLNet_TCP_GetPeerAddress(sock);
	int connectionID = getConnectionId(remoteIP);
	bool ConOpen = true;
	string a;
	int meslen = 0;
	while(ConOpen)
	{
		if(SDLNet_CheckSockets(ss, 1) > 0)
		{
			if(SDLNet_SocketReady(sock))
			{
				if((meslen = SDLNet_TCP_Recv(sock, buffer, 512)) > 1 && buffer[0] != -1 && buffer[0] != '\r' && buffer[0] != '\n')
				{
					buffer[meslen] = '\0';
					addReceivedMessage(buffer);
				}
			}
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

//worker thread deals with input
void WebOt::work(int workerID)
{
	p("worker started.\n");	
	string current = "";
	bool needed = true;
	int loops = 0;
	while (needed)
	{
		current = getReceivedMessage();
		if(current != "")
		{
			if(current[0] == '~')
			{
				//tildes signify a command to be forwarded
				//i.e. ~192.168.89.128:echo hello there
				//will send the command 'echo hello there' to 192.168.89.128
				//further: ~192.168.89.128:~106.50.26.49:echo hello there
				//will send ~106.50.26.49:echo hello there to 192.168.89.128 which will then send
				// 'echo hello there' to 106.50.26.49

				//one idea i want is shortened names for nodes ie j17
				//these names would be interchangeable with ip addresses
				//so: ~j17:echo hello there
				//would look up j17's ip address and forward the command to it

				//~!192.168.89.128:echo hello there
				//will perform a network lookup for 192.168.89.128
				//this will send out a whereis message and then send out a forward-chained message
				//to the node when/if it is found
				//note: this will work with short names, i.e: ~!j17:echo hello there

				//~*:[command] tells the program to send the command to all nodes attatched to the current node
				int colLoc = current.find(':');
				string ip = current.substr(1, colLoc -  1);
				IPaddress n;
				SDLNet_ResolveHost(&n, ip.c_str(), 2000);
				for(int i = 0; i < connections.size(); i++)
				{
					if(n.host == connections[i].addr.host)
					{
						std::lock_guard<std::mutex> lk(sockA_tex);
						sockAssigns[i].toDo.push(current.substr(colLoc + 1));
					}
				}
			}
			else if(current[0] == '$')
			{
				//this is the run module key
				//ie: $test
				//will search for the 'test' module and run it if it is found
			}
			else if(current.substr(0, 7) == "connect") //test command, supposed to make a new connection when prompted
			{
				makeConnection(current.substr(8), 2000);
			}
			else if(current.substr(0,5) == "echo ")
			{
				p(current.substr(5) + "\n");
			}
			else if(current.substr(0,7) == "whereis")
			{
				//check connection list, else propogate message.
				//need some sort of checking for circular propogation... request ID?
			}
			if(startsWith(current, "stop"))
			{
				p("Halting execution.\n");
				halt();
			}
		}

		loops++;
		if(loops == 10) //im not certain why i felt the need to do this... maybe i didnt like the idea of threads being killed instantly?
		{
			loops = 0;
			needed = amiNeeded(workerID);
		}
	}
	workerDied(workerID);
}

string WebOt::getAssignment(int ID)
{
	std::lock_guard<std::mutex> lk(sockA_tex);
	if(!sockAssigns[ID].toDo.empty())
	{
		string s = sockAssigns[ID].toDo.front();
		sockAssigns[ID].toDo.pop();
		return s;
	}
	else
	{
		return "";
	}
}

void WebOt::makeConnection(string host, int port)
{
	IPaddress remoteIP;
	TCPsocket sock;
	SDLNet_ResolveHost(&ip, host.c_str(), port);

	sock = SDLNet_TCP_Open(&ip);

	thread t(&WebOt::handleConnection, this, sock);
	t.detach();

}

void WebOt::workManager()
{
	//this function will be a separate process that measures traffic and the amount of messages coming in/out
	//if the workload is high, it may spawn new worker threads to manage 
	p("Work manager started.\n");
	bool alive = true;
	highestWorkerID = 0;


	while(alive)
	{
		if(numOfActiveWorkers == 0)
		{
			spawnWorker();
		}

		//insert logic for when to spawn a new worker
		if(getNumReceivedMessages() > 10)
			spawnWorker();

		//insert logic for when to kill off a worker
		if(getNumReceivedMessages() == 0 && this->numOfActiveWorkers > 1)
			killWorker();


		//Sleep(20); //sleep for 20 milliseconds, no sense in being too controlling
		SDL_Delay(20);
		alive = programIsRunning();
	}	
}