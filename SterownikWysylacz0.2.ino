/*

  Rezerwowane komórki EEPROM:
  Int to dwie komórki EEPROM
  6-7 liczba impulsow na litr
  Long to trzy komórki EEPROM
  20 --> 23 licznik impulsow w long czyli 32 bity.
  Cena za urządzenie 1000 zł :) 
  W cenie urządzenia: Płyta główna gotowa do podłączenia do czujnika np. Halla.
  Programowanie wg rządanych parametrów. 
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>


#ifndef STASSID
#define STASSID "SSID"
#define STAPSK  "PASSWORD"
#endif


void EEPROMWriteInt(int address, unsigned int value)
{
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);
  EEPROM.write(address, two);
  EEPROM.write(address + 1, one);
  EEPROM.commit();
}

void EEPROMWriteLong(int address, unsigned long value)
{
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);
  byte zero = ((value >> 16) & 0xFF);
  EEPROM.write(address, two);
  EEPROM.write(address + 1, one);
  EEPROM.write(address + 2, zero);
  EEPROM.commit();
}

unsigned int EEPROMReadInt(int address)
{
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);

  return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}
unsigned long EEPROMReadLong(int address)
{
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);
  long zero = EEPROM.read(address + 2);

  return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF) + ((zero << 16) & 0xFFFFFFFF);
}


const char *ssid = STASSID;
const char *password = STAPSK;

ESP8266WebServer server(80);

unsigned long imp_na_litr = 20;

boolean parzystosc_godziny = false;

const int led = 13;
const int hall_one = 0; //Hall w piwnicy

unsigned long licznik_impulsow = 0; //Licznik impulsow
unsigned long przelicznik_na_litry = 0;//licznik_impulsow/imp_na_litr;

unsigned int licznik_czasu=1; //Liczniki czasu są 16 bitowe i odpowiada 50 dniom. 
 float chwilowy = 0; //Ile czasu zajmuje nabicie 
byte count_to_ten = 0;


void ICACHE_RAM_ATTR dodaj_l_jeden() { //Ta funkcja jest realizowana przez drugi wątek esp. 
  licznik_impulsow++;
  
  if(count_to_ten%10==0){ //Co 10 rób pomiar chwilowego a co 1000 rob zapis do do eepromu
    chwilowy =((float)((float)10/(float)imp_na_litr)/ (float)(millis() - licznik_czasu)*1000)*60; //litrów na 
    licznik_czasu=millis();
    if(count_to_ten%1000==0){
      count_to_ten = 0;
      EEPROMWriteLong(20,licznik_impulsow);
    }
  }
  count_to_ten++;
  
}


void zapisz_wyniki(int no_of_args, ...) { //Zapakuj wartość impulsatora przepływomierza do pamięci EEPROM
  //Mam tylko 512 komórek pamięci zaczynając od czwartej.
  va_list argument_pointer;
  int i;
  va_start(argument_pointer, no_of_args);
  for (i = 20; i < no_of_args + 20; i++) {
    EEPROMWriteInt(i * 2, va_arg(argument_pointer, int));
  }
  va_end(argument_pointer);
}

void dodaj_sto_impulsow() {licznik_impulsow += 100;}
void odejmij_sto_impulsow() {licznik_impulsow -= 100;}
void zeruj_licznik() {licznik_impulsow = 0;}
void ustaw_licznik_impulsow(int x) {licznik_impulsow = x;}
void pierwsze_uruchomienie() { //Służy do pierwszego zapisania eeprom lub zresetowania ustawień.
  EEPROMWriteInt(6, 1000); //Zapisz 1000 impulsow na litr
  EEPROMWriteLong(20, 1); //Zresetuj licznik impulsow
}

void handleRoot() {
  digitalWrite(led, 1);
  char temp[600];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  int sumaryczny = licznik_impulsow/imp_na_litr;
  snprintf(temp, 600,

           "<html>\
  <head>\
    <meta http-equiv='refresh' content='5' meta charset='utf-8'/>\
    <title>Wodomierz 1</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Miernik w piwnicy</h1>\
    <p>Czas działania: %02d:%02d:%02d</p>\
    <p>Liczba impulsów: %d</p>\
    <p>Przepływ chwilowy: %.3f litrów/minutę </p>\
    <p>Przepływ sumaryczny: %d litrów </p>\
    <p>Impulsów na litr: %d </p>\
    <p>Tu pojawią się przyciski </p>\
    </body>\
</html>",

           hr, min % 60, sec % 60,licznik_impulsow, chwilowy, licznik_impulsow/imp_na_litr, imp_na_litr
          );
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}



void setup(void) {
  EEPROM.begin(512);
  //pierwsze_uruchomienie();
  licznik_impulsow = EEPROMReadLong(20);
  pinMode(hall_one, INPUT_PULLUP);
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  Serial.println("Łączenie z WIFI");
  WiFi.mode(WIFI_STA);
  /* 
    //Statyczne IP
    IPAddress ip(192,168,1,200);
    IPAddress gateway(192,168,1,254);
    IPAddress subnet(255,255,255,0);
    WiFi.config(ip, gateway, subnet);
  */
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("Próbuję połączyć się do sieci");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Załączam przerwania");
  attachInterrupt(digitalPinToInterrupt(hall_one), dodaj_l_jeden, FALLING); //Ten interrupt musi znaleźć się na końcu całego setupa
  Serial.println("Ok przeszedłem żywy przez setup:)");
}

void loop(void) {
  server.handleClient();
  MDNS.update();
}
