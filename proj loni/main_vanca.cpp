/*
 * Projekt:	Model SHO s prvky plánování a řízení
 * Autori:	Peter Cibula, xcibul08@stud.fit.vutbr.cz
 *		    Václav Neřád, xnerad00@stud.fit.vutbr.cz
 * Podnik:	Supermarket Kaufland
 * Datum:	17.12.2010
 * Predmet:	IMS, FIT VUT Brno
 */

#include "simlib.h"
#include <iostream>


using namespace std;


/////////////////////////// definice casovych udaju ///////////////////////////

// oteviraci doba je 7:00 az 22:00 takze jeden pracovni den ma 15 hodin

#define CAS_TYDEN 3600*15*7 // delka testovani TYDEN
#define CAS_5DNI 3600*15*5 // delka vsednich dnu
#define CAS_FREQ_BEGIN 8 // zacatek spicky 15 hod - 7 zacatek smeny == 8 hodin
#define CAS_FREQ_END 12 // konec spicky 19 hodin - 7 zacatek smeny == 12 hodin

#define CAS_TESTOVANI_BEGIN 0
#define CAS_TESTOVANI_END 3600*15*7 // celkova doba testovani





#define TIMEOUT_NEDELANI 300
#define TIMEOUT_ODCHOD_Z_POKLADNY 300

#define CAS_PRICHODU_ZAKAZNIKU_NORM 20
#define CAS_PRICHODU_ZAKAZNIKU_FREQ 20
#define CAS_PRICHODU_ZAKAZNIKU_WKND 20
#define CAS_OBSLUHA_NA_KASE Uniform(60,240)
//DODELAT DO PETRIHO SITE
#define CAS_PLATBY_PLATEBNI_KARTOU Exponential(60)  // nebo uniform (2,6)
#define CAS_NAKUPU Uniform(300, 1500)
#define CAS_UKLID 600
#define CAS_DOPLNENI_ZBOZI 900
#define CAS_PREVEZENI_KOSIKU Uniform(150, 360)
#define CAS_PRESTAVKA 1800

//pozice pokladen jsou cisla od 1 do 99 a urcuji cislo pokladny
#define POZICE_UKLID 100
#define POZICE_ZBOZI 101
#define POZICE_VOZIKY 102
#define POZICE_OBED 103
#define POZICE_END_OBED 104     //obeduje
#define POZICE_POKL_OBED 105    //je na pokladne, ale uz ma zaskok a chysta se na obed
#define POZICE_OBED_Z_UKLIDU 106    //byl nasilne poslan na prestavku z uklidu
#define POZICE_OBED_ZE_ZBOZI 107    //byl nasilne poslan na prestavku z dokladani zbozi
#define POZICE_TOTAL_END_OBED 108     //vraci se z obeda
#define POZICE_NBUSY 0          //not busy




#define POCET_PRESTAVKUJICICH 4 //maximalni pocet lidi na prestavce
////////////////////////////// definice konstant //////////////////////////////
// pocet otevrenych kas
// const int POCET_DEFAULTNE_OTEVRENYCH_KAS_NORM = 3;
// const int POCET_DEFAULTNE_OTEVRENYCH_KAS_FREQ = 4;
// const int POCET_DEFAULTNE_OTEVRENYCH_KAS_WKND = 6;

const int POCET_NAKUPNICH_VOZIKU = 300;

const int POCET_KAS_NA_PRODEJNE = 8;
const int POCET_DEFALT_KAS = 3;
const int POCET_PRODAVACEK_NA_PRODEJNE = 15;
const int KAPACITA_RADY = 5;
const int KAPACITA_ODLOZISTE_KOSIKU = 40;

int OtevrenePokladny = 0;
int VelikostRady = 0;
int Pouzite_voziky = 0;
int Prac_pozice[POCET_PRODAVACEK_NA_PRODEJNE] = {0};
int Prestavka[POCET_PRODAVACEK_NA_PRODEJNE] = {0};

/////////////////////////////// definice skladu ///////////////////////////////
Store Sklad_voziku("Sklad pripravenych voziku", POCET_NAKUPNICH_VOZIKU);
Store Kasa("Kasa",POCET_KAS_NA_PRODEJNE);



////////////////////////////// definice zarizeni //////////////////////////////
Facility Prodavacka[POCET_PRODAVACEK_NA_PRODEJNE];

Facility Pokladna[POCET_KAS_NA_PRODEJNE];
bool PokladnaSpustena[POCET_KAS_NA_PRODEJNE] = {false};

////////////////////////// definice fronty zakazniku //////////////////////////
Queue Fronta("Celkova fronta na pokladnach");


///////////////////////////// definice histogramu /////////////////////////////

Histogram Hist_Delka_fronty("Delka fronty", 1, 10);
Histogram Doba_obsluhy("Prumerna doba obsluhy zakaznika",1,1);
Histogram Doba_uklidu("Prumerna doba uklidu",1,10);
Histogram Doba_doplnovani("Prumerna doba doplnovani zbozi",1,10);
Histogram Doba_nedelani("Prumerna doba nic nedelani",1,10);

Stat Stat_Prichod_zakazniku("Prichod Zakazniku (sec)");
Stat Stat_Presun_kosiku("Doba presunu kosiku (sec)");
Stat Stat_obsluha_pokladny("Doba obsluhy pokladny (sec)");
Stat Stat_prestavka("Doba pracovniku na prestavce");
Stat Stat_zbozi("Doba doplnovani zbozi (sec)");
Stat Stat_uklid("Doba pracovaniku na uklidu (sec)");
Stat Stat_doba_platby("Doba platby na kase");
double stolen_time[POCET_PRODAVACEK_NA_PRODEJNE] = {0};
double doba_otevreni_pokladny[POCET_PRODAVACEK_NA_PRODEJNE] = {0};
double open_time[POCET_PRODAVACEK_NA_PRODEJNE] = {0};

int cnt=0;
int counter=0;



//////////////////////////////// pomocne funkce ///////////////////////////////
// vraci aktualni hodinu
int hodina(int cas)
{
    return (cas / 3600) % 15;
}

// hleda prodavacku ktera zrovna nepracuje
int VolnaProdavacka()
{
    int volna = -1;
    for (int i =0; i<POCET_PRODAVACEK_NA_PRODEJNE; i++){
        if((!Prodavacka[i].Busy()) && (Prac_pozice[i] == POZICE_NBUSY)){
            volna = i;
            break;
        }
        else{
        }
    }
    return volna;
}

// funkce najde prodavacku s neprioritni praci (vola se pokud nebyla nalezena volna prodavacka)
int NeprioritniProdavacka()
{
    int index = -1;
    for (int i =0; i<POCET_PRODAVACEK_NA_PRODEJNE; i++){
        if((Prac_pozice[i] == POZICE_UKLID) || (Prac_pozice[i] == POZICE_ZBOZI)){
            cout << Time << ": VYMENA NEP.PROD. zabiram uklid nebo zbozi" << endl;
            index = i;
            break;
        }
    }
    return index;
}

// funkce na zjistenie, jestli nektery pracovnik ma "pozici" OBED nebo TOTAL_END_OBED
int Obedy()
{
    int nasel = 0;
    for(int i =0; i<POCET_PRODAVACEK_NA_PRODEJNE; i++)
    {
        if((Prac_pozice[i] == POZICE_OBED) || (Prac_pozice[i] == POZICE_TOTAL_END_OBED))
        {
            nasel = 1;
            break;
        }
    }
    return nasel;
}

// funkce na zjisteni, jestli nektery pracovnik, ktery chce jit na prestavku je volny nebo na uklide/zbozi
int CakanieNaKosiky()
{
    int nasel = 0;
    for(int i =0; i<POCET_PRODAVACEK_NA_PRODEJNE; i++)
    {
        if((Prestavka[i] == 0) && ((Prac_pozice[i] == POZICE_NBUSY) || (Prac_pozice[i] == POZICE_UKLID) || (Prac_pozice[i] == POZICE_ZBOZI)))
        {
            nasel = 1;
            break;
        }
    }
    return nasel;
}

// funkce na zjisteni, jestli je pokladna spustena
int JeSpustena(int pok)
{
    //cout << "jevolna" << endl;
    int nasel = 0;
    for(int i =0; i<POCET_PRODAVACEK_NA_PRODEJNE; i++)
    {
        if((Prac_pozice[i] == pok+1) && Prodavacka[i].Busy())
        {
            nasel = 1;
            break;
        }
    }
    //cout << "jeKONECvolna: " << nasel << endl;
    return nasel;
}

///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// PROCESY ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Proces
class Vymena : public Process {
public:
    Vymena(int pri) : Process(pri) {} ;
    void Behavior() {
        double start_time;


        int i;
        int index = -1;
        int pokl = -1;
        int pozice = -1;
        cout << Time << ": " << "Vymena: ";
        for(i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            cout << Prac_pozice[i] << "   ";
        }
        for(i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            //cout << "!!!!!!!"<< Time << ": " << "fori:" << i << endl;
            if(Prac_pozice[i] == POZICE_TOTAL_END_OBED)
            {
                cout << endl << i+1 << ".";
                index = i;
                Prac_pozice[index] = POZICE_NBUSY;
                break;
            }
        }
        if(index == -1)
            cout << endl << endl << "VYMENA CHYBA!" << endl << endl;
        cout << " za ";
        for(i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            //cout << Time << ": " << "fori:" << i << endl;
            if((Prac_pozice[i] <= POCET_KAS_NA_PRODEJNE) && (Prac_pozice[i] > 0) && (Prestavka[i] == 0))
            {
                cout << i+1 << ". prodavacku" << endl;
                pokl = i;
                Prestavka[pokl] = 1;
                pozice = Prac_pozice[pokl];
                Prac_pozice[pokl] = POZICE_POKL_OBED;
                break;
            }
        }
        if (pokl != -1)
        {
            Prac_pozice[index] = pozice;
            cout << "cekam na odbizeni " << pokl+1 << ". prodavacky" << endl;
            WaitUntil(!Prodavacka[pokl].Busy());        //cekam az prodavacka z pokladny vypadne
            cout << "dockal sem sa odbizeni " << pokl+1 << ". prodavacky" << endl;
            Prac_pozice[pokl] = POZICE_OBED;
            // POSLAT JU NA OBED!!!!!!!!!!!!!
            start_time = Time;
            Seize(Prodavacka[index]);                   //ked odtial vypadne za akychkolvek pricin, nahradim ju
            PokladnaSpustena[pozice-1] = true;
            open_time[pozice-1]=Time;

            cout << Time << ": " << "VymenaKonec" << index+1 << ". za " << pokl+1 << ".prodavacku" << endl;
            plno:
            Wait(Exponential(300));
            WaitUntil(((VelikostRady < OtevrenePokladny/2) && (OtevrenePokladny > 3)) || (Prac_pozice[index] == POZICE_POKL_OBED)); // pocet lidi je mensi nez pocet otevrenych pokladen - uzaviram jednu pokladnu
            /** pRIDAT TIMEOUT*/
            if(!Pokladna[pozice].Busy()){
                doba_otevreni_pokladny[pozice-1] += Time - open_time[pozice-1];
                if(Prac_pozice[index] != POZICE_POKL_OBED){
                    PokladnaSpustena[pozice-1]=false;

                }
                ///PokladnaSpustena[pozice-1]=false;
                Release(Prodavacka[index]);///prehodit na konec ifu radsej - zaistenie vsetkych flagov
                Stat_obsluha_pokladny(Time - start_time);
                if(Prac_pozice[index] != POZICE_POKL_OBED)
                {
                    Prac_pozice[index] = POZICE_NBUSY;
                    OtevrenePokladny--;
                }
            }
            else if(Prac_pozice[index] == POZICE_POKL_OBED)
            {
                WaitUntil(!Pokladna[pozice].Busy());
                Release(Prodavacka[index]);
                Stat_obsluha_pokladny(Time - start_time);
                ///PokladnaSpustena[index]=false;
                doba_otevreni_pokladny[pozice-1] += Time - open_time[pozice-1];
            }
            else
            {
                Wait(1);
                goto plno;
            }
            cout << "Pokladna " << pozice << " zatvorena" << endl;
            ///Activate(Time+Exponential(300));  // za 5 minut exponencialne dojde k vymene pracovnic.....MINUTA (max 2)
        }
        else
        {
            Prac_pozice[index] = POZICE_NBUSY;
            cout << "nikoho" << endl;
            ///(new Pauza)->Activate();
        }
   }
};

// Proces
class Obed : public Process {
public:
    Obed(int pri) : Process(pri) {} ;
    void Behavior() {
        double start_time;

        int i;
        int index = -1;
        cout << Time << ": " << "Obed: ";
        for(i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            //cout << Time << ": " << "fori:" << i << endl;
            if(Prac_pozice[i] == POZICE_OBED)
            {
                cout << i << endl;
                cout << Time << ": OBED: ";
                for(int j=0;j<POCET_PRODAVACEK_NA_PRODEJNE;j++)
                {
                    cout << Prac_pozice[j] << "   ";
                }
                cout << endl;
                index = i;

                Prac_pozice[index] = POZICE_END_OBED;   ///z uklidu ma NBUSY!!!
                Seize(Prodavacka[index]);               ///POZOR! na branie tych co su na kase
                start_time = Time;
                Wait(CAS_PRESTAVKA);
                Stat_prestavka(Time - start_time);
                Release(Prodavacka[index]);

                Prac_pozice[index] = POZICE_TOTAL_END_OBED;
                cout << Time << ": Konec obeda " << index+1 << ". prodavacky" << endl;
                (new Vymena(4))->Activate();
                break;
            }
        }
    }
};

// Proces
class Pauza : public Process {
public:
	Pauza(int pri) : Process(pri) {} ;
    void Behavior(){
        ///Wait(3*3600);
        cout << Time << ": " << "Pauza - bude puding: " << endl;
        int i,j;
        int pocet = 0;
        int chcuZrat = 0;
        for(i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            cout << Prac_pozice[i] << "   ";
        }
        cout << endl;
        for(i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            if(Prac_pozice[i] == POZICE_OBED)
            {
                cout << "na obed ceka: " << i;
                if(!Prodavacka[i].Busy())       //vzdy by sa to malo dostat sem, lebo sa ceka na od"busy"eni ve fci "Vymena"
                {
                    cout << " not busy" << endl;
                    (new Obed(10))->Activate();
                }
                else
                {
                    cout << " busy" << endl;
                    WaitUntil(!Prodavacka[i].Busy());
                    cout << "end of busy" << endl;
                    (new Obed(10))->Activate();
                }
                pocet++;
            }
            else if(Prac_pozice[i] == POZICE_END_OBED)
            {
                cout << "je na obede: " << i << endl;
                pocet++;
            }
            else if(Prac_pozice[i] == POZICE_TOTAL_END_OBED)
            {
                cout << "doobedoval, ide na vymenu: " << i << endl;
                pocet++;
            }
            else if(Prac_pozice[i] == POZICE_POKL_OBED)
            {
                cout << "pokladna sa chysta na obed: " << i << endl;
                pocet++;
            }
        }
        cout << "zere(u)/ide(u) zrat: " << pocet << " pracovnikov"<< endl;
        for(i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            if(Prestavka[i] == 0)
            {
                cout << "dalsi chce zrat" << endl;
                chcuZrat = 1;
                break;
            }///otestovat aby sa nazral aspon nekdo!!!!!
        }
        if(chcuZrat)
        {
            Start:
            for(j=0;j<4;j++)
            {
                //cout << Time << ": " << "forj:" << j << endl;
                if(pocet == POCET_PRESTAVKUJICICH)
                    break;
                for(i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
                {
                    //cout << Time << ": " << "fori:" << i << " " << pocet << endl;
                    if(pocet == POCET_PRESTAVKUJICICH)
                        break;
                    if(Prestavka[i] == 0)
                    {
                        switch(j) {
                            case 0:
                                //cout << Time << ": caseNBUSY" << endl;
                                if(Prac_pozice[i] == POZICE_NBUSY)
                                {
                                    cout << Time << ": NBUSY " << i << endl;
                                    pocet++;
                                    Prestavka[i] = 1;
                                    Prac_pozice[i] = POZICE_OBED;
                                    //(new Obed(i))->Activate();
                                    (new Obed(10))->Activate();
                                }
                                break;
                            case 1:
                                //cout << Time << ": caseUKLID" << endl;
                                if(Prac_pozice[i] == POZICE_UKLID)
                                {
                                    cout << Time << ": UKLID " << i << endl;
                                    pocet++;
                                    Prestavka[i] = 1;
                                    Prac_pozice[i] = POZICE_OBED;///_Z_UKLIDU;
                                    //(new Obed(i))->Activate();
                                    (new Obed(10))->Activate();
                                }
                                break;
                            case 2:
                                //cout << Time << ": caseZBOZI" << endl;
                                if(Prac_pozice[i] == POZICE_ZBOZI)
                                {
                                    cout << Time << ": ZBOZI " << i << endl;
                                    pocet++;
                                    Prestavka[i] = 1;
                                    Prac_pozice[i] = POZICE_OBED;///_ZE_ZBOZI;
                                    //(new Obed(i))->Activate();
                                    (new Obed(10))->Activate();
                                }
                                break;
                            case 3: //pridat kosiky??? dorobit aby ich vratil a siel az potom na obed
                                //cout << Time << ": caseVOZIKY" << endl;
                                if(Prac_pozice[i] == POZICE_VOZIKY)
                                {
                                    //cout << Time << ": VOZIKY " << i << endl;
                                    //Wait(1);
                                    cout << "goto Goto Start:-)" << endl;
                                    //chcuZrat = 0;
                                    WaitUntil((chcuZrat = CakanieNaKosiky()) == 1);
                                    cout << "goto 2 Start:-)" << endl;
                                    goto Start;
                                }
                                break;
                        }
                    }
                }
            }
            ///if(pocet < 2)        //poslem na prestavku aspon 2 zamestnancov

            cout << Time << ": " << "konec pauzy" << endl;
            Wait(1);
            WaitUntil((chcuZrat = Obedy()) == 1);
            cout << "KKKKOOOOOONNNNNEEEEECCCCNNNEEEEEEEEEEE!!!!!!!!!!!!!!" << endl;
            //WaitUntil((chcuZrat = Obedy2()) == 1);
            //Wait(1800);
            (new Pauza(3))->Activate();
        }
        else if(pocet != 0)
        {
            cout << Time << ": " << "konec pauzy" << endl;
            Wait(1);
            WaitUntil((chcuZrat = Obedy()) == 1);
            cout << "KKKKOOOOOONNNNNEEEEECCCCNNNEEEEEEEEEEE!!!!!!!!!!!!!!" << endl;
            //WaitUntil((chcuZrat = Obedy2()) == 1);
            //Wait(1800);
            (new Pauza(3))->Activate();
        }
        else
        {
            for(i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
            {
                Prestavka[i]=0;
            }
        }
    }
};

// Prodavacka preveze 40 kosiku do skladu voziku pro dalsi zakazniky
class PresunVoziku: public Process{
public:
	PresunVoziku(int pri) : Process(pri) {} ;
    void Behavior(){
        double start_time = Time;
        double steal_time = 0;
        bool zabran_prac = false;

        int index;
        int old_pozice=-1;
        cout << Time << ": START VOZIKU: ";
        for(int i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            cout << Prac_pozice[i] << "   ";
        }
        cout << endl;
        index = VolnaProdavacka();
        if (index == -1){
            for (int i =0; i<POCET_PRODAVACEK_NA_PRODEJNE; i++){
                if(Prac_pozice[i] == POZICE_UKLID || Prac_pozice[i] == POZICE_ZBOZI){
                    zabran_prac = true;
                    steal_time = Time;

                    cout << Time << ": VYMENA VOZIKY zabiram uklid nebo zbozi" << endl;
                    index = i;
                    old_pozice = Prac_pozice[i];
                    break;
                }
            }
            if (index == -1){
                cout << Time<<" !!!!!!NENALEZEN NIKDO NA VYMENU NA VOZIKY" << endl;
                WaitUntil(((index = NeprioritniProdavacka() ) > -1 ) || ((index = VolnaProdavacka()) > -1 ));
                old_pozice = Prac_pozice[index];
                zabran_prac = true;
                steal_time = Time;

                cout << Time << " !!!!!!NALEZEN NEKDO NA VYMENU NA VOZIKY AZ PO CEKANI" << endl;

            }

        }
        if(index != -1){

            Prac_pozice[index] = POZICE_VOZIKY;
            Seize(Prodavacka[index],1);
            Wait(CAS_PREVEZENI_KOSIKU);
            if(zabran_prac == true){
                stolen_time[index] += Time - steal_time;
            }
            Leave(Sklad_voziku,40);
            Release(Prodavacka[index]);

            if(old_pozice == -1){
                Prac_pozice[index] = POZICE_NBUSY;
            }
            else{
                Prac_pozice[index] = old_pozice;
            }
            cout << Time << ": KONEC VOZIKU: ";
            for(int i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
            {
                cout << Prac_pozice[i] << "   ";
            }
            cout << endl;

            Stat_Presun_kosiku(Time - start_time);

        }

    }
};

// Proces simulujici Uklid obchodu (neprioritni prace)
class Uklid : public Process {
    double Pocatek;
    void Behavior() {
        double start_time = Time;
        int index;
        index = VolnaProdavacka();
        if (index != -1){

            cout << Time << ": START UKLIDU: ";
            for(int i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
            {
                cout << Prac_pozice[i] << "   ";
            }
            cout << endl;

            Prac_pozice[index] = POZICE_UKLID;
            Seize(Prodavacka[index]);
            Wait(CAS_UKLID);
            Release(Prodavacka[index]);
            cout << Time << ": KONEC UKL " << index+1 << ". " << Prac_pozice[index] << endl;
            if(Prac_pozice[index] != POZICE_END_OBED)
                Prac_pozice[index] = POZICE_NBUSY;

            Stat_uklid((Time - start_time) - stolen_time[index] );
            stolen_time[index] = 0;
            cout << Time << ": KONEC UKLIDU: ";
            for(int i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
            {
                cout << Prac_pozice[i] << "   ";
            }
            cout << endl;
        }
    }
};

// Proces simulujici Doplnovani zbozi v obchode (neprioritni prace)
class DoplneniZbozi : public Process {
    void Behavior() {
        double start_time = Time;
        int index;
        index = VolnaProdavacka();
        if (index != -1){
            cout << Time << ": START  ZBOZI: ";
            for(int i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
            {
                cout << Prac_pozice[i] << "   ";
            }
            cout << endl;

            Prac_pozice[index] = POZICE_ZBOZI;
            Seize(Prodavacka[index]);
            Wait(CAS_DOPLNENI_ZBOZI);
            Release(Prodavacka[index]);
            cout << Time << ": KONEC ZBO " << index+1 << ". " << Prac_pozice[index] << endl;
            if(Prac_pozice[index] != POZICE_END_OBED)
                Prac_pozice[index] = POZICE_NBUSY;

            if((Time - start_time) - stolen_time[index] > 1500 ){
                Print("*****************************************************");
                Print("************* tady a ted %f *******************************",Time);
            }
            Stat_zbozi((Time - start_time) - stolen_time[index]);
            stolen_time[index] = 0;
            cout << Time << ": " << "KONEC  ZBOZI: ";
            for(int i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
            {
                cout << Prac_pozice[i] << "   ";
            }
            cout << endl;
        }

    }
};

// Proces simulujici Otevreni nove pokladny
class SpusteniPokladny: public Process{
public:
	SpusteniPokladny(int Priorita): Process(Priorita) {}
    void Behavior(){
        double start_time = -99999999;
        double steal_time = 0;
        bool zabran_prac = false;
        if(OtevrenePokladny <POCET_KAS_NA_PRODEJNE){

        cout << Time << ": OTEVRENI POKLADNY: ";
        for(int i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            cout << Prac_pozice[i] << "   ";
        }
        cout << endl;

        int index;
        int old_pozice = -1;
        index = VolnaProdavacka();

        if (index == -1){
            cout << Time << ": jdu menit NA POKLADNU" << endl;
            for (int i =0; i<POCET_PRODAVACEK_NA_PRODEJNE; i++){

                if((Prac_pozice[i] == POZICE_UKLID) || (Prac_pozice[i] == POZICE_ZBOZI)){
                    cout << Time << ": "<< OtevrenePokladny << " pokladen otevreno" << endl;
                    cout << Time << ": VYMENA POKLADNA zabiram uklid nebo zbozi s indexem: " << i << endl;
                    cout << Time << ": prodavacka " << i << " je na " << Prac_pozice[i];
                    index = i;
                    old_pozice = Prac_pozice[i];
                    zabran_prac = true;
                    steal_time = Time;
                    //Seize(Prodavacka[index],2);
                    break;
                }
            }
            if (index == -1){ // neni prodavacka ktera nepracuje ani ktera dela neprioritni praci, cekame

                                cout << Time<<" !!!!!!NENALEZEN NIKDO NA POKLADNU"  << endl;

                WaitUntil(((index = NeprioritniProdavacka() ) > -1 ) && ((index = VolnaProdavacka()) > -1 ) );
                zabran_prac = true;
                steal_time = Time;
                //index = NeprioritniProdavacka();
                old_pozice = Prac_pozice[index];
                cout << Time << " !!!!!!NALEZEN NEKDO NA POKLADNU AZ PO CEKANI"  << endl;


            }

        }

        // prirazeni pokladny
        //if (volna != -1){
        int j=0;
        for (j=0; j < POCET_KAS_NA_PRODEJNE; j++){
                if(PokladnaSpustena[j] == false){
                    PokladnaSpustena[j] = true;

                    start_time = Time;
                    Seize(Prodavacka[index],2);
                    open_time[j] = Time;
                    cout << Time << ": oteviram pokladnu, zatim otevreno " << OtevrenePokladny << " pokladen"<< endl;
                    OtevrenePokladny++;
                    break;
                }
        }
        cout << Time << ": prodavacka " << index+1 << " zabrana na " << j+1 << ". pokladnu, je otevreno " << OtevrenePokladny << " pokladen"<< endl;
        Prac_pozice[index] = j+1;
        // }
        plno:
        Wait(Exponential(300));
        WaitUntil(((VelikostRady < OtevrenePokladny) && (OtevrenePokladny > 3)) || (Prac_pozice[index] == POZICE_POKL_OBED)); // pocet lidi je mensi nez pocet otevrenych pokladen - uzaviram jednu pokladnu
        /** pRIDAT TIMEOUT*/
        if(!Pokladna[j].Busy()){

            if(Prac_pozice[index] != POZICE_POKL_OBED){
                PokladnaSpustena[j]=false;

            }
            ///PokladnaSpustena[j]=false;
            if(zabran_prac == true){
                stolen_time[index] += Time - steal_time;
            }

            Release(Prodavacka[index]);///prehodit na konec ifu radsej - zaistenie vsetkych flagov
            doba_otevreni_pokladny[j] +=Time - open_time[j];

            Stat_obsluha_pokladny(Time - start_time);
            if(Prac_pozice[index] != POZICE_POKL_OBED){
                if(old_pozice == -1){
                    Prac_pozice[index] = POZICE_NBUSY;
                }
                else{
                    Prac_pozice[index] = old_pozice;
                }
                OtevrenePokladny--;
            }
            cout << Time << ": zaviram kasu" << j+1 << ", uvolnuji prodavacku " << index << ", pokladen otevreno: " << OtevrenePokladny <<endl;


        }
        else if(Prac_pozice[index] == POZICE_POKL_OBED) // realne se nezavira pouze stridani na obed
        {
            WaitUntil(!Pokladna[j].Busy());
            if(zabran_prac == true){
                stolen_time[index] += Time - steal_time;
            }
            Release(Prodavacka[index]);
            doba_otevreni_pokladny[j] +=Time - open_time[j];
            Stat_obsluha_pokladny(Time - start_time);

            ///PokladnaSpustena[j]=false;
        }
        else{

            goto plno;
        }
        cout << Time << ": Konec pokladny " << j+1 << " s prodavackou " << index << endl;

        }
        else{
            cout << "++++++++++++++++++++++ toto mela osetrit priorita" << endl;

        }
        cout << Time << ": ZAVRENI POKLADNY: ";
        for(int i=0;i<POCET_PRODAVACEK_NA_PRODEJNE;i++)
        {
            cout << Prac_pozice[i] << "   ";
        }
        cout << endl;

    }
};

// Proces simulujici obsluhu zakaznika na kase
class ObsluhaNaKase : public Process {
public:
	ObsluhaNaKase(int pri) : Process(pri) {} ;

    void Behavior() {
        int VolnaPokladna = -1;
        int JeVolnaPokl = 0;
        double start_time;
        zpet:
        for (int a=0; a<POCET_KAS_NA_PRODEJNE; a++){ // hledani volne kasy
            if (!Pokladna[a].Busy() && PokladnaSpustena[a] == true && (JeVolnaPokl = JeSpustena(a))) {
                VolnaPokladna=a;
                break;
            }
        }
        if (VolnaPokladna == -1) { // neni volna kasa
            Into(Fronta);
            Passivate();
            goto zpet;
        }

        Seize(Pokladna[VolnaPokladna]);
        start_time = Time;
        cnt++;
        if(Random() < 0.3){
              Wait(CAS_OBSLUHA_NA_KASE + CAS_PLATBY_PLATEBNI_KARTOU);
        }
        else{
            Wait(CAS_OBSLUHA_NA_KASE);
        }
        //Leave(Sklad_voziku,1);
        Pouzite_voziky++;

        if (Pouzite_voziky>40){
            Pouzite_voziky-=40;
            (new PresunVoziku(1))->Activate();
        }
        Stat_doba_platby(Time - start_time);
        Release(Pokladna[VolnaPokladna]);

        if (Fronta.Length()>0) {
            (Fronta.GetFirst())->Activate();
        }
        // otevreni nove pokladny
        if ((Fronta.Length() > (unsigned) (OtevrenePokladny * KAPACITA_RADY)) && (OtevrenePokladny < POCET_KAS_NA_PRODEJNE) && (OtevrenePokladny < POCET_PRODAVACEK_NA_PRODEJNE-3) ){
            cout << Time<< ": Pokousim se otevrit novou kasu, prave je otevreno " << OtevrenePokladny << " pokladen" << endl;
            (new SpusteniPokladny(2))->Activate();
        }



    }
};

// Proces simulujici prichod a nakup zakaznika
class NakupZakaznika: public Process{
public:
	NakupZakaznika(int pri) : Process(pri) {} ;
    void Behavior(){
        counter++;
        Enter(Sklad_voziku, 1);
        Wait(CAS_NAKUPU);
        (new ObsluhaNaKase(0))->Activate();
    }
};

bool PrvniSpusteni=true;

// Proces simulujici delbu prace
class RozdeleniPrace: public Process{
public:
	void Behavior(){
        if (PrvniSpusteni){
            for(int i =0; i < POCET_DEFALT_KAS; i++){
                (new SpusteniPokladny(1))->Activate();
            }
            PrvniSpusteni=false;
        }

        for (int i=0; i< POCET_PRODAVACEK_NA_PRODEJNE; i++){
            if (!Prodavacka[i].Busy()){
                double nahodne = Random();

                if(nahodne < 0.3 ){
                    (new Uklid)->Activate();
                }
                //else if(nahodne < 0.9){
                 //   (new DoplneniZbozi)->Activate();
                //}
                else{
                    // nekteri nebudou delat nic a budou v zaloze
                     (new DoplneniZbozi)->Activate();
                }

            }


        }

        (new RozdeleniPrace)->Activate(Time + 300);
	}

};



///////////////////////////////////////////////////////////////////////////////
////////////////////////////////// UDALOSTI ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
double minuly_prichod = 0;
// generator prichodu zakazniku
class GenZakazniku : public Event {
	void Behavior() {
		double ac_time = Time; // aktualni cas
		int hr_time = hodina(ac_time);
		double next_activation;

        if(Time != 0){ // zanedbavame prvni pruchod ktery pouze planuje prichod
            Stat_Prichod_zakazniku(Time - minuly_prichod);
            minuly_prichod=ac_time;
        }

		if (ac_time > CAS_5DNI){
            next_activation=Time + Exponential(CAS_PRICHODU_ZAKAZNIKU_WKND); // vikend prichod po 20 sekundach
		}
		else if(hr_time > CAS_FREQ_BEGIN && hr_time < CAS_FREQ_END ){
            next_activation=Time + Exponential(CAS_PRICHODU_ZAKAZNIKU_FREQ); // spicka mimo vikend po 45 sekundach
		}
		else {
            next_activation=Time + Exponential(CAS_PRICHODU_ZAKAZNIKU_NORM); // ostatni cas po 120 sekundach
		}

        Activate(next_activation); // vikend prichod po 20 sekundach
        (new NakupZakaznika(0))->Activate(next_activation); // aktivace nakupu novym zakaznikem
	}
};


// Udalost regulujici prestavky zamestnancu
class Casovac : public Event {
    void Behavior() {
		(new Pauza(3))->Activate(Time + 3*3600);
		(new Casovac)->Activate(Time + 8*3600);/// zmenit na 8*3600
	}
};


int main()
{
    Init(CAS_TESTOVANI_BEGIN,CAS_TESTOVANI_END);
    (new RozdeleniPrace)->Activate();
    (new Casovac)->Activate();
    (new GenZakazniku)->Activate();
    double celkovy_cas = CAS_TESTOVANI_END - CAS_TESTOVANI_BEGIN;

    Run();
    Print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    Print("+----------------------------Statistiky zakazniku----------------------------+\n");
    Print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    Stat_Prichod_zakazniku.Output();
    Print("\n");
    Fronta.Output();
    Print("\n");
    Print("\n");

    Print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    Print("+------------------------Statistiky vyuziti pracovniku-----------------------+\n");
    Print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    Stat_Presun_kosiku.Output();
    Print("+ Celkovy cas = %.1f minut = %.3f %% pracovni doby 1 zamestnance\n",(Stat_Presun_kosiku.Sum()/60),  ((Stat_Presun_kosiku.Sum())/(celkovy_cas*POCET_PRODAVACEK_NA_PRODEJNE))*100 );
    Print("+----------------------------------------------------------+\n");
    Sklad_voziku.Output();
    Print("\n");

    Stat_obsluha_pokladny.Output();
    Print("+ Celkovy cas = %.1f minut = %.3f %% pracovni doby 1 zamestnance\n",(Stat_obsluha_pokladny.Sum()/60),  ((Stat_obsluha_pokladny.Sum())/(celkovy_cas*POCET_PRODAVACEK_NA_PRODEJNE))*100 );
    Print("+----------------------------------------------------------+\n");

    Print("+----------------------------------------------------------+\n");
    Print("| STATISTIC* Doba pracovniku na prestavce                  |\n");
    Print("+----------------------------------------------------------+\n");
    Print("| Konstantni delka prestavky = %d minut                    |\n",(CAS_PRESTAVKA/60));
    Print("| Celkovy pocet prestavek = %d                            |\n",Stat_prestavka.Number());
    Print("+----------------------------------------------------------+\n");
    Print("+ Celkovy cas = %.1f minut = %.3f %% pracovni doby 1 zamestnance\n",(Stat_prestavka.Sum()/60),  ((Stat_prestavka.Sum())/(celkovy_cas*POCET_PRODAVACEK_NA_PRODEJNE))*100 );
    Print("+----------------------------------------------------------+\n");


    Stat_uklid.Output();
    Print("+ Celkovy cas = %.1f minut = %.3f %% pracovni doby 1 zamestnance\n",(Stat_uklid.Sum()/60),  ((Stat_uklid.Sum())/(celkovy_cas*POCET_PRODAVACEK_NA_PRODEJNE))*100 );
    Print("+----------------------------------------------------------+\n");
    Print("\n");

    Stat_zbozi.Output();
    Print("+ Celkovy cas = %.1f minut = %.3f %% pracovni doby 1 zamestnance\n",(Stat_zbozi.Sum()/60),  ((Stat_zbozi.Sum())/(celkovy_cas*POCET_PRODAVACEK_NA_PRODEJNE))*100 );
    Print("+----------------------------------------------------------+\n");
    Print("\n");

    Print("+----------------------------------------------------------+\n");
    Print("| STATISTIC* Nevyuzity cas pracovniku                      |\n");
    Print("+----------------------------------------------------------+\n");

    double nevyuzity_cas = celkovy_cas*POCET_PRODAVACEK_NA_PRODEJNE - Stat_Presun_kosiku.Sum() - Stat_obsluha_pokladny.Sum() - Stat_prestavka.Sum() - Stat_uklid.Sum() - Stat_zbozi.Sum() ;
    Print("+ Celkovy cas = %.1f minut = %.3f %% pracovni doby 1 zamestnance\n",(nevyuzity_cas/60),  ((nevyuzity_cas)/(celkovy_cas*POCET_PRODAVACEK_NA_PRODEJNE))*100 );
    Print("+----------------------------------------------------------+\n");


    Print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    Print("+----------------------------------Pokladny----------------------------------+\n");
    Print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    Stat_doba_platby.Output();
    Print("+ celkem %.3f      +\n", Stat_doba_platby.Sum());

    for(int i =0; i< POCET_KAS_NA_PRODEJNE; i++){
        Pokladna[i].Output();
        if(Prodavacka[i].Busy()){
            Print ("%.3f += ", doba_otevreni_pokladny[i]);
            doba_otevreni_pokladny[i] += Time - open_time[i];
            Print("%.3f - %.3f = %.3f\n",Time, open_time[i], (Time - open_time[i]));

        }
        Print("Prodavacka (%.6f * %.3f = %.3f \n",Pokladna[i].tstat.MeanValue() , celkovy_cas , (Pokladna[i].tstat.MeanValue() * celkovy_cas) );
        Print("Doba oteve / %.3f )* 100 = \n",doba_otevreni_pokladny[i]);


        Print("Vysledek %.1f %% \n ",( ( (Pokladna[i].tstat.MeanValue() * celkovy_cas)/(doba_otevreni_pokladny[i]))*100 ) );


    }
for (int i = 0 ; i< 20; i++)
Print("%.6f\n", Exponential(60));


    //Doba_uklidu.Output();
    //Doba_doplnovani.Output();
    return 0;
}
