#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>
#include <ESP8266HTTPClient.h>
#include <HX711.h>
#include "ZonkyScaleProperties.h"

/* PINs of the indication LED diodes */
#define RED_LED D8
#define YELLOW_LED D7
#define BLUE_LED D6
#define GREEN_LED D5

#define SCALE_DOUT_PIN D1
#define SCALE_SCK_PIN D2

float calibration_factor = 683350;
float offset = 226219; // tare of empty coffee jug

HX711 scale(SCALE_DOUT_PIN, SCALE_SCK_PIN);

float tolerance = 0.005; // 5 grams
float lastWeight = -100000;
long lastWeightTime = -1;
boolean saturated = false;

void setup() {
  Serial.begin(9600);
  
  if (! connectWifi()) {
      Serial.println("Wifi connection error. Going to sleep.");
      
      signalErrorBlick();
  } else {
      Serial.println("Wifi connection OK.");
  }

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  

  scale.set_scale(calibration_factor);
  scale.set_offset(offset); //Reset the scale to 0
 
  long zero_factor = scale.read_average(); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);

  lastWeight = scale.get_units();
  lastWeightTime = millis();
  saturated = false;

}

void loop() {
  checkWeightLoop();
  checkNoJugLoop();
}


unsigned long lastWeightCheck = millis();
void checkWeightLoop() {
  if (millis() - lastWeightCheck < 250) {
    return;
  }
  lastWeightCheck = millis();
  
  float weight = scale.get_units();
  Serial.println(String(weight, 3));

  float diff = weight - lastWeight;
  if (diff < 0) {
    diff *= -1;
  }
  
  if (diff <= tolerance) {
    // same weight

    if (saturated == false && millis() - lastWeightTime > 2000) {
      // same and saturated
            
      saturated = true;
      Serial.println("Weight is saturated.");
      
      signalWeight(lastWeight);
    }
    
  } else {
    // different weight
    lastWeight = weight;
    lastWeightTime = millis();
    saturated = false;

    Serial.println("Not saturated.");
  }
}

unsigned long lastNoJugCheck = millis();
unsigned long lastNoJugBlickTime = millis();
bool on = false;
void checkNoJugLoop() {
  if (millis() - lastNoJugCheck < 1000) {
    return;
  }
  lastNoJugCheck = millis();

  if (saturated && lastWeight < -0.150 && millis() - lastNoJugBlickTime > 1000) {
    on = !on;
    digitalWrite(RED_LED, on ? HIGH : LOW);
  }
}

void signalWeight(float weight) {
  if (weight < -0.150) { // less than -150g - jug is not placed
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
  } else if (weight < 0.060) { // less than 60g (60ml)
    digitalWrite(RED_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
  } else if (weight < 0.300) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
  } else if (weight < 0.800) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(BLUE_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
  } else {
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
  }

  // Send to influx
  if (weight > -0.150) {
    HTTPClient http;
    http.begin(influxUrl);
    http.setAuthorization(influxUser, influxPass);
    int httpCode = http.POST((String)"zonky-coffee-scale weight=" + weight);

    Serial.print("Influx DB result: ");
    Serial.println(httpCode);

    if (httpCode < 0) {
      signalErrorBlick();
    } else {
      String payload = http.getString();   //Get the request response payload
      Serial.println(payload); 
    }
  }
}

/**
 * Connect the Wifi
 */
bool connectWifi() {
    int connWait = 15; //seconds
    int delayMs = 50; //ms
    int ticksWait = connWait * 1000 / delayMs;
    
    Serial.println("\nChecking the wifi.");

    if (WiFi.SSID() != ssid) {
      Serial.println("\nConnecting the wifi.");
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      
      WiFi.begin(ssid, ssidPass);
    }
    
    while (ticksWait-- > 0 && WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      Serial.print(WiFi.status());
      
      digitalWrite(LED_BUILTIN, LOW);
      delay(5);
      digitalWrite(LED_BUILTIN, HIGH);
      
      delay(delayMs);
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi not connected");
      
      return false;
    }
    
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("mac address: ");
    Serial.println(WiFi.macAddress());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("GW address: ");
    Serial.println(WiFi.gatewayIP().toString());

    String macStr = WiFi.BSSIDstr();
    Serial.print("APMAC: ");
    Serial.println(macStr);
    
    
    return true;
}


/**
 * In case of an error, blink with the RED diode. SOS Morse Code
 */
void signalErrorBlick() {
  // S morse code
  
  for (int i = 0; i < 3; i++) {
    blick(RED_LED, 50);
    delay(100);
  }

  delay(100);

  // O morse code
  for (int i = 0; i < 3; i++) {
    blick(RED_LED, 150);
    delay(100);
  }

  delay(100);

  // S morse code
  for (int i = 0; i < 3; i++) {
    blick(RED_LED, 50);
    delay(100);
  }
}

void blick(int pin, int onTime) {
  digitalWrite(pin, HIGH);
  delay(onTime);
  digitalWrite(pin, LOW);
}
