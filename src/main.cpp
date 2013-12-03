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

#define req_treatment_T Uniform(0.01, 0.1)

#define sim_T 3600*24*7

const int server_A_req_cap = 10000;
const int server_B_req_cap = 5000;
const int server_C_req_cap = 2500;
const int server_D_req_cap = 3000;

Store server_A("Server A", server_A_req_cap);
Store server_B("Server B", server_B_req_cap);
Store server_C("Server C", server_C_req_cap);
Store server_D("Server D", server_D_req_cap);

/* zarizeni ???*/
//Facility

bool server_overload_flag = false;

double server_A_req_count = 0;
double server_B_req_count = 0;
double server_C_req_count = 0;
double server_D_req_count = 0;

/* histogramy ...*/

class loadBalancing : public Process {
public:
	loadBalancing(int priority):Process(priority) {}
	void Behavior() {

	}
};


class RequestGeneratorUS : public Event {
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


