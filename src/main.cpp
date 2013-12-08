/**
 * Projekt do predmetu IMS - Model cloudoveho centra
 * Vypracoval: 	Jiri Navratil, xnavra36@stud.fit.vutbr.cz
 *				Petr Caha, xcahab00@stud.fit.vutbr.cz
**/

#include "simlib.h"
#include <iostream>
#include <sstream>
#include <utility>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <math.h>
#include <stdarg.h>

using namespace std;

#define hour * (3600 * 1.0e3)
#define hours hour
#define minute  * (60 * 1.0e3)
#define minutes minute
#define second  * 1.0e3
#define seconds	second
#define miliseconds * 1.0

#define SERVERS_COUNT 4
#define OVERLOAD_RATIO 0.8
#define sim_T 24 hours

//#define DEBUG 1

// auxiliary fuction which returns summary of server components
// NOT USED !!!
int sumArr(int * arr) {
	int val = 0;
	for(int i = 0; i < SERVERS_COUNT; i++) {
		val += arr[i];
	}
	return val;
}

int Uniform(int min, int max) {
  return int( Uniform(double(min), double(max)) );
}

int SERVERS_NCPU[SERVERS_COUNT] = { 32, 16, 16, 8 };
// netlink request capacity had to be lowered due to evaluating low double value as 0 -> cant simulate so frequent requests 
int SERVERS_NETLINK_CAP[SERVERS_COUNT] = { 100, 75, 50, 50 };
int SERVERS_NDISK[SERVERS_COUNT] = { 10, 6, 6, 4 };
int SERVERS_NMEM[SERVERS_COUNT] = { 8192, 4096, 2048, 2048 };
int SERVERS_NCHANNEL[SERVERS_COUNT] = { 3, 2, 1, 1 };

// SADLY NOT WORKING - cannot define array, if the size is not known at compile runtime
//#define TOTAL_CPUS sumArr(SERVERS_NCPU)
//#define TOTAL_DISKS sumArr(SERVERS_NDISK)
//#define TOTAL_CHANNELS sumArr(SERVERS_NCHANNEL)

// SO IT IS NESSESARY TO DEFINE TOTAL AMOUNT OF CPUS, DISKS AND CHANNELS THIS WAY =(
// IT IS SUMMARY OF ALL SERVERS COMPONENTS
#define TOTAL_CPUS 32 + 16 + 16 + 8  	//all cpus
#define TOTAL_DISKS 10 + 6 + 6 + 4 	 	//all disks
#define TOTAL_CHANNELS 3 + 2 + 1 + 1 	//all channels

double SERVERS_SEARCH_T[SERVERS_COUNT] = { 20 miliseconds, 60 miliseconds, 80 miliseconds, 100 miliseconds };
double SERVERS_TR_T[SERVERS_COUNT] = { 5 miliseconds, 10 miliseconds, 20 miliseconds , 25 miliseconds };

// !!! COMPILE ERROR ON LINE RIGHT AFTER USING SERVERS_REQ_TREATMENT_T[i].first or .second - INITIALIZED IN MAIN ... =(
// some compiler bug - seems like related only to some versions =(

typedef struct {
	double val[2];
} MyPair;

MyPair SERVERS_REQ_TREATMENT_T[SERVERS_COUNT] = {
	{{ 0.025 seconds, 0.05 seconds }},
	{{ 0.05 seconds, 0.1 seconds }},
	{{ 0.05 seconds, 0.1 seconds }},
	{{ 0.05 seconds, 0.15 seconds }}
};

MyPair SERVERS_NETLINK_TREATMENT_T[SERVERS_COUNT] = {
	{{ 0.1 seconds, 0.25 seconds }},
	{{ 0.1 seconds, 0.35 seconds }},
	{{ 0.1 seconds, 0.5 seconds }},
	{{ 0.1 seconds, 0.5 seconds }}
};

Store servers_net_links[SERVERS_COUNT];
Store servers_memory[SERVERS_COUNT];

Facility servers_cpus[TOTAL_CPUS];
Queue servers_cpus_queues[SERVERS_COUNT];

Facility servers_channels[TOTAL_CHANNELS];
Queue servers_channels_queues[SERVERS_COUNT];

Facility servers_hdds[TOTAL_DISKS];

Histogram clusterOverloadTime("Cluster is overloaded", 0, 0.01 second, 10);
Histogram clusterOverwhelmedTime("Cluster is overwhelmed", 0, 0.1 second, 10);
Histogram clusterNormalStateTime("Cluster face to normal traffic", 0, 30 minutes);
Histogram treatmentTime("Cluster net link treatment time", 0.1 second, 0.025 second, 25);
Histogram netLayerReqWalkthroughtTime("Cluster request net layer walkthrough time ", 0.05 second, 0.1 second, 30);
Histogram clusterDataReplicationCycleTime("Cluster data replication cycle time", 2 seconds, 2 seconds, 15);
Histogram serverDataReplicationCycleTime[SERVERS_COUNT];

Stat clusterNetlinkLoadPercentage("Cluster netlink load percentage");
Stat clusterCPUsLoadPercentage("Cluster CPUs load percentage");
Stat serversNetlinksLoadPercentage[SERVERS_COUNT];
Stat serversCPUsLoadPercentage[SERVERS_COUNT];

bool overloaded = false; // zatizeni
bool overwhelmed = false; // zahlceni
bool normal_state_changed = false;
double overloaded_T = 0;
double overwhelmed_T = 0;
double normal_state_T = 0;

double cluster_req_count = 0;
double net_ignored_req_count = 0;
double app_ignored_req_count = 0;
double file_share_upload_actions = 0;
double file_share_upload_actions_buf = 0;
double server_net_link_req_count[SERVERS_COUNT] = { 0.0 };

// NOT USED
//auxiliary (variadic) function,that returns summary of all parameters passed to this function
// 1st parameter has to be the count of parameters passed to the function excluding this parameter
int sum(int count, ...) {
	va_list ap;
	int aux = 0;
	va_start(ap, count);
	for(int j = 0; j < count; j++) {
		aux += va_arg(ap, int);
	}
	va_end(ap);
	return aux;
}

string convertInt(int number)
{
   stringstream ss;
   ss << number;
   return ss.str();
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

int checkNetlinksOverwhelm() {
	int val = 0;
	for( int i = 0; i < SERVERS_COUNT; i++) {
		if (servers_net_links[i].Full()) val++;
	}
	return val;
}

class clusterLoadCheck : public Process {
public:
	void Behavior() {
		int i,j;
		double servers_netlink_load_percentage[SERVERS_COUNT];
		double servers_cpu_load_percentage[SERVERS_COUNT];
		double total_cluster_cpu_load_percentage = 0.0;
		double total_cluster_netlink_load_percentage = 0.0;
		int overloaded_servers_cpus = 0;

		for(i = 0; i < SERVERS_COUNT; i++) {
			double used = servers_net_links[i].Used();
			double cap = servers_net_links[i].Capacity();
			servers_netlink_load_percentage[i] = used/cap;
			total_cluster_netlink_load_percentage += servers_netlink_load_percentage[i];
		}
		total_cluster_netlink_load_percentage = total_cluster_netlink_load_percentage / SERVERS_COUNT;

		for(i = 0; i < SERVERS_COUNT; i++) {
			int init = 0;
			int end = 0;
			int counter = 0;

			if (i == 0) {
				init = 0;
				end = SERVERS_NCPU[i];
			}
			else {
				for(j = 0; j <= i; j++) {
					if (j != i) init += SERVERS_NCPU[j];
					end += SERVERS_NCPU[j];
				}
			}

			for(j = init; j < end; j++) {
				if (! servers_cpus[j].Busy())
					break;
				counter++;
			}
			if (j == end) overloaded_servers_cpus++;

			servers_cpu_load_percentage[i] = ((double)counter) / SERVERS_NCPU[i];
			total_cluster_cpu_load_percentage += servers_cpu_load_percentage[i];
		}

		total_cluster_cpu_load_percentage = total_cluster_cpu_load_percentage / SERVERS_COUNT;

		#ifdef DEBUG
			for(i = 0; i < SERVERS_COUNT; i++) {
				cout << "server " << i + 1 << " netlink load: " << servers_netlink_load_percentage[i] << endl;
				cout << "server " << i + 1 << " cpus load: " << servers_cpu_load_percentage[i] << endl;
			}
		#endif

		//add value to statistics
		for(i = 0; i < SERVERS_COUNT; i++) {
			serversCPUsLoadPercentage[i](servers_cpu_load_percentage[i]);
			serversNetlinksLoadPercentage[i](servers_netlink_load_percentage[i]);
		}

		clusterCPUsLoadPercentage(total_cluster_cpu_load_percentage);
		clusterNetlinkLoadPercentage(total_cluster_netlink_load_percentage);

		int overloaded_cluster_netlinks = 0;
		for(i = 0; i < SERVERS_COUNT; i++) {
			if(servers_netlink_load_percentage[i] >= OVERLOAD_RATIO) overloaded_cluster_netlinks++;
		}

		if (overloaded_cluster_netlinks >= 2 || total_cluster_cpu_load_percentage >= OVERLOAD_RATIO ) {

			if (checkNetlinksOverwhelm() >= SERVERS_COUNT - 1 || overloaded_servers_cpus == SERVERS_COUNT) {
				
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
						cout << "Cluster turned to normal state, overwhelmed time:" << Time - overwhelmed_T << endl;
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
						cout << "Cluster turned to normal state, overloaded time:" << Time - overloaded_T << endl;
					#endif

					clusterOverloadTime(Time - overloaded_T);
				}
				overloaded = false;
				overloaded_T = 0;
			}

			if (normal_state_T == 0)
				normal_state_T = Time;
		}
	}
};


// process, which simulates passage through the net layer
class incomingClusterRequest : public Process {
	double net_layer_T;
	double app_layer_T;
	double req_size;
	double tcpu;
	int mem;
public:
	incomingClusterRequest(int priority) : Process(priority) {
		net_layer_T = Time;
		app_layer_T = 0.0;
		req_size = Uniform(10, 1000);
		mem = Uniform(10, 65);
		tcpu = 0;
	};
	void Behavior() {
		cluster_req_count++;
		
		#ifdef DEBUG
			cout << "incoming cluster request. Total requests count:" << cluster_req_count << endl;
		#endif

		(new clusterLoadCheck)->Activate();

		double t;
		int i;

		// randomly access one of the server - SHOULD BE MORE COMPLEX  ... server 1 is stronger ? -> bigger chance
		int idx = Uniform(0, SERVERS_COUNT);

		repeat_net_layer:
		
			if (overwhelmed) {
				#ifdef DEBUG
					cout << "cluster is overwhelmed - ignoring incoming request. Total ingnored requests count:" << net_ignored_req_count << endl;
				#endif
				net_ignored_req_count++;
				Terminate();
			}
			else if (overloaded) {
				#ifdef DEBUG
					cout << "request is trying to get treated by server " << idx << ". total requests count:" << server_net_link_req_count[idx] << endl;
				#endif
				
				t = Time;

				int start = 0;
				int end = 0;

				if (idx == 0) {
					start = 0;
					end = SERVERS_NCPU[idx];
				}
				else {
					for(i = 0; i <= idx; i++) {
						if (i != idx) start += SERVERS_NCPU[i];
						end += SERVERS_NCPU[i];
					}
				}

				// NEBO AKORAT KDYZ NENI PLNE ZABRANO ??? TZN. ZE PRI ZATEZENI PROSTE ZKUSIM ZABRAT, PRIPADNE LOAD BALACING i S CPU ? DO STATISTIK KOUKAT V PETRI SITI JE NEMOZNE
				//if (servers_net_links[idx].Full() || serversCPUsLoadPercentage[idx].MeanValue() >= OVERLOAD_RATIO) {

				if (servers_net_links[idx].Full()) {
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
				}

				Enter(servers_net_links[idx], 1);

				/*
				pair<double, double> aux = SERVERS_REQ_TREATMENT_T[idx];
				cout << aux.first;
				cout << aux.second;
				!!! ACCESING PAIR ITEM CAUSE ERROR : expected unqualified-id before * token ????????
				*/

				MyPair my = SERVERS_NETLINK_TREATMENT_T[idx];
				Wait(Uniform(my.val[0], my.val[1]));

				for(i = start; i < end; i++) {
					if (! servers_cpus[i].Busy()) {
						Seize(servers_cpus[i]);
						break;
					}
				}
				if (i == end) {
					Leave(servers_net_links[idx], 1);
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
				}
				
				//Wait(Uniform(SERVERS_REQ_TREATMENT_T[idx].first, SERVERS_REQ_TREATMENT_T[idx].second));
				my = SERVERS_REQ_TREATMENT_T[idx];
				Wait(Uniform(my.val[0], my.val[1]));
				Leave(servers_net_links[idx], 1);
				Release(servers_cpus[i]);
				treatmentTime(Time - t);
				netLayerReqWalkthroughtTime(Time - net_layer_T);
				server_net_link_req_count[idx]++;
			}
			// LOAD BALANCING STALE NEBO NE -> TZN. ZE BY LOADBALANCING FUNGOVAL JEN PRI ZATEZI, VIZ r.322
			else if ((double)servers_net_links[idx].Used()/servers_net_links[idx].Capacity() < OVERLOAD_RATIO) {
			// NEBO DOKONCE LOAD BALANCING JAK MA BYT KDYZ NENI ZATEZ -> PREPOSILAT NA MIN VYTIZENOU LINKU
				#ifdef DEBUG
					cout << "request is treated by server "<< idx << ". total requests count:" << server_net_link_req_count[idx] << endl;
				#endif
				t = Time;

				Enter(servers_net_links[idx], 1);
				MyPair my = SERVERS_NETLINK_TREATMENT_T[idx];
				Wait(Uniform(my.val[0], my.val[1]));

				int start = 0;
				int end = 0;

				if (idx == 0) {
					start = 0;
					end = SERVERS_NCPU[idx];
				}
				else {
					for(i = 0; i <= idx; i++) {
						if (i != idx) start += SERVERS_NCPU[i];
						end += SERVERS_NCPU[i];
					}
				}

				for(i = start; i < end; i++) {
					if (!servers_cpus[i].Busy()) {
						Seize(servers_cpus[i]);
						break;
					}
				}
				if (i == end) {
					Leave(servers_net_links[idx], 1);
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
				}

				//Wait(Uniform(SERVERS_REQ_TREATMENT_T[idx].first, SERVERS_REQ_TREATMENT_T[idx].second));
				my = SERVERS_REQ_TREATMENT_T[idx];
				Wait(Uniform(my.val[0], my.val[1]));
				Leave(servers_net_links[idx], 1);
				Release(servers_cpus[i]);
				treatmentTime(Time - t);
				netLayerReqWalkthroughtTime(Time - net_layer_T);
				server_net_link_req_count[idx]++;
			}
			else {
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
			}
		// end of repeat_net_layer

		double app_svc_choice = Random();
		double app_svc_action_choice = Random();
		
		if (app_svc_choice <= 0.01) {
			// 1% bad app request - no application installed on that server
			app_ignored_req_count++;
			Terminate();
		}
		// 45% file sharing service
		else if (app_svc_choice <= 0.46) {
			// 70% download
			if (app_svc_action_choice <= 0.7) {

			}
			// 30% upload
			else {
				file_share_upload_actions++;
				file_share_upload_actions_buf++;
			}

		}
		// 35% chance to video streaming
		else if (app_svc_choice <= 0.81) {

		}
		//19% chance to another service ... ??? 
		else {

		}
	}
};

class dataReplication : public Process {
private:
	double one_cycle_T;
	double cluster_cycle_T;
public:
	dataReplication(int priority) : Process(priority), one_cycle_T(0.0), cluster_cycle_T(0.0) {};
	void Behavior() {
		repeat:
			WaitUntil(file_share_upload_actions_buf > 0);
			file_share_upload_actions_buf--;
			int i,j;

			one_cycle_T = Time;
			cluster_cycle_T = Time;
			// pro vsechny servery -> jedno cpu -> po porade vsechny disky s prioritou obsadim na mmntik
			for(i = 0; i < SERVERS_COUNT; i++) {
				int init = 0;
				int end = 0;
				int idx = 0;

				if (i == 0) {
					init = 0;
					end = SERVERS_NCPU[i];
				}
				else {
					for(j = 0; j <= i; j++) {
						if (j != i) init += SERVERS_NCPU[j];
						end += SERVERS_NCPU[j];
					}
				}

				for(j = init; j < end; j++) {
					if (! servers_cpus[j].Busy()) {
						idx = j;
						Seize(servers_cpus[j]/*, 2*/);
						break;
					}
				}
				if (j == end) {
					idx = Uniform(init, end);
					Seize(servers_cpus[idx], 2);
				}

				init = 0;
				end = 0;

				if (i == 0) {
					init = 0;
					end = SERVERS_NDISK[i];
				}
				else {
					for(j = 0; j <= i; j++) {
						if (j != i) init += SERVERS_NDISK[j];
						end += SERVERS_NDISK[j];
					}
				}

				for(j = init; j < end; j++) {
					Seize(servers_hdds[j], 2);
					// TO CHANGE
					Wait(Uniform(0.25 seconds, 0.75 second));
					Release(servers_hdds[j]);
				}
				Release(servers_cpus[idx]);

				serverDataReplicationCycleTime[i](Time - one_cycle_T);
			}

			clusterDataReplicationCycleTime(Time - cluster_cycle_T);

			goto repeat;
	}
};

class RequestGeneratorUs : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "generating request in US (more frequent)" << endl;
		#endif
		(new incomingClusterRequest(0))->Activate();
		Activate(Time + Exponential(0.02 seconds));
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

		if (parseTime("day") == 0 && parseTime("hours") == 0) // initial scheduling
			Activate(Time + 14 hours);
		// to 9:59:59...pm, next activation is scheduled at 2pm next day
		else if (parseTime("hours") == 22)
			Activate(Time + (16 hours));
		else
			Activate(Time + Exponential(0.002 seconds));

	}
};

// simulating overwhelm
class DDosGenerator : public Event {
	void Behavior() {

		// 15 minutes long attempt to achieve DDoS
		if (parseTime("day") == 0 && parseTime("hours") == 0) // initial scheduling on first day 2:30 am
			Activate(Time + 2 hours + 30 minutes);
		else if (parseTime("day") == 0 && parseTime("hours") == 2 && parseTime("minutes") >= 30 && parseTime("minutes") <= 45) {
			
			#ifdef DEBUG
				cout << "trying to achieve DDoS attack." << endl;
			#endif

			(new incomingClusterRequest(0))->Activate();
			(new incomingClusterRequest(0))->Activate();

			Activate(Time + Exponential(0.005 seconds));
			//cout << "day: " << parseTime("day") << " dayhours: " << parseTime("hours") << " dayminutes: " << parseTime("minutes") << endl; 
		}
		else {
			// scheduling at another day ???
			//Activate(Time + (2 hours + 30 minutes));
		}
	}
};

class adminTasksGenerator : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "incoming admin task" << endl;
		#endif

		if (parseTime("day") == 0 && parseTime("hours") == 0) // initial scheduling
			Activate(Time + 15 hours);
		// one hour long administration work from 3pm to 4pm
		else if (parseTime("hours") >= 15 && parseTime("hours") < 16) {
			// administration tasks have higher priority
			(new incomingClusterRequest(1))->Activate();
			Activate(Time + Exponential(0.1 second));
		}
		else {
			// scheduling on another day - admin task ended at 4pm, nest activation is scheduled on 3pm next day
			Activate(Time + 23 hours);
		}
	}
};

class RequestGeneratorEu : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "generating request in EU (less frequent)" << endl;
		#endif
		(new incomingClusterRequest(0))->Activate();
		Activate(Time + Exponential(0.05 seconds));
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

	int i, j, init_cpu, init_channel, end_cpu, end_channel;

	for (i = 0; i < SERVERS_COUNT; i++) {
		servers_net_links[i].SetCapacity(SERVERS_NETLINK_CAP[i]);
		servers_memory[i].SetCapacity(SERVERS_NMEM[i]);
	}

	string links_names, cpus_names;

	char net[SERVERS_COUNT][80];
	char cpu[SERVERS_COUNT][80];
	char data_repl_ser[SERVERS_COUNT][80];
	string nb;
	
	for(i = 0; i < SERVERS_COUNT; i++) {
		net[i][0] = '\0';
		cpu[i][0] = '\0';
		data_repl_ser[i][0] = '\0';
		nb = convertInt(i + 1);
		strcpy(net[i], "Server ");
		strcpy(cpu[i], "Server ");
		strcpy(data_repl_ser[i], "Server ");
		strcat(net[i], nb.c_str());
		strcat(cpu[i], nb.c_str());
		strcat(data_repl_ser[i], nb.c_str());
		strcat(net[i], " netlink load percentage");
		strcat(cpu[i], " CPUs load percentage");
		strcat(data_repl_ser[i], " data replication cycle time");
		
		serversNetlinksLoadPercentage[i].SetName((const char *) net[i]);
		serversCPUsLoadPercentage[i].SetName((const char * ) cpu[i]);
		serverDataReplicationCycleTime[i].SetName((const char *) data_repl_ser[i]);
		serverDataReplicationCycleTime[i].Init(0.5 second, 2 second, 10);
	}

	// set shared queues for server CPUs and channels
	for(i = 0; i < SERVERS_COUNT; i++) {
		init_cpu = 0;
		init_channel = 0;

		end_cpu = 0;
		end_channel = 0;

		if (i == 0) {
			init_cpu = 0;
			init_channel = 0;
			end_cpu = SERVERS_NCPU[i];
			end_channel = SERVERS_NCHANNEL[i];
		}
		else {
			for(j = 0; j <= i; j++) {
				if (j != i) {
					init_cpu += SERVERS_NCPU[j];
					init_channel += SERVERS_NCHANNEL[j];
				}
				end_cpu += SERVERS_NCPU[j];
				end_channel += SERVERS_NCHANNEL[j];
			}
		}

		for(j = init_cpu; j < end_cpu; j++) {
			servers_cpus[j].SetQueue(servers_cpus_queues[i]);
		}

		for(j = init_channel; j < end_channel; j++) {
			servers_channels[j].SetQueue(servers_channels_queues[i]);
		}
	}

	(new RequestGeneratorUs)->Activate();
	(new highTrafficGenerator)->Activate();
	(new DDosGenerator)->Activate();
	(new adminTasksGenerator)->Activate();
	(new dataReplication(1))->Activate();

	Run();

	if (!normal_state_changed)
		clusterNormalStateTime(Time - normal_state_T);

	cout << "Number of received requests: " << cluster_req_count << endl;
	cout << "ignored net layer requests: " << net_ignored_req_count << endl;
	cout << "Ignored net layer requests ratio: " << (double)(net_ignored_req_count/cluster_req_count) << endl;
	cout << "Ignored application layer requests: " << app_ignored_req_count << endl;
	cout << "Number of fileshare upload requests: " << file_share_upload_actions << endl;

	// statistics output
	clusterOverloadTime.Output();
	clusterOverwhelmedTime.Output();
	clusterNormalStateTime.Output();

	clusterNetlinkLoadPercentage.Output();

	for (i = 0; i < SERVERS_COUNT; i++) {
		serversNetlinksLoadPercentage[i].Output();
	}

	clusterCPUsLoadPercentage.Output();

	for (i = 0; i < SERVERS_COUNT; i++) {
		serversCPUsLoadPercentage[i].Output();
	}

	treatmentTime.Output();
	netLayerReqWalkthroughtTime.Output();

	clusterDataReplicationCycleTime.Output();
	for(i = 0; i < SERVERS_COUNT; i++) {
		serverDataReplicationCycleTime[i].Output();
	}
/*
	for (i = 0; i < SERVERS_COUNT; i++) {
		servers_cpus_queues[i].Output();
	}
*/
	//SIMLIB_statistics.Output();
}