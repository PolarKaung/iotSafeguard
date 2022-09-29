#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Arduino_JSON.h>
#include <ArduinoJson.h>
#include <Wire.h>

///////////////////////////// Credentials
char* ap_name     = "ESP8266";
char* ap_pass     = "12345678";

char read_api_key[] = "GLI0P25NWBMETU3V";
char write_api_key[] = "EX03OH5GJT7BY02J";

char * user_read_api_key;
char * user_write_api_key;

char username[20] = "AustonMMCampus";
char password[20] = "helloyangon";

#define relay D7



/////////////////////////////////////////////////////////////////

///////////////////////////////////// Data
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

char serverName[]  = "api.thingspeak.com";
static const char PROGMEM channelID[]   = "1854352";
const char * userChannelID;

enum PARAMETERS {
  VOLTAGE = 1, CURRENT
};

char parameter_list[5][10] = {"field1", "field2"};

char str_buffer[25];

static const char PROGMEM GET_REQUEST[] = "GET ";

uint16_t under_v;
uint16_t over_v;
uint16_t over_c;

const uint16_t under_v_h = 180;
const uint16_t over_v_h = 260;

double voltage;
double current;

long upload_delay = 0;

long debounce_timing = 2000;
long prev_debounce_time = 0;

////////////////////////////////////////////////////////////////////

////////////////////////////////////// Flags
bool SET_ROUTER   = false;
bool SET_SETUP    = false;
bool relay_state = false;
/////////////////////////////////////////////////////

ESP8266WebServer server(80);
WiFiClient client;

void Connect_to_WiFi(const char * _username, const char * _password) {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("Connected to "));
    Serial.println(username);
    return;
  }
  Serial.print(F("Connecting to "));
  Serial.println(_username);
  WiFi.begin(_username, _password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println("");
  Serial.println(F("WiFi connected"));
  Serial.println(WiFi.localIP());
}

void Setup_Server_for_Credentials(char * _ap_name, char * _ap_pass) {
  Serial.print(F("Setting AP ...."));
  WiFi.softAP(_ap_name, _ap_pass);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  IPAddress IP = WiFi.softAPIP();
  Serial.print(F("AP IP address: "));
  Serial.println(IP);
  delay(100);
  server.on(F("/"), handle_OnConnect);
  server.on(F("/config"), handle_OnConfig);
  server.begin();
  pinMode(relay, OUTPUT);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);
  Setup_Server_for_Credentials(ap_name, ap_pass);
}

void loop() {
  server.handleClient();
  //  long value = analogRead();
  request_voltage();
  request_current();

  if (relay_state) {
    voltage = voltage * 1.095;
  } else {
    
//    if (millis() - prev_debounce_time > debounce_timing) {
//      voltage = voltage * 0.90272;
//    } else {
//      voltage = voltage * 1.124;
//    }
    voltage = voltage;
    
  }
  
  if (SET_ROUTER) {
    Connect_to_WiFi(username, password);

    if (millis() - upload_delay > 20000) {
      if (WiFi.status() == WL_CONNECTED) {
        send_Data(voltage, current);
        upload_delay = millis();
      }
    }
  }
  Serial.print(F("Voltage: "));
  Serial.print(voltage);
  Serial.print(F("\tCurrent: "));
  Serial.print(current);
  if (over_v >= over_v_h) {
    over_v = over_v_h;
  }
  if (under_v <= under_v_h) {
    under_v = under_v_h;
  }
  if (under_v > over_v) {
    under_v = under_v_h;
    over_v = over_v_h;
  }
  if (voltage >= over_v || voltage <= under_v) {
    Serial.println("\tRelay OFF");
    prev_debounce_time = millis();
    digitalWrite(relay, LOW);
    relay_state = false;
  }
  else {
    Serial.println("\tRelay ON");
    digitalWrite(relay, HIGH);
    relay_state = true;
  }
//    Serial.print(F("Undervoltage: "));
//    Serial.println(under_v);
//    Serial.print(F("Overvoltage: "));
//    Serial.println(over_v);

  Serial.println();
  Serial.println(F("============================================================="));
}

uint16_t get_Parameter(uint8_t _parameter, char * _response) {
  String getStr = F("http://");
  memcpy_P(&str_buffer, &serverName, sizeof(serverName));
  getStr += str_buffer;
  getStr += F("/channels/");
  memcpy_P(&str_buffer, &channelID, sizeof(channelID));
  getStr += str_buffer;
  getStr += F("/fields/");
  getStr += _parameter;
  getStr += F(".json?api_key=");
  memcpy_P(&str_buffer, &read_api_key, sizeof(read_api_key));
  getStr += str_buffer;
  getStr += "&results=1";

  if (client.connect(serverName, 80)) {
    //    Serial.println(getStr);
    memcpy_P(&str_buffer, &GET_REQUEST, sizeof(GET_REQUEST));
    client.print(str_buffer + getStr + "\n");
    client.print(F("HTTP/1.1\n"));
    client.print(F("Host: api.thingspeak.com\n"));
    client.print(F("Connection: close\n\n\n"));
    while (client.available() == 0);
    uint16_t i = 0;
    while (client.available()) {
      _response[i++] = client.read(); \
    }
    _response[i] = 0;
  }
  client.stop();
  //  Serial.println(_response);

  const int capacity =  2 * JSON_OBJECT_SIZE(8);
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, _response);
  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.f_str());
  }
  strcpy(str_buffer, parameter_list[_parameter - 1]);

  return atoi(doc["feeds"][0][str_buffer]);
}

void get_Data() {
  char response[500];
  Serial.print(F("Voltage: "));
  Serial.println(get_Parameter(VOLTAGE, response));
  Serial.print(F("Current: "));
  Serial.println(get_Parameter(CURRENT, response));

}

void send_Data(uint16_t _voltage, uint16_t _current) {
  if (client.connect(serverName, 80)) {
    Serial.println(F("Connected"));
    String getStr = "/update?api_key=";
    getStr += write_api_key;
    getStr += "&field1=";
    getStr += _voltage;
    getStr += "&field2=";
    getStr += _current;
    client.print(String("GET ") + getStr + String("\n"));
    client.print("HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n\n\n");
  }
  client.stop();
}

void handle_OnConnect() {
  Serial.println("Connected..");
  if (!SET_ROUTER) {
    server.on("/add", handle_get_credentials);
    server.send(200, F("text/html"), F("<html><head><title>WiFi Configuration</title></head><body><form action=\"/add\" method=\"POST\"><label for=\"fname\">SSID:</label><br><input type=\"text\" id=\"fname\" name=\"fname\"><br><label for=\"lname\">Password:</label><br><input type=\"text\" id=\"lname\" name=\"lname\"><input type=\"submit\" value=\"Submit\"></form></body></html>"));
  } else {
    server.send(200, F("text/html"), F("<html><head><title>WiFi Configuration</title></head><body>Press Reset Button To Reconfigure Your WiFi Credentials</body></html>"));
  }
}

void handle_get_credentials() {
  if (!SET_ROUTER) {
    Serial.println(F("Connected to get data..."));
    strcpy(username, server.arg(0).c_str());
    strcpy(password, server.arg(1).c_str());
    Serial.print(F("Username: "));
    Serial.println(username);
    Serial.print(F("Password: "));
    Serial.println(password);
    server.send(200, F("text/html"), F("<html><head></head><body><form action=\"/add\" method=\"POST\"><label for=\"fname\">First name:</label><br><input type=\"text\" id=\"fname\" name=\"fname\"><br><label for=\"lname\">Last name:</label><br><input type=\"text\" id=\"lname\" name=\"lname\"><input type=\"submit\" value=\"Submit\"></form></body></html>"));
    delay(500);
    SET_ROUTER = true;
  } else {
    server.send(200, F("text/html"), F("<html><head></head><body>Page Doesn't Exit</body></html>"));
  }

}

//void handle_notfound() {
//  server.send(200, F("text/html"), F("<html><head></head><body>Not Found</body></html>"));
//}

void handle_OnConfig() {
  Serial.println(F("Connected to config"));
  if (server.args() == 0) {
    server.send(200, F("text/html"), String("<html><head><title>Safegaurd Configuration</title></head><body><h1>Voltage: ") + voltage + String("</h1><br><form action=\"/config\" method=\"POST\"><label for=\"under\">Under-Voltage(Hardware Limit: ") + under_v_h + String("):</label><br><input type=\"text\" id=\"under\" name=\"underv\"><br><label for=\"over\">Over-Voltage(Hardware Limit: ") + over_v_h + String("):</label><br><input type=\"text\" id=\"over\" name=\"overv\"><input type=\"submit\" value=\"Submit\"></form></body></html>"));
  } else {
    char over[10];
    char under[10];
    if (strlen(server.arg(0).c_str()) < 10 && atoi(server.arg(0).c_str()) >= under_v_h) {
      strcpy(under, server.arg(0).c_str());
    } else {
      server.send(200, F("text/html"), F("<html><head><title>Error</title></head><body>Over Voltage Too Long or Violate Hardware Limit</body></html>"));
      return;
    }
    if (strlen(server.arg(1).c_str()) < 10 && atoi(server.arg(1).c_str()) <= over_v_h) {
      strcpy(over, server.arg(1).c_str());
    } else {
      server.send(200, F("text/html"), F("<html><head><title>Error</title></head><body>Under Voltage Too Long or Violates Hardware Limit</body></html>"));
      return;
    }
    if (atoi(under) > atoi(over)) {
      server.send(200, F("text/html"), F("<html><head><title>Error</title></head><body>Under Voltage Larger than Over Voltage</body></html>"));
      return;
    }
    under_v = atoi(under);
    over_v = atoi(over);
    server.send(200, F("text/html"), String("<html><head><title>Safe Guard Configuration</title></head><body><h1>Voltage: ") + voltage + String("</h1><br>Current Configuration <br> Under-Voltage: ") + String(under_v) + String("<br> Over-Voltage: ") + String(over_v) + String("<br> Set New Configuration <br> <form action=\"/config\" method=\"POST\"><label for=\"Under-Voltage\">Under-Voltage:</label><br><input type=\"text\" id=\"Under-Voltage\" name=\"underVoltage\"><br><label for=\"Over-Volgage\">Over-Voltage:</label><br><input type=\"text\" id=\"Over-Voltage\" name=\"overVoltage\"><input type=\"submit\" value=\"Submit\"></form></body></html>"));
  }


}


void request_voltage() {
  Wire.beginTransmission(8);
  Wire.write("voltage");
  Wire.endTransmission();
  delay(50);
  Wire.requestFrom(8, 20);
  char buffer1[20];
  uint8_t i = 0;
  char c;
  while (Wire.available()) {
    c = Wire.read();
    if ( (c >= 0x30 && c <= 0x7a) || c == 0x20 || c == 0x2E) {
      buffer1[i++] = c;
    }
  }
  buffer1[i] = 0;
  voltage = atof(buffer1);
}


void request_current() {
  Wire.beginTransmission(8);
  Wire.write("current");
  Wire.endTransmission();
  delay(50);
  Wire.requestFrom(8, 20);
  char buffer1[20];
  uint8_t i = 0;
  char c;
  while (Wire.available()) {
    c = Wire.read();
    if ( (c >= 0x30 && c <= 0x7a) || c == 0x20 || c == 0x2E) {
      buffer1[i++] = c;
    }
  }
  buffer1[i] = 0;
  current = atof(buffer1);
}

