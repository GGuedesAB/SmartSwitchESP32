#include "WiFi.h"
#include "WiFiClient.h"
#include "WebServer.h"
#include "ESPmDNS.h"
#include "ArduinoJson.h"
#include "EEPROM.h"

//To do stuff:
//    **Use pass by reference!
//    (Maybe) Create credentials page before WiFi connection page
//    Save credentials encrypted in Flash memory
//    (Maybe) Figure out how to define first connection

typedef struct SSIDListCount {
  char** connectionName;
  int numberOfConnections;
} SSIDList;

const byte interruptPin = 27;
const byte relayPin = 25;

int connection_status;
char buff [20];
String new_ssid;
String new_password;
SSIDList* mainWiFiList;

int WIFICONNECTED = 0;
int CREDENTIALS_PRESENT = 0;
String global_light_status;
const String light_off = "off";
const String light_on = "on";

WebServer server(80);
 
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

String prepareConnectionPage () {
  //Builds WiFi connections list when client requests the page
  mainWiFiList = availableSSIDs();
  //printSSIDList(wifiList);
  int i = 0;
  int j = 0;
  String wifiOptions;
  String temp;
  //Builds the drop down list with all the WiFi connections found
  for (i=0; i<mainWiFiList->numberOfConnections; ++i){
    temp = String(mainWiFiList->connectionName[i]);
    wifiOptions+= "<option value= \"" +temp+ "\">" +temp+ "</option>";
  }
  //-----------------------------------------------------------------------------------------------------------//
  String htmlPage = 
  String("")+
  "<html>"+
  "<head>"+
  "<title>ESP32 WiFi setup</title>"+
  "<meta charset = \"UTF-8\" />"+
  "</head>"+
  "<body>"+
    "<h1>Connect to your host AP!</h1>"+
      "<fieldset>"+
        "<legend>Choose the WiFi you want to connect:</legend>"+
          "<p>"+
            "<label>Select from the list below:</label>"+
            "<form action='/connection' method='post'>"+
              "<select name= \"wifiList\">"+
                wifiOptions+ 
              "</select>"+
              +"<p>   </p>"+
              "<p>Host Password:</p>" +
              "<input type='password' name='hostPSW'><br><br>"+
              "<input type='submit' value='Connect'>"+
            "</form>"+
          "</p>"+
        "</fieldset>"+
  "</body>"+
  "</html>";
  //-----------------------------------------------------------------------------------------------------------//

  for (i=0; i<mainWiFiList->numberOfConnections; i++) {
    free(mainWiFiList->connectionName[i]);
  }
  free(mainWiFiList);
  return htmlPage;
}

String prepareTryingPage (char* ssid_trial) {
  //-----------------------------------------------------------------------------------------------------------//
  String htmlPage = 
  String("")+
  "<html>"+
  "<head>"+
  "<title>Trying connection</title>"+
  "<meta charset = \"UTF-8\" />"+
  "</head>"+
  "<body>"+
    "<h1>ESP32 is trying to connect to " + String(ssid_trial) +"</h1>"+
      "<fieldset>"+
        "<legend>Please wait.</legend>"+
        "<p>If ESP32 does not connect, reconnect to ESPSoft and retry.</p>"
        "</fieldset>"+
  "</body>"+
  "</html>";
  //-----------------------------------------------------------------------------------------------------------//
  return htmlPage;
}

String prepareNotFound () {
  String htmlPage;
  //-----------------------------------------------------------------------------------------------------------//
  if (!WIFICONNECTED) {
    htmlPage = 
    String("")+
    "<html>"+
    "<head>"+
    "<title>Not found</title>"+
    "<meta charset = \"UTF-8\" />"+
    "</head>"+
    "<body>"+
      "<h1>404 Page not found!</h1>"+
        "<fieldset>"+
          "Please go to <a href='http://192.168.4.1'>connection setup</a> page."+
          "</fieldset>"+
    "</body>"+
    "</html>";
  }
  else {
    htmlPage = 
    String("")+
    "<html>"+
    "<head>"+
    "<title>Not found</title>"+
    "<meta charset = \"UTF-8\" />"+
    "</head>"+
    "<body>"+
      "<h1>404 Page not found!</h1>"+
        "<fieldset>"+
          "Please enter a valid URL."+
          "</fieldset>"+
    "</body>"+
    "</html>";
  }
  //-----------------------------------------------------------------------------------------------------------//
  return htmlPage;
}

String prepareTest () {
  //-----------------------------------------------------------------------------------------------------------//
  String htmlPage = 
  String("")+
  "<html>"+
  "<head>"+
  "<title>I'm connected!</title>"+
  "<meta charset = \"UTF-8\" />"+
  "</head>"+
  "<body>"+
    "<h1>I got connected!</h1>"+
      "<fieldset>"+
        "Show some respect."+
        "</fieldset>"+
  "</body>"+
  "</html>";
  //-----------------------------------------------------------------------------------------------------------//
  return htmlPage;
}

String prepareValues () {
  String htmlPage;
  StaticJsonBuffer<300> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();
  if (global_light_status == "on"){
    JSONencoder["light_status"] = "on";
  }
  else if (global_light_status == "off") {
    JSONencoder["light_status"] = "off";
  }
  else {
    emergencyFixLight();
    JSONencoder["light_status"] = "off";
  }
  //-----------------------------------------------------------------------------------------------------------//
  JSONencoder.printTo(htmlPage);
  //------Serial.println("");-----------------------------------------------------------------------------------------------------//
  return htmlPage;
}

String lightError () {
  //-----------------------------------------------------------------------------------------------------------//
  String htmlPage = "Wrong command format.";
  //-----------------------------------------------------------------------------------------------------------//
  return htmlPage;
}

String lightConfirmation () {
  //-----------------------------------------------------------------------------------------------------------//
  String htmlPage = "Command received.";
  //-----------------------------------------------------------------------------------------------------------//
  return htmlPage;
}

void sendLightError () {
  char htmlChar [1000];
  String htmlToSend = lightError();
  htmlToSend.toCharArray(htmlChar, 1000);
  server.send(200, "text/html", htmlChar);
}

void sendLightConfirmation () {
  char htmlChar [1000];
  String htmlToSend = lightConfirmation();
  htmlToSend.toCharArray(htmlChar, 1000);
  server.send(200, "text/html", htmlChar);
}

SSIDList* availableSSIDs () {
  int i = 0;
  SSIDList* list = (SSIDList*) malloc (sizeof(SSIDList));
  list->numberOfConnections = WiFi.scanNetworks();
  list->connectionName = (char**) malloc (list->numberOfConnections*sizeof(char*));
  if (list->connectionName == NULL) {
    Serial.println("Not enough memory to mount available WiFi list. (1)");
    exit(1);
  }
  for (i=0; i<list->numberOfConnections; ++i){
    list->connectionName[i] = (char*) malloc (20*sizeof(char));
    if (list->connectionName[i] == NULL) {
      Serial.println("Not enough memory to mount available WiFi list. (2)");
      exit(1);
    }
    String temp = WiFi.SSID(i);
    temp.toCharArray(list->connectionName[i], 20);
  }
  return list;
}

int createNewConnection (char* ssid, char* password) {
  int i = 0;
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Could not connect to this AP, try another one.");
    //Invalidate credentials in Flash
    EEPROM.write(0,0);
    EEPROM.commit();
    return 0;
  }
  
  else{
    if (!CREDENTIALS_PRESENT) {
      String flash_ssid = String(ssid);
      String flash_pswd = String(password);
      EEPROM.write(0,1);
      EEPROM.writeString(1,flash_ssid);
      EEPROM.writeString(61,flash_pswd);
      EEPROM.commit();
    }
    WIFICONNECTED = 1;
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp32")) {
      Serial.println("MDNS responder started");
      return 1;
    }
    else {
      return 0; 
    }
  }
}

void handleRoot () {
  if (!WIFICONNECTED) {
    char htmlChar [3000];
    String htmlToSend = prepareConnectionPage();
    htmlToSend.toCharArray(htmlChar, 3000);
    server.send(200, "text/html", htmlChar); 
  }
  else {
    handleNotFound();
  }
}

void handleCreatingNewConnection () { 
  char ssid_buff [60];
  char pswd_buff [60];
  if (!WIFICONNECTED) {
    if (!CREDENTIALS_PRESENT){
      String new_ssid = String (server.arg(0));
      String new_pswd = String (server.arg(1));
      new_ssid.toCharArray(ssid_buff, 60);
      new_pswd.toCharArray(pswd_buff, 60);
      Serial.print("Connecting to: ");
      Serial.println(new_ssid);
      Serial.print("With password: ");
      Serial.println(new_pswd);
      char htmlChar [2000];
      String htmlToSend = prepareTryingPage(ssid_buff);
      htmlToSend.toCharArray(htmlChar, 2000);
      server.send(200, "text/html", htmlChar);
      delay(2000);
      if (createNewConnection(ssid_buff, pswd_buff)){
        Serial.println("Connection successful.");
        WiFi.softAPdisconnect (true);
      }
      else {
        Serial.println("Connection failed, something went wrong.");
        delay(1000);
        ESP.restart();
      }
    }
    else {
      String flash_ssid = EEPROM.readString(1);
      String flash_pswd = EEPROM.readString(61);
      flash_ssid.toCharArray(ssid_buff, 60);
      flash_pswd.toCharArray(pswd_buff, 60);
      Serial.print("Connecting to: ");
      Serial.println(ssid_buff);
      Serial.print("With password: ");
      Serial.println(pswd_buff);
      if (createNewConnection(ssid_buff, pswd_buff)){
        Serial.println("Connection successful.");
      }
      else {
        Serial.println("Connection failed, something went wrong.");
        delay(1000);
        ESP.restart();
      }
    }
  }
  else {
    //Because ESP32 is already connected somewhere else
    handleNotFound();
  }
}

void handleCommands () {
  if (!WIFICONNECTED) {
    handleNotFound();    
  }
  else {
    StaticJsonBuffer<300> JSONbuffer;
    JsonObject& JSONencoder = JSONbuffer.parseObject(server.arg(0));
    if (!JSONencoder.success()){
      Serial.println("Could not parse JSON, either it is not a JSON or something went wrong internally.");
      sendLightError();  
    }
    else {
      String light_status = String(JSONencoder["light_status"].as<char*>());
      if (light_status.equals("on")) {
        if (global_light_status == "on"){
          Serial.println("Lights are already on.");
          sendLightConfirmation();
        }
        else{
          //Turn lights on
          digitalWrite(relayPin, HIGH);
          global_light_status = "on";
          Serial.println("Turning lights on.");
          sendLightConfirmation();
        }
      }
      else if (light_status.equals("off")) {
        if (global_light_status == "off"){
          Serial.println("Lights are already off.");
          sendLightConfirmation();
        }
        else {
          digitalWrite(relayPin, LOW);
          global_light_status = "off";
          Serial.println("Turning lights off.");
          sendLightConfirmation(); 
        }
      }
      else {
        Serial.println("Could not recognize command, keeping devices as they are.");
        sendLightError();
      }
    }
  }
}

void handleValues () {
  if (WIFICONNECTED) {
    char htmlChar [1000];
    String htmlToSend = prepareValues();
    htmlToSend.toCharArray(htmlChar, 1000);
    server.send(200, "text/html", htmlChar); 
  }
  else {
    handleNotFound();
  }
}

void handleNotFound () {
  char htmlChar [1000];
  String htmlToSend = prepareNotFound();
  htmlToSend.toCharArray(htmlChar, 1000);
  server.send(200, "text/html", htmlChar);
}

void printSSIDList (SSIDList* toBePrinted) {
  int i = 0;
  Serial.println("-------------------------Print func called----------------------");
  Serial.println(toBePrinted->numberOfConnections);
  for (i=0; i<toBePrinted->numberOfConnections; ++i){
    Serial.println(toBePrinted->connectionName[i]);
  }
}

void emergencyFixLight () {
  Serial.println("Critical error, light control variable is undefined.");
  global_light_status = "off";
  digitalWrite(relayPin, LOW);
}

void setup() {
  WIFICONNECTED = 0;
  EEPROM.begin(512);
  CREDENTIALS_PRESENT = EEPROM.read(0);
  Serial.begin(115200);
  global_light_status = "off";
  Serial.println(global_light_status);
  //Turn off the lights
  delay(200);
  if (!CREDENTIALS_PRESENT){
    boolean result = WiFi.softAP("ESPsoft2", "maniot@winet");
    if(result == true) {
      Serial.println("Ready");
    }
    else {
      Serial.println("Failed!");
    }
  }
  else {
    Serial.println("Found credentials in flash. Trying connection.");
    handleCreatingNewConnection();
  }

  pinMode(interruptPin, INPUT);
  pinMode(relayPin, OUTPUT);
  
  server.on("/", handleRoot);
  server.on("/connection", handleCreatingNewConnection);
  server.on("/commands", handleCommands);
  server.on("/values", handleValues);
  server.onNotFound(handleNotFound);
  server.begin();
}
 
void loop() {
  server.handleClient();
  if(digitalRead(interruptPin) == HIGH) {
    Serial.println("There was an interruption.");
    if (global_light_status == "on") {
      global_light_status = "off";
      digitalWrite(relayPin, LOW);
    }
    else if (global_light_status == "off") {
      global_light_status = "on";
      digitalWrite(relayPin, HIGH);
    }
    else {
      emergencyFixLight();
    }
    delay(200);
  }
}
