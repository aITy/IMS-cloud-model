/**
 * Projekt do predmetu IMS - Model cloudoveho centra
 * Vypracoval: 	Jiri Navratil, xnavra36@stud.fit.vutbr.cz
 *				Petr Caha, xcahab00@stud.fit.vutbr.cz
**/

// temporary
#include "../simlib/simlib/src/simlib.h"

//#define servers_count 4

/*#define req_gen_T_Us Uniform(0.01, 0.5)
#define req_gen_T_Eu Uniform(0.1, 1)
#define req_gen_T_Gen Uniform(0.01, 1)*/

#define minute         * (60 * 1.0e3)
#define minutes        minute
#define second        * 1.0e3
#define seconds       second
#define miliseconds    * 1.0

#define A_req_treatment_T Uniform(0.001, 0.01)
#define B_req_treatment_T Uniform(0.005, 0.1)
#define C_req_treatment_T Uniform(0.01, 0.1)
#define D_req_treatment_T Uniform(0.005, 0.1)

// ty statistiky dropboxu jsou za 42 dni
#define sim_T 3600*24*42 seconds

const int server_A_net_link_cap = 10000;
const int server_B_net_link_cap = 5000;
const int server_C_net_link_cap = 2500;
const int server_D_net_link_cap = 3000;

const int server_A_NCPU = 8;
const int server_B_NCPU = 4;
const int server_C_NCPU = 3;
const int server_D_NCPU = 4;

const int server_A_NDISK = 10;
const int server_B_NDISK = 6;
const int server_C_NDISK = 4;
const int server_D_NDISK = 6;

const int server_A_NCHANNEL = 2;
const int server_B_NCHANNEL = 1;
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

Store server_A_net_link("Server A - network link", server_A_net_link_cap);
Store server_B_net_link("Server B - network link", server_B_net_link_cap);
Store server_C_net_link("Server C - network link", server_C_net_link_cap);
Store server_D_net_link("Server D - network link", server_D_net_link_cap);

Store server_A_mem("Server A memory", 8192);
Store server_A_mem("Server B memory", 4096);
Store server_A_mem("Server C memory", 2048);
Store server_A_mem("Server D memory", 2048);

Facility  A_cpu[server_A_NCPU];
Queue     A_queue_cpu("Server A CPU queue");

Facility  B_cpu[server_B_NCPU];
Queue     B_queue_cpu("Server B CPU queue");

Facility  C_cpu[server_C_NCPU];
Queue     C_queue_cpu("Server C CPU queue");

Facility  D_cpu[server_D_NCPU];
Queue     D_queue_cpu("Server D CPU queue");

Facility  A_hdd[server_A_NDISK];
Facility  B_hdd[server_B_NDISK];
Facility  C_hdd[server_C_NDISK];
Facility  D_hdd[server_D_NDISK];

Facility  A_channel[server_A_NCHANNEL];
Queue     A_queue_channel("Server A channel queue");

Facility  B_channel[server_B_NCHANNEL];
Queue     B_queue_channel("Server B channel queue");

Facility  C_channel[server_C_NCHANNEL];
Queue     C_queue_channel("Server C channel queue");

Facility  D_channel[server_D_NCHANNEL];
Queue     D_queue_channel("Server D channel queue");

Facility overloaded("Cluster overload flag");
//jak modelovat cas kdy je cluster vytizen - globalni promena ? to asi ne ... :D
double overloaded_T = 0;
// v petriho siti je to modelovano zarizenim - takze bych pouzil radeji to
bool cluster_overloaded = false;

double server_A_net_link_req_count = 0;
double server_B_net_link_req_count = 0;
double server_C_net_link_req_count = 0;
double server_D_net_link_req_count = 0;

Histogram clusterOverloadTime("Cluster overload time", 0, 0.1, 30);
Histogram treatmentTime("Treatment time", 0, 2 seconds, 30);

/** TODO:
 *	detekce kdy je cluster zas v poradku + zapis do Histogramu Time - overloaded_T
 *	to jeste zalezi,jak to udelat spravne
 *	to, co je vyreseno goto by melo asi byt nejak reseno frontou
 **/

class loadBalancing : public Process {
public:
	loadBalancing(int priority):Process(priority) {}
	void Behavior() {
		// Kde zrusit flag cluster_overloaded ???!!!
		double prob = Random();
		if (prob < 0.25) {
			// A
			A_server:
			if (cluster_overloaded) {
			//if (!overloaded.Busy()) {
				
				Enter(server_A_net_link, 1);
				Wait(A_req_treatment_T);
				Leave(server_A_net_link, 1);
			}
			else if (server_A_net_link.Used()/server_A_net_link.Capacity() < 0.9) {
				Enter(server_A_net_link, 1);
				Wait(A_req_treatment_T);
				Leave(server_A_net_link, 1);
			}
			else goto B_server;
		}
		else if (prob < 0.5) {
			// B
			B_server:
			if (cluster_overloaded) {
			//if (!overloaded.Busy()) {
				Enter(server_B_net_link, 1);
				Wait(B_req_treatment_T);
				Leave(server_B_net_link, 1);
			}
			else if (server_B_net_link.Used()/server_B_net_link.Capacity() < 0.9) {
				Enter(server_B_net_link, 1);
				Wait(B_req_treatment_T);
				Leave(server_B_net_link, 1);
			}
			else goto C_server;

		}
		else if (prob < 0.75) {
			// C
			C_server:
			if (cluster_overloaded) {
			//if (!overloaded.Busy()) {
				Enter(server_C_net_link, 1);
				Wait(C_req_treatment_T);
				Leave(server_C_net_link, 1);
			}
			else if (server_C_net_link.Used()/server_C_net_link.Capacity() < 0.9) {
				Enter(server_C_net_link, 1);
				Wait(C_req_treatment_T);
				Leave(server_C_net_link, 1);
			}
			else goto D_server;

		}
		else {
			// D
			D_server:
			if (cluster_overloaded && server_D_net_link.Full()) {
			//if (!overloaded.Busy() && server_D_net_link.Full()) {
				// cluster je zahlcen - odchod transakce ze systemu
			}
			else if (cluster_overloaded) {
			//else if (!overloaded.Busy()) {
				Enter(server_D_net_link, 1);
				Wait(D_req_treatment_T);
				Leave(server_D_net_link, 1);
			}
			else if (server_D_net_link.Used()/server_D_net_link.Capacity() < 0.9) {
				Enter(server_D_net_link, 1);
				Wait(D_req_treatment_T);
				Leave(server_D_net_link, 1);
			}
			else {
				cluster_overloaded = true;
				// vygenerovani zarizeni nebo jak to spravne udelat ze zarizenim a ne flagem
				
				overloaded_T = Time;
				goto A_server;
			}
		}
	}
};


class RequestGeneratorUs : public Event {
public:
	void Behavior() {
		(new loadBalancing(0))->Activate();
		Activate(Time + Uniform(0.01, 0.5));
	}
};

class RequestGeneratorEu : public Event {
public:
	void Behavior() {
		(new loadBalancing(0))->Activate();
		Activate(Time + Uniform(0.1, 1));
	}
};

class RequestGeneratorGeneral : public Event {
public:
	void Behavior() {
		(new loadBalancing(0))->Activate();
		Activate(Time + Uniform(0.01, 1));
	}
};


int main() {
	Init(0, sim_T);

	// General test
	(new RequestGeneratorGeneral)->Activate();
	// .
	// .
	// .
	Run();

	// histogram output
	clusterOverloadTime.Output();
	treatmentTime.Output();
	
/*
	// EU test
	(new RequestGeneratorEu)->Activate();


	// US test
	(new RequestGeneratorUs)->Activate();

*/

}