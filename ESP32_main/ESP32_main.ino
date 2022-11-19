#include <MQTT.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <OneWire.h>
#include "RTClib.h"
RTC_DS3231 rtc;
#include <EEPROM.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 23 //пин OneWire
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress podachaNom = { //подача
  0x28, 0x88, 0x52, 0x49, 0xF6, 0x3A, 0x3C, 0xBB
};
DeviceAddress obratkaTempNom = { //обратка
  0x28, 0xFF, 0x08, 0x79, 0xA2, 0x16, 0x03, 0x50
};
DeviceAddress hallTempNom = { //комнатный датчик
  0x28, 0xB6, 0x51, 0x79, 0xA2, 0x15, 0x03, 0x80
};
DeviceAddress koridorTempNom = { //коридор
  0x10, 0x77, 0x2A, 0xA6, 0x01, 0x08, 0x00, 0xDE
};
DeviceAddress outsideTempNom = { //адрес датчика улицы
  0x28, 0x03, 0x42, 0x49, 0xF6, 0x4F, 0x3C, 0x9C
};
DeviceAddress boilerTempNom = {
  0x28, 0x15, 0xB6, 0x49, 0xF6, 0xD4, 0x3C, 0x2C
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WiFiClient espClient;
IPAddress MQTTserver(192, 168, 0, 235);
PubSubClient client(espClient, MQTTserver);
String pubTopic = "/ESP32/DATA/";
String cderfv;
char* controlTopic = "/ESP32/CONTROL/#";
String current_topic = "/ESP32/CONTROL/";
String mqtt_user = "";
String mqtt_pass = "";
String mqtt_client = "ESP32";
bool conectMQTT = 0;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
  struct MySettings {
  byte need_boiler_temp;
  };
*/

//пины
int vent = 33; //вентиляция
int enableServo = 16; //вкл. серву
int pump = 32; //насос

int twistServo = 17; //направление
int heat = 27; //тен

int light_in_toilet = 18;

int servoStatusTime = 0;
bool valve_is_closed = false;
int lastTempCheckTime = 0;
float needHomeTemp = 23.5; //необходимая температура
float setTemp = needHomeTemp;
float calcTemp = needHomeTemp;
float home_temp_for_calculate = needHomeTemp;
int modifikator = 0;
float calcWeatherTemp;
float outside_temperature_now;
int cwtUpdate = 0;

byte cur_hour;

float avergeTemp;
float delta = 0;
float podacha;
float podacha_old;
float obratka;
float room_temperature_now;
float koridor_temperature_now;
float boilerTemp;
float max_boiler_temp = 85;
float need_boiler_temp = 85;
float newTemp;
//float outsideTemp;
int servo_time_enable = 1250;
bool stoppump = 0;
unsigned long stoppumptime = 0;
bool adjustment = 0;

float temp_var;
float home_temperature; //температура в доме

//переменные для расчета медианы
unsigned long medTiming = 0; // Переменная для хранения точки отсчета
const int bufSize = 9; // количество элементов должно быть нечётным
static float buf[bufSize]; // массив нескольких последних элементов
static int pos = 0;  // текущая позиция в массиве
float room_temperature_mediana_9min;

//переменные для расчета медианы коридора
unsigned long koridor_medTiming = 0; // Переменная для хранения точки отсчета
const int koridor_bufSize = 9; // количество элементов должно быть нечётным
static float koridor_buf[koridor_bufSize]; // массив нескольких последних элементов
static int koridor_pos = 0;  // текущая позиция в массиве
float koridor_temperature_mediana_9min;

//уличная медиана 15 минутная
unsigned long medTiming2 = 0; // Переменная для хранения точки отсчета
const int bufSize2 = 5; // количество элементов должно быть нечётным
static float buf2[bufSize2]; // массив нескольких последних элементов
static int pos2 = 0;  // текущая позиция в массиве
float outside_15min_mediana_temperature = 0;

//среднесуточная
unsigned long medTiming3 = 0; // Переменная для хранения точки отсчета
const int bufSize3 = 25; // количество элементов должно быть нечётным
static float day_temp_array[bufSize3]; // массив нескольких последних элементов
static int pos3 = 0;  // текущая позиция в массиве
float medOutSideDayTemp;

//30 минутная комнатная медиана
unsigned long home_30m_mediana_timing = 0; // Переменная для хранения точки отсчета
const int home_30m_mediana_buf_size = 11; // количество элементов должно быть нечётным
static float home_30m_mediana_buf[home_30m_mediana_buf_size]; // массив нескольких последних элементов
static int home_30m_mediana_buf_pos = 0;  // текущая позиция в массиве
float home_30m_mediana;

unsigned long setTempTiming = 0;
unsigned long time_tualet_zanat = 0;
unsigned long time_tualet_ne_zanat = 0;
bool tualetZanat = false;
bool need_vent_tualet = false;
bool need_vent_tualet_all = false;
bool need_vent = false;
bool control_need_vent = false;

String sbj;
unsigned long lastCheck = 0; // Переменная для хранения точки отсчета
unsigned long lostwifi = 0;

float getMediana (float arr[], int arr_size) {
  float temp_arr[arr_size];
  float mediana;
  for (int i = 0; i < arr_size; i++) {
    temp_arr[i] = arr[i];
  }
  for (int i = 0; i < arr_size; i++) {
    for (int j = i; j < arr_size; j++) {
      if (temp_arr[i] < temp_arr[j]) {
        int k = temp_arr[i];
        temp_arr[i] = temp_arr[j];
        temp_arr[j] = k;
      }
    }
  }
  int cde = round(arr_size / 2);
  return temp_arr[cde];
}

void calcNeedBoilerTemp() {
  if(medOutSideDayTemp > 15) {
    need_boiler_temp = max_boiler_temp - 20;
  } else if(medOutSideDayTemp > 10) {
    need_boiler_temp = max_boiler_temp - 15;
  } else if(medOutSideDayTemp > 5) {
    need_boiler_temp = max_boiler_temp - 10;
  } else {
    need_boiler_temp = max_boiler_temp;
  }
  client.publish("/ESP32/DATA/LOG", String("Перерасчет need_boiler_temp: " + String(need_boiler_temp)));
}

void setDayTempArray(float value) { //float *arr, int bufSize,
  for (int i = 0; i < bufSize3; i++) {
    day_temp_array[i] = value;
  }
  EEPROM.put(0, day_temp_array);
  EEPROM.commit();
}

void addValue(float *arr, int &pos, int bufSize, float value) {
  if (value != -127) {
    arr[pos] = value;
    pos++;
    if (pos == bufSize) pos = 0;
  }
}

void addValueNew(float *arr, int bufSize, float value) {
  if (value != -127) {
    for (int i = (bufSize - 1); i > 0; i--) {
      arr[i] = arr[i - 1];
    }
    arr[0] = value;
  }
}

void fillBuf(float *arr, int bufSize, float value) {
  if (value != -127) {
    for (int i = 0; i < bufSize; i++) {
      arr[i] = value;
    }
  }
}

//считаем температуру подачи
void calcSetTemp(float outdoor_mediana_temp) {
  home_temp_for_calculate = home_temperature;
  if (home_temp_for_calculate == -127) {
    home_temp_for_calculate = needHomeTemp;
  }
  calcTemp = 27.76 - 0.2483 * 2 * ceil(outdoor_mediana_temp / 2);
  modifikator = -1 * ceil(home_temp_for_calculate - needHomeTemp);
  if (modifikator >= 1 || modifikator <= -1) {
    setTemp = calcTemp + modifikator;
  } else {
    setTemp = calcTemp;
  }
  if (setTemp > 38) {
    setTemp = 38;
  }
  if (setTemp < needHomeTemp) {
    setTemp = needHomeTemp; //нет смысла занижать подачу ниже требуемой температуры в доме
  }
}

void restart() {
  Serial.println("Will reset and try again...");
  abort();
}

byte get_hour() {
  DateTime now = rtc.now();
  return now.hour();
}

byte get_minute() {
  DateTime now = rtc.now();
  return now.minute();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  EEPROM.begin(512);
  delay(100);

  Serial.println("Setup");
  pinMode(enableServo, OUTPUT);
  pinMode(twistServo, OUTPUT);
  digitalWrite(twistServo, HIGH);
  pinMode(pump, OUTPUT);
  pinMode(heat, OUTPUT);
  pinMode(vent, OUTPUT);
  pinMode(light_in_toilet, INPUT);

  //Подключаемся к WiFi//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  const char* ssid = "OpenWrt17";
  const char* password = "valdenlove";
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  WiFi.setHostname("ESP32");

  int retries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retries < 50)) {
    retries++;
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    WiFi.printDiag(Serial);
    //Подключаемся к MQTT
    Serial.println("");
    Serial.println("Connecting to MQTT broker ");
    if (client.connect(MQTT::Connect(mqtt_client).set_auth(mqtt_user, mqtt_pass))) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(controlTopic);
    } else {
      Serial.println("Fail to Connecting to MQTT broker");
    }
  } else {
    Serial.println("WiFi can't connected!");
  }
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.println("Start");
  client.publish("/ESP32/DATA/LOG", " ");
  delay(500);
  client.publish("/ESP32/DATA/LOG", "Запуск отопительного МК");
  ///////////////////////////////////////////////////////////////////////////////////


  sensors.begin();
  sensors.setResolution(podachaNom, 10);
  sensors.setResolution(hallTempNom, 10);
  sensors.setResolution(koridorTempNom, 10);
  sensors.setResolution(outsideTempNom, 10);
  sensors.setResolution(obratkaTempNom, 10);
  sensors.setResolution(boilerTempNom, 10);
  delay(500);
  sensors.requestTemperatures();
  delay(500);
  //подача
  podacha = sensors.getTempC(podachaNom);
  client.publish("/ESP32/DATA/LOG", String("Подача: " + String(podacha)));
  Serial.println(String("Подача: " + String(podacha)));
  //обратка
  obratka = sensors.getTempC(obratkaTempNom);
  client.publish("/ESP32/DATA/LOG", String("Обратка: " + String(obratka)));
  Serial.println(String("Обратка: " + String(obratka)));
  //бак
  boilerTemp = sensors.getTempC(boilerTempNom);
  client.publish("/ESP32/DATA/LOG", String("Бак: " + String(boilerTemp)));
  Serial.println(String("Бак: " + String(boilerTemp)));
  //
  client.set_callback(callback);
  ///////////////////////////////////////////////////////////////////////////////////
  outside_temperature_now = sensors.getTempC(outsideTempNom);
  client.publish("/ESP32/DATA/LOG", String("Улица: " + String(outside_temperature_now)));
  Serial.println(String("Улица: " + String(outside_temperature_now)));
  EEPROM.get(0, day_temp_array);
  medOutSideDayTemp = getMediana(day_temp_array, bufSize3);
  if (isnan(medOutSideDayTemp) || medOutSideDayTemp == -127) {
    medOutSideDayTemp = outside_temperature_now;
    setDayTempArray(outside_temperature_now);
    client.publish("/ESP32/DATA/LOG", String("medOutSideDayTemp isnan: " + String(medOutSideDayTemp)));
    client.publish("/ESP32/DATA/LOG", String("getMediana medOutSideDayTemp: " + String(getMediana(day_temp_array, bufSize3))));
  } else {
    client.publish("/ESP32/DATA/LOG", String("medOutSideDayTemp: " + String(medOutSideDayTemp)));
  }
  //
  room_temperature_now = sensors.getTempC(hallTempNom);
  client.publish("/ESP32/DATA/LOG", String("Зал: " + String(room_temperature_now)));
  Serial.println(String("Зал: " + String(room_temperature_now)));
  if (room_temperature_now == -127) {
    room_temperature_mediana_9min = needHomeTemp;
  } else {
    room_temperature_mediana_9min = room_temperature_now;
  }
  fillBuf(buf, bufSize, room_temperature_mediana_9min);
  //
  koridor_temperature_now = sensors.getTempC(koridorTempNom);
  client.publish("/ESP32/DATA/LOG", String("Коридор: " + String(koridor_temperature_now)));
  Serial.println(String("Коридор: " + String(koridor_temperature_now)));
  if (koridor_temperature_now == -127) {
    koridor_temperature_mediana_9min = needHomeTemp;
  } else {
    koridor_temperature_mediana_9min = koridor_temperature_now;
  }
  fillBuf(koridor_buf, bufSize, koridor_temperature_mediana_9min);
  //
  fillBuf(home_30m_mediana_buf, home_30m_mediana_buf_size, koridor_temperature_mediana_9min);
  home_30m_mediana = koridor_temperature_mediana_9min;
  //
  if (outside_temperature_now == -127) {
    outside_15min_mediana_temperature = medOutSideDayTemp;
  } else {
    outside_15min_mediana_temperature = outside_temperature_now;
  }
  fillBuf(buf2, bufSize2, outside_15min_mediana_temperature);

  home_temperature = home_30m_mediana;

  client.publish("/ESP32/DATA/START", "1");
  if (!rtc.begin()) {
    client.publish("/ESP32/DATA/LOG", "Couldn't find RTC");
    Serial.println("Couldn't find RTC");
  } else if (rtc.lostPower()) {
    //Нужно установить время
    client.publish("/ESP32/DATA/LOG", "RTC lost power, let's set the time!");
    Serial.println("RTC lost power, let's set the time!");
  }
  client.publish("/ESP32/DATA/LOG", "Переходим к циклу.");
  if (outside_15min_mediana_temperature < 15) {
    client.publish("/ESP32/DATA/LOG", "Прокачиваем 2 минуты");
    Serial.println("Прокачиваем 2 минуты");
    digitalWrite(pump, HIGH); //включаем насос
    delay(120000);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  if (millis() - lastCheck > 50 or millis() - lastCheck < 0) {
    if (!client.loop()) {
      Serial.println("Connecting to MQTT broker ");
      if (client.connect(MQTT::Connect(mqtt_client).set_auth(mqtt_user, mqtt_pass))) {
        Serial.println("Connected to MQTT broker");
        client.subscribe(controlTopic);
        //client.publish("/ESP32/DATA/LOG", "Connected to MQTT broker");
      } else {
        Serial.println("Fail to Connecting to MQTT broker");
        if (lostwifi == 0) {
          Serial.println("lostwifi == 0");
          lostwifi = millis();
        } else if (millis() - lostwifi > 1200000 or millis() - lostwifi < 0) {
          restart();
        }
      }
    } else {
      lostwifi = 0;
    }
    lastCheck = millis();
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //расчет необходимой температуры подачи РАЗ В ЧАС
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (digitalRead(pump) == HIGH and (millis() - setTempTiming > 3600000 or millis() - setTempTiming < 0 or setTempTiming == 0) and (millis() - cwtUpdate > 3900000 or cwtUpdate == 0) and medOutSideDayTemp != -127 and medOutSideDayTemp != 85) {
    setTempTiming = millis();
    calcSetTemp(medOutSideDayTemp);
    client.publish("/ESP32/DATA/LOG", "Используем уличную среднесуточную температуру " + String(medOutSideDayTemp));
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //расчет медианы комнатной температуры РАЗ В МИНУТУ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (millis() - medTiming > 60000 or millis() - medTiming < 0 or medTiming == 0) {
    medTiming = millis();
    sensors.requestTemperatures();
    //вычисляем комнатную медиану////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    room_temperature_now = sensors.getTempC(hallTempNom);
    if (room_temperature_now == -127 or room_temperature_now == 85) {
      //client.publish("/ESP32/DATA/LOG", String("Тупит комнатный датчик: " + String(room_temperature_now)));
    } else {
      addValue(buf, pos, bufSize, room_temperature_now); //добавляем новое значение
      room_temperature_mediana_9min = getMediana(buf, bufSize);
    }
    //вычисляем коридорную медиану////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    koridor_temperature_now = sensors.getTempC(koridorTempNom);
    if (koridor_temperature_now == -127 or koridor_temperature_now == 85) {
      client.publish("/ESP32/DATA/LOG", String("Тупит коридорный датчик: " + String(koridor_temperature_now)));
    } else {
      addValue(koridor_buf, koridor_pos, koridor_bufSize, koridor_temperature_now); //добавляем новое значение
      koridor_temperature_mediana_9min = getMediana(koridor_buf, koridor_bufSize);
    }
    //вентиляция/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (outside_15min_mediana_temperature > 25) {
      need_vent = false;
    } else {
      if (medOutSideDayTemp < 15) {
        if (home_temperature <= needHomeTemp + 0.5) {
          need_vent = false;
        } else if (home_temperature >= (needHomeTemp + 1)) {
          need_vent = true;
        }
      } else {
        if (home_30m_mediana < needHomeTemp) {
          need_vent = false;
        } else if (home_30m_mediana > needHomeTemp) {
          need_vent = true;
        }
      }
    }
    //////////////////////////////////////////////////////
    //расчет медианы уличной температуры РАЗ В 3 МИНУТЫ///
    //////////////////////////////////////////////////////
    if (millis() - medTiming2 > 180000 or millis() - medTiming2 < 0 or medTiming2 == 0) {
      medTiming2 = millis();
      //sensors.requestTemperatures();
      //
      //вычисляем уличную медиану////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      outside_temperature_now = sensors.getTempC(outsideTempNom);
      if (outside_temperature_now == -127 or outside_temperature_now == 85) {
        client.publish("/ESP32/DATA/LOG", String("Тупит уличный датчик: " + String(outside_temperature_now)));
      } else {
        addValue(buf2, pos2, bufSize2, outside_temperature_now);
        outside_15min_mediana_temperature = getMediana(buf2, bufSize2);
      }
      //вычисляем 30 минутную домовую медиану
      if (koridor_temperature_now == -127 or koridor_temperature_now == 85) {
        client.publish("/ESP32/DATA/LOG", String("Тупит коридорный датчик: " + String(koridor_temperature_now)));
      } else {
        addValue(home_30m_mediana_buf, home_30m_mediana_buf_pos, home_30m_mediana_buf_size, koridor_temperature_now);
        home_30m_mediana = getMediana(home_30m_mediana_buf, home_30m_mediana_buf_size);
        home_temperature = home_30m_mediana;
        client.publish("/ESP32/DATA/home_temperature_30m_mediana", String(home_30m_mediana));
      }
      //бак
      cur_hour = get_hour();
      boilerTemp = sensors.getTempC(boilerTempNom);
      client.publish("/ESP32/DATA/BAK/temperature", String(boilerTemp));
      if (boilerTemp == -127) {
        digitalWrite(heat, LOW);
        client.publish("/ESP32/DATA/LOG", "Тупит датчик бака: " + String(boilerTemp));
      } else {
        if (cur_hour == 23 || cur_hour < 7) {
          if (boilerTemp >= need_boiler_temp || boilerTemp >= 95) {
            digitalWrite(heat, LOW);
          } else {
            digitalWrite(heat, HIGH);
          }
        } else {
          digitalWrite(heat, LOW);
        }
      }
    }
    //Отсылаем данные///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (digitalRead(pump) == HIGH) {
      podacha = sensors.getTempC(podachaNom);
      obratka = sensors.getTempC(obratkaTempNom);
      room_temperature_now = sensors.getTempC(hallTempNom);
      if (podacha != -127 && podacha != 85 && obratka  != -127 && obratka != 85) {
        client.publish("/ESP32/DATA/OTOPLENIE/PODACHA", String(podacha));
        client.publish("/ESP32/DATA/OTOPLENIE/OBRATKA", String(obratka));
        client.publish("/ESP32/DATA/OTOPLENIE/DELTA", String(podacha - obratka));
        client.publish("/ESP32/DATA/OTOPLENIE/MODIFIKATOR", String(modifikator));
        avergeTemp = (podacha + obratka) / 2;
        client.publish("/ESP32/DATA/OTOPLENIE/AVERGE_TEMP", String(avergeTemp));
      }
    }
    client.publish("/ESP32/DATA/outside_temperature_now", String(sensors.getTempC(outsideTempNom)));
    client.publish("/ESP32/DATA/koridor_temperature_now", String(sensors.getTempC(koridorTempNom)));
    client.publish("/ESP32/DATA/outside_temperature_mediana_15min", String(outside_15min_mediana_temperature));
    if (room_temperature_now != -127 && room_temperature_now != 85) {
      client.publish("/ESP32/DATA/room_temperature_now", String(room_temperature_now));
    }
    client.publish("/ESP32/DATA/room_temperature_mediana_9min", String(room_temperature_mediana_9min));
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //вдруг лето
    if (digitalRead(pump) == HIGH && medOutSideDayTemp >= 10 && home_temperature >= needHomeTemp) {
      digitalWrite(pump, LOW); //выключаем насос
      client.publish("/ESP32/DATA/LOG", "На улице тепло, выключаем насос.");
    } else if (digitalRead(pump) == HIGH) {
      if (millis() - cwtUpdate > 3606000 and cwtUpdate > 0 and outside_15min_mediana_temperature >= 10 and home_temperature >= needHomeTemp) {
        client.publish("/ESP32/DATA/LOG", "Подерживаем в полу необходимую температуру.");
        setTemp = needHomeTemp;
      } else if (millis() - cwtUpdate <= 3606000 and cwtUpdate > 0 and calcWeatherTemp >= 10 and home_temperature > needHomeTemp) {
        client.publish("/ESP32/DATA/LOG", "Подерживаем в полу необходимую температуру.");
        setTemp = needHomeTemp;
      } else {

      }
    } else if (
      (digitalRead(pump) == LOW and stoppump == 0 and medOutSideDayTemp < 10)
      || (outside_15min_mediana_temperature < 15 and home_temperature <= needHomeTemp)
      ) {
      digitalWrite(pump, HIGH); //включаем насос
      client.publish("/ESP32/DATA/LOG", "Пора топить, включаем насос.");
    } else if (home_temperature < 10) {
      digitalWrite(pump, HIGH); //включаем насос
    }
    if (digitalRead(pump) == HIGH) { //если насос работает то шлем данные
      client.publish("/ESP32/DATA/OTOPLENIE/need_averge_temperature", String(setTemp));
      client.publish("/ESP32/DATA/OTOPLENIE/calcTemp", String(calcTemp));
    }
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //расчет медианы дневной уличной температуры РАЗ В ЧАС////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (millis() - medTiming3 > 3600000 or millis() - medTiming3 < 0 or medTiming3 == 0) {
    medTiming3 = millis();
    addValueNew(day_temp_array, bufSize3, outside_15min_mediana_temperature);
    medOutSideDayTemp = getMediana(day_temp_array, bufSize3);

    EEPROM.put(0, day_temp_array);
    EEPROM.commit();
    /*
        String tlog = "";
        for (int i = 0; i < bufSize3; i++) {
          tlog += String(day_temp_array[i]) + " ";
        }
        client.publish("/ESP32/DATA/LOG", String("day_temp_array : " + String(tlog)));
    */

    calcNeedBoilerTemp();

    client.publish("/ESP32/DATA/MEDOUTSIDEDAYTEMP", String(medOutSideDayTemp));
    client.publish("/ESP32/DATA/MEDOUTTEMPARHIV", String(outside_15min_mediana_temperature));
  }
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //смесительный сервопривод
  if (adjustment == 1 or (millis() - lastTempCheckTime) > 180000 or (millis() - lastTempCheckTime) < 0) {
    sensors.requestTemperatures();
    podacha = sensors.getTempC(podachaNom);
    obratka = sensors.getTempC(obratkaTempNom);

    if (digitalRead(pump) == HIGH && podacha != -127 && podacha != 85 && obratka  != -127 && obratka != 85) {
      avergeTemp = (podacha + obratka) / 2;
      if (avergeTemp >= (setTemp - 0.24) && avergeTemp <= (setTemp + 0.24)) {
        digitalWrite(enableServo, LOW); // температура в норме
        lastTempCheckTime = millis();
        if (adjustment == 1) {
          //client.publish("/ESP32/DATA/LOG", String("avergeTemp=" + String(avergeTemp)));
        }
        adjustment = 0;
      } else {
        adjustment = 1;
        if (avergeTemp < (setTemp - 0.24)) {
          digitalWrite(twistServo, LOW);
          sbj = "Увеличиваем ";
          servo_time_enable = 2500; //1250
        } else if (avergeTemp > (setTemp + 0.24)) {
          digitalWrite(twistServo, HIGH);
          sbj = "Уменьшаем ";
          servo_time_enable = 2500; //1250
        }
        if (avergeTemp < (setTemp - 4) or avergeTemp > (setTemp + 4)) {
          servo_time_enable = 3000;
        }
        if (digitalRead(enableServo) == 0 and ((millis() - servoStatusTime) > 60000 or (millis() - servoStatusTime) < 0)) {
          digitalWrite(enableServo, HIGH); //крутим
          //client.publish("/ESP32/DATA/LOG", String(sbj + "подачу." + " avergeTemp=" + String(avergeTemp)));
          servoStatusTime = millis();
        }
        if (digitalRead(enableServo) == 1 and ((millis() - servoStatusTime) > servo_time_enable or (millis() - servoStatusTime) < 0)) {
          digitalWrite(enableServo, LOW); // ждем
          servoStatusTime = millis();
        }
      }
    }

    //контроль предельной температуры теплоносителя после смесителя
    if (stoppump == 0 and (podacha > 45 or (podacha == -127 and podacha_old == -127))) {
      digitalWrite(pump, LOW); //выключаем насос
      if (podacha == -127 and podacha_old == -127) {
        client.publish("/ESP32/DATA/LOG", "Тупит датчик подачи: " + String(podacha) + ". Насос отключен.");
      } else {
        client.publish("/ESP32/DATA/LOG", "Высокая температура: " + String(podacha) + ". Насос отключен.");
      }
      stoppump = 1;
      stoppumptime = millis();
      digitalWrite(twistServo, HIGH);
      digitalWrite(enableServo, HIGH);
      delay(180000);
      digitalWrite(enableServo, LOW);
      valve_is_closed = true;
    }

    if (stoppump == 1 and (millis() - stoppumptime > 600000 or millis() - stoppumptime < 0) and podacha != -127 and podacha_old != -127) {
      client.publish("/ESP32/DATA/LOG", "Включаем насос.");
      stoppump = 0;
    }
    podacha_old = podacha;
  }
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //вентиляция /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ////туалет занят                          и  (прошло более 10 минут с прошлого раза    или сбросился счетчик)
  if (digitalRead(light_in_toilet) == HIGH and (millis() - time_tualet_ne_zanat > 600000 or time_tualet_ne_zanat == 0 or millis() - time_tualet_ne_zanat < 0)) { //time_tualet_ne_zanat - time_tualet_zanat < 80000 or
    if (tualetZanat == false) {
      time_tualet_zanat = millis();
      tualetZanat = true;
      need_vent_tualet = true;
    }
    if (millis() - time_tualet_zanat > 80000) {
      need_vent_tualet_all = true;
    }
  } else if (digitalRead(light_in_toilet) == LOW) {
    if (tualetZanat == true) {
      time_tualet_ne_zanat = millis();
      tualetZanat = false;
    }
    //выключаем вентиляцию если (прошло больше 10 минут после выключения света или сбросился счетчик                   или прошло менее 80 секунд)
    if (need_vent_tualet == true and (millis() - time_tualet_ne_zanat > 600000 or millis() - time_tualet_ne_zanat < 0 or time_tualet_ne_zanat - time_tualet_zanat < 80000)) {
      time_tualet_ne_zanat = 0;
      time_tualet_zanat = 0;
      need_vent_tualet = false;
    }
  }
  
  if (need_vent_tualet == true or need_vent == true or control_need_vent == true) {
    if(digitalRead(vent) == LOW) {
      client.publish("/ESP32/DATA/LOG", "vent_tualet ON");
    }
    digitalWrite(vent, HIGH);
  } else {
    if(digitalRead(vent) == HIGH) {
      client.publish("/ESP32/DATA/LOG", "vent_tualet OFF");
    }
    digitalWrite(vent, LOW);
  }
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //yield();
}

/*
    DateTime now = rtc.now();
    Serial.println();
    Serial.print();
        Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
        Serial.print(" (");
    //Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    Serial.print("Temperature: ");
    Serial.print(rtc.getTemperature());
    Serial.println(" C");

    Serial.println();
    delay(3000);
*/

//Начало блока обработки полученных данных
void callback(const MQTT::Publish& sub) { //Есть запись в топике
  Serial.print("Get data from subscribed topic ");
  Serial.print(sub.topic());  // Название топика
  Serial.print(" => ");
  Serial.println(sub.payload_string()); // Данные из топика

  if (sub.topic() == current_topic + "VENT") {  //0, 4, 5, 12, 13, 14, 15, 16
    if (sub.payload_string() == "ON") {
      control_need_vent = true;
    }
    if (sub.payload_string() == "OFF") {
      control_need_vent = false;
    }
    cderfv = pubTopic + "VENT";
  }
  if (sub.topic() == current_topic + "TEMP") {  //установка новой температуры
    String MyStr = sub.payload_string();
    char myStr8[32];
    MyStr.toCharArray(myStr8, MyStr.length());    // копирование String в массив myStr8
    float newTemp = atof(myStr8);  //  преобразование в float
    Serial.print(newTemp);
  }
  if (sub.topic() == current_topic + "SET_TIME") {
    String MyStr = sub.payload_string();
    int set_time = MyStr.toInt();
    rtc.adjust(DateTime(set_time));
    delay(100);
    DateTime now = rtc.now();
    client.publish("/ESP32/DATA/TIME", String(now.year(), DEC) + "-" + String(now.month(), DEC) + "-" + String(now.day(), DEC) + " " + String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC));
    cderfv = pubTopic + "SET_TIME";
  }
  if (sub.topic() == current_topic + "CALCNEEDTEMP") {
    String MyStr = sub.payload_string();
    calcWeatherTemp = MyStr.toInt();  //  преобразование в float
    cwtUpdate = millis();
    cderfv = pubTopic + "CALCNEEDTEMP";
    calcSetTemp(calcWeatherTemp);
    if (digitalRead(pump) == HIGH) {
      client.publish("/ESP32/DATA/LOG", String("Используем прогноз: " + String(calcWeatherTemp) + " гр., calcTemp = " + String(calcTemp) + ", setTemp=" + String(setTemp)));
    }
  }
  if (sub.topic() == current_topic + "BAK/SET_TEMP") {  //установка новой температуры
    cderfv = pubTopic + "BAK/SET_TEMP";
    String MyStr = sub.payload_string();
    char myStr8[32];
    MyStr.toCharArray(myStr8, sizeof(myStr8));
    newTemp = atof(myStr8);
    if (newTemp <= 90 and newTemp > 0) {
      need_boiler_temp = newTemp;
      /*
        EEPROM.put(0, need_temp);
        EEPROM.commit();
      */
    }
  }
  if (sub.topic() == current_topic + "RESTART") {
    cderfv = pubTopic + "RESTART";
    if (sub.payload_string() == "1") {
      client.publish("/ESP32/DATA/LOG", "Перезагрузка");
      delay(100);
      ESP.restart();
    }
  }
  if (sub.topic() == current_topic + "day_temp_array") {
    cderfv = pubTopic + "day_temp_array";
    if (sub.payload_string() == "1") {
      String tlog = "";
      for (int i = 0; i < bufSize3; i++) {
        tlog += String(day_temp_array[i]) + " ";
      }
      client.publish("/ESP32/DATA/LOG", String("day_temp_array : " + String(tlog)));
    }
  }

  MQTT::Publish newpub(cderfv, sub.payload(), sub.payload_len());
  client.publish(newpub);
}
