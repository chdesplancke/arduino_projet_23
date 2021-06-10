#include <Wire.h>
#include <WiFi.h>
#include <string.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include "DFRobot_SHT20.h"
#include "Adafruit_TSL2591.h"

#define RED 5
#define WHITE 4

WiFiClient client;
DFRobot_SHT20 sht20;
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
StaticJsonDocument<200> doc; // Allocation en espace mémoire (200 octets) pour gérer les JSON

byte mac[6];
char* ssid = "";
char* password = "";
char* authKey = "e7e09c4a8c99af1af25682de6633628956118d93a8ce81012fb973d6fc1f9749";

void setup(){
    Serial.println("Initialize System");
    pinMode (RED, OUTPUT);
    pinMode (WHITE, OUTPUT);
    Serial.begin(115200);

    setupSensors(); // On initialise les capteurs

    Serial.print("Connecting to "); Serial.println(ssid);
    WiFi.begin(ssid, password); //On se connecte au réseau wifi
    while (WiFi.status() != WL_CONNECTED) { //Tant que non connecté au wifi
        delay(500);
    }

    Serial.println("ESP32Wifi initialized");
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    WiFi.macAddress(mac); //On stocke l'adresse mac dans la chaîne
    blinks(RED);
    connectSocket();
}

void loop() {
    while(client.connected()){ //Tant que la connexion au socket est toujours active
        if(client.available()){ // client.available() renvoi le nombre byte "en attente" que le serveur a envoyé à l'ESP32. Si client.available renvoi un nombre différent de 0, des données sont prêtes à être reçu.
            readSocket();
        }
    }
    Serial.println("Disconnected");
    blinks(RED);
    delay(1000);
    connectSocket(); // On tente de se reconnecter au socket
}

/*
 * Fonction permettant d'initialiser un socket TCP vers l'IP et le port.
 *
 * Dés que celui-ci est connecté, on envoie un JSON contenant une clé d'identification
 * et l'adresse MAC de l'ESP32 pour se connecter.
 */
void connectSocket(){
    String json;

    Serial.println("Trying to connect to server...");
    if (client.connect("141.94.21.229", 52275)) {
        Serial.println("Connected");
        WiFi.macAddress(mac);

        doc["authKey"] = authKey;
        doc["macaddr"] = getMacAddress(mac);
        serializeJson(doc, json);
        client.write(json.c_str());
        doc.clear();
        blinks(WHITE);
    }
}

/*
 * Fonction permettant de lire chaque byte recu via le socket TCP et de convertir en chaine de caractères interprétable.
 *
 * Le serveur envoi toujours un JSON avec le même squelette, on extrait alors les informations simplement en variables.
 */
void readSocket(){
    int dataLength = client.available(); //On stocke le nombre de byte reçu par le serveur
    String json, message = "";

    for(int i = 0; i < dataLength; i++){
        message.concat((char)client.read()); //On récupére un byte qu'on cast en char pour le stocker dans un String
    }

    DeserializationError error = deserializeJson(doc, message.c_str()); // On déserialize le JSON
    if (!error) { //S'il y a une erreur, on ne fait rien
        int sender = doc["sender"];
        const char* request_type = doc["request"]["type"];
        const char* request_content = doc["request"]["content"];
        doc.clear();

        if(strcmp(request_type,"GET") == 0 && strcmp(request_content,"all") == 0){ // Requête de récupération des informations (GET) de tout les capteurs (all)
            doc["receiver"] = sender;
            doc["macaddr"] = getMacAddress(mac);
            JsonObject dfrobot_sht20 = doc.createNestedObject("dfrobot_sht20");
            dfrobot_sht20["hum"] = sht20.readHumidity();
            dfrobot_sht20["temp"] = sht20.readTemperature();
            doc["dfrobot_sen0308"]["hum"] = analogRead(34);
            doc["adafruit_tsl2591"]["light"] = tsl.getLuminosity(TSL2591_VISIBLE);

            serializeJson(doc, json); // On génére le JSON
            client.write(json.c_str()); //On envoie le JSON via le socket TCP
            doc.clear();
            blinks(WHITE);
        }
    }else{ //On affiche l'erreur
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
}

/*
 * Fonction permettant de faire clignotter la led rouge ou blanche
 */
void blinks(int color){
    digitalWrite (color, HIGH);
    delay(200);
    digitalWrite (color, LOW);
}

/*
 * Fonction permettant d'intialiser les capteurs
 */
void setupSensors(){
    Serial.println("Starting DFRobot SSHT20 configuration...");
    sht20.initSHT20();
    delay(100);
    sht20.checkSHT20();

    Serial.println("Starting Adafruit TSL2591 configuration...");
    if (tsl.begin()){
        Serial.println("TSL2591 sensor found!");
        tsl.setGain(TSL2591_GAIN_MED);
        tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
    }else {
        Serial.println("TSL2591 sensor not found.");
    }
    blinks(RED);
}

/*
 * Fonction permettant de convertir un tableau de byte (contenant l'adresse MAC) et de renvoyer un String.
 */
String getMacAddress(byte mac[6]){
    String macString = "";
    macString.concat(String(mac[5],HEX));
    macString.concat(":");
    macString.concat(String(mac[4],HEX));
    macString.concat(":");
    macString.concat(String(mac[3],HEX));
    macString.concat(":");
    macString.concat(String(mac[2],HEX));
    macString.concat(":");
    macString.concat(String(mac[1],HEX));
    macString.concat(":");
    macString.concat(String(mac[0],HEX));

    return macString;
}