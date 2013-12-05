/**
 * Projekt do predmetu IMS - Model cloudoveho centra
 * Vypracoval: 	Jiri Navratil, xnavra36@stud.fit.vutbr.cz
 *				Petr Caha, xcahab00@stud.fit.vutbr.cz
**/

// temporary
#include "simlib.h"
#include <iostream>

using namespace std;

//#define servers_count 4

#define minute  * (60 * 1.0e3)
#define minutes minute
#define second  * 1.0e3
#define seconds	second
#define miliseconds * 1.0

#define A_req_treatment_T Uniform(0.001, 0.01)
#define B_req_treatment_T Uniform(0.005, 0.1)
#define C_req_treatment_T Uniform(0.01, 0.1)
#define D_req_treatment_T Uniform(0.025, 0.1)

#define DEBUG 1

// ty statistiky dropboxu jsou za 42 dni
#define sim_T 3600*24*42 seconds

const int server_A_net_link_cap = 10000;
const int server_B_net_link_cap = 5000;
const int server_C_net_link_cap = 2500;
const int server_D_net_link_cap = 3000;

const int server_A_NCPU = 8;
const int server_B_NCPU = 6;
const int server_C_NCPU = 3;
const int server_D_NCPU = 4;

const int server_A_NDISK = 10;
const int server_B_NDISK = 6;
const int server_C_NDISK = 4;
const int server_D_NDISK = 6;

const int server_A_NCHANNEL = 3;
const int server_B_NCHANNEL = 2;
const int server_C_NCHANNEL = 1;
const int server_D_NCHANNEL = 1;

const double server_A_SEARCH_T = 20 miliseconds;
const double server_B_SEARCH_T = 60 miliseconds;
const double server_C_SEARCH_T = 100 miliseconds;
const double server_D_SEARCH_T = 80 miliseconds;

const double server_A_TRANSFER_T = 5 miliseconds;
const double server_B_TRANSFER_T = 10 miliseconds;
const double server_C_TRANSFER_T = 25 miliseconds;
const double server_D_TRANSFER_T = 15 miliseconds;

/*
*	TY FRONTY TAM NEJSPIS NEBUDOU,
*	PROTOZE MI ZAHAZUJEM POZADAVKY,JE-LI CLUSTER PLNE VYTIZEN
*/

Store server_A_net_link("Server A - network link", server_A_net_link_cap);
Queue server_A_net_link_queue("Server A - network link queue");

Store server_B_net_link("Server B - network link", server_B_net_link_cap);
Queue server_B_net_link_queue("Server B - network link queue");

Store server_C_net_link("Server C - network link", server_C_net_link_cap);
Queue server_C_net_link_queue("Server C - network link queue");

Store server_D_net_link("Server D - network link", server_D_net_link_cap);
Queue server_D_net_link_queue("Server D - network link queue");

Store server_A_mem("Server A memory", 8192);
Store server_B_mem("Server B memory", 4096);
Store server_C_mem("Server C memory", 2048);
Store server_D_mem("Server D memory", 2048);

Facility  A_cpu[server_A_NCPU];
Queue     A_cpu_queue("Server A CPU queue");

Facility  B_cpu[server_B_NCPU];
Queue     B_cpu_queue("Server B CPU queue");

Facility  C_cpu[server_C_NCPU];
Queue     C_cpu_queue("Server C CPU queue");

Facility  D_cpu[server_D_NCPU];
Queue     D_cpu_queue("Server D CPU queue");

Facility  A_hdd[server_A_NDISK];
Facility  B_hdd[server_B_NDISK];
Facility  C_hdd[server_C_NDISK];
Facility  D_hdd[server_D_NDISK];

Facility  A_channel[server_A_NCHANNEL];
Queue     A_channel_queue("Server A channel queue");

Facility  B_channel[server_B_NCHANNEL];
Queue     B_channel_queue("Server B channel queue");

Facility  C_channel[server_C_NCHANNEL];
Queue     C_channel_queue("Server C channel queue");

Facility  D_channel[server_D_NCHANNEL];
Queue     D_channel_queue("Server D channel queue");

bool overloaded = false;
bool overwhelmed = false;
double overloaded_T = 0;
double overwhelmed_T = 0;
double normal_state_T = 0;

double cluster_req_count = 0;
double ignored_req_count = 0;
double server_A_net_link_req_count = 0;
double server_B_net_link_req_count = 0;
double server_C_net_link_req_count = 0;
double server_D_net_link_req_count = 0;

Histogram clusterOverloadTime("Cluster face to heavy traffic - Cluster is overloaded", 0, 0.1, 30);
Histogram clusterOverwhelmedTime("Cluster is overwhelmed", 0, 0.1, 30);
Histogram clusterNormalStateTime("Cluster face to normal traffic", 10, 10, 50);
Histogram treatmentTime("Cluster net link treatment time", 0, 2 seconds, 30);

/** TODO:
 *	detekce kdy je cluster zas v poradku + zapis do Histogramu Time - overloaded_T
 *	to jeste zalezi,jak to udelat spravne
 *	to, co je vyreseno goto by melo asi byt nejak reseno frontou
 **/

int checkClusterCPUsOverload() {
 	int overloaded_count = 0;
	int i;
	for(i = 0; i < server_A_NCPU; i++) {
		if (! A_cpu[i].Busy())
			break;
	}
	if (i == server_A_NCPU - 1) {
		overloaded_count++;
	}
	for(i = 0; i < server_B_NCPU; i++) {
		if (! B_cpu[i].Busy())
			break;
	}
	if (i == server_B_NCPU - 1) {
		overloaded_count++;
	}
	for(i = 0; i < server_C_NCPU; i++) {
		if (! C_cpu[i].Busy())
			break;
	}
	if (i == server_C_NCPU - 1) {
		overloaded_count++;
	}
	for(i = 0; i < server_D_NCPU; i++) {
		if (! D_cpu[i].Busy())
			break;
	}
	if (i == server_D_NCPU - 1) {
		overloaded_count++;
	}

	#ifdef DEBUG
		cout << "servers cpu overloaded count:" << overloaded_count << endl;
	#endif

	return overloaded_count;
}

class clusterOverloadCheck : public Process {
public:
	void Behavior() {

		int ret = checkClusterCPUsOverload();

		if (server_A_net_link.Used()/server_A_net_link.Capacity() >= 0.9
			&& server_B_net_link.Used()/server_B_net_link.Capacity() >= 0.9
			&& server_C_net_link.Used()/server_C_net_link.Capacity() >= 0.9
			&& server_D_net_link.Used()/server_D_net_link.Capacity() >= 0.9
			&& ret >= 3 ) {

			if (server_A_net_link.Full() && server_B_net_link.Full() && server_C_net_link.Full()
				&& server_D_net_link.Full() && ret == 4) {
				
				#ifdef DEBUG
					cout << "cluster is overwhelmed" << endl;
				#endif

				overwhelmed = true;
				overwhelmed_T = Time;
			}
			else {

				#ifdef DEBUG
					cout << "cluster is overloaded" << endl;
				#endif 

				overloaded = true;
				overloaded_T = Time;
			}
		}
		else {
			#ifdef DEBUG
				cout << "cluster is in normal state" << endl;
			#endif
			if (overwhelmed) {
				if (overwhelmed_T != 0) {

					#ifdef DEBUG
						cout << "Cluster turned to normal state, overwhelmed time:" << Time - overwhelmed_T << "endl";
					#endif

					clusterOverwhelmedTime(Time - overwhelmed_T);
				}
				overwhelmed = false;
				overwhelmed_T = 0;
			}
			else if (overloaded) {
				// beware of initial check
				if (overloaded_T != 0) {

					#ifdef DEBUG
						cout << "Cluster turned to normal state, overloaded time:" << Time - overloaded_T << "endl";
					#endif

					clusterOverloadTime(Time - overloaded_T);
				}
				overloaded = false;
				overloaded_T = 0;
			}
		}

		//Terminate();	
	}
};

class incomingClusterRequest : public Process {
public:
	incomingClusterRequest(int priority) : Process(priority) {};
	void Behavior() {
		cluster_req_count++;
		
		#ifdef DEBUG
			cout << "incoming cluster request. Total requests count:" << cluster_req_count << endl;
		#endif

		(new clusterOverloadCheck)->Activate();
		double prob = Random();
		double t;
		// 35% chance,that request will be treated by server A
		if (prob < 0.35) {
			A_server:
			if (overloaded) {
				server_A_net_link_req_count++;
				#ifdef DEBUG
					cout << "request treated by server A. total requests count:" << server_A_net_link_req_count << endl;
				#endif
				t = Time;
				Enter(server_A_net_link, 1);
				Wait(A_req_treatment_T);
				Leave(server_A_net_link, 1);
				treatmentTime(Time - t);
			}
			else if (server_A_net_link.Used()/server_A_net_link.Capacity() < 0.9) {
				server_A_net_link_req_count++;
				#ifdef DEBUG
					cout << "request treated by server A. total requests count:" << server_A_net_link_req_count << endl;
				#endif
				t = Time;
				Enter(server_A_net_link, 1);
				Wait(A_req_treatment_T);
				Leave(server_A_net_link, 1);
				treatmentTime(Time - t);
			}
			else goto B_server;
		}
		// 25% chance,that request will be treated by server B
		else if (prob < 0.6) {
			B_server:
			if (overloaded) {
				server_B_net_link_req_count++;
				#ifdef DEBUG
					cout << "request treated by server B. total requests count:" << server_B_net_link_req_count << endl;
				#endif
				t = Time;
				Enter(server_B_net_link, 1);
				Wait(B_req_treatment_T);
				Leave(server_B_net_link, 1);
				treatmentTime(Time - t);
			}
			else if (server_B_net_link.Used()/server_B_net_link.Capacity() < 0.9) {
				server_B_net_link_req_count++;
				#ifdef DEBUG
					cout << "request treated by server B. total requests count:" << server_B_net_link_req_count << endl;
				#endif
				t = Time;
				Enter(server_B_net_link, 1);
				Wait(B_req_treatment_T);
				Leave(server_B_net_link, 1);
				treatmentTime(Time - t);
			}
			else goto C_server;
		}
		// 25% chance,that request will be treated by server C
		else if (prob < 0.85) {
			C_server:
			if (overloaded) {
				server_C_net_link_req_count++;
				#ifdef DEBUG
					cout << "request treated by server C. total requests count:" << server_C_net_link_req_count << endl;
				#endif
				t = Time;
				Enter(server_C_net_link, 1);
				Wait(C_req_treatment_T);
				Leave(server_C_net_link, 1);
				treatmentTime(Time - t);
			}
			else if (server_C_net_link.Used()/server_C_net_link.Capacity() < 0.9) {
				server_C_net_link_req_count++;
				#ifdef DEBUG
					cout << "request treated by server C. total requests count:" << server_C_net_link_req_count << endl;
				#endif
				t = Time;
				Enter(server_C_net_link, 1);
				Wait(C_req_treatment_T);
				Leave(server_C_net_link, 1);
				treatmentTime(Time - t);
			}
			else goto D_server;
		}
		// 15% chance,that request will be treated by server D
		else {
			D_server:
			if (overwhelmed) {
				// cluster je zahlcen - odchod transakce ze systemu
				ignored_req_count++;
				#ifdef DEBUG
					cout << "cluster is overwhelmed - ignoring incoming request. Total ingnored requests count:" << ignored_req_count << endl;
				#endif
				Terminate();
			}
			else if (overloaded) {
				server_D_net_link_req_count++;
				#ifdef DEBUG
					cout << "request treated by server D. total requests count:" << server_D_net_link_req_count << endl;
				#endif
				t = Time;
				Enter(server_D_net_link, 1);
				Wait(D_req_treatment_T);
				Leave(server_D_net_link, 1);
				treatmentTime(Time - t);
			}
			else if (server_D_net_link.Used()/server_D_net_link.Capacity() < 0.9) {
				server_D_net_link_req_count++;
				#ifdef DEBUG
					cout << "request treated by server D. total requests count:" << server_D_net_link_req_count << endl;
				#endif
				t = Time;
				Enter(server_D_net_link, 1);
				Wait(D_req_treatment_T);
				Leave(server_D_net_link, 1);
				treatmentTime(Time - t);
			}
			else {				
				goto A_server;
			}
		}
	}
};

class loadBalancing : public Process {
public:
	loadBalancing(int priority) : Process(priority) {};
	void Behavior() {
		WaitUntil(server_A_net_link_queue.Length()>0
			|| server_B_net_link_queue.Length()>0
			|| server_C_net_link_queue.Length()>0
			|| server_D_net_link_queue.Length()>0);

		//blbost, ten request je sice ve fronte, ale vola se na nej activate a tim
		// se zas dostane do pohybu,takze chovani musi byt napsano pro ten request
		// nejde si vratit z fronty dany process a nakladat s nim nejak
		// fronta nema takove metody, mozna to pujde jinak,ale co jsem koukal,tak
		// ani Entity, ani process ani queue nic takoveho nema,protoze je to myslenkove blbe

	}
};

/*
class RequestGeneratorUs : public Event {
	void Behavior() {
		(new loadBalancing(0))->Activate();
		Activate(Time + Exponential(0.05));
	}
public:
	RequestGeneratorUs(){Activate();}
};
*/
class RequestGeneratorUs : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "generating request" << endl;
		#endif
		(new incomingClusterRequest(0))->Activate();
		Activate(Time + Exponential(0.05));
	}
};

class RequestGeneratorEu : public Event {
	void Behavior() {
		(new loadBalancing(0))->Activate();
		Activate(Time + Exponential(0.1));
	}
};

class RequestGeneratorGeneral : public Event {
	void Behavior() {
		(new loadBalancing(0))->Activate();
		Activate(Time + Exponential(0.075));
	}
};

int main() {
	Init(0, sim_T);

	int i;

	// set shared queues for servers CPUs
	for (i = 0; i < server_A_NCPU; i++) A_cpu[i].SetQueue(A_cpu_queue);
	for (i = 0; i < server_B_NCPU; i++) B_cpu[i].SetQueue(B_cpu_queue);
	for (i = 0; i < server_C_NCPU; i++) C_cpu[i].SetQueue(C_cpu_queue);
	for (i = 0; i < server_D_NCPU; i++) D_cpu[i].SetQueue(D_cpu_queue);

	//set shared queues for channels
	//??? is it really nessassary to use channels ???
	for (i = 0; i < server_A_NCHANNEL; i++) A_channel[i].SetQueue(A_channel_queue);
	for (i = 0; i < server_B_NCHANNEL; i++) B_channel[i].SetQueue(B_channel_queue);
	for (i = 0; i < server_C_NCHANNEL; i++) C_channel[i].SetQueue(C_channel_queue);
	for (i = 0; i < server_D_NCHANNEL; i++) D_channel[i].SetQueue(D_channel_queue);


	// General test
	(new RequestGeneratorUs)->Activate();
	// .
	// .
	// .
	Run();

	// histogram output
	clusterOverloadTime.Output();
	clusterOverwhelmedTime.Output();
	treatmentTime.Output();

}