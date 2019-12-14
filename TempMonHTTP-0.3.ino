// ------------------------------------------------------------------------------------------------------------------------------------------------
// Project:   TempMonHTTP
// ------------------------------------------------------------------------------------------------------------------------------------------------
// Version:   0.3
// Date:      12 December 2019
// Author:    Greg Howell <gjhmac@gmail.com>
// ------------------------------------------------------------------------------------------------------------------------------------------------
// Version    Date              Comments
// ------------------------------------------------------------------------------------------------------------------------------------------------
// 0.1        13 January 2019   Initial Development
// 0.2        03 August 2019    Updated
// 0.3        12 December 2019  Added 5 min averaging for previous hour, modified HTML to display averages
// ------------------------------------------------------------------------------------------------------------------------------------------------
// Description:
// ------------------------------------------------------------------------------------------------------------------------------------------------
//  - Read temperature and humidty from a DHT22 sensor and serves it as part of a web page
//  - Builds with Arduino 1.8.10
// ------------------------------------------------------------------------------------------------------------------------------------------------

#include "DHT.h"        // DHT humidity/temperature sensors
#include <SPI.h>        // SPI for Ethernet
#include <Ethernet.h>   // Ethernet

#define DHTPIN 2        // what pin the DHT sensor is connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
// 5 minutes in milliseconds
#define FIVEMIN (1000UL * 60 * 5)
unsigned long rolltime = millis() + FIVEMIN;

float temperatures[12]; // array to store 12 temperature readings (~ 1 hour of readings)
float humidities[12];   // array to store 12 humidity readings (~ 1 hour of readings)
bool validreadings[12]; // array to track whether the readings in the arrays are valid (for statistics etc.)
int array_index;        // index to start of arrays (essentially being used as circular buffers)

float avgtemperature;   // average temperature
float avghumidity;      // average humidity

DHT dht(DHTPIN, DHTTYPE);

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

byte gateway[] = { 192, 168, 0, 1 };
byte subnet[] = { 255, 255, 255, 0 };

IPAddress ip(192, 168, 0, 110);   // IP address
EthernetServer server(80);        // Server Port

// ------------------------------------------------------------------------------------------------------------------------------------------------
// setup()
// ------------------------------------------------------------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  dht.begin();                                // start the DHT22 sensor
  Ethernet.begin(mac, ip, gateway, subnet);   // start the Ethernet connection
  server.begin();                             // start the HTTP server
  Serial.print("TempMonHTTP 0.3 is at ");
  Serial.println(Ethernet.localIP());

  // initialise the arrays
  for (int i=0;i<12;i++) {
    temperatures[i] = 0.0;
    humidities[i] = 0.0;
    validreadings[i] = false;
  }
  // initialise
  array_index = 0;
  avgtemperature = 0.0;
  avghumidity = 0.0;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------
// loop()
// ------------------------------------------------------------------------------------------------------------------------------------------------
void loop() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  bool dhtSuccess = false;

  // used to calculate averages
  int denom = 0;
  float numtemperature = 0.0;
  float numhumidity = 0.0;

  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h)) {
    Serial.println("Failed to read from DHT");
  } else {
    dhtSuccess = true;
  }

  // store readings every 5 minutes
  if((long)(millis() - rolltime) >= 0) {
    // write data the arrays
    if(dhtSuccess){
      temperatures[array_index] = t;
      humidities[array_index] = h;
      validreadings[array_index] = true;
    }
    else{
      temperatures[array_index] = 0.0;
      humidities[array_index] = 0.0;
      validreadings[array_index] = false;      
    }
    // increment or roll over the array index
    if(array_index > 10){
      array_index = 0;
    }
    else{
      array_index = array_index + 1;
    }
    rolltime += FIVEMIN;
    
    // calculate averages
    for (int i=0;i<12;i++) {
      if (validreadings[i]) {
        denom++;
        numtemperature = numtemperature + temperatures[i];
        numhumidity = numhumidity + humidities[i];
      }
    }
    // check for zero valid readings...
    if (denom != 0){
      avgtemperature = numtemperature / denom;
      avghumidity = numhumidity / denom;
    }
  }
  
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 30");        // refresh the page automatically every 30 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<head>");
          client.println("<title>TempMonHTTP v0.3</title>");
          client.println("<style>");
          client.println("table, th, td {border: 1px solid black;border-collapse: collapse;}");
          client.println("th, td {padding: 5px;}");
          client.println("p {font-family: courier;font-size: 150%;}");
          client.println("p1 {font-family: courier;font-size: 100%;color:CornflowerBlue}");
          client.println("</style>");
          client.println("</head>");
          client.println("<body>");

          if (dhtSuccess){
            client.print("<table><tr><td>");
            client.print("<p>Temperature:</p></td><td><p>Current: ");
            client.print(t);
            client.print(" &deg;C</p><p1>Average Last Hour: ");
            client.print(avgtemperature);
            client.print(" &deg;C</p1></td></tr><tr><td><p>Humidity:</p></td><td><p>Current: ");
            client.print(h);
            client.print(" %\t</p><p1>Average Last Hour: ");
            client.print(avghumidity);           
            client.print(" %\t</p1></td></tr></table>");
          }
          else {
            client.println("<p>Data not available</p>");
          }
          
          client.println("</body>");
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}
