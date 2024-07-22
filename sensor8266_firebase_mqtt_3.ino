#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>

// Definições do WiFi
#define WIFI_SSID "wifi-zone-ufam-1"
#define WIFI_PASSWORD ""

// Definições do Firebase
#define FIREBASE_HOST "temperature-9ab56-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyBu_fHooG-QFKhNhpqUCIxQVCDBGaqabSs"

// Definição do pino do sensor DHT e tipo de sensor
#define DHTPIN D4 // Pino onde o sensor está conectado no NodeMCU (GPIO2)
#define DHTTYPE DHT11 // Defina como DHT11 se estiver usando um DHT11

// Definições do MQTT
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_topic = "esp8266/sensor1";
const char* mqtt_topic_temp = "esp8266/sensor1/temperatura";
const char* mqtt_topic_hum = "esp8266/sensor1/umidade";

WiFiClient espClient2;
PubSubClient client(espClient2);

DHT dht(DHTPIN, DHTTYPE);
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -14400, 60000); // Configuração para o fuso horário de Manaus (UTC -4)

String mqtt_client_id;

void setup() {
  Serial.begin(115200);
  setup_wifi();

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  dht.begin();

  timeClient.begin(); // Inicializa o cliente NTP

  mqtt_client_id = "esp8266_" + WiFi.macAddress();
  client.setServer(mqtt_server, 1883);
  connectToMqtt();
}

void setup_wifi() {
  delay(100);
  Serial.print("Conectando ao WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Conectado ao WiFi");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    if (client.connect(mqtt_client_id.c_str())) {
      Serial.println("connected");
      client.subscribe(mqtt_topic_temp);
      client.subscribe(mqtt_topic_hum);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void connectToMqtt() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");

    if (client.connect(mqtt_client_id.c_str())) {
      Serial.println("conectado");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

void publishMQTT(const char* topic, const char* payload) {
  if (client.publish(topic, payload)) {
    Serial.print("Mensagem enviada ao tópico ");
    Serial.println(topic);
  } else {
    Serial.print("Falha ao enviar mensagem ao tópico ");
    Serial.println(topic);
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("Falha ao ler do sensor DHT!");
    return;
  }

  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();

  Serial.print("Hora: ");
  Serial.print(formattedTime);
  Serial.print("  Temperatura: ");
  Serial.print(temperatura);
  Serial.print(" °C");
  Serial.print("  Umidade: ");
  Serial.print(umidade);
  Serial.println(" %");

  if (Firebase.setFloat(firebaseData, "/sensor1/temperatura", temperatura)) {
    Serial.println("Temperatura enviada com sucesso");
  } else {
    Serial.println("Falha ao enviar temperatura");
    Serial.println(firebaseData.errorReason());
  }

  if (Firebase.setFloat(firebaseData, "/sensor3/umidade", umidade)) {
    Serial.println("Umidade enviada com sucesso");
  } else {
    Serial.println("Falha ao enviar umidade");
    Serial.println(firebaseData.errorReason());
  }

  if (Firebase.setString(firebaseData, "/sensor3/hora", formattedTime)) {
    Serial.println("Hora enviada com sucesso");
  } else {
    Serial.println("Falha ao enviar hora");
    Serial.println(firebaseData.errorReason());
  }

  //Envia a temperatura para o tópico MQTT específico
  String tempMsg = String(temperatura);
  char tempMessage[100];
  tempMsg.toCharArray(tempMessage, 100);
  publishMQTT(mqtt_topic_temp, tempMessage);

  //Envia a umidade para o tópico MQTT específico
  String humMsg = String(umidade);
  char humMessage[100];
  humMsg.toCharArray(humMessage, 100);
  publishMQTT(mqtt_topic_hum, humMessage);

  String msg = "{\"temperature\":" + String(temperatura) + ",\"humidity\":" + String(umidade) + ",\"time\":\"" + formattedTime + "\"}";
  char message[100];
  msg.toCharArray(message, 100);
  publishMQTT(mqtt_topic, message);


  delay(60000);

  
}
