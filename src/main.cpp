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

#define A_req_treatment_T Uniform(0.1 seconds, 1 seconds)
#define B_req_treatment_T Uniform(0.25 seconds, 1 seconds)
#define C_req_treatment_T Uniform(0.5 seconds, 1 seconds)
#define D_req_treatment_T Uniform(0.5 seconds, 1.5 seconds)

#define OVERLOAD_RATIO (double)0.9

//#define DEBUG 1

#define sim_T 3600 * 24 seconds

const int server_A_net_link_cap = 500;
const int server_B_net_link_cap = 250;
const int server_C_net_link_cap = 200;
const int server_D_net_link_cap = 100;

const int server_A_NCPU = 4;
const int server_B_NCPU = 3;
const int server_C_NCPU = 2;
const int server_D_NCPU = 1;

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

bool overloaded = false;//zatizeni
bool overwhelmed = false;//zahlceni (DDoS)
double overloaded_T = 0;
double overwhelmed_T = 0;
double normal_state_T = 0;
bool normal_state_changed = false;

double cluster_req_count = 0;
double ignored_req_count = 0;
double server_A_net_link_req_count = 0;
double server_B_net_link_req_count = 0;
double server_C_net_link_req_count = 0;
double server_D_net_link_req_count = 0;

Histogram clusterOverloadTime("Cluster face to heavy traffic - overloaded", 0, 0.25 seconds, 30);
Histogram clusterOverwhelmedTime("Cluster is overwhelmed", 0, 0.5 seconds, 30);
Histogram clusterNormalStateTime("Cluster face to normal traffic", 0, 25 seconds);
Histogram treatmentTime("Cluster net link treatment time", 0, 0.025 seconds, 30);

Stat clusterLoadPercentage("Cluster load percentage %");
Stat serverAloadPercentage("Server A load percentage %");
Stat serverBloadPercentage("Server B load percentage %");
Stat serverCloadPercentage("Server C load percentage %");
Stat serverDloadPercentage("Server D load percentage %");

int dayhours() {
	return ((int) (Time / 3600)) % 24;
}

int checkClusterCPUsOverload() {
 	int overloaded_count = 0;
	int i;
	for(i = 0; i < server_A_NCPU; i++) {
		if (! A_cpu[i].Busy())
			break;
	}
	if (i == server_A_NCPU - 1) overloaded_count++;

	for(i = 0; i < server_B_NCPU; i++) {
		if (! B_cpu[i].Busy())
			break;
	}
	if (i == server_B_NCPU - 1) overloaded_count++;

	for(i = 0; i < server_C_NCPU; i++) {
		if (! C_cpu[i].Busy())
			break;
	}
	if (i == server_C_NCPU - 1) overloaded_count++;

	for(i = 0; i < server_D_NCPU; i++) {
		if (! D_cpu[i].Busy())
			break;
	}
	if (i == server_D_NCPU - 1) overloaded_count++;

	#ifdef DEBUG
		cout << "servers cpu overloaded count:" << overloaded_count << endl;
	#endif

	return overloaded_count;
}

class clusterOverloadCheck : public Process {
public:
	void Behavior() {

		int ret = checkClusterCPUsOverload();
		double server_A_load_percentage = (double)server_A_net_link.Used()/server_A_net_link.Capacity();
		double server_B_load_percentage = (double)server_B_net_link.Used()/server_B_net_link.Capacity();
		double server_C_load_percentage = (double)server_C_net_link.Used()/server_C_net_link.Capacity();
		double server_D_load_percentage = (double)server_D_net_link.Used()/server_D_net_link.Capacity();

		#ifdef DEBUG
			cout << "server A load: " << server_A_load_percentage << endl;
			cout << "server B load: " << server_B_load_percentage << endl;
			cout << "server C load: " << server_C_load_percentage << endl;
			cout << "server D load: " << server_D_load_percentage << endl;
		#endif

		serverAloadPercentage(server_A_load_percentage);
		serverBloadPercentage(server_B_load_percentage);
		serverCloadPercentage(server_C_load_percentage);
		serverDloadPercentage(server_D_load_percentage);
		clusterLoadPercentage( (server_A_load_percentage + server_B_load_percentage
							+ server_C_load_percentage + server_D_load_percentage) / 4);

		int overloaded_servers = 0;
		if (server_A_load_percentage >= OVERLOAD_RATIO) overloaded_servers++;
		if (server_B_load_percentage >= OVERLOAD_RATIO) overloaded_servers++;
		if (server_C_load_percentage >= OVERLOAD_RATIO) overloaded_servers++;
		if (server_D_load_percentage >= OVERLOAD_RATIO) overloaded_servers++;

		if (overloaded_servers >= 3){//&& ret >= 2) {

			if (server_A_net_link.Full() && server_B_net_link.Full() && server_C_net_link.Full()
				&& server_D_net_link.Full() ){//&& ret == 4) {
				
				#ifdef DEBUG
					cout << "cluster is overwhelmed" << endl;
				#endif

				overwhelmed = true;
				normal_state_changed = true;
				overwhelmed_T = Time;
			}
			else {

				#ifdef DEBUG
					cout << "cluster is overloaded" << endl;
				#endif 

				overloaded = true;
				normal_state_changed = true;
				overloaded_T = Time;
			}
			clusterNormalStateTime(Time - normal_state_T);
			normal_state_T = 0;
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

			if (normal_state_T == 0)
				normal_state_T = Time;
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
			else if ((double)server_A_net_link.Used()/server_A_net_link.Capacity() < OVERLOAD_RATIO) {
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
			else if ((double)server_B_net_link.Used()/server_B_net_link.Capacity() < OVERLOAD_RATIO) {
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
			else if ((double)server_C_net_link.Used()/server_C_net_link.Capacity() < OVERLOAD_RATIO) {
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
			else if ((double)server_D_net_link.Used()/server_D_net_link.Capacity() < OVERLOAD_RATIO) {
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

class RequestGeneratorUs : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "generating request in US (more often)" << endl;
		#endif
		(new incomingClusterRequest(0))->Activate();
		Activate(Time + Exponential(0.05 seconds));
	}
};

class highTrafficGenerator : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "generating high traffic" << endl;
		#endif

		if (dayhours() >= 14)
			(new incomingClusterRequest(0))->Activate();

		// to 22:00, next activation will be in 16 hours, that is at 2pm next day
		if (dayhours() == 23)
			Activate(Time + (16 * 3600 seconds));
		else
			Activate(Time + Exponential(0.025 seconds));
	}
};

// simulating overwhelm
class DDosGenerator : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "trying to achieve DDoS attack." << endl;
		#endif
		// testing ... Dal bych to jen jednou treba 3 den nebo neco ...
		if (dayhours() == 2) {
			(new incomingClusterRequest(0))->Activate();
		}

	}
};

class RequestGeneratorEu : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "generating request in EU (less often)" << endl;
		#endif
		(new incomingClusterRequest(0))->Activate();
		Activate(Time + Exponential(0.1 seconds));
	}
};

class RequestGeneratorGeneral : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "generating general request averaged time" << endl;
		#endif
		(new incomingClusterRequest(0))->Activate();
		Activate(Time + Exponential(0.075 seconds));
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
	(new highTrafficGenerator)->Activate();
	// .
	// .
	// .
	Run();

	if (!normal_state_changed)
		clusterNormalStateTime(Time - normal_state_T);

	cout << "Number of received requests: " << cluster_req_count << endl;
	cout << "Number of ignored requests: " << ignored_req_count << endl;
	cout << "Ignored requests ratio: " << (double)ignored_req_count/cluster_req_count << endl;

	// histogram output
	clusterOverloadTime.Output();
	clusterOverwhelmedTime.Output();
	clusterNormalStateTime.Output();
	clusterLoadPercentage.Output();
	serverAloadPercentage.Output();
	serverBloadPercentage.Output();
	serverCloadPercentage.Output();
	serverDloadPercentage.Output();
	treatmentTime.Output();

}