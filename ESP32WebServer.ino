#include "WiFi.h"
#include "WiFiClient.h"
#include "WebServer.h"
#include "ESPmDNS.h"
#include "ArduinoJson.h"

typedef struct SSIDListCount {
  char** connectionName;
  int numberOfConnections;
} SSIDList;

const byte interruptPin = 27;
const byte relayPin = 25;

TaskHandle_t wifiListHandle;
static uint8_t taskCoreZero = 0;
static uint8_t taskCoreOne  = 1;

int connection_status;
char buff [20];
String new_ssid;
String new_password;
SSIDList* mainWiFiList;

int WIFICONNECTED = 0;
String global_light_status;
const String light_off = "off";
const String light_on = "on";

WebServer server(80);
 
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

String prepareConnectionPage () {
  //Builds WiFi connections list when client requests the page
  //printSSIDList(wifiList);
  int i = 0;
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
  //-----------------------------------------------------------------------------------------------------------//
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
    delay(1000);
    ESP.restart();
  }
  
  else{
    WIFICONNECTED = 1;
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    //Passes handle to function that deletes task
    vTaskDelete(wifiListHandle);

    if (MDNS.begin("esp32")) {
      Serial.println("MDNS responder started");
    }
    else {
      return 0; 
    }
    return 1;
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
  if (!WIFICONNECTED) {
    char ssid_buff [60];
    char pswd_buff [60];
    String new_ssid = String (server.arg(0));
    new_ssid.toCharArray(ssid_buff, 60);
    String new_password = String (server.arg(1));
    new_password.toCharArray(pswd_buff, 60);
  
    //Freeing them mallocs because if it fails we will reload WiFi list
    int i = 0;
    for (i=0; i<mainWiFiList->numberOfConnections; ++i){
      free(mainWiFiList->connectionName[i]);
    }
    free(mainWiFiList->connectionName);

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
    }
  }
  else {
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

 
void setup () {
  Serial.begin(115200);
  global_light_status = "off";
  Serial.println(global_light_status);
  boolean result = WiFi.softAP("ESPsoft", "maniot@winet");
  if(result == true) {
    Serial.println("Ready");
  }
  else {
    Serial.println("Failed!");
  }

  pinMode(interruptPin, INPUT);
  pinMode(relayPin, OUTPUT);
  
  server.on("/", handleRoot);
  server.on("/connection", handleCreatingNewConnection);
  server.on("/commands", handleCommands);
  server.on("/values", handleValues);
  server.onNotFound(handleNotFound);
  server.begin();

  xTaskCreatePinnedToCore(
                          createWiFiList,   /* função que implementa a tarefa */
                          "WiFi List",      /* nome da tarefa */
                          10000,            /* número de palavras a serem alocadas para uso com a pilha da tarefa */
                          NULL,             /* parâmetro de entrada para a tarefa (pode ser NULL) */
                          2,                /* prioridade da tarefa (0 a N) */
                          &wifiListHandle,  /* referência para a tarefa (pode ser NULL) */
                          taskCoreZero);    /* Núcleo que executará a tarefa */
  delay(500); //tempo para a tarefa iniciar
                          

  xTaskCreatePinnedToCore(
                          handleLightSwitch, /* função que implementa a tarefa */
                          "Light Switch",    /* nome da tarefa */
                          10000,             /* número de palavras a serem alocadas para uso com a pilha da tarefa */
                          NULL,              /* parâmetro de entrada para a tarefa (pode ser NULL) */
                          1,                 /* prioridade da tarefa (0 a N) */
                          NULL,              /* referência para a tarefa (pode ser NULL) */
                          taskCoreZero);     /* Núcleo que executará a tarefa */

  delay(500); //tempo para a tarefa iniciar

  xTaskCreatePinnedToCore(
                          webServiceRun,    /* função que implementa a tarefa */
                          "Web Service",    /* nome da tarefa */
                          10000,            /* número de palavras a serem alocadas para uso com a pilha da tarefa */
                          NULL,             /* parâmetro de entrada para a tarefa (pode ser NULL) */
                          3,                /* prioridade da tarefa (0 a N) */
                          NULL,             /* referência para a tarefa (pode ser NULL) */
                          taskCoreOne);     /* Núcleo que executará a tarefa */
  delay(500); //tempo para a tarefa iniciar
}

void loop () {
  
}

void webServiceRun (void* pvParameters) {
  Serial.println("Starting WebService.");
  while (true) {
    server.handleClient(); 
  }
}

void createWiFiList (void* pvParameters) {
  while (true){
    Serial.println("Generating WiFi list...");
    mainWiFiList = availableSSIDs();
    Serial.println("WiFi list finished.");
    vTaskDelay(10000);
  }
}
 
void handleLightSwitch (void* pvParameters) {
  Serial.println("Starting Switch.");
  while(true) {
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
      vTaskDelay(500);
    }
    else {
      vTaskDelay(10);
    }
  }
}
