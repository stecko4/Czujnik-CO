/*Connecting the BME280 Sensor:
Sensor              ->  Board
-----------------------------
Vin (Voltage In)    ->  3.3V
Gnd (Ground)        ->  Gnd
SDA (Serial Data)   ->  D2 on NodeMCU / Wemos D1 PRO
SCK (Serial Clock)  ->  D1 on NodeMCU / Wemos D1 PRO */

//BME280 definition and Mutichannel_Gas_Sensor
#include <EnvironmentCalculations.h>
#include <Wire.h>
#include "MutichannelGasSensor.h"
#include "cactus_io_BME280_I2C.h"
//Create BME280 object
BME280_I2C bme(0x76);			//I2C using address 0x76

//Wyswietlacz OLED
#include <Arduino.h>
#include <U8g2lib.h>
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
// End of constructor list

int		OLED_ON = 1;		//Deklaracja zmiennej załączenia wyświetlacza OLED gdy sygnał z aplikacji Blynk
int		Alarm_Gazowy = 0;	//Deklaracja zmiennej załączenia wyświetlacza OLED gdy przekroczone wartości stężenia gazów
const int	BathFan = D5;		//Deklaracja pinu na który zostanie wysłany sygnał załączenia wentylatora
const int	Buzzer = D6;		//Deklaracja pinu na który zostanie wysłany sygnał alarmu buzzer
const int	Piec_CO = D7;		//Deklaracja pinu na którym będzie odczyt czy piec CO grzeje
float		HumidHist = 5;		//histereza dla wilgotności
float		SetHumid = 75;		//Wilgotności przy której załączy się wentylator
float		temp(NAN), hum(NAN), pres(NAN), dewPoint(NAN), absHum(NAN), heatIndex(NAN);
float		Metan;
float		Tlenek_Wegla;
int		Alarm30(NAN), Alarm50(NAN), Alarm100(NAN), Alarm300(NAN); //Alarmy dla poszczególnych stężeń CO mierzone w ppm
const int	RESET_MULTIGAS = D7;


/*Stężenie tlenku węgla (CO)  Minimalny czas aktywacji czujnika tlenku węgla  Maksymalny czas aktywacji czujnika tlenku węgla
30 ppm  120 minut –
50 ppm  60 minut  90 minut
100 ppm 10 minut  40 minut
300 ppm – 3 minuty  */

/* http://wiki.seeedstudio.com/Grove-Multichannel_Gas_Sensor/
c = gas.measure_NH3();		// Ammonia NH3 1 – 500ppm
c = gas.measure_CO();		// Carbon monoxide CO 1 – 1000ppm
c = gas.measure_NO2();		// Nitrogen dioxide NO2 0.05 – 10ppm
c = gas.measure_C3H8();		// Propane C3H8 >1000ppm
c = gas.measure_C4H10();	// Iso-butane C4H10 >1000ppm
c = gas.measure_CH4();		// Methane CH4 >1000ppm
c = gas.measure_H2();		// Hydrogen H2 1 – 1000ppm
c = gas.measure_C2H5OH();	// Ethanol C2H5OH 10 – 500ppm
*/


//for OTA
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
bool OTAConfigured = 0;

//#define BLYNK_DEBUG // Optional, this enables lots of prints
#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
WidgetTerminal terminal(V40);	//Attach virtual serial terminal to Virtual Pin V40
#include <SimpleTimer.h>

const char	ssid[]			= "XXXX";
const char	pass[]			= "XXXX";
const char	auth[]			= "XXXX";	//Token Łazienka Przychojec
const int	checkInterval		= 30000;	//Co 30s zostanie sprawdzony czy jest sieć Wi-Fi i czy połączono z serwerem Blynk

SimpleTimer timer;
SimpleTimer Main_Timer;
SimpleTimer Alarm_Counter;

void blynkCheck() {					//Sprawdza czy połączone z serwerem Blynk
	if (WiFi.status() == 3) {
		if (!Blynk.connected()) {
			Serial.println("WiFi OK, trying to connect to the Blynk server...");
			Blynk.connect();
		}
	}

	if (WiFi.status() == 1) {
		Serial.println("No WiFi connection, offline mode.");
	}
}

BLYNK_CONNECTED() {					//Informacja że połączono z serwerem Blynk, synchronizacja danych
	Serial.println("Reconnected, syncing with cloud.");
	Blynk.syncAll();
}

void OTA_Handle() {					//Deklaracja OTA_Handle:
	if (OTAConfigured == 1) {
		if (WiFi.waitForConnectResult() == WL_CONNECTED) {
			ArduinoOTA.handle();
		}
	}
	else {
		if (WiFi.waitForConnectResult() == WL_CONNECTED) {
			// Port defaults to 8266
			// ArduinoOTA.setPort(8266);

			// Hostname defaults to esp8266-[ChipID]
			ArduinoOTA.setHostname("ESP8266_Przychojec");

			// No authentication by default
			// ArduinoOTA.setPassword("admin");

			// Password can be set with it's md5 value as well
			// MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
			// ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

			ArduinoOTA.onStart([]() {
				String type;
				if (ArduinoOTA.getCommand() == U_FLASH) { type = "sketch";}
				else { type = "filesystem";}  // U_SPIFFS
				// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
				Serial.println("Start updating " + type);
			});
			
			ArduinoOTA.onEnd([]() {
				Serial.println("\nEnd");
			});
			
			ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
			Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
			});
			
			ArduinoOTA.onError([](ota_error_t error) {
				Serial.printf("Error[%u]: ", error);
				if (error == OTA_AUTH_ERROR) { Serial.println("Auth Failed");}
				else if (error == OTA_BEGIN_ERROR) { Serial.println("Begin Failed");}
				else if (error == OTA_CONNECT_ERROR) { Serial.println("Connect Failed");}
				else if (error == OTA_RECEIVE_ERROR) { Serial.println("Receive Failed");}
				else if (error == OTA_END_ERROR) { Serial.println("End Failed");}
			});
			
			ArduinoOTA.begin();

			Serial.println("Ready");
			Serial.print("OTA IP address: ");
			Serial.println(WiFi.localIP());

			OTAConfigured = 1;
		}
	}
}

void MainFunction() {					//Robi wszystko co powinien

	Read_BME280_Values();				//Odczyt danych z czujnika BME280
	MultiGas_Values();				//Odczyt danych z czujnika Mutichannel_Gas_Sensor
	Gas_Alarms_Count();				//Sprawdza stężenie i zlicza czas jego przekroczenie, na tej podstawie włączany jest alarm
	OLED_Display();					//Włącza lub wyłącza wyświetlacz OLED
	Wyslij_Dane();					//Wysyła dane do serwera Blynk
}

void Bathrum_Humidity_Control() {			//Załączanie wentylatora w łazience jeśli warunek spełniony
	if (hum >= SetHumid + HumidHist) {
		digitalWrite(BathFan, HIGH);		// turn on relay with voltage HIGH
	}
	else if (hum <= SetHumid - HumidHist) {
		digitalWrite(BathFan, LOW);		// turn off relay with voltage LOW
	}
}

void Read_BME280_Values() {				//Odczyt z czujnika BME280, temperatura, wilgotność i ciśnienie
	bme.readSensor();				//Odczyt wskazań z czujnika BME280
	pres = bme.getPressure_MB();
	hum = bme.getHumidity();
	temp = bme.getTemperature_C();
	EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
	EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
	//Dane obliczane na podstawie danych z czujnika
	dewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
	absHum = EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit);
	heatIndex = EnvironmentCalculations::HeatIndex(temp, hum, envTempUnit);
}

void MultiGas_Values() {				//Odczyt z czujnika Grove-Multichannel_Gas_Sensor, stężenie CO i CH4
	//http://wiki.seeedstudio.com/Grove-Multichannel_Gas_Sensor/

	Metan = gas.measure_CH4();			// Methane CH4 >1000ppm
	Tlenek_Wegla = gas.measure_CO();		// Carbon monoxide CO 1 – 1000ppm

	/*
	NH3 = gas.measure_NH3();			// Ammonia NH3 1 – 500ppm
	CO = gas.measure_CO();				// Carbon monoxide CO 1 – 1000ppm
	NO2 = gas.measure_NO2();			// Nitrogen dioxide NO2 0.05 – 10ppm
	C3H8 = gas.measure_C3H8();			// Propane C3H8 >1000ppm
	C4H10 = gas.measure_C4H10();			// Iso-butane C4H10 >1000ppm
	CH4 = gas.measure_CH4();			// Methane CH4 >1000ppm
	H2 = gas.measure_H2();				// Hydrogen H2 1 – 1000ppm
	C2H5OH = gas.measure_C2H5OH();			// Ethanol C2H5OH 10 – 500ppm */
}

void Multi_Gas_Reset() {				//Reset czujnika gazu
	gas.powerOff();
	OLED_Display();
	delay(1000);
	u8g2.clearBuffer();
	u8g2.setFont(u8g_font_helvB10);
	u8g2.drawStr( 3, 25, "MULTIGAS");
	u8g2.drawStr( 3, 50, "RESET !");
	u8g2.sendBuffer();
	delay(1500);
	gas.begin(0x04);   //the default I2C address of the slave is 0x04
	gas.powerOn();

	Metan = gas.measure_CH4();          // Methane CH4 >1000ppm
	Tlenek_Wegla = gas.measure_CO();    // Carbon monoxide CO 1 – 1000ppm

	//then pulse reset, see page 309 of datasheet
	//digitalWrite (RESET_MULTIGAS, LOW);
	//delay (100);  // pulse for at least 2 clock cycles
	//digitalWrite (RESET_MULTIGAS, HIGH);
	//delay(1500);
}

void Gas_Senor_Heating() {
	Metan = gas.measure_CH4();		// Methane CH4 >1000ppm
	Tlenek_Wegla = gas.measure_CO();	// Carbon monoxide CO 1 – 1000ppm
	OLED_Display();
	delay(1500);				// wait 1,5s

	int Metan_final = 1000;
	if (Metan < 0) { //Komunikat 'ERROR -1'
		do {
			Metan = gas.measure_CH4();	// Methane CH4 >1000ppm
			u8g2.clearBuffer();
			u8g2.setFontMode(1);
			u8g2.setFont(u8g_font_helvB10);
			u8g2.drawStr( 3, 59, "ERROR -1");
			u8g2.sendBuffer();
			delay(1500);
			Multi_Gas_Reset();
		} while (Metan < 0); 
	}
	else if (Metan > 1000){
		Rozgrzewanie();
	}
	else{
		u8g2.clearBuffer();
		u8g2.setFont(u8g_font_helvB10);
		u8g2.drawStr( 3, 25, "ROZGRZEWANIE");
		u8g2.drawStr( 3, 50, "ZAKONCZONE");
		u8g2.sendBuffer();
		delay(1500);
	}
	Metan = gas.measure_CH4();			// Methane CH4 >1000ppm
	if (Metan > 1000){
		Rozgrzewanie();
	}
	else{
		u8g2.clearBuffer();
		u8g2.setFont(u8g_font_helvB10);
		u8g2.drawStr( 3, 25, "ROZGRZEWANIE");
		u8g2.drawStr( 3, 50, "ZAKONCZONE");
		u8g2.sendBuffer();
		delay(1500);
	}
}

void Rozgrzewanie() {					//Rozgrzewanie czujnika gazu
	int Metan_initial = gas.measure_CH4();
	u8g2.clearBuffer();
	u8g2.setFontMode(1);
	u8g2.setFont(u8g_font_helvB10);
	u8g2.drawStr( 3, 50, "ROZGRZEWANIE");
	u8g2.drawFrame(0,10,128,20); //rysuje ramki dla progressbar (x,y,szerokość,wysokość)
	int progress = 3;
	u8g2.drawBox(3,13,progress,14); // początek progessbar

	do {
		delay(3000);			// wait 3s
		Metan = gas.measure_CH4();	// Methane CH4 >1000ppm
		//progress = (Metan_initial - Metan) * 119 / (Metan_initial - 1000);
		progress = map(Metan, Metan_initial, 1000, 3, 119); //wyliczanie progresu dla paska postępu od 3 do 119
		if (Metan < Metan_initial){
			Serial.print("map( ");
			Serial.print(Metan);
			Serial.print(", ");
			Serial.print(Metan_initial);
			Serial.print(", 1000, 3, 119) = ");
			Serial.println(String(progress));

			u8g2.setFontMode(0);
			u8g2.setDrawColor(0);
			u8g2.drawBox(3,13,124,14); // kasowanie progresu, czasem jest regres
			u8g2.setDrawColor(1);
			u8g2.drawBox(3,13,int(map(Metan, Metan_initial, 1000, 3, 119)),14); // początek progessbar
			u8g2.setDrawColor(0);
			u8g2.drawBox(50,15,28,10); // początek progessbar
			u8g2.setDrawColor(1);
			u8g2.setFont(u8g2_font_helvB08_tr);
			//postępu w procentów  
			u8g2.setCursor(55,24);
			u8g2.print(String(int(map(Metan, Metan_initial, 1000, 0, 100))) + "%");
			u8g2.sendBuffer();
		}
		else {
			Serial.print("map( ");
			Serial.print(Metan);
			Serial.print(", ");
			Serial.print(Metan_initial);
			Serial.print(", 1000, 3, 119) = ");
			Serial.print(String(progress));
			Serial.println("   BLAD!");
		}
	} while (Metan > 1000);

	u8g2.clearBuffer();
	u8g2.setFont(u8g_font_helvB10);
	u8g2.drawStr( 3, 25, "ROZGRZEWANIE");
	u8g2.drawStr( 3, 50, "ZAKONCZONE");
	u8g2.sendBuffer();
	delay(1500);
}

void Wyslij_Dane() {					//Wysyła dane na serwer Blynk
	//BME280
	Blynk.virtualWrite(V0, temp);			//Temperatura [°C]
	Blynk.virtualWrite(V1, hum);			//Wilgotność [%]
	Blynk.virtualWrite(V2, pres);			//Ciśnienie [hPa]
	Blynk.virtualWrite(V3, dewPoint);		//Temperatura punktu rosy [°C]
	Blynk.virtualWrite(V4, absHum);			//Wilgotność bezwzględna [g/m³]
	Blynk.virtualWrite(V5, heatIndex);		//Temperatura odczuwalna [°C] 
	//Multigas Sensor
	Blynk.virtualWrite(V7, Tlenek_Wegla);		//Stężenie Tlenku Węgla [ppm]
	Blynk.virtualWrite(V8, Metan);			//Stężenie Metanu [ppm]  
	//Blynk.virtualWrite(V9, BathFan);		//Stan włączenia wentylatora Wł/Wył
	//Blynk.virtualWrite(V10, Piec_CO);		//Stan włączenia pieca CO Wł/Wył
	Blynk.virtualWrite(V25, map(WiFi.RSSI(), -105, -40, 0, 100) ); //Siła sygnału Wi-Fi [%]
}

BLYNK_WRITE(V40) {	//Obsługa terminala
	String TerminalCommand = param.asStr();
	TerminalCommand.toLowerCase();

	if (String("ports") == TerminalCommand) {
		terminal.clear();
		terminal.println("PORT     DESCRIPTION        UNIT");
		terminal.println("V0   ->  Temperature        °C");
		terminal.println("V1   ->  Humdity            %");
		terminal.println("V2   ->  Pressure           HPa");
		terminal.println("V3   ->  DewPoint           °C");
		terminal.println("V4   ->  Abs Humdity        g/m³");
		terminal.println("V5   ->  Heat Index         °C");
		terminal.println("V7   ->  Tlenek_Wegla       ppm");
		terminal.println("V8   ->  Metan              ppm");
		terminal.println("V20  <-  OLED_ON            1/0");
		terminal.println("V21  <-  GasReset           1/0");
		terminal.println("V25  ->  WiFi Signal        %");
		terminal.println("V40 <->  Terminal           String");
	}
	else if (String("values") == TerminalCommand) {
		terminal.clear();
		terminal.println("PORT   DATA              VALUE");
		terminal.print("V0     Temperature   =   ");
		terminal.print(temp);
		terminal.println(" °C");
		terminal.print("V1     Humdity       =   ");
		terminal.print(hum);
		terminal.println(" %");
		terminal.print("V3     Pressure      =   ");
		terminal.print(pres);
		terminal.println(" HPa");
		terminal.print("V4     DewPoint      =   ");
		terminal.print(dewPoint);
		terminal.println(" °C");
		terminal.print("V5     Abs Humdity   =   ");
		terminal.print(absHum);
		terminal.println(" g/m3");
		terminal.print("V6     Heat Index    =   ");
		terminal.print(heatIndex);
		terminal.println(" °C");
		terminal.print("V7     Tlenek Węgla  =   ");
		terminal.print(Tlenek_Wegla);
		terminal.println(" ppm");
		terminal.print("V8     Metan         =   ");
		terminal.print(Metan);
		terminal.println(" ppm");
		terminal.print("V20    OLED_ON       =   ");
		terminal.print(OLED_ON);
		terminal.println(" ");
		terminal.print("V25    WiFi Signal   =   ");
		terminal.print(map(WiFi.RSSI(), -105, -40, 0, 100));
		terminal.println(" %");
	}
	else if (String("hello") == TerminalCommand) {
		terminal.clear();
		terminal.println("Hi Łukasz. Have a great day!");
	}
	else {
		terminal.clear();
		terminal.println("Type 'PORTS' to show list") ;
		terminal.println("Type 'VALUES' to show list") ;
		terminal.println("or 'HELLO' to say hello!") ;
	}
	// Ensure everything is sent
	terminal.flush();
}

void OLED_Display() {					//Włącza lub wyłącza wyświetlanie danych na ekranie OLED
	Serial.print("OLED_ON = ");
	Serial.println(OLED_ON);
	if (OLED_ON == 1 || Alarm_Gazowy == 1){
		//Wyświetlanie dane na OLED
		u8g2.clearBuffer();
		u8g2.setFontMode(1);
		u8g2.setFont(u8g2_font_helvR08_tf);  
		u8g2.drawStr(0,8,"Stezenie CO:"); 
		u8g2.drawStr(0,42,"Stezenie Metanu:");
		u8g2.setFont(u8g2_font_logisoso16_tf); 
		u8g2.setCursor(0,28);
		u8g2.print(String(int(Tlenek_Wegla)) + " ppm");
		u8g2.setCursor(0,61);
		u8g2.print(String(int(Metan)) + " ppm");
		u8g2.sendBuffer(); 
	}
	else if (OLED_ON == 0 && Alarm_Gazowy == 0){
		u8g2.clearBuffer();
		u8g2.sendBuffer();
	}
}

void Gas_Alarms_Count() {				//Funkcja uruchamiana co 1s dolicza sekundę do poszczególnych alarmów jeśli stężenie przekroczone określony próg
	/*Stężenie tlenku węgla (CO) Minimalny czas aktywacji czujnika tlenku węgla  Maksymalny czas aktywacji czujnika tlenku węgla
	30 ppm  120 minut –
	50 ppm  60 minut  90 minut
	100 ppm 10 minut  40 minut
	300 ppm – 3 minuty  */

	if (Tlenek_Wegla < 30) {				//Zeruje czasy wszystkich alarmów
		Alarm30 = 0;
	}
	else if (Tlenek_Wegla > 30 && Tlenek_Wegla < 50) {	//Dodaje sekundę do czasu pierwszego alarmu
		Alarm30 = Alarm30 +1;
		Alarm50 = 0;
		Alarm100 = 0;
		Alarm300 = 0;
	}
	else if (Tlenek_Wegla > 50 && Tlenek_Wegla < 100) {	//Dodaje sekundę do czasu pierwszego i drugiego alarmu
		Alarm30 = Alarm30 +1;
		Alarm50 = Alarm50 +1;
		Alarm100 = 0;
		Alarm300 = 0;
	}
	else if (Tlenek_Wegla > 100 && Tlenek_Wegla < 300) {	//Dodaje sekundę do czasu pierwszego drugiego i trzeciego alarmu
		Alarm30 = Alarm30 +1;
		Alarm50 = Alarm50 +1;
		Alarm100 = Alarm100 +1;
		Alarm300 = 0;
	}
	else if (Tlenek_Wegla > 300 ) {				//Dodaje sekundę do czasu wszystkich alarmów
		Alarm30 = Alarm30 +1;
		Alarm50 = Alarm50 +1;
		Alarm100 = Alarm100 +1;
		Alarm300 = Alarm300 +1;
	}
	Alarm_Check();						//Włącza buzer i ekran OLED z odczytami jeśli wartości są przekroczone
}

void Alarm_Check() {					//Funkcja sprawdza czy należy uruchomić alarm czyli włączyć ekran z informacją o stężeniu gazów i uruchomić buzer
	/*Stężenie tlenku węgla (CO) Minimalny czas aktywacji czujnika tlenku węgla  Maksymalny czas aktywacji czujnika tlenku węgla
	30 ppm  120 minut –
	50 ppm  60 minut  90 minut
	100 ppm 10 minut  40 minut
	300 ppm – 3 minuty  */
	//Alarm dla przekroczenia stężenia CO
	if (Alarm30 > 20 || Metan == 10000) {		//Stężenie CO utrzymuje się powyżej 30ppm przez ponad 120minut
		tone(Buzzer, 3000, 250);					//Send 3KHz sound signal...
		Alarm_Gazowy = 1;						//Włącza ekran OLED z odczytami
	}
	else if (Alarm50 > 3600 || Metan == 10000) {	//Stężenie CO utrzymuje się powyżej 50ppm przez ponad 60minut
		tone(Buzzer, 3000, 500);					//Send 3KHz sound signal...
		Alarm_Gazowy = 1;						//Włącza ekran OLED z odczytami   
	}
	else if (Alarm100 > 600 || Metan == 10000) {	//Stężenie CO utrzymuje się powyżej 100ppm przez ponad 10minut
		tone(Buzzer, 3000, 500);					//Send 3KHz sound signal...
		Alarm_Gazowy = 1;						//Włącza ekran OLED z odczytami
	}
	else if (Alarm300 > 600 || Metan == 10000) {	//Stężenie CO utrzymuje się powyżej 300ppm przez ponad 3minuty
		tone(Buzzer, 3000, 500);					//Send 3KHz sound signal...
		Alarm_Gazowy = 1;						//Włącza ekran OLED z odczytami
	}
	else { //Wyłączenie alarmu
		noTone(Buzzer);							//Stop sound...
		Alarm_Gazowy = 0;						//Wyłącza ekran OLED z odczytami    
	}

	//Alarm dla przekroczenia stężenia CH4
	if (Metan == 10000) {				//Stężenie CH4 Metanu Przekroczyło stężenie 1% czyli 10000ppm
		tone(Buzzer, 3000, 500);					//Send 1KHz sound signal...
		Alarm_Gazowy = 1;							//Włącza ekran OLED z odczytami
	}
	else { //Wyłączenie alarmu
		noTone(Buzzer);							//Stop sound...
		Alarm_Gazowy = 0;						//Wyłącza ekran OLED z odczytami
	}
}

BLYNK_WRITE(V20) {					//Włączanie i wyłączanie wyświetlacza z poziomu aplikacji BLYNK
	OLED_ON = param.asInt(); 
}

BLYNK_WRITE(V21) {					//Reset sensora gazu
	int GasReset = param.asInt(); 
	if (GasReset == 1) {
		gas.powerOff();
		//Wyświetlanie dane na OLED
		u8g2.clearBuffer();
		u8g2.setFontMode(1);
		u8g2.setFont(u8g2_font_logisoso16_tf); 
		u8g2.setCursor(0,28);
		u8g2.print("RESET");
		u8g2.setCursor(0,61);
		u8g2.print("MUTLIGAS");
		u8g2.sendBuffer(); 
		delay(3000);
		gas.powerOn();
		u8g2.clearBuffer();
		u8g2.sendBuffer();
	}
}
/***********************************************************************************************/

void setup(){
	Serial.begin(9600);
	WiFi.begin(ssid, pass);
	Serial.println("Connecting to BLYNK");
	Blynk.config(auth);

	timer.setInterval(checkInterval, blynkCheck);		// Multiple timer https://codebender.cc/example/SimpleTimer/SimpleTimerAlarmExample#SimpleTimerAlarmExample.ino
	Main_Timer.setInterval(3000, MainFunction);		// 1000 = 1s
	Alarm_Counter.setInterval(1000, Gas_Alarms_Count);	//Co 1s uruchamia funkcje i dolicza sekundę do poszczególnych alarmów

	//Ustawianie pinów
	pinMode (RESET_MULTIGAS, OUTPUT);
	digitalWrite (RESET_MULTIGAS, HIGH);
	//pinMode(BathFan, OUTPUT);
	//pinMode(Buzzer, OUTPUT);
	//pinMode(Piec_CO, INPUT);


	//inicjowanie wyświetlacza
	//pinMode(9, OUTPUT);
	//digitalWrite(9, 0); // default output in I2C mode for the SSD1306 test shield: set the i2c adr to 0
	u8g2.begin();
	u8g2.enableUTF8Print(); 
	u8g2.clearBuffer();
	u8g2.setFontMode(1);
	u8g2.setFont(u8g_font_helvB18);    
	u8g2.setCursor(0,22);
	u8g2.print(F("CZUJNIK")); 
	u8g2.setCursor(0,46);
	u8g2.print(F("GAZU")); 
	u8g2.setFont(u8g_font_helvR08); 
	u8g2.setCursor(0, 64);
	u8g2.print("Connecting to: "+String(ssid));
	u8g2.sendBuffer();


	// Inicjalizacja Grove - Multichannel Gas Sensor
	gas.begin(0x04);   //the default I2C address of the slave is 0x04
	gas.powerOn();
	Gas_Senor_Heating();

	//inicjowanie czujnika BME280
	if (!bme.begin()) {
		Serial.println("Could not find a valid BME280 sensor, check wiring!");
		while (1);
	}

	bme.setTempCal(-7.1);			// Temp was reading high so subtract 7.1 degree 
}

void loop(){
	timer.run();
	Main_Timer.run();
	OTA_Handle();			//Obsługa OTA (Over The Air) wgrywanie nowego kodu przez Wi-Fi
	//Alarm_Counter.run();
	if (Blynk.connected()) Blynk.run();
}