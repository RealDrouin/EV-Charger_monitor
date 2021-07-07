/*
   made by Real Drouin
   2021/juil/05

   Serial 115200 baud
   ev-charger.local
   ev-charger.local/api
   ////////////////////////////////
   Setup Mode Actived! for 5min
   SSID: ev-setup
   PASS: ev-charger
   IP: 192.168.4.1
*/

const String ver = "Ver 1.3";

#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include <EmonLib.h>

// Energy Monitor
EnergyMonitor emon;
float Ical = 0.0; // 88.5
double watiosTotal = 0;               // Measured energy in watios/h
double kiloWattHours = 0;             // Measured energy in KWh
double totalKiloWattHours = 0;
double rmsCurrent = 0;                // Measured current in A
double rmsPower = 0;                  // Measured power in W
unsigned long lastTimeMeasure = 0;    // Last time measure in ms
double mainsVoltage = 220.00;
long lastEMRead = 0;  // Last EM Read
unsigned long startSession = 0;
double energyCost = 0.10;
double sessionCost = 0.00;
double totalEnergyCost = 0.00;

bool session = false;

String timeLive = "Reboot!";

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <BlynkSimpleEsp8266.h>

#define Status_LED 2 // D4
#define Status_LED_On digitalWrite(Status_LED, HIGH)
#define Status_LED_Off digitalWrite(Status_LED, LOW)

String AuthToken = "";
String BlynkServer = "";
int BlynkPort;
volatile bool Connected2Blynk = false;

volatile bool Setup = false;

// System Uptime
byte hh = 0; byte mi = 0; byte ss = 0; // hours, minutes, seconds
unsigned int dddd = 0; // days (for if you want a calendar)
unsigned long lastTick = 0; // time that the clock last "ticked"
char timestring[25]; // for output

String webSite, javaScript, XML, header, footer, ssid, password;

///////// Button CSS /////////
const String button = ".button {background-color: white;border: 2px solid #555555;color: black;padding: 16px 32px;text-align: center;text-decoration: none;display: inline-block;font-size: 16px;margin: 4px 2px;-webkit-transition-duration: 0.4s;transition-duration: 0.4s;cursor: pointer;}.button:hover {background-color: #555555;color: white;}.disabled {opacity: 0.6;pointer-events: none;}";
/////////////////////////////
//////// CSS FORM ///////////
const String form = "input[type=number]{padding: 10px;border:2px solid #ff1100;font-size:30px;-webkit-border-radius:5px;border-radius:5px;}input[type=text]{padding: 10px;border:2px solid #ff1100;font-size: 30px;-webkit-border-radius: 5px;border-radius: 5px;}input[type=submit] {padding: 5px 20px;background: #ff1100;border: 0 none;cursor: pointer;font-size: 30px;color: white;-webkit-border-radius: 20px;border-radius: 5px;}";
/////////////////////////////

byte percentQ = 0; // wifi signal strength %

ESP8266WebServer server(80);
WiFiClient client;
String readString;

// OTA UPDATE
ESP8266HTTPUpdateServer httpUpdater; //ota

///////////
// Blynk //
///////////////////////////////////////////
BlynkTimer timer;

BLYNK_WRITE(V4) {
  float readTotalKiloWattHours = param.asFloat();
  totalKiloWattHours = readTotalKiloWattHours;
  totalEnergyCost = totalKiloWattHours * energyCost;
}

BLYNK_WRITE(V10) {
  Serial.println(F("Erase ssid and passwword and reboot in setup mode."));
  EEPROM.begin(512);
  delay(200);

  // Erase SSID and PASSWORD
  for (int i = 34; i < 100; ++i)
  {
    EEPROM.write(i, 0);
  }
  delay(100);
  EEPROM.commit();
  delay(200);
  EEPROM.end();
  delay(200);
  WiFi.disconnect();
  delay(3000);
  ESP.restart();
}

BLYNK_WRITE(V11) {
  totalKiloWattHours = 0;
  totalEnergyCost = 0.00;

  Blynk.virtualWrite(V4, 0);
  Blynk.virtualWrite(V5, 0);
}

bool isFirstConnect = true;
// This function will run every time Blynk connection is established
BLYNK_CONNECTED() {
  if (isFirstConnect) {
    // Request Blynk server to re-send latest values for all pins
    // Blynk.syncAll();

    // You can also update individual virtual pins like this:
    Blynk.syncVirtual(V4);
    isFirstConnect = false;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Rebooted!"));

  pinMode(Status_LED, OUTPUT); // Led

  //////////////////////////////////////
  // START EEPROM
  EEPROM.begin(512);
  delay(200);

  //////////////////////////////////////
  // Init EEPROM
  byte Init = EEPROM.read(451);

  if (Init != 111) {
    for (int i = 0; i < 512; ++i)
    {
      EEPROM.write(i, 0);
    }
    delay(100);
    EEPROM.commit();
    delay(200);

    EEPROM.write(451, 111);
    delay(100);
    EEPROM.commit();
    delay(200);

    // Erase SSID and PASSWORD
    for (int i = 34; i < 100; ++i)
    {
      EEPROM.write(i, 0);
    }
    delay(100);
    EEPROM.commit();
    delay(200);
    Serial.println(F("Init eeprom!"));
  }

  Serial.println(F("Read Config!"));
  //////////////////////////////////////
  // READ EEPROM SSID & PASSWORD
  String esid;
  for (int i = 34; i < 67; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  delay(200);
  ssid = esid;

  String epass = "";
  for (int i = 67; i < 100; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  delay(200);
  password = epass;

  //////////////////////////////////////
  // READ EEPROM ENERGY COST
  int indexEnergyCost = EEPROM.read(200) + 201;
  String readEnergyCost = "";
  for (int i = 201; i < indexEnergyCost; ++i)
  {
    readEnergyCost += char(EEPROM.read(i));
  }
  energyCost = readEnergyCost.toDouble();

  //////////////////////////////////////
  // READ EEPROM SENSOR CALIBRATION
  int indexCal = EEPROM.read(210) + 211;
  String readCal = "";
  for (int i = 211; i < indexCal; ++i)
  {
    readCal += char(EEPROM.read(i));
  }
  Ical = readCal.toFloat();

  /////////////////////////////////////////
  // READ EEPROM CHARGING VOLTAGE STATION
  int indexVol = EEPROM.read(220) + 221;
  String readVol = "";
  for (int i = 221; i < indexVol; ++i)
  {
    readVol += char(EEPROM.read(i));
  }
  mainsVoltage = readVol.toDouble();

  //////////////////////////////////////
  // Blynk Ip Local Server
  int IndexBlynkServer = EEPROM.read(306) + 307;
  String ReadBlynkServer = "";
  for (int i = 307; i < IndexBlynkServer; ++i)
  {
    ReadBlynkServer += char(EEPROM.read(i));
  }
  BlynkServer = ReadBlynkServer;

  /////////////////////////////////////
  // Blynk Port Local Server
  int IndexBlynkPort = EEPROM.read(390) + 391;
  String ReadBlynkPort = "";
  for (int i = 391; i < IndexBlynkPort; ++i)
  {
    ReadBlynkPort += char(EEPROM.read(i));
  }
  BlynkPort = ReadBlynkPort.toInt();

  //////////////////////////////////////
  //BLYNK AUTH TOKEN
  int IndexToken = EEPROM.read(400) + 401;
  String ReadToken = "";
  for (int i = 401; i < IndexToken; ++i)
  {
    ReadToken += char(EEPROM.read(i));
  }
  AuthToken = ReadToken;

  EEPROM.end();

  ////////////////
  // Cal Sensor //
  ///////////////////////////////////////////////////////////////
  emon.current(A0, Ical);             // Current: input pin A6=D4, calibration factor

  for (int i = 0; i < 5; i++) {
    emon.calcIrms(1480);
  }

  ///////////////////
  // SSID PASSWORD //
  ///////////////////////////////////////////////////////////////
  server.on("/WiFi", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String WifiSsid = server.arg("ssid");
    String WifiPassword = server.arg("pass");

    if (WifiPassword.length() > 9 and WifiPassword.length() < 33) {
      // START EEPROM
      EEPROM.begin(512);
      delay(200);

      for (int i = 34; i < 100; ++i)
      {
        EEPROM.write(i, 0);
      }
      delay(100);
      EEPROM.commit();
      delay(200);

      for (int i = 0; i < WifiSsid.length(); ++i)
      {
        EEPROM.write(34 + i, WifiSsid[i]);
      }

      for (int i = 0; i < WifiPassword.length(); ++i)
      {
        EEPROM.write(67 + i, WifiPassword[i]);
      }
      delay(100);
      EEPROM.commit();
      delay(200);
      EEPROM.end();

      handleREBOOT();
    }
    else if (WifiPassword.length() <= 9 or WifiPassword.length() > 32) {
      server.send(200, "text/html", "<header><h1>Error!, Please enter valid PASS! min10 max32 character <a href=/wifisetting >Back!</a></h1></header>");
    }
    else {
      server.send(200, "text/html", "<header><h1>Error!, Please enter PASS! <a href=/wifisetting >Back!</a></h1></header>");
    }
  });


  ////////
  // API //
  ///////////////////////////////////////////////////////////////
  server.on("/api", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String JsonResponse = "{\n\"ev-charger\": ";
    JsonResponse += "{\n\"charging rate\":\"" + String (rmsCurrent) + "A\", \n";
    JsonResponse += "\"energy charged\":\"" + String (kiloWattHours) + "kWh\", \n";
    JsonResponse += "\"money spent\":\"" + String (sessionCost) + "$\", \n";
    JsonResponse += "\"status\":\"" + String (timeLive) + "\", \n";;
    JsonResponse += "\"energy consumption total\":\"" + String (totalKiloWattHours) + "kWh\", \n";
    JsonResponse += "\"total money spent\":\"" + String (totalEnergyCost) + "$\"";
    JsonResponse += "\n}\n}";

    server.send(200, "application/json",  JsonResponse);
  });

  /////////////////
  // Calibration //
  ////////////////////////////////////////////////////////////
  server.on("/Sensor", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String readCost = server.arg("cost");
    String readCal = server.arg("cal");
    String readVol = server.arg("vol");

    EEPROM.begin(512);
    delay(200);

    if (readCost != NULL) {
      EEPROM.write(200, readCost.length());
      for (int i = 0; i < readCost.length(); ++i)
      {
        EEPROM.write(201 + i, readCost[i]);
      }
      delay(100);
      EEPROM.commit();

      energyCost = readCost.toDouble();
    }
    /////////////////////////////////////////////////////
    if (readCal != NULL) {
      EEPROM.write(210, readCal.length());
      for (int i = 0; i < readCal.length(); ++i)
      {
        EEPROM.write(211 + i, readCal[i]);
      }
      delay(100);
      EEPROM.commit();

      Ical = readCal.toFloat();

      ////////////////
      // Cal Sensor //
      ///////////////////////////////////////////////////////////////
      emon.current(A0, Ical);             // Current: input pin A6=D4, calibration factor

      for (int i = 0; i < 5; i++) {
        emon.calcIrms(1480);
      }
    }
    /////////////////////////////////////

    if (readVol != NULL) {
      EEPROM.write(220, readVol.length());
      for (int i = 0; i < readVol.length(); ++i)
      {
        EEPROM.write(221 + i, readVol[i]);
      }
      delay(100);
      EEPROM.commit();

      mainsVoltage = readVol.toDouble();
    }

    delay(200);
    EEPROM.end();
    handleCAL();
  });

  ///////////////
  // Blynk Key //
  ////////////////////////////////////////////////////////////
  server.on("/Blynk", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String ReadKey = server.arg("key");
    String ReadBlynkIp = server.arg("server");
    String ReadBlynkPort = server.arg("port");

    if (ReadKey.length() < 33) {
      EEPROM.begin(512);
      delay(200);

      EEPROM.write(400, ReadKey.length());
      for (int i = 0; i < ReadKey.length(); ++i)
      {
        EEPROM.write(401 + i, ReadKey[i]);
      }
      delay(100);
      EEPROM.commit();
      delay(200);

      AuthToken = ReadKey;
      EEPROM.end();

      Blynk.config(AuthToken.c_str());
      Blynk.connect();
    }
    handleBLYNK();
  });

  //////////////////
  // Blynk Server //
  ////////////////////////////////////////////////////////////
  server.on("/BlynkServer", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String ReadBlynkServer = server.arg("server");
    String ReadBlynkPort = server.arg("port");

    EEPROM.begin(512);
    delay(200);

    EEPROM.write(306, ReadBlynkServer.length());
    for (int i = 0; i < ReadBlynkServer.length(); ++i)
    {
      EEPROM.write(307 + i, ReadBlynkServer[i]);
    }

    EEPROM.write(390, ReadBlynkPort.length());
    for (int i = 0; i < ReadBlynkPort.length(); ++i)
    {
      EEPROM.write(391 + i, ReadBlynkPort[i]);
    }

    delay(100);
    EEPROM.commit();
    delay(200);
    EEPROM.end();

    BlynkServer = ReadBlynkServer;
    BlynkPort = ReadBlynkPort.toInt();

    if (BlynkServer.length() > 5) {
      Blynk.config(AuthToken.c_str(), BlynkServer.c_str(), BlynkPort);
    }
    else {
      Blynk.config(AuthToken.c_str(), "blynk-cloud.com", 8442);
    }

    Blynk.connect();

    handleBLYNK();
  });

  //////////////////////
  // WiFi Connection //
  ////////////////////////////////////////////////////////////
  WiFi.disconnect();
  Serial.println(F("Connecting... to Network!"));
  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.hostname("ev-charger");
  delay(100);
  WiFi.begin(ssid.c_str(), password.c_str());
  Setup = false;
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(i);
    i++;
    delay(1000);
    if (i > 15) {
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("Connected !"));
    Serial.println(WiFi.localIP());
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
  }
  else {
    WiFi.disconnect();
    delay(200);
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAP("ev-setup", "ev-charger");
    Serial.println(F("Setup Mode Actived! for 5min"));
    Serial.println(F("SSID: ev-setup"));
    Serial.println(F("PASS: ev-charger"));
    Serial.println(F("IP: 192.168.4.1"));
    Setup = true;
  }

  if (Setup == false) {
    httpUpdater.setup(&server, "/firmware", "admin", password.c_str());

    server.onNotFound(handleNotFound);
    server.on("/", handleOnConnect);
    server.on("/cal", handleCAL);
    server.on("/wifisetting", handleWIFISETTING);
    server.on("/blynk", handleBLYNK);
    server.on("/reboot", handleREBOOT);

    ///////////
    // Blynk //
    ///////////////////////////////////////////////////
    if (BlynkServer.length() > 5) {
      Blynk.config(AuthToken.c_str(), BlynkServer.c_str(), BlynkPort);
    }
    else {
      Blynk.config(AuthToken.c_str(), "blynk-cloud.com", 8442);
    }

    Blynk.connect();

    timer.setInterval(1000L, readSensor);
    ///////////////////////////////////////////////////

  }
  else {
    server.onNotFound(handleNotFound);
    server.on("/", handleWIFISETTING);
  }

  server.begin();
  delay(5000);

  if (WiFi.status() == WL_CONNECTED) {

    if (MDNS.begin("ev-charger", WiFi.localIP())) {
      Serial.println("MDNS Started");
    } else {
      Serial.println("MDNS started FAILED");
    }
    MDNS.addService("http", "tcp", 80);
  }
}

void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    Connected2Blynk = Blynk.connected();
    MDNS.update();
    timer.run();
  }

  if (Connected2Blynk) {
    Blynk.run();
  }

  server.handleClient();

  // System Uptime
  if ((micros() - lastTick) >= 1000000UL) {
    lastTick += 1000000UL;
    ss++;
    if (ss >= 60) {
      ss -= 60;
      mi++;
    }
    if (mi >= 60) {
      mi -= 60;
      hh++;
    }
    if (hh >= 24) {
      hh -= 24;
      dddd++;
    }
  }

  ////// RESET SETUP MODE AFTER 5MIN //////
  if (Setup == true) {
    if (millis() > 300000) {
      WiFi.disconnect();
      delay(3000);
      ESP.restart();
    }
  }
}
///////////////////////// END LOOP ////////////////////////////

///////////////////
// HANDLE REBOOT //
////////////////////////////////////////////////////////////
void handleREBOOT() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  String spinner = (F("<html>"));
  spinner += (F("<head><center><meta http-equiv=\"refresh\" content=\"30;URL='http://ev-charger.local/'\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>"));
  spinner += (F("body {margin: 0;text-align: center;font-family: Arial, Helvetica, sans-serif; background-color:#555888; color:white;}"));
  spinner += (F(".loader {border: 16px solid #f3f3f3;border-radius: 50%;border-top: 16px solid #3498db;"));
  spinner += (F("width: 120px;height: 120px;-webkit-animation: spin 2s linear infinite;animation: spin 2s linear infinite;}"));

  spinner += (F("@-webkit-keyframes spin {0% { -webkit-transform: rotate(0deg); }100% { -webkit-transform: rotate(360deg); }}"));

  spinner += (F("@keyframes spin {0% { transform: rotate(0deg); }100% { transform: rotate(360deg); }}"));

  spinner += (F("</style></head>"));

  spinner += (F("<br><b><h1 style='font-family:verdana; color:white; font-size:400%; text-align:center;'>EV-Charger</font></h1></b><hr><hr>"));
  spinner += (F("<p><h2>Rebooting Please Wait...</h2></p>"));
  spinner += (F("<div class=\"loader\"></div>"));
  spinner += (F("</body></center></html>"));

  server.send(200, "text/html",  spinner);

  delay(1000);
  WiFi.disconnect();
  delay(3000);
  ESP.restart();
}

///////////////////////
// HANDLE SENSOR CAL //
////////////////////////////////////////////////////////////
void handleCAL() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  buildHeader();
  webSite = header;

  webSite += (F("<div class='topnav' id='myTopnav'>"));
  webSite += (F("<a href='/'>Exit</a>"));
  webSite += (F("<a href='/cal' class='active'>Sensor Calibration</a>"));
  webSite += (F("<a href='/wifisetting'>Wifi</a>"));
  webSite += (F("<a href='/blynk'>Blynk</a>"));
  webSite += (F("<a href='/reboot' onclick=\"return confirm('Are you sure ? ');\">Reboot</a>"));
  webSite += (F("<a href='/firmware'>Firmware Update</a>"));
  webSite += (F("<a href='javascript:void(0);' class='icon' onclick='myFunction()'>"));
  webSite += (F("<i class='fa fa-bars'></i></a>"));
  webSite += (F("</div>"));

  webSite += (F("<br><b><h1 style='font-family:verdana; color:white; font-size:400%; text-align:center;'>EV-Charger</font></h1></b><hr><hr>"));

  webSite += (F("<p><h1 style='font-family:verdana; color:blue; font-size:200%;'>ENERGY COST</h1></p>"));
  webSite += (F("<form action='Sensor' method='post'>"));
  webSite += (F("<p>Cost per kWh: "));
  webSite += String(energyCost);
  webSite += (F("</p>"));
  webSite += (F("<p><input type='number' min='0.00' max='5.00' step='0.01' name='cost' placeholder='Cost per kWh' /></p>"));
  //////////////////////////
  webSite += (F("<p><h1 style='font-family:verdana; color:blue; font-size:200%;'>SENSOR CALIBRATION</h1></p>"));
  webSite += (F("<p>Ical: "));
  webSite += String(Ical);
  webSite += (F("</p>"));
  webSite += (F("<p><input type='number' min='0.00' max='200.00' step='0.01' name='cal' placeholder='Ical' /></p>"));
  ///////////////////////////
  webSite += (F("<p><h1 style='font-family:verdana; color:blue; font-size:200%;'>CHARGING STATION VOLTAGE</h1></p>"));
  webSite += (F("<p>Volts: "));
  webSite += String(mainsVoltage);
  webSite += (F("</p>"));
  webSite += (F("<p><input type='number' min='215' max='245' step='1' name='vol' placeholder='Volts' /></p>"));
  //////////////////////////
  webSite += (F("<br><input type='submit' value='Save' /></form>"));
  ///////////////////////////////////////////
  webSite += (F("<br><hr><hr>"));

  buildFooter();
  webSite += footer;

  server.send(200, "text/html",  webSite);
}
//////////////////
// WIFI SETTING //
////////////////////////////////////////////////////////////
void handleWIFISETTING() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  buildHeader();
  webSite = header;

  webSite += (F("<div class='topnav' id='myTopnav'>"));
  webSite += (F("<a href='/'>Exit</a>"));
  webSite += (F("<a href='/cal'>Sensor Calibration</a>"));
  webSite += (F("<a href='/wifisetting' class='active'>Wifi</a>"));
  webSite += (F("<a href='/blynk'>Blynk</a>"));
  webSite += (F("<a href='/reboot' onclick=\"return confirm('Are you sure ? ');\">Reboot</a>"));
  webSite += (F("<a href='/firmware'>Firmware Update</a>"));
  webSite += (F("<a href='javascript:void(0);' class='icon' onclick='myFunction()'>"));
  webSite += (F("<i class='fa fa-bars'></i></a>"));
  webSite += (F("</div>"));

  webSite += (F("<br><b><h1 style='font-family:verdana; color:white; font-size:400%; text-align:center;'>EV-Charger</font></h1></b><hr><hr>"));

  webSite += (F("<h1 style='font-family:verdana; color:blue;'><u>Wireless Network</u></h1>\n"));

  if (Setup == false) {
    if (WiFi.status() == WL_CONNECTED) {
      String IP = (WiFi.localIP().toString());
      webSite += (F("<p>Network Connected! to <mark>"));
      webSite += WiFi.SSID();
      webSite += (F("</mark></p>"));
      webSite += (F("<p>Ip: "));
      webSite += IP;
      webSite += (F("</p><p>"));
      webSite += (F("ev-charger.local"));
      webSite += (F("</p><hr>"));
    }
    else {
      webSite += (F("<p><font color=red>Network Not Connected!</font></p>"));
    }
  }
  else {
    webSite += (F("<hr><p><font color=blue><u>Wifi Scan</u></font></p>"));

    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks(false, true);

    // sort by RSSI
    int indices[n];
    for (int i = 0; i < n; i++) {
      indices[i] = i;
    }
    for (int i = 0; i < n; i++) {
      for (int j = i + 1; j < n; j++) {
        if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
          std::swap(indices[i], indices[j]);
        }
      }
    }

    String st = "";
    if (n > 5) n = 5;
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      st += "<small><li>";
      st += WiFi.RSSI(indices[i]);
      st += " dBm, ";
      st += WiFi.SSID(indices[i]);
      st += "</small></li>";
    }

    webSite += (F("<p>"));
    webSite += st;
    webSite += (F("</p>"));

    //// WiFi SSID & PASSWORD
    webSite += (F("<hr><hr><h1 style='font-family:verdana; color:blue;'><u>Wifi Ssid & Pass</u></h1>\n"));
    webSite += (F("<form method='get' action='WiFi'><label>SSID: </label><input name='ssid' type='text' maxlength=32><br><br><label>PASS: </label><input name='pass' type='password' maxlength=32><br><br><input type='submit'></form>"));

  }

  webSite += (F("<br><p><b>Reset:</b> V10 Blynk Button for erase ssid and password and active Setup Mode,</p>"));
  webSite += (F("<p>Controller reboot in Setup Mode, SSID ev-setup.</p>"));
  webSite += (F("<p>Ip: 192.168.4.1</p>"));
  webSite += (F("<hr><hr>"));

  buildFooter();
  webSite += footer;

  server.send(200, "text/html",  webSite);
}

//////////////////
// HANDLE BLYNK //
////////////////////////////////////////////////////////////
void handleBLYNK() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  buildHeader();
  webSite = header;

  webSite += (F("<div class='topnav' id='myTopnav'>"));
  webSite += (F("<a href='/'>Exit</a>"));
  webSite += (F("<a href='/cal'>Sensor Calibration</a>"));
  webSite += (F("<a href='/wifisetting'>Wifi</a>"));
  webSite += (F("<a href='/blynk' class='active'>Blynk</a>"));
  webSite += (F("<a href='/reboot' onclick=\"return confirm('Are you sure ? ');\">Reboot</a>"));
  webSite += (F("<a href='/firmware'>Firmware Update</a>"));
  webSite += (F("<a href='javascript:void(0);' class='icon' onclick='myFunction()'>"));
  webSite += (F("<i class='fa fa-bars'></i></a>"));
  webSite += (F("</div>"));

  webSite += (F("<br><b><h1 style='font-family:verdana; color:white; font-size:400%; text-align:center;'>EV-Charger</font></h1></b><hr><hr>"));

  // BLYNK CONFIG

  webSite += (F("<p><h1 style='font-family:verdana; color:blue; font-size:200%;'>Please Download Blynk APP.</h1></p>"));
  webSite += (F("<p><form method='post' action='Blynk'><label>AuthToken</label><br><br><input type='text' name='key' maxlength='32' value="));
  webSite += String(AuthToken);
  webSite += (F("><br><br><br><input type='submit' value='Save'></form></p>\n"));

  webSite += (F("<br><hr><p><h1 style='font-family:verdana; color:blue; font-size:200%;'>Blynk Server</h1></p>"));
  webSite += (F("<p><form method='post' action='BlynkServer'><label>Server</label><br><br><input type='text' name='server' maxlength='80' value="));

  if (BlynkServer.length() > 5) {
    webSite += String(BlynkServer);
  }
  else {
    webSite += (F("blynk-cloud.com"));
  }

  webSite += (F("><br><br><label>Port</label><br><br><input type='number' name='port' maxlength='4' value="));

  if (BlynkServer.length() > 5) {
    webSite += String(BlynkPort);
  }
  else {
    webSite += (F("8442"));
  }

  webSite += (F("><br><br><p><input type='submit' value='Save'></form></p>\n"));

  webSite += (F("<br><hr><hr>"));

  buildFooter();
  webSite += footer;

  server.send(200, "text/html",  webSite);
}

void handleOnConnect() {
  server.send(200, "text/html", SendHTML());
}

/////////////////////
// HANDLE NOTFOUND //
////////////////////////////////////////////////////////////
void handleNotFound() {
  server.sendHeader("Location", "/", true);  //Redirect to our html web page
  server.send(302, "text/plane", "");
}

String SendHTML() {

  int gaugePercent = map(rmsCurrent, 0, 100, 0, 180);
  if (gaugePercent > 180) {
    gaugePercent = 180;
  }

  String ptr = (F("<!DOCTYPE html>"));
  ptr += (F("<html lang='en'>"));

  ptr += (F("<head>"));
  //  <!-- Required meta tags -->
  ptr += (F("<title>EV-Charger</title>"));
  ptr += (F("<meta charset='utf-8'>"));

  ptr += (F("<meta name='viewport' content='width=device-width, initial-scale=1.0' shrink-to-fit=no'>"));
  ptr += (F("<meta name='description' content='EV-Charger'>"));
  ptr += (F("<meta name='author' content='Made by Real Drouin'>"));

  ptr += (F("<link href='https://fonts.googleapis.com/css?family=Open+Sans:300,400,600' rel='stylesheet'>"));
  ptr += (F("<style>"));
  ptr += (F("html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #444444;}"));
  ptr += (button);

  //////////// CSS GAUGE //////////
  ptr += (F("body { background-color:#555888; font-family:sans-serif; color:#fff; text-align:center }"));

  ptr += (F(".sc-gauge  { width:200px; height:100px; margin:50px auto; }"));
  ptr += (F(".sc-background { position:relative; height:100px; margin-bottom:10px; background-color:#fff; border-radius:150px 150px 0 0; overflow:hidden; text-align:center; }"));
  ptr += (F(".sc-mask { position:absolute; top:20px; right:20px; left:20px; height:80px; background-color:#555888; border-radius:150px 150px 0 0 }"));
  ptr += (F(".sc-percentage { position:absolute; top:100px; left:-200%; width:400%; height:400%; margin-left:100px; background-color:#00aeef; }"));
  ptr += (F(".sc-percentage { transform:rotate("));
  ptr += int(gaugePercent);
  ptr += (F("deg); transform-origin:top center; }"));


  ptr += (F(".sc-min { float:left; }"));
  ptr += (F(".sc-max { float:right; }"));
  ptr += (F(".sc-value { position:absolute; top:50%; left:0; width:100%;  font-size:48px; font-weight:700 }"));
  //////////////////////////////////////

  ptr += (F("</style>"));

  // AJAX script
  ptr += (F("<script>\n"));
  ptr += (F("setInterval(loadDoc,1000);\n")); // Update WebPage Every 1sec
  ptr += (F("function loadDoc() {\n"));
  ptr += (F("var xhttp = new XMLHttpRequest();\n"));
  ptr += (F("xhttp.onreadystatechange = function() {\n"));
  ptr += (F("if (this.readyState == 4 && this.status == 200) {\n"));
  ptr += (F("document.body.innerHTML =this.responseText}\n"));
  ptr += (F("};\n"));
  ptr += (F("xhttp.open(\"GET\", \"/\", true);\n"));
  ptr += (F("xhttp.send();\n"));
  ptr += (F("}\n"));
  ptr += (F("</script>\n"));
  ///////////////////////////////////////

  ptr += (F("</head>"));
  ptr += (F("<br><b><h1 style='font-family:verdana;color:white; font-size:400%; text-align: center;'>EV-Charger</font></h1></b>"));

  if (Connected2Blynk) {
    ptr += (F("<p><mark>Blynk Server Connected!</mark></p>"));
  }
  else {
    ptr += (F("<p><mark>Blynk Server Disconnected!</mark></p>"));
  }

  ptr += (F("<hr><hr>"));

  ptr += (F("</body>"));

  ptr += (F("<b><h1 style='font-family:verdana;color:blue; font-size:200%; text-align: center;'>YOUR CURRENT SESSION</font></h1></b>"));

  /////// GAUGE //////
  ptr += (F("<div class='sc-gauge'>"));
  ptr += (F("<div class='sc-background'>"));
  ptr += (F("<div class='sc-percentage'></div>"));
  ptr += (F("<div class='sc-mask'></div>"));
  ptr += (F("<span class='sc-value'>"));
  ptr += int(rmsCurrent);
  ptr += (F("</span>"));
  ptr += (F("</div>"));
  ptr += (F("<span class='sc-min'>0</span>"));
  ptr += (F("<span class='sc-max'>100A</span>"));
  ptr += (F("</div>"));
  ptr += (F("Charging rate<small> V0</small>"));
  ptr += (F("<hr>"));
  ptr += (F("<h1>"));
  ptr += String(kiloWattHours);
  ptr += (F("kWh</h1>"));
  ptr += (F("Energy charged<small> V1</small>"));
  ptr += (F("<hr>"));
  ptr += (F("<h1>"));
  ptr += String(sessionCost);
  ptr += (F("$</h1>"));
  ptr += (F("Money spent <small> V2</small>"));
  ptr += (F("<hr>"));
  ptr += (F("<h1>"));
  ptr += String(timeLive);
  ptr += (F("</h1>"));
  ptr += (F(" <small> V3</small>"));
  ptr += (F("<hr><hr>"));
  ptr += (F("<b><h1 style='font-family:verdana;color:blue; font-size:200%; text-align: center;'>ENERGY TOTAL</font></h1></b>"));
  ptr += (F("<h1>"));
  ptr += String(totalKiloWattHours);
  ptr += (F("kWh</h1>"));
  ptr += (F("ENERGY CONSUMPTION TOTAL<small> V4</small>"));
  ptr += (F("<h1>"));
  ptr += String(totalEnergyCost);
  ptr += (F("$</h1>"));
  ptr += (F("TOTAL MONEY SPENT<small> V5</small>"));
  ptr += (F("<hr><hr>"));
  ptr += (F("<br><p><a href ='/wifisetting' class='button'>Admin</a></p>"));

  //////////////////////////////////////////////////////////////////////

  ptr += (F("<hr><br><p><font color = 'blue'><i>Signal Strength: </i></font> "));
  ptr += String(percentQ);
  ptr += (F(" %</p>"));
  // System Uptime
  sprintf(timestring, "%d days %02d:%02d:%02d", dddd , hh, mi, ss);
  ptr += (F("<p>System Uptime: "));
  ptr += String(timestring);
  ptr += (F("</p>"));

  //////////////////////////////////////

  ptr += (F("<p><small>EV-Charger "));
  ptr += (ver);
  ptr += (F("</p><p><small>Made by Real Drouin ve2cuz@gmail.com</small></p>"));
  ptr += (F("</html>\n"));
  return ptr;
}

//////////////////
// BUILD HEADER //
////////////////////////////////////////////////////////////
void buildHeader() {

  header = "";
  header += (F("<!doctype html>\n"));
  header += (F("<html lang='en'>"));

  header += (F("<head>"));
  //  <!-- Required meta tags -->
  header += (F("<title>EV-Charger</title>"));
  header += (F("<meta charset='utf-8'>"));
  header += (F("<meta name='viewport' content='width=device-width, initial-scale=1.0' shrink-to-fit=no'>"));
  header += (F("<meta name='description' content='EV-Charger'>"));
  header += (F("<meta name='author' content='Made by Real Drouin'>"));
  header += (F("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css'>"));

  header += (F("<style>body { background-color:#555888; font-family:sans-serif; color:#fff; text-align:center }"));
  header += (F(".topnav {overflow: hidden;background-color: #333;}"));
  header += (F(".topnav a {float: left;display: block;color: #f2f2f2;text-align: center;padding: 14px 16px;text-decoration: none;font-size: 17px;}"));
  header += (F(".topnav a:hover {background-color: #1d1d21;color: white;}"));
  header += (F(".topnav a.active {background-color: #5d6c9c;color: white;}"));
  header += (F(".topnav .icon {display: none;}"));

  header += (F("@media screen and (max-width: 600px) {.topnav a:not(:first-child) {display: none;}.topnav a.icon {float: right;display: block;}}"));
  header += (F("@media screen and (max-width: 600px) {.topnav.responsive {position: relative;}.topnav.responsive .icon {position: absolute;right: 0;top: 0;}"));
  header += (F(".topnav.responsive a {float: none;display: block;text-align: left;}}"));

  header += String(button);
  header += String(form);
  header += (F("</style>\n"));

  header += (F("</head>\n"));
  header += (F("<body>\n"));
}

//////////////////
// BUILD FOOTER //
////////////////////////////////////////////////////////////
void buildFooter() {

  footer = (F("<br>"));
  footer += (F("<address> Contact: <a href='mailto:ve2cuz@gmail.com'>Real Drouin</a>"));
  footer += (F("</address>"));
  footer += (F("<p><small>Made by Real Drouin, EV-Charger "));
  footer += String(ver);
  footer += (F("</small>"));
  footer += (F("</footer>"));

  footer += (F("<script>function myFunction() {var x = document.getElementById('myTopnav');if (x.className === 'topnav') {x.className += ' responsive';} else {x.className = 'topnav';}}</script>"));

  footer += (F("</body>\n"));
  footer += (F("</html>\n"));
}

void BlynkBroadcast() {
  Blynk.virtualWrite(V0, rmsCurrent);
  Blynk.virtualWrite(V1, kiloWattHours);
  Blynk.virtualWrite(V2, sessionCost);
  Blynk.virtualWrite(V3, timeLive);
  Blynk.virtualWrite(V4, totalKiloWattHours);
  Blynk.virtualWrite(V5, totalEnergyCost);
}

void readSensor() {
  Status_LED_On;
  percentQ = 0;

  if (WiFi.RSSI() <= -100) {
    percentQ = 0;
  } else if (WiFi.RSSI() >= -50) {
    percentQ = 100;
  } else {
    percentQ = 2 * (WiFi.RSSI() + 100);
  }

  if (lastTimeMeasure == 0) {
    lastTimeMeasure = millis();
  }

  unsigned long now = millis();
  if (now - lastEMRead > 1000) {
    lastEMRead = now;
    rmsCurrent = emon.calcIrms(1480);  // Calculate Irms only


    if (rmsCurrent > 0.5) {
      rmsPower = rmsCurrent * mainsVoltage;  // Calculates RMS Power

      double watios = ((double)rmsPower * ((millis() - lastTimeMeasure) / 1000.0) / 3600.0); // Calculates kilowatt hours used since last reboot. 3600 = 60min*60sec / 1000.0 watios = kwh
      watiosTotal += watios;
      // watiosTotal += ((double)rmsPower * ((millis() - lastTimeMeasure) / 1000.0) / 3600.0); // Calculates kilowatt hours used since last reboot. 3600 = 60min*60sec / 1000.0 watios = kwh

      //////// LIVE CACULATE ////////
      kiloWattHours = watiosTotal / 1000.0;
      sessionCost = kiloWattHours * energyCost;

      //////// TOTAL CACULATE ////////
      totalKiloWattHours += watios / 1000.0;
      totalEnergyCost = totalKiloWattHours * energyCost;

      if (session == false) {
        session = true;
        kiloWattHours = 0;
        sessionCost = 0.00;
        startSession = millis();

        if (Connected2Blynk) {
          Blynk.notify("Charging Started!");
        }
      }

      unsigned long allSec = (millis() - startSession)  / 1000;
      int secsRemaining = allSec % 3600;
      int Hrs = allSec / 3600;
      int Min = secsRemaining / 60;
      int Sec = secsRemaining % 60;

      char buf[21];
      sprintf(buf, "Duration: %02dh:%02dm:%02ds", Hrs, Min, Sec);
      timeLive = buf;
    }
    else {
      if (session == true) {
        session = false;

        String reports = "Energy: ";
        reports += String(kiloWattHours);
        reports += "kWh, Cost: ";
        reports += String(sessionCost);
        reports += "$, ";
        reports += String(timeLive);

        if (Connected2Blynk) {
          Blynk.notify("Charging Session Reports: " + String(reports));
          Blynk.email("Charging Session Reports: ", reports);
        }
      }

      timeLive = "Stanby!";
      watiosTotal = 0;
      rmsPower = 0;
    }

    //lastTimeMeasure = millis();
  }

  if (Connected2Blynk) {
    BlynkBroadcast();
  }

  if (session == true) {
    Serial.print(" [LIVE] Energy charged: ");
    Serial.print(kiloWattHours);
    Serial.print("kWh ");
    Serial.print(" Money spent: ");
    Serial.print(sessionCost);
    Serial.println("$ ");
    Serial.print(" [TOTAL] Energy charged: ");
    Serial.print(totalKiloWattHours);
    Serial.print("kWh ");
    Serial.print(" Money spent: ");
    Serial.print(totalEnergyCost);
    Serial.println("$ ");
    Serial.println("");
  }
  else {
    Serial.println(" [Stanby] ");
    Serial.print(" [TOTAL] Energy charged: ");
    Serial.print(totalKiloWattHours);
    Serial.print("kWh ");
    Serial.print(" Money spent: ");
    Serial.print(totalEnergyCost);
    Serial.println("$ ");
    Serial.println("");
  }

  Status_LED_Off;
  lastTimeMeasure = millis();
}

