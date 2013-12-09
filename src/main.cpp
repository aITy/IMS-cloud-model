/**
 * Projekt do predmetu IMS - Model cloudoveho centra
 * Vypracoval: 	Jiri Navratil, xnavra36@stud.fit.vutbr.cz
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
#include <vector>

using namespace std;

#define hour * (3600 * 1.0e3)
#define hours hour
#define minute  * (60 * 1.0e3)
#define minutes minute
#define second  * 1.0e3
#define seconds	second
#define miliseconds * 1.0

#define SERVERS_COUNT 4
#define SERVERS_TOOKDOWN 0
#define OVERLOAD_RATIO 0.7
#define sim_T 24 hours

//#define DEBUG 1

bool SERVERS_RUNNING[SERVERS_COUNT];
int SERVERS_NCPU[SERVERS_COUNT] = { 32, 24, 16, 16 };

// netlink request capacity had to be lowered due to evaluating low double value as 0 -> cant simulate so frequent requests 
int SERVERS_NETLINK_CAP[SERVERS_COUNT] = { 100, 75, 50, 50 };
int SERVERS_NDISK[SERVERS_COUNT] = { 32, 32, 24, 16 };
int SERVERS_NMEM[SERVERS_COUNT] = { 16384, 8192, 8192, 8192 };

// SO IT IS NESSESARY TO DEFINE TOTAL AMOUNT OF CPUS AND DISKS THIS WAY =(
// IT IS SUMMARY OF ALL SERVERS COMPONENTS - THE RESULT VALUE MUST MATCH THE COMPONENT SUM defined above
#define TOTAL_CPUS 32 + 24 + 16 + 16  	//all cpus
#define TOTAL_DISKS 32 + 32 + 24 + 16	//all disks

const double SERVERS_SEARCH_T[SERVERS_COUNT] = { 0.02 second, 0.06 second, 0.08 second, 0.1 second };

const double SERVERS_U_TRANSFER_T[SERVERS_COUNT] = { 0.1 second, 0.15 seconds, 0.175 second , 0.225 second };
const double SERVERS_D_TRANSFER_T[SERVERS_COUNT] = { 0.05 second, 0.01 second, 0.02 second, 0.05 second };

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
Facility servers_hdds[TOTAL_DISKS];

Queue servers_cpus_queues[SERVERS_COUNT];
Queue servers_hdds_queues[SERVERS_COUNT];
Queue servers_mems_queues[SERVERS_COUNT];

Histogram clusterOverloadTime("Cluster is overloaded", 0, 0.01 second, 10);
Histogram clusterOverwhelmedTime("Cluster is overwhelmed", 0, 0.1 second, 10);
Histogram clusterNormalStateTime("Cluster face to normal traffic", 0, 30 minutes);
Histogram treatmentTime("Cluster net link treatment time", 0.1 second, 0.025 second, 25);
Histogram responseTime("request response time", 0, 0.25 second, 15);
Histogram netLayerReqWalkthroughtTime("Request net layer walkthrough time", 0.05 second, 0.1 second, 15);
Histogram fileshareDownloadTime[SERVERS_COUNT];
Histogram fileshareUploadTime[SERVERS_COUNT];
Histogram emailServiceTime[SERVERS_COUNT];

Histogram clusterDataReplicationCycleTime("Cluster data replication cycle time", 0, 0.25 second, 15);
Histogram serverDataReplicationCycleTime[SERVERS_COUNT];

Stat clusterNetlinkLoadPercentage("Cluster netlink load percentage");
Stat clusterCPUsLoadPercentage("Cluster CPUs load percentage");
Stat serversNetlinksLoadPercentage[SERVERS_COUNT];
Stat serversCPUsLoadPercentage[SERVERS_COUNT];

bool overloaded = false;
bool overwhelmed = false;
bool normal_state_changed = false;
double overloaded_T = 0;
double overwhelmed_T = 0;
double normal_state_T = 0;

double cluster_req_count = 0;
double net_layer_req_count = 0;
double app_layer_req_count = 0;
double net_ignored_req_count = 0;
double app_ignored_req_count = 0;
double file_share_download_actions = 0; 
double file_share_upload_actions = 0;
double video_streaming_actions = 0;
double data_replication_buf = 0;
double email_actions = 0;
double server_net_link_req_count[SERVERS_COUNT];

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

int Uniform(int min, int max) {
  return int( Uniform(double(min), double(max)) );
}

// auxiliary function that convert integer number into string
string convertInt(int number)
{
   stringstream ss;
   ss << number;
   return ss.str();
}

// function,that returns actual time as a part of the day based on given parameter
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
			if ( SERVERS_RUNNING[i] == false ) {
				servers_netlink_load_percentage[i] = 1;
				total_cluster_netlink_load_percentage += 1;
				continue;
			}
			double used = servers_net_links[i].Used();
			double cap = servers_net_links[i].Capacity();
			servers_netlink_load_percentage[i] = used/cap;
			total_cluster_netlink_load_percentage += servers_netlink_load_percentage[i];
		}
		total_cluster_netlink_load_percentage = total_cluster_netlink_load_percentage / SERVERS_COUNT;

		for(i = 0; i < SERVERS_COUNT; i++) {
			if (! SERVERS_RUNNING[i]) {
				overloaded_servers_cpus++;
				servers_cpu_load_percentage[i] = 1;
				total_cluster_cpu_load_percentage += 1;
				continue;
			}
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
			if (! SERVERS_RUNNING[i]) {
				serversCPUsLoadPercentage[i](1);
				serversNetlinksLoadPercentage[i](1);
				continue;
			}
			serversCPUsLoadPercentage[i](servers_cpu_load_percentage[i]);
			serversNetlinksLoadPercentage[i](servers_netlink_load_percentage[i]);
		}

		clusterCPUsLoadPercentage(total_cluster_cpu_load_percentage);
		clusterNetlinkLoadPercentage(total_cluster_netlink_load_percentage);

		int overloaded_cluster_netlinks = 0;
		for(i = 0; i < SERVERS_COUNT; i++) {
			if (!SERVERS_RUNNING[i]) {
				overloaded_cluster_netlinks++;
				continue;
			}
			if(servers_netlink_load_percentage[i] >= OVERLOAD_RATIO) overloaded_cluster_netlinks++;
		}

		if (overloaded_cluster_netlinks >= 2 || total_cluster_cpu_load_percentage >= OVERLOAD_RATIO ) {

			if (checkNetlinksOverwhelm() >= SERVERS_COUNT - 1 || overloaded_servers_cpus >= SERVERS_COUNT - 1) {
				
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
	int mem;
	int idx;
public:
	incomingClusterRequest(int priority) : Process(priority) {
		net_layer_T = Time;
		mem = Uniform(1, 33);
		idx = 0;
	};
	void Behavior() {
		cluster_req_count++;
		net_layer_req_count++;
		
		#ifdef DEBUG
			cout << "incoming cluster request. Total requests count:" << cluster_req_count << endl;
		#endif

		(new clusterLoadCheck)->Activate();

		double t;
		int i,j;
		
		// randomly access on of the servers - using auxiliary Uniform function
		idx = Uniform((int)0, (int)SERVERS_COUNT);
		for(i = 0; i < SERVERS_COUNT; i++) {
			if (SERVERS_RUNNING[idx])
				break;
			else
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
		}

		// ALL SERVERS ARE DOWN
		if (i == SERVERS_COUNT) {
			net_ignored_req_count++;
			Terminate();
		}

		repeat_net_layer:

			// check whether at least one server is running
			for(i = 0; i < SERVERS_COUNT; i++) {
				if (SERVERS_RUNNING[i])
					break;
			}
			// al servers down
			if (i == SERVERS_COUNT) {
				net_ignored_req_count++;
				Terminate();
			}

			// check whether the choosed server is running, otherwise try to access next server
			if (!SERVERS_RUNNING[idx]) {
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_net_layer;
			}
		
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

				if (!SERVERS_RUNNING[idx]) {
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
				}

				if (servers_net_links[idx].Full()) {
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
				}

				if (!SERVERS_RUNNING[idx]) {
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

				if (!SERVERS_RUNNING[idx]) {
					Leave(servers_net_links[idx], 1);
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
				}

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

				if (!SERVERS_RUNNING[idx]) {
					Leave(servers_net_links[idx], 1);
					Release(servers_cpus[i]);
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
				}
				
				//Wait(Uniform(SERVERS_REQ_TREATMENT_T[idx].first, SERVERS_REQ_TREATMENT_T[idx].second));
				my = SERVERS_REQ_TREATMENT_T[idx];
				Wait(Uniform(my.val[0], my.val[1]));
				Leave(servers_net_links[idx], 1);
				Release(servers_cpus[i]);
				if (servers_cpus_queues[idx].Length() > 0)
					(servers_cpus_queues[idx].GetFirst())->Activate();
				treatmentTime(Time - t);
				netLayerReqWalkthroughtTime(Time - net_layer_T);
				server_net_link_req_count[idx]++;
			}
			// LOAD BALANCING STALE NEBO NE -> TZN. ZE BY LOADBALANCING FUNGOVAL JEN PRI ZATEZI, VIZ r.379 - BLBOST
			else if ((double)servers_net_links[idx].Used()/servers_net_links[idx].Capacity() < OVERLOAD_RATIO) {
			// NEBO DOKONCE LOAD BALANCING JAK MA BYT KDYZ NENI ZATEZ -> PREPOSILAT NA MIN VYTIZENOU LINKU ???
				#ifdef DEBUG
					cout << "request is treated by server "<< idx << ". total requests count:" << server_net_link_req_count[idx] << endl;
				#endif
				t = Time;

				if (!SERVERS_RUNNING[idx]) {
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
				}

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

				if (!SERVERS_RUNNING[idx]) {
					Leave(servers_net_links[idx], 1);
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
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

				if (!SERVERS_RUNNING[idx]) {
					Release(servers_cpus[i]);
					Leave(servers_net_links[idx], 1);
					idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
				}

				//Wait(Uniform(SERVERS_REQ_TREATMENT_T[idx].first, SERVERS_REQ_TREATMENT_T[idx].second));
				my = SERVERS_REQ_TREATMENT_T[idx];
				Wait(Uniform(my.val[0], my.val[1]));
				Leave(servers_net_links[idx], 1);
				Release(servers_cpus[i]);
				if (servers_cpus_queues[idx].Length() > 0)
					(servers_cpus_queues[idx].GetFirst())->Activate();
				treatmentTime(Time - t);
				netLayerReqWalkthroughtTime(Time - net_layer_T);
				net_layer_T = Time - net_layer_T;
				server_net_link_req_count[idx]++;
			}
			else {
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
					goto repeat_net_layer;
			}
		// end of repeat_net_layer

		double app_svc_choice = Random();
		double app_svc_action_choice = Random();

		app_layer_req_count++;

		int start_cpu = 0;
		int start_hdd = 0;
		int end_cpu = 0;
		int end_hdd = 0;
		
		if (app_svc_choice < 0.01) {
			// 1% bad app request - such an application is not installed in the cluster
			app_ignored_req_count++;
			Terminate();
		}
		// 45% file sharing service
		else if (app_svc_choice < 0.46) {
			
			double file_share_download_T = Time;
			double file_share_upload_T = Time;

			repeat_mem_fileshare:

			start_cpu = 0;
			end_cpu = 0;
			start_hdd = 0;
			end_hdd = 0;

			if (!SERVERS_RUNNING[idx]) {
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_fileshare;
			}

			if (servers_memory[idx].Free() < (unsigned) mem) {
				Into(servers_mems_queues[idx]);
				Passivate();
				goto repeat_mem_fileshare;
			}

			Enter(servers_memory[idx], mem);

			repeat_cpu_fileshare_upload:

			if (!SERVERS_RUNNING[idx]) {
				Leave(servers_memory[idx], mem);
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_fileshare;
			}

			if (idx == 0) {
				start_cpu = 0;
				end_cpu = SERVERS_NCPU[idx];

				start_hdd = 0;
				end_hdd = SERVERS_NDISK[idx];
			}
			else {
				start_cpu = 0;
				end_cpu = 0;
				start_hdd = 0;
				end_hdd = 0;

				for(i = 0; i <= idx; i++) {
					if (i != idx) {
						start_cpu += SERVERS_NCPU[i];
						start_hdd += SERVERS_NDISK[i];
					}
					end_cpu += SERVERS_NCPU[i];
					end_hdd += SERVERS_NDISK[i];
				}
			}

			if (!SERVERS_RUNNING[idx]) {
				Leave(servers_memory[idx], mem);
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_fileshare;
			}

			for(i = start_cpu; i < end_cpu; i++) {
				if (!servers_cpus[i].Busy()) {
					Seize(servers_cpus[i]);
					break;
				}
			}

			if (i == end_cpu) {
				Into(servers_cpus_queues[idx]);
				Passivate();
				goto repeat_cpu_fileshare_upload;
			}

			if (!SERVERS_RUNNING[idx]) {
				Release(servers_cpus[i]);
				Leave(servers_memory[idx], mem);
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_fileshare;
			}

			Wait(Exponential(SERVERS_SEARCH_T[idx]));

			Release(servers_cpus[i]);

			if (servers_cpus_queues[idx].Length() > 0)
				(servers_cpus_queues[idx].GetFirst())->Activate();

			repeat_hdd_fileshare_upload:

			if (!SERVERS_RUNNING[idx]) {
				Leave(servers_memory[idx], mem);
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_fileshare;
			}

			for(j = start_hdd; j < end_hdd; j++) {
				if (! servers_hdds[j].Busy()) {
					Seize(servers_hdds[j]);
					break;
				}
			}

			if (j == end_hdd) {
				Into(servers_hdds_queues[idx]);
				Passivate();
				goto repeat_hdd_fileshare_upload;
			}

			// 55% download
			if (app_svc_action_choice < 0.55) {

				Wait(Exponential(SERVERS_D_TRANSFER_T[idx]));
			}
			// 45% upload
			else {
				Wait(Exponential(SERVERS_U_TRANSFER_T[idx]));
			}	

			Release(servers_hdds[j]);

			if (servers_hdds_queues[idx].Length() > 0)
				(servers_hdds_queues[idx].GetFirst())->Activate();

			Leave(servers_memory[idx], mem);

			if (servers_mems_queues[idx].Length() > 0)
				(servers_mems_queues[idx].GetFirst())->Activate();

			if (app_svc_action_choice < 0.55) {
				fileshareDownloadTime[idx](Time - file_share_download_T);
				responseTime(net_layer_T + (Time - file_share_download_T));

				file_share_download_actions++;
			}
			else {

				fileshareUploadTime[idx](Time - file_share_upload_T);
				responseTime(net_layer_T + (Time - file_share_upload_T));

				file_share_upload_actions++;
				//trigger data replication
				data_replication_buf++;
			}	

		}
		// 35% chance to mail service
		//else if (app_svc_choice < 0.81) {

		// 55% mail service
		else {

			double email_T = Time;

			repeat_mem_email:

			start_cpu = 0;
			end_cpu = 0;
			start_hdd = 0;
			end_hdd = 0;

			if (!SERVERS_RUNNING[idx]) {
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_email;
			}

			if (servers_memory[idx].Free() < (unsigned) mem) {
				Into(servers_mems_queues[idx]);
				Passivate();
				goto repeat_mem_email;
			}

			Enter(servers_memory[idx], mem);

			if (idx == 0) {
				start_cpu = 0;
				end_cpu = SERVERS_NCPU[idx];

				start_hdd = 0;
				end_hdd = SERVERS_NDISK[idx];
			}
			else {
				start_cpu = 0;
				end_cpu = 0;
				start_hdd = 0;
				end_hdd = 0;
				for(i = 0; i <= idx; i++) {
					if (i != idx) {
						start_cpu += SERVERS_NCPU[i];
						start_hdd += SERVERS_NDISK[i];
					}
					end_cpu += SERVERS_NCPU[i];
					end_hdd += SERVERS_NDISK[i];
				}
			}

			repeat_cpu_email:

			if (!SERVERS_RUNNING[idx]) {
				Leave(servers_memory[idx], mem);
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_email;
			}

			for(i = start_cpu; i < end_cpu; i++) {
				if (!servers_cpus[i].Busy()) {
					Seize(servers_cpus[i]);
					break;
				}
			}
			
			// idx == 0 - 3 SEGFAULT -> 4 servers_cpus_queues ... ??? same as following error
			if (i == end_cpu) {
				//cout << idx << endl;
				Into(servers_cpus_queues[idx]);
				Passivate();
				goto repeat_cpu_email;
			}

			if (!SERVERS_RUNNING[idx]) {
				Leave(servers_memory[idx], mem);
				Release(servers_cpus[i]);
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_email;
			}


			//cout << "idx " << idx << endl;
			// SEG FAULT ON IDX == 1 ???????? IMPOSSIBLE due to const double SERVERS_SEARCH_T[SERVERS_COUNT]
			// idx cannot be higher than SERVERS_COUNT - 1 or bellow 0 ...
			Wait(Exponential(SERVERS_SEARCH_T[idx]));

			Release(servers_cpus[i]);

			if (servers_cpus_queues[idx].Length() > 0)
				(servers_cpus_queues[idx].GetFirst())->Activate();

			repeat_hdd_email:

			if (!SERVERS_RUNNING[idx]) {
				Leave(servers_memory[idx], mem);
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_email;
			}

			for(j = start_hdd; j < end_hdd; j++) {
				if (! servers_hdds[j].Busy()) {
					Seize(servers_hdds[j]);
					break;
				}
			}

			if (j == end_hdd) {
				Into(servers_hdds_queues[idx]);
				Passivate();
				goto repeat_hdd_email;
			}

			if (!SERVERS_RUNNING[idx]) {
				Leave(servers_memory[idx], mem);
				Release(servers_hdds[j]);
				idx = (idx == SERVERS_COUNT - 1) ? 0 : idx + 1;
				goto repeat_mem_email;
			}

			Wait(Exponential(0.1 * ((SERVERS_D_TRANSFER_T[idx] + SERVERS_U_TRANSFER_T[idx])/2)));

			Release(servers_hdds[j]);

			if (servers_hdds_queues[idx].Length() > 0)
				(servers_hdds_queues[idx].GetFirst())->Activate();

			Leave(servers_memory[idx], mem);

			if (servers_mems_queues[idx].Length() > 0)
				(servers_mems_queues[idx].GetFirst())->Activate();

			emailServiceTime[idx](Time - email_T);

			email_actions++;
			// trigger data replication
			data_replication_buf++;
		}
		//19% chance to video streaming service
		/*else {

			video_streaming_actions++;
		}*/
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
			WaitUntil(data_replication_buf > 0);
			data_replication_buf--;

			int i,j;

			one_cycle_T = Time;
			cluster_cycle_T = Time;
			// pro vsechny servery -> jedno cpu, po porade vsechny disky s prioritou obsadim na mmntik
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

				repeat_cpu:

				for(j = init; j < end; j++) {
					if (! servers_cpus[j].Busy()) {
						idx = j;
						Seize(servers_cpus[j], 2);
						break;
					}
				}
				if (j == end) {
					Into(servers_cpus_queues[i]);
					Passivate();
					goto repeat_cpu;
				}

				Wait(Exponential(SERVERS_SEARCH_T[i]));
				Release(servers_cpus[idx]);

				if (servers_cpus_queues[i].Length() > 0)
					(servers_cpus_queues[i].GetFirst())->Activate();

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

				repeat_hdd:

				for(j = init; j < end; j++) {
					
					if (!servers_hdds[j].Busy()) {
						Seize(servers_hdds[j], 2);
						Wait(Uniform(0.001 second, 0.005 second));
						Release(servers_hdds[j]);

						if (servers_hdds_queues[i].Length() > 0)
							(servers_hdds_queues[i].GetFirst())->Activate();

						break;
					}
				}
				if (j == end) {
					Into(servers_hdds_queues[i]);
					Passivate();
					goto repeat_hdd;
				}

				serverDataReplicationCycleTime[i](Time - one_cycle_T);
			}

			clusterDataReplicationCycleTime(Time - cluster_cycle_T);

//			Wait(Uniform(2 second, 5 second));

			goto repeat;
	}
};

class RequestGeneratorUs : public Event {
	void Behavior() {
		#ifdef DEBUG
			cout << "generating request in US (more frequent)" << endl;
		#endif
		(new incomingClusterRequest(0))->Activate();
		Activate(Time + Exponential(0.01 seconds));
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
			Activate(Time + Exponential(0.02 second));

	}
};

// generating admin task process with higher priority
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
			Activate(Time + Exponential(0.2 second));
		}
		else {
			// scheduling on another day - admin task ended at 4pm, nest activation is scheduled on 3pm next day
			Activate(Time + 23 hours);
		}
	}
};

// trying to simulate overwhelm state
class DosGenerator : public Event {
	void Behavior() {

		// 15 minutes long attempt to achieve DoS
		if (parseTime("day") == 0 && parseTime("hours") == 0) // initial scheduling on first day 2:30 am
			Activate(Time + 2 hours + 30 minutes);
		
		if (parseTime("day") == 0 && parseTime("hours") == 2 && parseTime("minutes") >= 30 && parseTime("minutes") <= 45) {
			
			#ifdef DEBUG
				cout << "trying to achieve DDoS attack." << endl;
			#endif

			(new incomingClusterRequest(0))->Activate();
			//(new incomingClusterRequest(0))->Activate();

			Activate(Time + Exponential(0.0045 seconds));
		}
	}
};

class ServersTakedown : public Event {
public:
	bool down;
	vector<int> ind;

	ServersTakedown() {
		down = false;
		for(int i = 0; i < SERVERS_TOOKDOWN; i++) {
			ind.push_back(0);
		}
	}

	void Behavior() {
		// in high traffic (from 2pm to 10 pm) -> take down one of the server for 5 minutes at 3:01pm
		if (down) {
			for(int i = 0; i < SERVERS_TOOKDOWN; i++) {
				SERVERS_RUNNING[ind[i]] = true;
			}
			down = false;
		}
		if (parseTime("day") == 0 && parseTime("hours") == 15 && !down) {
			int pom = Uniform((int)0, (int)SERVERS_COUNT);

			for(int i = 0; i < SERVERS_TOOKDOWN; i++) {
				ind[i] = (pom == SERVERS_COUNT - 1) ? 0 : pom + 1;
				SERVERS_RUNNING[ind[i]] = false;
				down = true;
			}
			Activate(Time + 1 hour);
		}
		if (parseTime("day") == 0 && parseTime("hours") == 0)
			Activate(Time + 15 hours + 1 minute);
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
			cout << "generating general request time (average value of US and EU generator)" << endl;
		#endif
		(new incomingClusterRequest(0))->Activate();
		Activate(Time + Exponential(0.035 seconds));
	}
};

int main() {
	Init(0, sim_T);

	int i, j, init_cpus, init_hdds, end_cpus, end_hdds;

	for (i = 0; i < SERVERS_COUNT; i++) {
		servers_net_links[i].SetCapacity(SERVERS_NETLINK_CAP[i]);
		servers_memory[i].SetCapacity(SERVERS_NMEM[i]);
		server_net_link_req_count[i] = 0.0;
		SERVERS_RUNNING[i] = true;
	}

	string links_names, cpus_names;

	char net[SERVERS_COUNT][80];
	char cpu[SERVERS_COUNT][80];
	char data_repl_ser[SERVERS_COUNT][80];
	char upload[SERVERS_COUNT][80];
	char download[SERVERS_COUNT][80];
	char mail[SERVERS_COUNT][80];
	string nb;
	
	for(i = 0; i < SERVERS_COUNT; i++) {
		net[i][0] = '\0';
		cpu[i][0] = '\0';
		data_repl_ser[i][0] = '\0';
		upload[i][0] = '\0';
		download[i][0] = '\0';
		mail[i][0] = '\0';
		nb = convertInt(i + 1);
		strcpy(net[i], "Server ");
		strcpy(cpu[i], "Server ");
		strcpy(data_repl_ser[i], "Server ");
		strcpy(upload[i], "Server ");
		strcpy(download[i], "Server ");
		strcpy(mail[i], "Server ");
		strcat(net[i], nb.c_str());
		strcat(cpu[i], nb.c_str());
		strcat(data_repl_ser[i], nb.c_str());
		strcat(upload[i], nb.c_str());
		strcat(download[i], nb.c_str());
		strcat(mail[i], nb.c_str());
		strcat(net[i], " netlink load rate");
		strcat(cpu[i], " CPUs load rate");
		strcat(data_repl_ser[i], " data replication cycle time");
		strcat(upload[i], " fileshare upload time");
		strcat(download[i], " fileshare download time");
		strcat(mail[i], " mail service time");
		
		serversNetlinksLoadPercentage[i].SetName((const char *) net[i]);
		serversCPUsLoadPercentage[i].SetName((const char * ) cpu[i]);
		serverDataReplicationCycleTime[i].SetName((const char *) data_repl_ser[i]);
		fileshareUploadTime[i].SetName((const char *) upload[i]);
		fileshareDownloadTime[i].SetName((const char *) download[i]);
		emailServiceTime[i].SetName((const char *) mail[i]);
		serverDataReplicationCycleTime[i].Init(0, 0.25 second, 10);
		fileshareUploadTime[i].Init(0.025 second, 0.2 second, 15);
		fileshareDownloadTime[i].Init(0.025 second, 0.2 second, 15);
		emailServiceTime[i].Init(0.05 second, 0.1 second, 15);
	}

	// set shared queues for server CPUs and disks
	for(i = 0; i < SERVERS_COUNT; i++) {
		init_cpus = 0;
		init_hdds = 0;
		end_cpus = 0;
		end_hdds = 0;

		if (i == 0) {
			end_cpus = SERVERS_NCPU[i];
			end_hdds = SERVERS_NDISK[i];
		}
		else {
			for(j = 0; j <= i; j++) {
				if (j != i) {
					init_cpus += SERVERS_NCPU[j];
					init_hdds += SERVERS_NDISK[j];
				}
				end_cpus += SERVERS_NCPU[j];
				end_hdds += SERVERS_NDISK[j];
			}
		}

		for(j = init_cpus; j < end_cpus; j++) {
			servers_cpus[j].SetQueue(servers_cpus_queues[i]);
		}

		for(j = init_hdds; j < end_hdds; j++) {
			servers_hdds[j].SetQueue(servers_hdds_queues[i]);
		}

		servers_memory[i].SetQueue(servers_mems_queues[i]);
	}

	(new RequestGeneratorUs)->Activate();
	(new highTrafficGenerator)->Activate();
	(new DosGenerator)->Activate();
	(new adminTasksGenerator)->Activate();
	(new ServersTakedown())->Activate();
	(new dataReplication(2))->Activate();

	Run();

	if (!normal_state_changed)
		clusterNormalStateTime(Time - normal_state_T);

	cout << "Number of received requests: " << cluster_req_count << endl;
	cout << "Number of received net layer request: " << net_layer_req_count << endl;
	cout << "Number of received application layer requests: " << app_layer_req_count << endl;
	cout << "Number of received fileshare requests: " << file_share_download_actions + file_share_upload_actions << endl;
	cout << "Number of received fileshare download requests: " << file_share_download_actions << endl;
	cout << "Number of received fileshare upload requests: " << file_share_upload_actions << endl;
	cout << "Fileshare download ratio: " << (double)file_share_download_actions/(file_share_download_actions + file_share_upload_actions) << endl;
	cout << "Fileshare upload ratio: " << (double)file_share_upload_actions/(file_share_download_actions + file_share_upload_actions) << endl;
	cout << "Number of received email requests: " << email_actions << endl;
	//cout << "Number of received video streaming requests: " << video_streaming_actions << endl;
	cout << "Filesharing usage ratio: " << ((file_share_upload_actions + file_share_download_actions) / app_layer_req_count) * 100 << " %" << endl;
	cout << "Email service usage ratio: " << (email_actions / app_layer_req_count) * 100 << " %" << endl;
	//cout << "Video streaming usage ratio: " << (video_streaming_actions / app_layer_req_count) * 100 << " %" << endl;
	cout << "Ignored net layer requests: " << net_ignored_req_count << endl;
	cout << "Ignored net layer requests ratio: " << ((double)(net_ignored_req_count/net_layer_req_count) * 100) << " %" << endl;
	cout << "Ignored application layer requests(bad requests): " << app_ignored_req_count << endl;
	cout << "Ingored application layer requests ratio: " << ((double)(app_ignored_req_count/app_layer_req_count)*100) << " %" << endl;

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

	for (i = 0; i < SERVERS_COUNT; i++) {
		fileshareDownloadTime[i].Output();
	}

	for (i = 0; i < SERVERS_COUNT; i++) {
		fileshareUploadTime[i].Output();
	}

	for(i = 0; i < SERVERS_COUNT; i++) {
		emailServiceTime[i].Output();
	}

	responseTime.Output();

	clusterDataReplicationCycleTime.Output();
	for (i = 0; i < SERVERS_COUNT; i++) {
		serverDataReplicationCycleTime[i].Output();
	}
/*
	for (i = 0; i < SERVERS_COUNT; i++) {
		cout << "cpu " << i << endl;
		servers_cpus_queues[i].Output();
	}

	for(i = 0; i < SERVERS_COUNT; i++) {
		cout << "hdd " << i << endl;
		servers_hdds_queues[i].Output();
	}

	SIMLIB_statistics.Output();
*/
}