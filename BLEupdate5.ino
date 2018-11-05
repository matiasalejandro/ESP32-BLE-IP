/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/
#include <BLEDevice.h>
#include <BLEServer.h>

#include <BLEUtils.h>
#include <BLE2902.h>

//#include <WiFiClient.h>
#include <ESP32WebServer.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <EEPROM.h>

#include <WebSocketsServer.h>
//#include <Arduino.h>





ESP32WebServer server(80);
ESP32WebServer server1(80);
WebSocketsServer webSocket = WebSocketsServer(81);
// Client variables 
char linebuf[80];
int charcount=0;
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
String ssid="";
String password="";
String ssid3="";
String password3 ="";
String muestraIP="";
String muestraIP2="";
String descon="";
String date ="";
char ssid4[50];      
char pass4[50];
String update1 ="";
String mensaje = "";
int contconexion = 0;
bool user = false;
bool pass = false;
String newValue ="";
const int led1 = 16;      // the number of the LED pin
//const int led2 =  17;      // the number of the LED pin



#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

const char* serverIndex = "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'>"
"</form>"
"<div id='prg'>progress: 0%</div>"
"<script>"
"$('form').submit(function(e){"
    "e.preventDefault();"
      "var form = $('#upload_form')[0];"
      "var data = new FormData(form);"
      " $.ajax({"
            "url: '/update',"
            "type: 'POST',"               
            "data: data,"
            "contentType: false,"                  
            "processData:false,"  
            "xhr: function() {"
                "var xhr = new window.XMLHttpRequest();"
                "xhr.upload.addEventListener('progress', function(evt) {"
                    "if (evt.lengthComputable) {"
                        "var per = evt.loaded / evt.total;"
                        "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
                    "}"
               "}, false);"
               "return xhr;"
            "},"                                
            "success:function(d, s) {"    
                "console.log('success!')"
           "},"
            "error: function (a, b, c) {"
            "}"
          "});"
"});"
"</script>";


String pagina ="<html>"
"<head>"
"<script>"
"var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);"
"connection.onopen = function ()       { connection.send('Connect ' + new Date()); };"
"connection.onerror = function (error) { console.log('WebSocket Error ', error);};"
"connection.onmessage = function (e)   { console.log('Server: ', e.data);};"
"function sendRGB() {"
" var r = parseInt(document.getElementById('r').value).toString(16);"
" var g = parseInt(document.getElementById('g').value).toString(16);"
" var b = parseInt(document.getElementById('b').value).toString(16);"
" if(r.length < 2) { r = '0' + r; }"
" if(g.length < 2) { g = '0' + g; }"
" if(b.length < 2) { b = '0' + b; }"
" var rgb = '#'+r+g+b;"
" console.log('RGB: ' + rgb);"
" connection.send(rgb);"
"}"
"</script>"
"</head>"
"<body>"
"LED Control:<br/><br/>"
"R: <input id='r' type='range' min='0' max='255' step='1' value='0' oninput='sendRGB();'/><br/>"
"G: <input id='g' type='range' min='0' max='255' step='1' value='0' oninput='sendRGB();'/><br/>"
"B: <input id='b' type='range' min='0' max='255' step='1' value='0'oninput='sendRGB();'/><br/>"
"</body>"
"</html>";




void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send message to client
            webSocket.sendTXT(num, "Connected");
        }
            break;
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);

            if(payload[0] == '#') {
                // we get RGB data

                // decode rgb data
                uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);

                ledcWrite(0 ,    abs(255 - (rgb >> 16) & 0xFF) );
                //ledcWrite(LED_GREEN,  abs(255 - (rgb >>  8) & 0xFF) );
               // ledcWrite(LED_BLUE,   abs(255 - (rgb >>  0) & 0xFF) );
            }
            break;
    }
}





class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
     std::string rxValue = pCharacteristic->getValue();


      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        Serial.println(rxValue.c_str());
        Serial.println("*********");



                if(user) {
                 ssid=(rxValue.c_str());
                // Serial.println("LED 1 ON");
                  user = false;      
                 }

               if(rxValue=="ssid") {
                // Serial.println("LED 1 ON");
                 newValue ="ssid";
                 user =true;
                  }
                     

                  
                if (pass){
                 password=(rxValue.c_str());
                // Serial.println("LED 1 ON");
                 pass=false;
                }   
                
               if (rxValue=="pass") {
                //Serial.println("LED 1 OFF");
                newValue="password";
                pass=true;
                }

                    
               if (rxValue=="update") {
             //   Serial.println("update");
                date="update";
               }
               if (rxValue=="showIP") {
             //   Serial.println("update");
                date="showIP";
               }

                   if(rxValue=="conec") {
                // Serial.println("LED 1 ON");
                   date="5";

                  }

 }  

 
   }
};

//----------------Función para grabar en la EEPROM-------------------
void grabar(int addr, String a) {
  int tamano = a.length(); 
  char inchar[50]; 
  a.toCharArray(inchar, tamano+1);
  for (int i = 0; i < tamano; i++) {
    EEPROM.write(addr+i, inchar[i]);
  }
  for (int i = tamano; i < 50; i++) {
    EEPROM.write(addr+i, 255);
  }
  EEPROM.commit();
}

//-----------------Función para leer la EEPROM------------------------
String leer(int addr) {
   byte lectura;
   String strlectura;
   for (int i = addr; i < addr+50; i++) {
      lectura = EEPROM.read(i);
      if (lectura != 255) {
        strlectura += (char)lectura;
      }
   }
   return strlectura;
}



void setup() {
   
   EEPROM.begin(512);

   leer(0).toCharArray(ssid4, 50);
   leer(50).toCharArray(pass4, 50);
   update1=leer(100);
  Serial.begin(115200);
  




       if (update1=="") {    
          WiFi.begin(ssid4,pass4);
             while (WiFi.status() != WL_CONNECTED and contconexion <10) { //Cuenta hasta 50 si no se puede conectar lo cancela
           ++contconexion;
            delay(250);
           Serial.print(".");

            delay(250);

            }
            if (contconexion <10) {   
               Serial.println("");
            Serial.println("WiFi conectado");
            Serial.println(WiFi.localIP());
             muestraIP=WiFi.localIP().toString();
             muestraIP2=muestraIP;
             
               }  
 
            else { 
            Serial.println("");
            descon="25";
            Serial.println("Error de conexion");
           } 



                 if (!MDNS.begin("esp32")) {
                  Serial.println("Error setting up MDNS responder!");
                  while(1) {
                  delay(1000);
                        }     
                     }

 
                  // start webSocket server
               webSocket.begin();
               webSocket.onEvent(webSocketEvent);
                 // handle index
                    server.on("/", []() {
                   server.send(200, "text/html", pagina);
                        });
                   server.begin();


                
                Serial.println("HTTP server started");
                MDNS.addService("http", "tcp", 80);
                 MDNS.addService("ws", "tcp", 81);

                  ledcSetup(0,5000,8);
                  ledcAttachPin(led1,0);  
        }    

  // Create the BLE Device
  BLEDevice::init("UART Service");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

 
}



void loop() {

    if (deviceConnected) {


        if (ssid.length()>1){
       //  pTxCharacteristic->setValue(ssid.c_str());
       //  pTxCharacteristic->notify();
       //   delay(15); // bluetooth stack will go into congestion, if too many packets are sent
             Serial.println(ssid.c_str());
             ssid3 =ssid;
             ssid ="";
             grabar(0,ssid3);
             
  
         }

          if (password.length()>1){
            //  pTxCharacteristic->setValue(password.c_str());
           //   pTxCharacteristic->notify();
            //  delay(15); // bluetooth stack will go into congestion, if too many packets are sent
             Serial.println(password.c_str());
              password3 =password;
              password="";
              grabar(50,password3);
  /*            
              mensaje = "Configuracion Guardada...";
              const char* ssid1=&ssid3[0];
              const char* password1=&password3[0];
            
              contconexion = 0;
              WiFi.begin(ssid1,password1);
              
            while (WiFi.status() != WL_CONNECTED and contconexion <10) { //Cuenta hasta 50 si no se puede conectar lo cancela
           ++contconexion;
            delay(250);
           Serial.print(".");
            delay(250);

            }
            if (contconexion <10) {   
               Serial.println("");
            Serial.println("WiFi conectado");
            Serial.println(WiFi.localIP());
             muestraIP=WiFi.localIP().toString();
             muestraIP2=muestraIP;
             
               }  
 
            else { 
            Serial.println("");
            descon="25";
            Serial.println("Error de conexion");
           }
      */
       }




              if (date=="update") {

                 grabar(100,date);
                 date="Next boot update";
                 pTxCharacteristic->setValue(date.c_str());
                 pTxCharacteristic->notify();
                 delay(15); // bluetooth stack will go into congestion, if too many packets are sent
                 Serial.println("Next boot update");
                 date="";
                }
                     
              if (date=="showIP") {

                    if (muestraIP2.length()>1){
                    pTxCharacteristic->setValue(muestraIP2.c_str());
                    pTxCharacteristic->notify();
                    delay(15); // bluetooth stack will go into congestion, if too many packets are sent
                    date="";
                   }
                }

                    if (date=="5") {


                    if (descon=="25") {
                    pTxCharacteristic->setValue(descon.c_str());
                    pTxCharacteristic->notify();
                    delay(15); // bluetooth stack will go into congestion, if too many packets are sent
                    Serial.println(descon.c_str());
                    descon="";
                   }

                      
                    if (muestraIP2.length()>1){
                    pTxCharacteristic->setValue(date.c_str());
                    pTxCharacteristic->notify();
                    delay(15); // bluetooth stack will go into congestion, if too many packets are sent
                   
                   }
                    date="";
                  }
           
        if (newValue.length()>1){
         pTxCharacteristic->setValue(newValue.c_str());
         pTxCharacteristic->notify();
         delay(15); // bluetooth stack will go into congestion, if too many packets are sent
         Serial.println(newValue.c_str());
         newValue ="";
         }



 
   }
     
         
              if (update1=="update") { 
              contconexion = 0; 
              WiFi.begin(ssid4,pass4);           
             while (WiFi.status() != WL_CONNECTED and contconexion <10) { //Cuenta hasta 50 si no se puede conectar lo cancela
            ++contconexion;
            delay(250);
            Serial.print(".");
            delay(250);

            }
            if (contconexion <10) {   
               Serial.println("");
            Serial.println("WiFi conectado");
            Serial.println(WiFi.localIP());
             muestraIP=WiFi.localIP().toString();
             muestraIP2=muestraIP;
             
               }  
 
            else { 
            Serial.println("");
            descon="25";
            Serial.println("Error de conexion");
           }
           
              //pTxCharacteristic->setValue(update1.c_str());
             // pTxCharacteristic->notify();
             // delay(15); // bluetooth stack will go into congestion, if too many packets are sent
              server1.begin();
          
             

              server1.on("/", HTTP_GET, [](){
              server1.sendHeader("Connection", "close");
              server1.send(200, "text/html", serverIndex);
              });
                /*handling uploading firmware file */
                server1.on("/update", HTTP_POST, [](){
                server1.sendHeader("Connection", "close");
                server1.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
         
                },[](){
                HTTPUpload& upload = server1.upload();
                if(upload.status == UPLOAD_FILE_START){
                 Serial.printf("Update: %s\n", upload.filename.c_str());
                 if(!Update.begin(UPDATE_SIZE_UNKNOWN)){//start with max available size
               Update.printError(Serial);
               }
                 } else if(upload.status == UPLOAD_FILE_WRITE){
                /* flashing firmware to ESP*/
                if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
               Update.printError(Serial);
                }
                } else if(upload.status == UPLOAD_FILE_END){
                if(Update.end(true)){ //true to set the size to the current progress
               Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
               } else {
                Update.printError(Serial);
               }
               }
            });
              
             update1 ="";
             grabar(100,update1);
           }
    
     // disconnecting
        if (!deviceConnected && oldDeviceConnected) {
             delay(500); // give the bluetooth stack the chance to get things ready
             pServer->startAdvertising(); // restart advertising
              Serial.println("start advertising");
              oldDeviceConnected = deviceConnected;
           }

         

    // connecting
    if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
server1.handleClient();
delay(1);
     if (update1=="") {
      webSocket.loop();
       }


server.handleClient();
delay(1);

}

