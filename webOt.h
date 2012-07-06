#ifndef WEBOT_H
#define WEBOT_H
#include <thread>
#include <mutex>
#include <iostream>
#include <queue>
#include <vector>

#include "strExt.h"
#ifdef WIN32
#include "SDL_net.h"
#else
#include "SDL/SDL_net.h"
#endif

using std::vector;
using std::queue;
using std::cout;
using std::thread;

typedef struct socketAssignment
{
	queue<string> toDo;
	int completed;
}socketAssignment;
typedef struct connection
{
	int ping;
	IPaddress addr;
}connection;
typedef struct workerInfo
{
	bool shouldDie; //the work manager will set this to true when the given worker should stop executing
	bool died; //the worker sets this to true when it finally finishes
}workerInfo;
typedef struct search
{
	int source; //0- from a user 1- started by this node 2-received from another node
	int ID;
	int recvdFrom; //id of the node the request was received from
	//some sort of 'path so far' for multiple purposes:
	//      1) for returning the path to the target
	//      2) for checking if the request received contains this node already, to prevent circular loops
	int numSentOut; //the number of searches sent out from this node, used 
}search;
typedef struct netTableEntry
{
	string name;
	IPaddress addr; //maybe
	//mPath path;
	int avPing;
}netTableEntry;

class WebOt
{
private:
	vector<connection> connections;
	std::mutex con_tex;

	vector<socketAssignment> sockAssigns;
	std::mutex sockA_tex;

	queue<string> ReceivedMessages;
	std::mutex rcm_tex;

	vector<workerInfo> workInfo;
	std::mutex workInfo_tex;
	int numOfActiveWorkers;

	queue<int> unusedWorkerIDs;
	queue<int> usedWorkerIDs;
	int highestWorkerID;		

	bool running;
	std::mutex run_tex;
	bool haltSignal;

	std::mutex output_tex;


	IPaddress ip;

	TCPsocket csd, sd;

	string name;
	string RealName;
	string NodeInfo;

public:
	WebOt();
	~WebOt();

	int init();
	void run();

	void halt();

	void workManager()
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

	void spawnWorker()
	{
		int IdToAssign = 0;
		if(!unusedWorkerIDs.empty())
		{					
			IdToAssign = unusedWorkerIDs.front();
			unusedWorkerIDs.pop();
			while(!workerIsDead(IdToAssign))
			{
				unusedWorkerIDs.push(IdToAssign); //i need to be careful here, something could glitch up and this could potentially run forever and not spawn another proc
				IdToAssign = unusedWorkerIDs.front();
				unusedWorkerIDs.pop();
			}
		}
		else
		{
			IdToAssign = highestWorkerID++;				
		}
		thread newWorker(&WebOt::work, this, IdToAssign);
		usedWorkerIDs.push(IdToAssign);
		newWorker.detach();
		numOfActiveWorkers++;
	}

	void killWorker()
	{
		std::lock_guard<std::mutex> lk(workInfo_tex); //im hoping that this takes the scope of the if statement and is deleted after leaving it
		if(workInfo[usedWorkerIDs.front()].died == false)
		{
			workInfo[usedWorkerIDs.front()].shouldDie = true;
			unusedWorkerIDs.push(usedWorkerIDs.front());
			usedWorkerIDs.pop();
		}

	}

	void work(int workerID);

	string getReceivedMessage()
	{
		std::lock_guard<std::mutex> lk(rcm_tex);
		string rcm = "";
		if(ReceivedMessages.size() > 0)
		{
			rcm = ReceivedMessages.front();
			ReceivedMessages.pop();
		}
		return rcm;
	}
	void addReceivedMessage(string newMess)
	{
		std::lock_guard<std::mutex> lk(rcm_tex);
		ReceivedMessages.push(newMess);
	}

	int getNumReceivedMessages()
	{
		std::lock_guard<std::mutex> lk(rcm_tex);
		return ReceivedMessages.size();
	}

	bool programIsRunning()
	{
		std::lock_guard<std::mutex> lk(run_tex);
		return running;
	}

	bool amiNeeded(int workerID)
	{
		std::lock_guard<std::mutex> lk(workInfo_tex); //idea for workerInfo, add mutex to the struct 
		return !workInfo[workerID].shouldDie;	
	}

	void workerDied(int workerID)
	{
		std::lock_guard<std::mutex> lk(workInfo_tex);
		workInfo[workerID].died = true;
		numOfActiveWorkers--;
	}

	bool workerIsDead(int workerID)
	{
		std::lock_guard<std::mutex> lk(workInfo_tex);
		return workInfo[workerID].died;
	}


	//finds the connection id related to the ip, and if its not in the list, adds it and returns a new id
	//defaults to returning -1 when the address is not found, when create is set to true, will assign the ip a new ID and return it instead.
	int getConnectionId(IPaddress *ip, bool create);

	//thread safe cout - another option for later is to have this write to a specified stream so a gui(or other external) can connect into the framework and read the output
	void p(string s)
	{
		std::lock_guard<std::mutex> lk(output_tex);
		cout << s;
	}

	void makeConnection(string host, int port)
	{
		IPaddress remoteIP;
		TCPsocket sock;
		SDLNet_ResolveHost(&ip, host.c_str(), port);

		sock = SDLNet_TCP_Open(&ip);

		thread t(&WebOt::handleConnection, this, sock, &remoteIP);
		t.detach();

	}

	//For each new connection, a thread is spawned to handle it.
	//512 is a VERY arbitrary number, messages will most likely be much bigger(or smaller?)
	//eventually have option for udp connection as well
	void handleConnection(TCPsocket sock, IPaddress *remoteIP);

	void forwardConnection();

	string getAssignment(int ID)
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

};


#endif