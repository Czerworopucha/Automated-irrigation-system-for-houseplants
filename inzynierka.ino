#include <WiFiS3.h>
#include "wifi_pass.h"
//#include "FspTimer.h"

//380-390 = 100% moisture
//1018 = 0% moisture
//val = ((% * (max - min) / 100) + min
//30%
const uint16_t minMoisture = 578;

//1.2L/minute
//100ml
const unsigned long autoWateringTime = 60000/(1200/100);

uint8_t flowers = 0; //maximum of 6
bool apOff = 0;
bool autonomous = 0;

//uint16_t moistSensors[6] = {0, 0, 0, 0, 0, 0};
static const char sensorPins[6] = {A0,A1,A2,A3,A4,A5};

const long autoInterval = 30*60*1000; //0.5 hour

unsigned long prevMillis[6] = {0, 0, 0, 0, 0, 0};
unsigned long intervals[6] = {0, 0, 0, 0, 0, 0};

//int waterVolume[6] = {0, 0, 0, 0, 0, 0};
unsigned long controlledWateringTime[6] = {0, 0, 0, 0, 0, 0};

typedef void(*modeFunctionPointer)();
//static modeFunctionPointer modeFunctions[2] = {controlledMode, autonomousMode};

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int status = WL_IDLE_STATUS;
WiFiServer server(80);

void autonomousMode(){
  unsigned long currentMillis = millis();
  if (currentMillis - prevMillis[0] >= autoInterval){
    prevMillis[0] = currentMillis;
    getSensorData();
  }
}

void getSensorData(){
  for(uint8_t i = 0; i < flowers; i++){
    if(analogRead(sensorPins[i]) < minMoisture){
      digitalWrite(i+2, LOW);
      delay(autoWateringTime);
      digitalWrite(i+2, HIGH);
    }
  }
}

void controlledMode(){
  unsigned long currentMillis = millis();
  for(int i = 0; i < flowers; i++){
    if(currentMillis - prevMillis[i] >= intervals[i]){
      prevMillis[i] = currentMillis;
      digitalWrite(i+2, LOW);
      delay(controlledWateringTime[i]);
      digitalWrite(i+2, HIGH);
    }
  }
}

static modeFunctionPointer modeFunctions[2] = {controlledMode, autonomousMode};

void apWebsite(int *waterVolume, int *intervalsHours){
  WiFiClient client = server.available();  // listen for incoming clients
  if (client) {                    // if you get a client,
    Serial.println("new client");  // print a message out the serial port
    String currentLine = "";       // make a String to hold incoming data from the client
    while (client.connected()) {   // loop while the client's connected
      delayMicroseconds(10);       // This is required for the Arduino Nano RP2040 Connect - otherwise it will loop so fast that SPI will never be served.
      if (client.available()) {    // if there's bytes to read from the client,
        char c = client.read();    // read a byte, then
        Serial.write(c);           // print it out to the serial monitor
        if (c == '\n') {           // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("<html>");
            client.print("<head>");
            client.print("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.print("<title>Formularz</title>");
            client.print("<script>");
            client.print("function collectAndSetLink() {");
            client.print("    var autoMode = document.getElementById('autoMode').checked;");
            client.print("    var plantCount = document.getElementById('plantCount').value;");
            client.print("    var data = 'autoMode=' + autoMode + '&plantCount=' + plantCount;");

            for (int i = 1; i <= 6; i++) {
              client.print("    var interval" + String(i) + " = document.getElementById('interval" + String(i) + "').value;");
              client.print("    var water" + String(i) + " = document.getElementById('water" + String(i) + "').value;");
              client.print("    data += '&interval" + String(i) + "=' + interval" + String(i) + " + '&water" + String(i) + "=' + water" + String(i) + ";");
            }

            client.print("    window.location.href = '/submitData?' + data;");
            client.print("}");
            client.print("window.onload = function() {");
            client.print("    var inputs = document.querySelectorAll('input[type=number], select, input[type=checkbox]');");
            client.print("    inputs.forEach(function(input) {");
            client.print("        var savedValue = localStorage.getItem(input.id);");
            client.print("        if (savedValue !== null) {");
            client.print("            if (input.type === 'checkbox') {");
            client.print("                input.checked = (savedValue === 'true');");
            client.print("            } else {");
            client.print("                input.value = savedValue;");
            client.print("            }");
            client.print("        } else if (input.type === 'number') {");
            client.print("            input.value = 0;");
            client.print("        }");
            client.print("    });");
            client.print("};");
            client.print("</script>");
            client.print("</head>");
            client.print("<body>");

            client.print("<p style=\"font-size:1vw;\"><input type=\"checkbox\" id=\"autoMode\" onclick=\"localStorage.setItem('autoMode', this.checked)\"> Tryb automatyczny</p><br>");

            client.print("<p style=\"font-size:1vw;\">Ilosc roslin:</p>");
            client.print("<select id=\"plantCount\" style=\"font-size:1vw; width: 20vw;\" onchange=\"localStorage.setItem('plantCount', this.value)\">");
            for (int i = 1; i <= 6; i++) {
              client.print("<option value=\"" + String(i) + "\">" + String(i) + "</option>");
            }
            client.print("</select>");
            client.print("<span style=\"font-size:1vw;\"> maksymalna ilosc 6</span><br><br>");

            for (int i = 1; i <= 6; i++) {
              client.print("<p style=\"font-size:1vw;\">Numer " + String(i) + ":</p>");
              client.print("<label for=\"interval" + String(i) + "\" style=\"font-size:1vw;\">Interwal:</label><br>");
              client.print("<input type=\"number\" id=\"interval" + String(i) + "\" style=\"font-size:1vw; width: 20vw;\" value=\"0\" onchange=\"localStorage.setItem('interval" + String(i) + "', this.value)\"><br>");
              client.print("<label for=\"water" + String(i) + "\" style=\"font-size:1vw;\">Woda [ml]:</label><br>");
              client.print("<input type=\"number\" id=\"water" + String(i) + "\" style=\"font-size:1vw; width: 20vw;\" value=\"0\" onchange=\"localStorage.setItem('water" + String(i) + "', this.value)\"><br><br>");
            }

            client.print("<button style=\"font-size:1vw;\" onclick=\"collectAndSetLink()\">Wyslij wszystkie dane</button><br><br>");
            client.print("<a href=\"/exit\"><button style=\"font-size:1vw;\">Wyjdz</button></a>");

            client.print("</body>");
            client.print("</html>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } 
          else {  // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check the client data:
        if (currentLine.substring(0,16) == "GET /submitData?") {
          int ampersand = currentLine.indexOf('&');
          int equalsSign = currentLine.indexOf('=');

          //tryb automatyczny
          if (currentLine.substring(equalsSign + 1, ampersand) == "true") {
            autonomous = 1;
            ampersand = currentLine.indexOf('&', ampersand + 1);
            equalsSign = currentLine.indexOf('=', equalsSign + 1);
            flowers = (currentLine.substring(equalsSign + 1, ampersand)).toInt();

          } else if (currentLine.substring(equalsSign + 1, ampersand) == "false") {
            autonomous = 0;

            //ilosc roslin
            ampersand = currentLine.indexOf('&', ampersand + 1);
            equalsSign = currentLine.indexOf('=', equalsSign + 1);
            flowers = (currentLine.substring(equalsSign + 1, ampersand)).toInt();

            //interwaly i ilosc wody
            for (int i = 0; i < 6; i++) {
              ampersand = currentLine.indexOf('&', ampersand + 1);
              equalsSign = currentLine.indexOf('=', equalsSign + 1);
              intervalsHours[i] = (currentLine.substring(equalsSign + 1, ampersand)).toInt();

              ampersand = currentLine.indexOf('&', ampersand + 1);
              equalsSign = currentLine.indexOf('=', equalsSign + 1);
              waterVolume[i] = (currentLine.substring(equalsSign + 1, ampersand)).toInt();
            }
          }
        }
        if (currentLine.startsWith("GET /exit")) {
          apOff = 1;
        }
        
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

void pinInit(){
  //Initialize all possibly needed pins
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  //Set digital pins to high, so that the pumps don't run - low level trigger relays
  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);
  digitalWrite(7, HIGH);
}

void setup() {
  int waterVolume[6] = {0, 0, 0, 0, 0, 0}; //needed only in setup scope
  int intervalsHours[6] = {0, 0, 0, 0, 0, 0}; //needed only in setup scope

  //Serial Init
  Serial.begin(9600);
  pinInit();
  delay(100);

  if (WiFi.status() == WL_NO_MODULE) {
    while (true)
      ;
  }

  //default ip address for esp - 192.168.4.1
  WiFi.config(IPAddress(192, 168, 4, 1));

  Serial.print("Creating access point named: ");
  Serial.println(ssid);

  status = WiFi.beginAP(ssid, pass);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    while (true)
      ;
  }

  delay(10000);
  server.begin();

  //AccessPoint setup, get the needed data to run as configured
  while(!apOff){
    apWebsite(waterVolume, intervalsHours);
  }

  Serial.println("turning off ap");
  delay(5000);
  server.end();


  // Serial.println(apOff);
  // Serial.println(flowers);
  // Serial.println(autonomous);
  // for(int i = 0; i < 6; i++){
  //   Serial.print(i);
  //   Serial.print(".woda: ");
  //   Serial.println(waterVolume[i]);
  //   Serial.print(i);
  //   Serial.print(".interwal: ");
  //   Serial.println(intervalsHours[i]);
  // }

  if(!autonomous){
    for(int i = 0; i < flowers; i++){
      //calculate watering times for controlledMode
      controlledWateringTime[i] = 60000/(1200/waterVolume[i]);
      //calculate watering intervals
      intervals[i] = intervalsHours[i] *60*60*1000;
    }
  }

  delay(5000);
}

void loop() {
  modeFunctions[autonomous]();
}
