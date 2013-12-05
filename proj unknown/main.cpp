/*
 * Projekt:	System hromadne obsluhy: Vyrobni podnik
 * Autori:	Patrik Nemecek, xnemec33@stud.fit.vutbr.cz
 *		Jan Ondrousek, xondro04@stud.fit.vutbr.cz
 * Podnik:	Pekarna, vyroba a distribuce rohliku
 * Datum:	20.12.2010
 * Predmet:	IMS, FIT VUT Brno
 */

#include "simlib.h"

//definice casovych udaju
#define CAS_GENEROVANI_ROHLIKU 0.5
#define CAS_ROHLIK_V_KYNARNE 300
#define CAS_ROHLIK_V_PECI 600
#define CAS_ROHLIK_PADA_DO_BEDNY 0.3
#define CAS_PRESUN_BEDNY_NA_VOZIK Exponential(5)
#define CAS_PRESUN_VOZIKU_DO_EXPEDICE Uniform(20, 30)
#define CAS_VYLOZENI_VOZIKU_Z_AUTA Uniform(30, 90)
#define CAS_NALOZENI_VOZIKU_DO_AUTA Uniform(90, 150)
#define CAS_ROZVOZ Uniform(3600, 9000)
#define CAS 3600*24*7

#define CAS_GENEROVANI_POTREB_PEKARE Uniform(7200, 10800)
#define CAS_PAUZA_PEKARE Uniform(120, 300)
#define CAS_KONTROLA_ROHLIKU 0.1
#define CAS_VYHOZENI_SPATNYCH_ROHLIKU 0.3
#define CAS_OTEVRENI_SYPACIHO_ZARIZENI 1

//definice konstant
const int POCET_ROHLIKU_V_BEDNE = 60;
const int POCET_BEDEN_NA_VOZIK = 10;
const int POCET_VOZIKU_V_AUTE = 10;

const int KAPACITA_KYNARNY = 630;
const int KAPACITA_PECE = 1200;

const int POCET_PRAZDNYCH_BEDEN = 500;
const int POCET_PRAZDNYCH_VOZIKU = 50;

//sklady
Store Kynarna("Kynarna", KAPACITA_KYNARNY);
Store Pec("Pec", KAPACITA_PECE);
Store PrazdneBedny("Prazdne bedny", POCET_PRAZDNYCH_BEDEN);
Store PrazdneVoziky("Prazdne voziky", POCET_PRAZDNYCH_VOZIKU);

//zarizeni
Facility Pekar("Pekar");
Facility Rampa("Rampa");

int HotoveRohliky = 0;
int PlneBedny = 0;
int PlneVozikyVPekarne = 0;
int PlneVozikyVExpedici = 0;

//histogramy
Histogram DobaNaplneniBedny("Doba naplneni bedny", 10, 10);
Histogram DobaBednyPodSypacem("Doba bedny stravene pod sypacem rohliku", 10, 10);
Histogram DobaNaplneniVoziku("Doba naplneni voziku", 180, 110);
Histogram DobaPrepravyVozikuDoExpedice("Doba prepravy voziku do expedice", 180, 120);
Histogram DobaPauzyPekare("Doba pauzy pekare", 120, 18);
Histogram DobaStravenaNaRampe("Doba auta stravena na rampe", 1600, 280);
Histogram DobaRozvozu("Rozvoz trva", 3600, 540);


/*********************************************************************/
/**************************** PROCESY ********************************/
/*********************************************************************/

//proces simulujici vyrobu rohliku
class UpeceniRohliku: public Process{
public:
	UpeceniRohliku(int Priorita): Process(Priorita) {}
	void Behavior(){
		zpet:
		Enter(Kynarna, 1);
		Wait(CAS_ROHLIK_V_KYNARNE);
		Leave(Kynarna, 1);
		if(Random() < 0.95){
			Enter(Pec, 1);
			Wait(CAS_ROHLIK_V_PECI);
			Leave(Pec, 1);
			if(Random() < 0.98){
				Seize(Pekar);
				Wait(CAS_KONTROLA_ROHLIKU);
				Release(Pekar);
				HotoveRohliky++;
			}else{
				Seize(Pekar);
				Wait(CAS_VYHOZENI_SPATNYCH_ROHLIKU);
				Release(Pekar);
			}
		}else
			goto zpet;
	}
};

//proces simulujici naplneni jedne bedny rohlikama
class NaplneniBedny: public Process{
public:
	NaplneniBedny(int Priorita): Process(Priorita) {}
	void Behavior(){
		while(1){
			double start = Time;
			Enter(PrazdneBedny, 1);
			for(int i = 0; i < POCET_ROHLIKU_V_BEDNE; i++){
				WaitUntil(HotoveRohliky > 0);
				HotoveRohliky--;
				Wait(CAS_ROHLIK_PADA_DO_BEDNY);
			}
			PlneBedny++;
			DobaNaplneniBedny(Time - start);
			Seize(Pekar);
			Wait(CAS_OTEVRENI_SYPACIHO_ZARIZENI);
			Release(Pekar);
			DobaBednyPodSypacem(Time - start);
		}
	}
};

//proces simulujici naskladani deseti beden na jeden vozik
class NaplneniVoziku: public Process{
public:
	NaplneniVoziku(int Priorita): Process(Priorita) {}
	void Behavior(){
		while(1){
			double start = Time;
			Enter(PrazdneVoziky, 1);
			for(int i = 0; i < POCET_BEDEN_NA_VOZIK; i++){
				WaitUntil(PlneBedny > 0);
				PlneBedny--;
				Seize(Pekar);
				Wait(CAS_PRESUN_BEDNY_NA_VOZIK);
				Release(Pekar);
			}
			PlneVozikyVPekarne++;
			DobaNaplneniVoziku(Time - start);
		}
	}
};

//proces simulujici odvoz plneho voziku do expedice
class PresunVozikuDoExpedice: public Process{
public:
	PresunVozikuDoExpedice(int Priorita): Process(Priorita) {}
	void Behavior(){
		while(1){
			double start = Time;
			WaitUntil(PlneVozikyVPekarne > 0);
			Seize(Pekar);
			PlneVozikyVPekarne--;
			Wait(CAS_PRESUN_VOZIKU_DO_EXPEDICE);
			Release(Pekar);
			PlneVozikyVExpedici++;
			DobaPrepravyVozikuDoExpedice(Time - start);
		}
	}
};

//proces simulujici nalozeni, vylozeni zbozi do auta a rozvoz zbozi
class Auto: public Process{
private:
	bool PrvniNaklad;
public:
	Auto(): PrvniNaklad(true) {}
	void Behavior(){
		while(1){
			Seize(Rampa);
			double start = Time;
			if(!PrvniNaklad){
				for(int i = 0; i < POCET_VOZIKU_V_AUTE; i++){
					Wait(CAS_VYLOZENI_VOZIKU_Z_AUTA);
					Leave(PrazdneBedny, POCET_BEDEN_NA_VOZIK);
					Leave(PrazdneVoziky, 1);
				}
			}
			for(int i = 0; i < POCET_VOZIKU_V_AUTE; i++){
				WaitUntil(PlneVozikyVExpedici > 0);
				PlneVozikyVExpedici--;
				Wait(CAS_NALOZENI_VOZIKU_DO_AUTA);
			}
			Release(Rampa);
			DobaStravenaNaRampe(Time - start);
			double start1 = Time;
			Wait(CAS_ROZVOZ);
			DobaRozvozu(Time - start1);
			PrvniNaklad = false;
		}
	}
};

//proces simulujici pauzu pekare
class PauzaPekare: public Process{
public:
	void Behavior(){
		double start = Time;
		Seize(Pekar, 4);
		Wait(CAS_PAUZA_PEKARE);
		Release(Pekar);
		DobaPauzyPekare(Time - start);
	}
};


/*********************************************************************/
/*************************** UDALOSTI ********************************/
/*********************************************************************/

//udalost generujici procesy pro upeceni rohliku
class GeneratorRohliku: public Event{
public:
	void Behavior(){
		(new UpeceniRohliku(0))->Activate();
		Activate(Time+CAS_GENEROVANI_ROHLIKU);
	}
};

//udalost generujici pauzy pekare
class GeneratorPotrebPekare: public Event{
public:
	void Behavior(){
		(new PauzaPekare)->Activate();
		Activate(Time+CAS_GENEROVANI_POTREB_PEKARE);
	}
};


/*********************************************************************/
/************************* HLAVNI FUNKCE *****************************/
/*********************************************************************/
int main(){

	Init(0,CAS);

	//inicializace procesu a udalosti
	(new GeneratorPotrebPekare)->Activate();
	(new GeneratorRohliku)->Activate();
	(new NaplneniBedny(1))->Activate();
	(new NaplneniVoziku(2))->Activate();
	(new PresunVozikuDoExpedice(3))->Activate();
	(new Auto)->Activate();
	(new Auto)->Activate();
	(new Auto)->Activate();

	Run();

	//tisk statistik a histogramu
	Kynarna.Output();
	Pec.Output();
	PrazdneBedny.Output();
	PrazdneVoziky.Output();
	DobaNaplneniBedny.Output();
	DobaBednyPodSypacem.Output();
	DobaNaplneniVoziku.Output();
	DobaPrepravyVozikuDoExpedice.Output();
	Pekar.Output();
	DobaPauzyPekare.Output();
	Rampa.Output();
	DobaStravenaNaRampe.Output();
	DobaRozvozu.Output();
}
