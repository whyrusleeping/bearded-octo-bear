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
	string ipStr;
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
	void work(int workerID);
	void halt();

	void workManager();
	void spawnWorker();
    void killWorker();
	
    string getReceivedMessage();

    void addReceivedMessage(string newMess);

    int getNumReceivedMessages();


    bool programIsRunning();

    bool amiNeeded(int workerID);

    void workerDied(int workerID);

    bool workerIsDead(int workerID);


	//finds the connection id related to the ip, and if its not in the list, adds it and returns a new id
	//defaults to returning -1 when the address is not found, when create is set to true, will assign the ip a new ID and return it instead.
	int getConnectionId(IPaddress *ip, bool create);

	//thread safe cout - another option for later is to have this write to a specified stream so a gui(or other external) can connect into the framework and read the output


	void makeConnection(string host, int port);

	//For each new connection, a thread is spawned to handle it.
	//512 is a VERY arbitrary number, messages will most likely be much bigger(or smaller?)
	//eventually have option for udp connection as well
	void handleConnection(TCPsocket sock);
    void p(string s);
	void forwardConnection();

	string getAssignment(int ID);

};


#endif
