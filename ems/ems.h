/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifndef __EMSBUS_H
#define __EMSBUS_H

#define EMS_MAXBUFFERS		4
#define EMS_MAXBUFFERSIZE	128

#pragma pack(1)
typedef struct {
    uint32_t	sntp_timeStamp;
    uint32_t	sys_timeStamp;
    int16_t	    writePtr;
    char	    buffer[EMS_MAXBUFFERSIZE];
} _EMSRxBuf;

// === EMS telegram ===
// RCTimeMessage: src=0x10, type=0x06
typedef struct {
    uint8_t year;	//Systemzeit Jahr
    uint8_t month;	//Systemzeit Monat
    uint8_t hours;	//Systemzeit Stunden
    uint8_t days;	//Systemzeit Tage
    uint8_t minutes;	//Systemzeit Minuten
    uint8_t seconds;	//Systemzeit Sekunden
    uint8_t dayofweek;	//Wochentag (0=Mo … 6=SO)
    uint8_t bitfield;	//bit 0: Sommerzeit
			//bit 1: Funkuhr
			//bit 2: Uhrzeit fehlerhaft
			//bit 3: Datum fehlerhaft
			//bit 4: Uhr läuft
} _EMSRCTimeMessage;

// RCErrorMessages: src=0x10, type=0x10|0x11|0x12
typedef struct {
    char code[2];	// Displaycode
    uint16_t errnum;	// Fehlernummer
    uint8_t year:7;	// Bit 7..1 Jahr + 2000, Bit 8 - Datum/Uhrzeit folgt
    uint8_t dateTime:1;	//
    uint8_t month;	// Monat
    uint8_t hour;	// Stunde
    uint8_t day;	// Tag
    uint8_t min;	// Minute
    uint16_t length;	// Dauer
    uint8_t errsrc;	// Busadresse der Fehlerquelle
}_EMSRCErrorMessages;

// UBABetriebszeit: src=0x10, type=0x14
typedef struct {
    uint8_t optime[3];	// Gesamtbetriebszeit
}_EMSUBABetriebszeit;

// UBAWartungsdaten: src=0x08, type=0x15
typedef struct {
    uint8_t msg;	// Wartungsmeldungen (0=keine, 1=nach Betriebsstunden, 2=nach Datum)
    uint8_t pending;	// Betriebsstunden vor Wartung in 100h
    uint8_t day;	// Wartungsdatum Tag
    uint8_t month;	// Wartungsdatum Monat
    uint8_t year;	// Wartungsdatum Jahr
}_EMSUBAWartungsdaten;

// MC10Parameter: src=0x08, type=0x16
typedef struct {
    // Heizung am Kessel aktiviert 0=nein, 255=ja
    // Heizungs-Temperatureinstellung am Kessel
    // Kesselleistung max
    // Kesselleistung min
    // Abschalthysterese (relativ zu Vorlauf-Soll, positiver Wert, z.B. 0x06)
    // Einschalthysterese (relativ zu Vorlauf-Soll, negativer Wert, z.B. 0xfa)
    // Antipendelzeit
    // Kesselpumpennachlauf
    // Kesselkreispumpenmodulation max. Leistung
    // Kesselkreispumpenmodulation min. Leistung
}_EMSMC10Parameter;


// UBAMonitorFast: src=0x08, type=0x18
typedef struct {
    uint8_t vtsoll;	// Vorlauf Solltemperatur
    uint16_t vtist;	// Vorlauf Isttemperatur
    uint8_t kmax;	// Kessel maximale Leistung
    uint8_t kist;	// Kessel aktuelle Leistung
    uint8_t res[2];
    uint8_t gas:1;	// Gasarmatur EIN
    uint8_t res2:1;
    uint8_t fan:1;	// Gebläse EIN
    uint8_t ign:1;	// Zündung EIN
    uint8_t pump:1;	// Kesselkreispumpe EIN
    uint8_t valve:1;	// 3-Wege-Ventil auf WW
    uint8_t zirkulation:1;	// Zirkulation EIN
    uint8_t res3;
    uint16_t temp;	// Temperatur (DL-Erhitzer?) (fehlt=0x8000)
    uint16_t watertemp;	// Wassertemperatur (fehlt=0x8000)
    uint16_t rltemp;	// Rücklauf Temperatur (fehlt=0x8000)
    uint16_t current;	// Flammenstrom
    uint8_t pressure;	// Systemdruck (fehlt=0xff)
    uint8_t sc1;	// Service-Code 1. Zeichen
    uint8_t sc2;	// Service-Code 2.Zeichen
    uint16_t errcode;	// Fehlercode (Hi, Lo)
    uint16_t airtemp;	// Ansauglufttemperatur
}_EMSUBAMonitorFast;

// UBAMonitorSlow src: 0x08, type: 0x19
typedef struct {
    uint16_t outdoortemp;	// Außentemperatur
    uint16_t ktemp;	// Kessel-Ist-Temperatur (wenn Fühler fehlt, 0x8000)
    uint16_t abgastemp;	// Abgastemperatur (wenn Fühler fehlt, 0x8000)
    uint8_t modulation;	// Pumpenmodulation
    uint8_t starts[3];	// Brennerstarts
    uint8_t betriebszeit[3];	// Betriebszeit komplett
    uint8_t heizzeit[3];	// Betriebszeit heizen
    uint8_t some_time[3];	// noch eine Zeit
}_EMSUBAMonitorSlow;

// UBASollwerte src: 0x10/0x17, type: 0x1a
typedef struct {
    uint8_t ksoll;	// Kessel-Solltemperatur
    uint8_t leistunghk;	// 0 oder 100 Leistungsanforderung HK (?)
    uint8_t leistungww;	// 0 oder 100 Leistungsanforderung WW (?)
    uint8_t zero;	// immer 0
}_EMSUBASollwerte;

// UBAWartungsmeldung: 0x08, type: 0x1c
typedef struct {
    uint8_t service_req;	// Wartung fällig (0=nein, 3=ja, wegen Betriebsstunden, 8=ja, wegen Datum)
}_EMSUBAWartungsmeldung;

// WM10Status: 0x11, type=0x1e
typedef struct {
    uint8_t temp;	// Temperatur
}_EMSWM10Status;

// UBAParameterWW: 0x08, type=0x33
typedef struct {
    uint8_t bitfield;	// bit3: WW System Vorhanden
    uint8_t wwactive;	// WW am Kessel aktiviert 0=nein, 255=ja
    uint8_t wwtemp;	// WW-Solltemperatur (wenn Kessel nicht auf AUT, ist der dort eingestellte Wert fix. Wenn Kessel auf AUT, ist dieser Wert schreibbar und am RC3x änderbar.)
    uint8_t wwpump;	// Zirkulationspumpe vorhanden: 0-nein, 255-ja
    uint8_t wwswitch;	// Anz. Schaltpunkte Zirk-Pumpe 1..6 = 1x3min..6x3min, 7 ständig an
    uint8_t wwdisinfect;	// Solltemperatur termische Desinfektion
    uint8_t wwtype;	// Art des WW-Systems: 0-Ladepumpe, 255 3-W Ventil
}_EMSUBAParameterWW;

// UBAMonitorWWMessage: 0x08, type=0x34
typedef struct {
    uint8_t wwsoll;	// Warmwasser Temperatur Soll
    uint16_t wwist;	// Warmwasser Temperatur Ist
    uint16_t wwist2;	//Warmwasser Temperatur Ist 2. Fühler
    uint8_t b1_0:1;	// Tagbetrieb
    uint8_t b1_1:1;	// Einmalladung
    uint8_t b1_2:1;	// Thermische Desinfektion
    uint8_t b1_3:1;	// Warmwasserbereitung
    uint8_t b1_4:1;	// Warmwassernachladung
    uint8_t b1_5:1;	// Warmwasser-Temperatur OK
    uint8_t b1_res:2;	//
    uint8_t b2_0:1;	// Fühler 1 defekt
    uint8_t b2_1:1;	// Fühler 2 defekt
    uint8_t b2_2:1;	// Störung WW
    uint8_t b2_3:1;	// Störung Desinfektion
    uint8_t b2_res:4;
    uint8_t b3_0:1;	// Zirkulation Tagbetrieb
    uint8_t b3_1:1;	// Zirkulation Manuell gestartet
    uint8_t b3_2:1;	// Zirkulation läuft
    uint8_t b3_3:1;	// Ladevorgang WW läuft
    uint8_t b3_res:4;	//
    uint8_t wwtype;	// Art des Warmwassersystems s.u.
    uint8_t wwthrough;	// WW Durchfluss
    uint8_t wwtime[3];	// Warmwasserbereitungszeit
    uint8_t wwcount[3];	// Warmwasserbereitungen
}_EMSUBAMonitorWWMessage;

// Flags: 0xXX, type=0x35
typedef struct {
    uint8_t startStop;	// schreiben von dez.35 = Einmalladung starten, 3=Einmalladung abbrechen
}_EMSFlags;

// WWBetriebsart: 0x10, type=0x37
typedef struct {
    uint8_t wwprog;	// Programm Warmwasser 0=wie Heizkreise, 255=eigenes Programm
    uint8_t circprog;	// Programm Zirkulation 0=wie Warmwasser, 255=eigenes Programm
    uint8_t wwmode;	// Betriebsart WW 0-ständig aus, 1-ständig an, 2-Auto
    uint8_t circmode;	// Betriebsart Zirkulationspumpe 0-ständig aus, 1-ständig an, 2-Auto
    uint8_t disinfect;	// Thermische Desinfektion 0-Aus, 255-Ein
    uint8_t disinfect_d;	// Thermische Desinfektion Wochentag 0..6 = Mo..So, 7=täglich
    uint8_t disinfect_h;	// Thermische Desinfektion Stunde
    uint8_t maxtemp;	// Maximale Warmwassertemperatur
    uint8_t oneshoot;	// Einmalladungstaste 0-Aus, 255-Ein
}_EMSWWBetriebsart;

// HK1Betriebsart: 0x10, type=0x3D (0x47, 0x51, 0x5b)
typedef struct {
    uint8_t hktype;	// Heizart: 1 Heizkörper, 2 Konvektor, 3 Fußboden, 4 Raum Vorlauf
    uint8_t tnight;	// Raumtemperatur Nacht
    uint8_t tday;	// Raumtemperatur Tag
    uint8_t tholiday;	// Raumtemperatur Ferien
    uint8_t maxinfluence;	// Max. Raumtemperatureinfluss
    uint8_t toffset;	// Raumtemperaturoffset
    uint8_t mode;	// Betriebsart Heizkreis 0-Nacht, 1-Tag, 2-Auto
    uint8_t estrich;	// Estrichtrocknung 0-Aus, 255-Ein
    uint8_t vtmax;	// Maximale Vorlauftemperatur
    uint8_t vtmin;	// Minimale Vorlauftemperatur
    uint8_t tauslegung;	// Auslegungstemperatur
    uint8_t optimize;	// Optimierung Schaltprogramm 0-Aus, 255-Ein
    uint8_t level_summer;	// Schwelle Sommer-/Winterbetrieb
    uint8_t tantifreeze;	// Frostschutztemperatur
    uint8_t mode2;	// Betriebsart 0-Abschaltbetrieb, 1-Reduzierter Betrieb, 2-Raumhaltebetrieb, 3-Aussenhaltebetrieb
    uint8_t remotetype;	// Fernbedienungstyp 0-kein, 1-RC20, 2-RC3x
    uint8_t freezeprot;	// Frostschutz 0-kein, 1-Aussentemperatur, 2-Raumtemperatur 5°C
    uint8_t system;	// Heizsystem 1-Heizkörper, 2-Konvektor, 3-Fußboden [nur bei RC35]
    uint8_t fg;	// Führungsgröße 0-Aussentemperaturgeführt, 1-Raumtemperaturgeführt [nur bei RC35]
    uint8_t what;	// 0-aus
    uint8_t vtmax_35;	// maximale Vorlauftemperatur [wie Offset 20, nur bei RC35 vorhanden]
    uint8_t tauslegung_35;	// Auslegungstemperatur (Vorlauftemperatur bei minimaler Aussentemperatur (z.B. bei -10°C)) [wie Offset 22, nur bei RC35 vorhanden]
    uint8_t ttemp_35;	// Temporäre Raumtemperatur (bis zum nächsten Schaltpunkt, 0=abbrechen) [nur bei RC35]
    uint8_t abs_35;	// Absenkung unterbrochen unter [nur bei RC35]
    uint8_t treduced_norm_35;	// Temperaturschwelle Reduziert/Abschaltbetrieb bei Aussenhalt Normalbetrieb [nur bei RC35]
    uint8_t treduced_holiday_35;	// Temperaturschwelle Reduziert/Abschaltbetrieb bei Aussenhalt Urlaubsbetrieb [nur bei RC35]
    uint8_t mode_35;	// Absenkung Urlaub 2-Raumhaltebetrieb, 3-Aussenhaltebetrieb [nur bei RC35]
}_EMSHK1Betriebsart;

// HK1MonitorMessage: 0x10, type=0x3E (0x48, 0x52, 0x5C)
typedef struct {
    uint8_t optaus:1;	// Ausschaltoptimierung
    uint8_t optein:1;	// Einschaltoptimierung
    uint8_t automode:1;	// Automatikbetrieb
    uint8_t wwprio:1;	// WW-Vorrang
    uint8_t estrich:1;	// Estrichtrocknung
    uint8_t holiday:1;	// Urlaubsbetrieb
    uint8_t antifreeze:1;	// Frostschutz
    uint8_t manual:1;	// Manuell
    uint8_t mode_summer:1;	// Sommerbetrieb
    uint8_t mode_day:1;	// Tagbetrieb
    uint8_t fbnocomm:1;	// keine Kommunikation mit FB (?)
    uint8_t fberr:1;	// FB fehlerhaft (?)
    uint8_t sensorerr:1;	// Fehler Vorlauffühler (?)
    uint8_t maxvl:1;	// maximaler Vorlauf
    uint8_t exterr:1;	// externer Störeingang (?)
    uint8_t party:1;	// Party- Pausebetrieb
    uint8_t tsoll;	// Raumtemperatur Soll
    uint16_t tist;	// Raumtemperatur Ist (0x7d00 für HK abgeschaltet)
    uint8_t opteintm;	// Einschaltoptimierungszeit
    uint8_t optaustm;	// Ausschaltoptimierungszeit
    uint8_t hk1curve1;	// Heizkreis1 Heizkurve 10°C
    uint8_t hk1curve2;	// Heizkreis1 Heizkurve 0°C
    uint8_t hk1curve3;	// Heizkreis1 Heizkurve -10°C
    uint16_t deltat;	// Raumtemperatur-Änderungsgeschwindigkeit
    uint8_t pwrreq;	// Von diesem Heizkreis angeforderte Kesselleistung
    uint8_t b1_0:1;	// Schaltzustand ???
    uint8_t b1_1:1;	// Schaltzustand ???
    uint8_t party2:1;	// Schaltzustand Party
    uint8_t pause:1;	// Schaltzustand Pause
    uint8_t b1_4:1;	// Schaltzustand ???
    uint8_t b1_5:1;	// Schaltzustand ???
    uint8_t holiday2:1;	// Schaltzustand Urlaub
    uint8_t vacation:1;	// Schaltzustand Ferien
    uint8_t vtsoll_comp;	// Berechnete Solltemperatur Vorlauf
    uint8_t res:1;	// res
    uint8_t notroom:1;	// keine Raumtemperatur
    uint8_t nored:1;	// keine Absenkung
    uint8_t offbybc10:1;	// Heizbetrieb an BC10 abgeschaltet
}_EMSHK1MonitorMessage;

// HK1Schaltzeiten: 0x10, type=0x3F
typedef struct {
//    uint8_t ;	//
}_EMS_EMSHK1MonitorMessage;

// SM10Monitor: 0x30, type=0x97
typedef struct {
    uint16_t tcoll;	// Kollektortemperatur
    uint8_t modpump;	// Modulation Solarpumpe
    uint16_t tstock;	// Temperatur Speicher unten
    uint8_t pump;	// bit1: Pumpe(gesetzt=EIN)
    uint8_t optime[3];	// Betriebszeit
    uint8_t res;	//???
}_EMSSM10Monitor;

// WM10Parameter: 0x, type=0x
typedef struct {
    uint8_t on;	// Aktivierung 0=aus 255=an
}_EMSWM10Parameter;

// WM10Status2: 0x11, type=0x9C
typedef struct {
    uint16_t twm;	// Temperatur
    uint8_t res;	//	???
}_EMSWM10Status2;

// RCTempMessage: 0x10, type=0xA3
typedef struct {
    uint8_t toutdoor;	// gedämpfte Außentemperatur
    uint8_t flag1;	// Flag 1
    uint8_t flag2;	// Flag 2
    uint16_t troom;	// Raum-Ist
    uint16_t t1;	// Temperatur 1
    uint16_t t2;	// Temperatur 2
    uint16_t sensor1;	// Sensor? (0x8300 = Nicht vorhanden)
    uint16_t sensor2;	// Sensor? (0x8300 = Nicht vorhanden)
}_EMSRCTempMessage;

// Anlagenparameter: 0x10, type=0xA5
typedef struct {
    uint8_t toutmin;	// Minimale Aussentemperatur (i.a. -10°C)
    uint8_t buildtype;	// Gebäudeart 0=leicht, 1=mittel, 2=schwer
    uint8_t mode;	// Dämpfung Aussentemperatur 0=deaktiviert, 255=aktiviert
}_EMSAnlagenparameter;

// MM10Status: 0x21, type=0xAB
typedef struct {
    uint8_t tvsoll;	// Vorlaufsoll
    uint16_t tvist;	// Vorlaufist
    uint8_t stand;	// Stand
}_EMSMM10Status;

// MM10Parameter: 0x10, type=0xAA
typedef struct {
    uint8_t mixer;	// Mischeraktivierung 0=aus 255=an
    uint8_t mixertime;	// Mischernachlaufzeit
}_EMSMM10Parameter;

// MM10Parameter: 0x10, type=0xAC
typedef struct {
    uint8_t vlsoll;	// Vorlaufsoll
    uint8_t stand;	// Stand
}_EMSMM10Parameter2;

// RC20StatusMessage: 0x, type=0x
typedef struct {
    uint8_t daynight;	// b7: Tag/Nachtbetrieb
    uint8_t tsoll;	// Soll Raum-Temp
    uint8_t tist;	// Ist Raum-Temp
}_EMSRC20StatusMessage;

#pragma pack()

// ESP SDK: undocumented function
// after sntp_init, 'realtime_stamp' is filled by sntp result and gets incremented each second
extern uint32_t	realtime_stamp;

// EMSBus stati
#define EMSBUS_RDY 0x01       // bus initialized
#define EMSBUS_RX  0x02       // receiving from EMSBus
#define EMSBUS_TX  0x04       // sending to EMSBus
extern uint8_t	EMSBusStatus;

extern _EMSRxBuf emsTxBuf;
extern _EMSRxBuf *pEMSRxBuf;

#define EMSRingBuffSize 1024
#define EMSRingBuffMax  (EMSRingBuffSize - 64)
extern uint8_t *pEMSRingBuff;

extern void ICACHE_FLASH_ATTR emsInit(void);
extern void ICACHE_FLASH_ATTR emsTaskHandler(void *arg);
#endif
