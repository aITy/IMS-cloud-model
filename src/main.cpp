/**
 * Projekt do predmetu IMS - Model cloudoveho centra
 * Vypracoval: 	Jiri Navratil, xnavra36@stud.fit.vutbr.cz
 *				Petr Caha, xcahab00@stud.fit.vutbr.cz
**/

// temporary
#include "simlib.h"
#include <iostream>
#include <string.h>
#include <math.h>
#include <stdarg.h>

using namespace std;

//#define servers_count 4

#define hour * (3600 * 1.0e3)
#define hours hour
#define minute  * (60 * 1.0e3)
#define minutes minute
#define second  * 1.0e3
#define seconds	second
#define miliseconds * 1.0

#define A_req_treatment_T Uniform(0.0025 seconds, 0.05 seconds)
#define B_req_treatment_T Uniform(0.005 seconds, 0.1 seconds)
#define C_req_treatment_T Uniform(0.005 seconds, 0.1 seconds)
#define D_req_treatment_T Uniform(0.05 seconds, 0.15 seconds)

#define server_A_net_link_treatment_T Uniform(0.1 seconds, 0.25 seconds)
#define server_B_net_link_treatment_T Uniform(0.1 seconds, 0.35 seconds)
#define server_C_net_link_treatment_T Uniform(0.1 seconds, 0.5 seconds)
#define server_D_net_link_treatment_T Uniform(0.1 seconds, 0.5 seconds)

#define OVERLOAD_RATIO 0.8

//#define DEBUG 1

#define sim_T 24 hours

const int server_A_net_link_cap = 100;
const int server_B_net_link_cap = 75;
const int server_C_net_link_cap = 50;
const int server_D_net_link_cap = 50;

const int server_A_NCPU = 32;
const int server_B_NCPU = 16;
const int server_C_NCPU = 16;
const int server_D_NCPU = 8;

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

Stat clusterNetlinkLoadPercentage("Cluster netlink load percentage");
Stat serverAnetlinkLoadPercentage("Server A netlink load percentage");
Stat serverBnetlinkLoadPercentage("Server B netlink load percentage");
Stat serverCnetlinkLoadPercentage("Server C netlink load percentage");
Stat serverDnetlinkLoadPercentage("Server D netlink load percentage");

Stat clusterCPUsLoadPercentage("Cluster CPUs load percentage");
Stat serverACPUsLoadPercentage("Server A CPUs load percentage");
Stat serverBCPUsLoadPercentage("Server B CPUs load percentage");
Stat serverCCPUsLoadPercentage("Server C CPUs load percentage");
Stat serverDCPUsLoadPercentage("Server D CPUs load percentage");

double getMax(int count, ...) {
	va_list ap;
	double aux;
	double max = 0.0;
	va_start(ap, count);
	for(int j = 0; j < count; j++) {
		aux = va_arg(ap, double);
		if (aux >= max)
			max = aux;
	}
	va_end(ap);
	return max;
}

int parseTime(const char * str) {

    if (strcmp(str, "day") == 0)
    	return (int) (Time / ((double)1000*60*60*24.0));

    if (strcmp(str, "hours") == 0) {
    	int hours_nb = (int) (Time / ((double)1000*60*60.0));
    	return (hours_nb == 24) ? 0 : hours_nb;
    }
    if (strcmp(str, "minutes") == 0) {
    	int minutes_nb = (int) ((fmod(Time, 1000*60*60.0)) / ((double)1000*60.0));
    	return (minutes_nb == 60) ? 0 : minutes_nb;
    }
    if (strcmp(str, "seconds") == 0) {
    	int seconds_nb = (int) ((fmod((fmod(Time,1000*60*60.0)),1000*60.0)) / 1000);
    	return (seconds_nb == 60) ? 0 : seconds_nb;
    }

    return -1;
}

int checkClusterCPUsOverload() {
 	int overloaded_count = 0;
	int i;
	for(i = 0; i < server_A_NCPU; i++) {
		if (! A_cpu[i].Busy())
			break;
	}
	if (i == server_A_NCPU) overloaded_count++;

	for(i = 0; i < server_B_NCPU; i++) {
		if (! B_cpu[i].Busy())
			break;
	}
	if (i == server_B_NCPU) overloaded_count++;

	for(i = 0; i < server_C_NCPU; i++) {
		if (! C_cpu[i].Busy())
			break;
	}
	if (i == server_C_NCPU) overloaded_count++;

	for(i = 0; i < server_D_NCPU; i++) {
		if (! D_cpu[i].Busy())
			break;
	}
	if (i == server_D_NCPU) overloaded_count++;

	#ifdef DEBUG
		cout << "servers cpu overloaded count:" << overloaded_count << endl;
	#endif

	return overloaded_count;
}

class clusterOverloadCheck : public Process {
public:
	void Behavior() {

		//int ret = checkClusterCPUsOverload();
		double server_A_netlink_load_percentage = (double)server_A_net_link.Used()/server_A_net_link.Capacity();
		double server_B_netlink_load_percentage = (double)server_B_net_link.Used()/server_B_net_link.Capacity();
		double server_C_netlink_load_percentage = (double)server_C_net_link.Used()/server_C_net_link.Capacity();
		double server_D_netlink_load_percentage = (double)server_D_net_link.Used()/server_D_net_link.Capacity();

		double server_A_CPUs_load_percentage, server_B_CPUs_load_percentage,
			server_C_CPUs_load_percentage, server_D_CPUs_load_percentage;

		int i;
		for(i = 0; i < server_A_NCPU; i++) {
			if (! A_cpu[i].Busy())
				break;
		}
		server_A_CPUs_load_percentage = (double) i/server_A_NCPU;

		for(i = 0; i < server_B_NCPU; i++) {
			if (! B_cpu[i].Busy())
				break;
		}
		server_B_CPUs_load_percentage = (double) i/server_B_NCPU;

		for(i = 0; i < server_C_NCPU; i++) {
			if (! C_cpu[i].Busy())
				break;
		}
		server_C_CPUs_load_percentage = (double) i/server_C_NCPU;

		for(i = 0; i < server_D_NCPU; i++) {
			if (! D_cpu[i].Busy())
				break;
		}
		server_D_CPUs_load_percentage = (double) i/server_D_NCPU;

		#ifdef DEBUG
			cout << "server A netlink load: " << server_A_netlink_load_percentage << endl;
			cout << "server B netlink load: " << server_B_netlink_load_percentage << endl;
			cout << "server C netlink load: " << server_C_netlink_load_percentage << endl;
			cout << "server D netlink load: " << server_D_netlink_load_percentage << endl;
		#endif

		serverACPUsLoadPercentage(server_A_CPUs_load_percentage);
		serverBCPUsLoadPercentage(server_B_CPUs_load_percentage);
		serverCCPUsLoadPercentage(server_C_CPUs_load_percentage);
		serverDCPUsLoadPercentage(server_D_CPUs_load_percentage);
		clusterCPUsLoadPercentage((server_A_CPUs_load_percentage + server_B_CPUs_load_percentage
							+ server_C_CPUs_load_percentage + server_D_CPUs_load_percentage) / 4);

		#ifdef DEBUG
			cout << "server A netlink load: " << server_A_netlink_load_percentage << endl;
			cout << "server B netlink load: " << server_B_netlink_load_percentage << endl;
			cout << "server C netlink load: " << server_C_netlink_load_percentage << endl;
			cout << "server D netlink load: " << server_D_netlink_load_percentage << endl;
		#endif

		serverAnetlinkLoadPercentage(server_A_netlink_load_percentage);
		serverBnetlinkLoadPercentage(server_B_netlink_load_percentage);
		serverCnetlinkLoadPercentage(server_C_netlink_load_percentage);
		serverDnetlinkLoadPercentage(server_D_netlink_load_percentage);
		clusterNetlinkLoadPercentage( (server_A_netlink_load_percentage + server_B_netlink_load_percentage
							+ server_C_netlink_load_percentage + server_D_netlink_load_percentage) / 4);

		int overloaded_cluster_netlinks = 0;
		if (server_A_netlink_load_percentage >= OVERLOAD_RATIO) overloaded_cluster_netlinks++;
		if (server_B_netlink_load_percentage >= OVERLOAD_RATIO) overloaded_cluster_netlinks++;
		if (server_C_netlink_load_percentage >= OVERLOAD_RATIO) overloaded_cluster_netlinks++;
		if (server_D_netlink_load_percentage >= OVERLOAD_RATIO) overloaded_cluster_netlinks++;

		if (overloaded_cluster_netlinks >= 2 || ((server_A_CPUs_load_percentage + server_B_CPUs_load_percentage
					+ server_C_CPUs_load_percentage + server_D_CPUs_load_percentage) / 4 >= OVERLOAD_RATIO )) {

			if (server_A_net_link.Full() && server_B_net_link.Full() && server_C_net_link.Full()
				&& server_D_net_link.Full() && checkClusterCPUsOverload() == 4) {
				
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
		int i;
		// 35% chance,that request will be treated by server A
		if (prob < 0.35) {
			A_server:
			if (overwhelmed) {
				#ifdef DEBUG
					cout << "cluster is overwhelmed - ignoring incoming request. Total ingnored requests count:" << ignored_req_count << endl;
				#endif
				ignored_req_count++;
				Terminate();
			}
			else if (overloaded) {
				#ifdef DEBUG
					cout << "request treated by server A. total requests count:" << server_A_net_link_req_count << endl;
				#endif
				t = Time;

				if (server_A_net_link.Full())
					goto B_server;

				Enter(server_A_net_link, 1);
				Wait(server_A_net_link_treatment_T);

				for(i = 0; i < server_A_NCPU; i++) {
					if (!A_cpu[i].Busy()) {
						Seize(A_cpu[i]);
						break;
					}
				}
				if (i == server_A_NCPU) {
					Leave(server_A_net_link, 1);
					goto B_server;
				}
				
				Wait(A_req_treatment_T);
				Leave(server_A_net_link, 1);
				Release(A_cpu[i]);
				treatmentTime(Time - t);
				server_A_net_link_req_count++;
			}
			else if ((double)(server_A_net_link.Used()/server_A_net_link.Capacity()) < OVERLOAD_RATIO) {
				#ifdef DEBUG
					cout << "request treated by server A. total requests count:" << server_A_net_link_req_count << endl;
				#endif
				t = Time;

				Enter(server_A_net_link, 1);
				Wait(server_A_net_link_treatment_T);

				for(i = 0; i < server_A_NCPU; i++) {
					if (!A_cpu[i].Busy()) {
						Seize(A_cpu[i]);
						break;
					}
				}
				if (i == server_A_NCPU) {
					Leave(server_A_net_link, 1);
					goto B_server;
				}
				Wait(A_req_treatment_T);
				Leave(server_A_net_link, 1);
				Release(A_cpu[i]);
				treatmentTime(Time - t);
				server_A_net_link_req_count++;
			}
			else goto B_server;
		}
		// 30% chance,that request will be treated by server B
		else if (prob < 0.65) {
			B_server:
			if (overwhelmed) {
				#ifdef DEBUG
					cout << "cluster is overwhelmed - ignoring incoming request. Total ingnored requests count:" << ignored_req_count << endl;
				#endif
				Terminate();
				ignored_req_count++;
			}
			else if (overloaded) {
				
				#ifdef DEBUG
					cout << "request treated by server B. total requests count:" << server_B_net_link_req_count << endl;
				#endif
				t = Time;
				if (server_B_net_link.Full()) {
					goto C_server;
				}

				Enter(server_B_net_link, 1);
				Wait(server_B_net_link_treatment_T);

				for(i = 0; i < server_B_NCPU; i++) {
					if (!B_cpu[i].Busy()) {
						Seize(B_cpu[i]);
						break;
					}
				}
				if (i == server_B_NCPU) {
					Leave(server_B_net_link, 1);
					goto C_server;
				}

				Wait(B_req_treatment_T);
				Leave(server_B_net_link, 1);
				Release(B_cpu[i]);
				treatmentTime(Time - t);
				server_B_net_link_req_count++;
			}
			else if ((double)(server_B_net_link.Used()/server_B_net_link.Capacity()) < OVERLOAD_RATIO) {
				#ifdef DEBUG
					cout << "request treated by server B. total requests count:" << server_B_net_link_req_count << endl;
				#endif
				t = Time;

				Enter(server_B_net_link, 1);
				Wait(server_B_net_link_treatment_T);

				for(i = 0; i < server_B_NCPU; i++) {
					if (!B_cpu[i].Busy()) {
						Seize(B_cpu[i]);
						break;
					}
				}
				if (i == server_B_NCPU) {
					Leave(server_B_net_link, 1);
					goto C_server;
				}
				
				Wait(B_req_treatment_T);
				Leave(server_B_net_link, 1);
				Release(B_cpu[i]);
				treatmentTime(Time - t);
				server_B_net_link_req_count++;
			}
			else goto C_server;
		}
		// 25% chance,that request will be treated by server C
		else if (prob < 0.9) {
			C_server:
			if (overwhelmed) {
				#ifdef DEBUG
					cout << "cluster is overwhelmed - ignoring incoming request. Total ingnored requests count:" << ignored_req_count << endl;
				#endif
				ignored_req_count++;
				Terminate();
			}

			else if (overloaded) {
				
				#ifdef DEBUG
					cout << "request treated by server C. total requests count:" << server_C_net_link_req_count << endl;
				#endif
				t = Time;
				if (server_C_net_link.Full())
					goto D_server;

				Enter(server_C_net_link, 1);
				Wait(server_C_net_link_treatment_T);

				for(i = 0; i < server_C_NCPU; i++) {
					if (!C_cpu[i].Busy()) {
						Seize(C_cpu[i]);
						break;
					}
				}

				if (i == server_C_NCPU) {
					Leave(server_C_net_link, 1);
					goto D_server;
				}

				Wait(C_req_treatment_T);
				Leave(server_C_net_link, 1);
				Release(C_cpu[i]);
				treatmentTime(Time - t);
				server_C_net_link_req_count++;
			}
			else if ((double) (server_C_net_link.Used()/server_C_net_link.Capacity()) < OVERLOAD_RATIO) {

				#ifdef DEBUG
					cout << "request treated by server C. total requests count:" << server_C_net_link_req_count << endl;
				#endif
				t = Time;

				Enter(server_C_net_link, 1);
				Wait(server_C_net_link_treatment_T);

				for(i = 0; i < server_C_NCPU; i++) {
					if (!C_cpu[i].Busy()) {
						Seize(C_cpu[i]);
						break;
					}
				}

				if (i == server_C_NCPU) {
					Leave(server_C_net_link, 1);
					goto D_server;
				}

				Wait(C_req_treatment_T);
				Leave(server_C_net_link, 1);
				Release(C_cpu[i]);
				treatmentTime(Time - t);
				server_C_net_link_req_count++;
			}
			else goto D_server;
		}
		// 10% chance,that request will be treated by server D
		else {
			D_server:
			if (overwhelmed) {
				#ifdef DEBUG
					cout << "cluster is overwhelmed - ignoring incoming request. Total ingnored requests count:" << ignored_req_count << endl;
				#endif
				ignored_req_count++;
				Terminate();
			}
			else if (overloaded) {
				
				#ifdef DEBUG
					cout << "request treated by server D. total requests count:" << server_D_net_link_req_count << endl;
				#endif
				t = Time;

				if (server_D_net_link.Full()) {
					goto A_server;
				}

				Enter(server_D_net_link, 1);
				Wait(server_D_net_link_treatment_T);

				for(i = 0; i < server_D_NCPU; i++) {
					if (!D_cpu[i].Busy()) {
						Seize(D_cpu[i]);
						break;
					}
				}

				if (i == server_D_NCPU) {
					Leave(server_D_net_link, 1);
					goto A_server;
				}

				Wait(D_req_treatment_T);
				Leave(server_D_net_link, 1);
				Release(D_cpu[i]);
				treatmentTime(Time - t);
				server_D_net_link_req_count++;
			}
			else if ((double)(server_D_net_link.Used()/server_D_net_link.Capacity()) < OVERLOAD_RATIO) {
				
				#ifdef DEBUG
					cout << "request treated by server D. total requests count:" << server_D_net_link_req_count << endl;
				#endif
				t = Time;
				Enter(server_D_net_link, 1);
				Wait(server_D_net_link_treatment_T);

				for(i = 0; i < server_D_NCPU; i++) {
					if (!D_cpu[i].Busy()) {
						Seize(D_cpu[i]);
						break;
					}
				}

				if (i == server_D_NCPU) {
					Leave(server_D_net_link, 1);
					goto A_server;
				}

				Wait(D_req_treatment_T);
				Leave(server_D_net_link, 1);
				Release(D_cpu[i]);
				treatmentTime(Time - t);
				server_D_net_link_req_count++;
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
		Activate(Time + Exponential(0.025 seconds));
	}
};

class highTrafficGenerator : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "generating high traffic" << endl;
		#endif

		// from 2pm
		if (parseTime("hours") >= 14)
			(new incomingClusterRequest(0))->Activate();

		// to 10pm, next activation is scheduled at 2pm next day
		if (parseTime("hours") == 23)
			Activate(Time + (16 * 3600 seconds));
		else
			Activate(Time + Exponential(0.015 seconds));
	}
};

// simulating overwhelm
class DDosGenerator : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "trying to achieve DDoS attack." << endl;
			cout << "day: " << parseTime("day") << " dayhours: " << parseTime("hours") << " dayminutes: " << parseTime("minutes") << endl; 
		#endif
		(new incomingClusterRequest(0))->Activate();
		// testing ... Dal bych to jen jednou treba 3 den nebo neco ...
		// 15 minutes long attempt to achieve DDoS
		// !!! TOCHANGE day() == ?
		if (parseTime("day") == 0 && parseTime("hours") == 2 && parseTime("minutes") >= 30 && parseTime("minutes") <= 45) {

			Activate(Time + Exponential(0.005 seconds));
			//cout << "day: " << parseTime("day") << " dayhours: " << parseTime("hours") << " dayminutes: " << parseTime("minutes") << endl; 
		}
		else {
			Activate(Time + (2 hours + 30 minutes));
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
/*	for (i = 0; i < server_A_NCPU; i++) A_cpu[i].SetQueue(A_cpu_queue);
	for (i = 0; i < server_B_NCPU; i++) B_cpu[i].SetQueue(B_cpu_queue);
	for (i = 0; i < server_C_NCPU; i++) C_cpu[i].SetQueue(C_cpu_queue);
	for (i = 0; i < server_D_NCPU; i++) D_cpu[i].SetQueue(D_cpu_queue);

	//set shared queues for channels
	//??? is it really nessassary to use channels ???
	for (i = 0; i < server_A_NCHANNEL; i++) A_channel[i].SetQueue(A_channel_queue);
	for (i = 0; i < server_B_NCHANNEL; i++) B_channel[i].SetQueue(B_channel_queue);
	for (i = 0; i < server_C_NCHANNEL; i++) C_channel[i].SetQueue(C_channel_queue);
	for (i = 0; i < server_D_NCHANNEL; i++) D_channel[i].SetQueue(D_channel_queue);

*/
	// General test
	(new RequestGeneratorUs)->Activate();
	(new highTrafficGenerator)->Activate();
	(new DDosGenerator)->Activate();
	// .
	// .
	// .
	Run();

	if (!normal_state_changed)
		clusterNormalStateTime(Time - normal_state_T);

	cout << "Number of received requests: " << cluster_req_count << endl;
	cout << "Number of ignored requests: " << ignored_req_count << endl;
	cout << "Ignored requests ratio: " << (double)(ignored_req_count/cluster_req_count) << endl;

	// histogram output
	clusterOverloadTime.Output();
	clusterOverwhelmedTime.Output();
	clusterNormalStateTime.Output();

	clusterNetlinkLoadPercentage.Output();
	serverAnetlinkLoadPercentage.Output();
	serverBnetlinkLoadPercentage.Output();
	serverCnetlinkLoadPercentage.Output();
	serverDnetlinkLoadPercentage.Output();

	clusterCPUsLoadPercentage.Output();
	serverACPUsLoadPercentage.Output();
	serverBCPUsLoadPercentage.Output();
	serverCCPUsLoadPercentage.Output();
	serverDCPUsLoadPercentage.Output();

	treatmentTime.Output();

}