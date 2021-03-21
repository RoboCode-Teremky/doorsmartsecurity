/*
  код зроблено на конкурс robocode project 2020 командою Tomorrow's Tehnology.
  команда: Михайло Журавльов, Олесь Федірко, Держипольський Ярослав.
  ментор: Олександр Куцик.
*/


//бібліотеки
#include <ESP32Servo.h>
#include <analogWrite.h>
#include <ESP32Tone.h>
#include <ESP32PWM.h>
#include <HardwareSerial.h>
#include <FPM.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//відбиток пальця
HardwareSerial fserial(1);
FPM finger(&fserial);
FPM_System_Params params;

//приеднуемся до вай-фай, можливо написати інші
const char* ssid     = "RoboCode-Teremky-aud2";
const char* password = "robocode2";

//об'єкти та змінні для телеграму
String BOT_TOKEN = "1241263819:AAFJ0slMJhs3x4domW68VW68VG4hhOCFotQ"; //токен нашого бота
const unsigned long BOT_MTBS = 1000; //затримка між перевіркою оновлень
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; //час останнього оновлення
String CHAT_ID = "-1001173650003"; //айді чату за замовчуванням

//створення об'єкту серво двигуна
Servo servo;

//змінні для дверей
bool lock = true; //змінна для стану замку
bool isOpened = false; //змінна для стану дверей (закриті/відкриті)
bool isMoved = false; //змінна для стану руху перед дверима
bool buttonState;             // the current reading from the input pin
bool lastButtonState = LOW;   // the previous reading from the input pin
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 100;    // the debounce time; increase if the output flickers
const unsigned long finger_delay = 1000; //затримка між перевыркою на наявність відбитку
unsigned long finger_lasttime = 0; //час останньої перевірки відбитку пальців
bool allow_delete = false; //дозвіл на видалення відбитку

//змінні для гри
int one;
int two;
int quize;
bool play = false; //дозвіл на гру

void setup()
{
  Serial.begin(115200);

  // підключення до Wi-Fi
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  bot.sendMessage(CHAT_ID, "Bot started up");

  // ArduinoOTA для оновлення по Wi-Fi
  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  //ініціалізація датчику відбитку пальця
  Serial.print("1:N MATCH test");
  fserial.begin(9600, SERIAL_8N1, 16, 17);

  if (finger.begin()) {
    finger.readParams(&params);
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    bot.sendMessage(CHAT_ID, "Did not find fingerprint sensor :(");
    //while (1) yield();
  }

  //порти
  servo.attach(13);
  pinMode(12, INPUT); //кнопка
  pinMode(14, INPUT); //кінцевий вимикач
  pinMode(27, INPUT); //ІЧ датчик руху
  pinMode(2, OUTPUT); //вбудований світлодіод

}

void loop() {
  //Перевіряємо наявність нової програми
  ArduinoOTA.handle();

  //відкриваємо чи закриваємо двурі в залежності від змінної lock
  if (lock)
    servo.write(0); //закриваємо двері
  else
    servo.write(180); //відкриваємо двері
  digitalWrite(2,lock);
  //відкриваємо та закриваємо двері при натисканні на кнопку
  //взято з прикладу debounce
  bool b = digitalRead(12);
  
  if (b != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (b != buttonState) {
      buttonState = b;
      if (buttonState == HIGH) {
        lock = !lock;
      }
    }
  }

  lastButtonState=b;

  //відправляємо повідомлення про відкриті двері
  bool o = digitalRead(14);
  if (o != isOpened) {
    isOpened = o;
    if (isOpened) {
      bot.sendMessage(CHAT_ID, "Door is opened", "");
    }
    else
      bot.sendMessage(CHAT_ID, "Door is closed", "");

  }

  //відправляємо повідомлення про наявність руху перед дверима
  bool m = digitalRead(27);
  if (m != isMoved) {
    isMoved = m;
    if (isMoved)
      bot.sendMessage(CHAT_ID, "Someone at front of the door!");
  }

  //перевіряємо на наявність нових повідомлень для телеграм-боту
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      //bot.sendMessage(CHAT_ID, "got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }

  //перевіряємо на наявність видбитку пальця
  //if (millis() - finger_lasttime > finger_delay)
  //{
    int16_t p = -1;
    p = finger.getImage();
    if (p != FPM_NOFINGER)
      search_database();
    //finger_lasttime = millis();
  //}
}

//обробка команд чату
void handleNewMessages(int numNewMessages) {

  for (int i = 0; i < numNewMessages; i++)
  {
    CHAT_ID = bot.messages[i].chat_id; //закоментувати для безпеки
    String text = bot.messages[i].text;

    //отримати ім'я співбесідника
    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    //перевірка дій чату
    if (text == "/send_test_action")
    {
      bot.sendChatAction(CHAT_ID, "typing");
      delay(4000);
      bot.sendMessage(CHAT_ID, "Did you see the action message?");
    }

    //привітання
    if (text == "/start")
    {
      bot.sendMessage(CHAT_ID, "Hello" + from_name + "Welcome DoorSS (Smart & Safe) Telegram Bot.\n" +
                      "This project created by \"Tomorrow's Technology\" Team.\n" +
                      "Type /help to get list of all available commands.");
    }

    //відкриття замку
    if (text == "/unlock")
    {
      lock = false;
      bot.sendMessage(CHAT_ID, "Door is unlocked.");
    }

    //закриття замку
    if (text == "/lock")
    {
      lock = true;
      bot.sendMessage(CHAT_ID, "Door is locked.");
    }

    //вивід повної інформації
    if (text == "/status")
    {
      bot.sendMessage(CHAT_ID,
                      "Chat ID:" + String(CHAT_ID) + "\n" +
                      "Locked: " + String(lock) + "\n" +
                      "Opened: " + String(isOpened) + "\n" +
                      "Movement: " + String(isMoved));
    }

    if (text == "/help")
    {
      String help = "/help - help.\n";
      help += "/start - welcome message.\n";
      help += "/send_test_action - test chat action.\n";
      help += "/unlock - unlock the door.\n";
      help += "/lock - lock the door.\n";
      help += "/status - get all main variables.\n";
      help += "/findprint - force find fingerprint.\n";
      help += "/newprint - write a new fingerprint in memory.\n";
      help += "/deleteprint - remove fingerprint from memory.\n";
      help += "/play - play the chat game.";
      bot.sendMessage(CHAT_ID, help);
    }

    //fingerprint
    //примусово запустити пошук відбитку
    if (text == "/findprint") {
      search_database();
    }

    //записати новий відбиток
    if (text == "/newprint") {
      int16_t fid;
      get_free_id(&fid);
      enroll_finger(fid);
    }

    //видалення відбитку
    if (allow_delete == true) {
      int16_t fid = text.toInt();
      deleteFingerprint(fid);
      bot.sendMessage(CHAT_ID, "ID deleted: " + String(fid));
      allow_delete = false;
    }

    if (text == "/deleteprint") {
      bot.sendMessage(CHAT_ID, "ID?");
      allow_delete = true;
    }

    //гра
    if (play == true) {
      if (text == String(quize))
        bot.sendMessage(CHAT_ID, "You are right!");
      else
        bot.sendMessage(CHAT_ID, "You are wrong!");
      play=false;
    }

    if (text == "/play") {
      play=true;
      one = random(1, 10);
      two = random(1, 10);
      int doin = random(1, 5);
      switch (doin) {
        case 1:
          quize = one + two;
          bot.sendMessage(CHAT_ID, String(one) + "+" + String(two));
          break;
        case 2:
          quize = one - two;
          bot.sendMessage(CHAT_ID, String(one) + "-" + String(two));
          break;
        case 3:
          quize = one * two;
          bot.sendMessage(CHAT_ID, String(one) + "*" + String(two));
          break;
        case 4:
          quize = one / two;
          bot.sendMessage(CHAT_ID, String(one) + "/" + String(two));
          break;
        default:
          break;
      }

    }

  }

}

//знати вільне ID для пальця
bool get_free_id(int16_t * fid) {
  int16_t p = -1;
  for (int page = 0; page < (params.capacity / FPM_TEMPLATES_PER_PAGE) + 1; page++) {
    p = finger.getFreeIndex(page, fid);
    switch (p) {
      case FPM_OK:
        if (*fid != FPM_NOFREEINDEX) {
          bot.sendMessage(CHAT_ID, "Free slot at ID ");
          bot.sendMessage(CHAT_ID, String(*fid));
          return true;
        }
        break;
      case FPM_PACKETRECIEVEERR:
        bot.sendMessage(CHAT_ID, "Communication error!");
        return false;
      case FPM_TIMEOUT:
        bot.sendMessage(CHAT_ID, "Timeout!");
        return false;
      case FPM_READ_ERROR:
        bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
        return false;
      default:
        bot.sendMessage(CHAT_ID, "Unknown error!");
        return false;
    }
    yield();
  }

  bot.sendMessage(CHAT_ID, "No free slots!");
  return false;
}

//знайти відбиток
int search_database(void) {
  int16_t p = -1;

  // first get the finger image
  //bot.sendMessage(CHAT_ID, "Waiting for valid finger");
  //bot.sendChatAction(CHAT_ID, "typing");
  while (p != FPM_OK) {
    p = finger.getImage();
    switch (p) {
      case FPM_OK:
        bot.sendMessage(CHAT_ID, "Image taken");
        break;
      case FPM_NOFINGER:
        //Serial.print(".");
        break;
      case FPM_PACKETRECIEVEERR:
        bot.sendMessage(CHAT_ID, "Communication error");
        break;
      case FPM_IMAGEFAIL:
        bot.sendMessage(CHAT_ID, "Imaging error");
        break;
      case FPM_TIMEOUT:
        bot.sendMessage(CHAT_ID, "Timeout!");
        //return 0;
        break;
      case FPM_READ_ERROR:
        bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
        break;
      default:
        bot.sendMessage(CHAT_ID, "Unknown error");
        break;
    }
    yield();
  }

  // convert it
  p = finger.image2Tz();
  switch (p) {
    case FPM_OK:
      bot.sendMessage(CHAT_ID, "Image converted");
      break;
    case FPM_IMAGEMESS:
      bot.sendMessage(CHAT_ID, "Image too messy");
      return p;
    case FPM_PACKETRECIEVEERR:
      bot.sendMessage(CHAT_ID, "Communication error");
      return p;
    case FPM_FEATUREFAIL:
      bot.sendMessage(CHAT_ID, "Could not find fingerprint features");
      return p;
    case FPM_INVALIDIMAGE:
      bot.sendMessage(CHAT_ID, "Could not find fingerprint features");
      return p;
    case FPM_TIMEOUT:
      bot.sendMessage(CHAT_ID, "Timeout!");
      return p;
    case FPM_READ_ERROR:
      bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
      return p;
    default:
      bot.sendMessage(CHAT_ID, "Unknown error");
      return p;
  }

  // search the database for the converted print
  uint16_t fid, score;
  p = finger.searchDatabase(&fid, &score);

  /* now wait to remove the finger, though not necessary;
     this was moved here after the search because of the R503 sensor,
     which seems to wipe its buffers after each scan */
  //bot.sendMessage(CHAT_ID, "Remove finger");
  //bot.sendChatAction(CHAT_ID, "typing");
  while (finger.getImage() != FPM_NOFINGER) {
    delay(500);
  }

  if (p == FPM_OK) {
    //відбикот правильний
    bot.sendMessage(CHAT_ID, "Found a print match!");
    lock = false;
  } else if (p == FPM_PACKETRECIEVEERR) {
    bot.sendMessage(CHAT_ID, "Communication error");
    return p;
  } else if (p == FPM_NOTFOUND) {
    bot.sendMessage(CHAT_ID, "Did not find a match");
    return p;
  } else if (p == FPM_TIMEOUT) {
    bot.sendMessage(CHAT_ID, "Timeout!");
    return p;
  } else if (p == FPM_READ_ERROR) {
    bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
    return p;
  } else {
    bot.sendMessage(CHAT_ID, "Unknown error");
    return p;
  }

  // знайдено!
  bot.sendMessage(CHAT_ID, "Found ID #");
  bot.sendMessage(CHAT_ID, String(fid));
  bot.sendMessage(CHAT_ID, " with confidence of ");
  bot.sendMessage(CHAT_ID, String(score));
}

//запам'ятати відбиток
int16_t enroll_finger(int16_t fid) {
  int16_t p = -1;
  bot.sendMessage(CHAT_ID, "Waiting for valid finger to enroll");
  bot.sendChatAction(CHAT_ID, "typing");
  while (p != FPM_OK) {
    p = finger.getImage();
    switch (p) {
      case FPM_OK:
        bot.sendMessage(CHAT_ID, "Image taken");
        break;
      case FPM_NOFINGER:
        //Serial.print(".");
        break;
      case FPM_PACKETRECIEVEERR:
        bot.sendMessage(CHAT_ID, "Communication error");
        break;
      case FPM_IMAGEFAIL:
        bot.sendMessage(CHAT_ID, "Imaging error");
        break;
      case FPM_TIMEOUT:
        bot.sendMessage(CHAT_ID, "Timeout!");
        break;
      case FPM_READ_ERROR:
        bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
        break;
      default:
        bot.sendMessage(CHAT_ID, "Unknown error");
        break;
    }
    yield();
  }


  p = finger.image2Tz(1);
  switch (p) {
    case FPM_OK:
      bot.sendMessage(CHAT_ID, "Image converted");
      break;
    case FPM_IMAGEMESS:
      bot.sendMessage(CHAT_ID, "Image too messy");
      return p;
    case FPM_PACKETRECIEVEERR:
      bot.sendMessage(CHAT_ID, "Communication error");
      return p;
    case FPM_FEATUREFAIL:
      bot.sendMessage(CHAT_ID, "Could not find fingerprint features");
      return p;
    case FPM_INVALIDIMAGE:
      bot.sendMessage(CHAT_ID, "Could not find fingerprint features");
      return p;
    case FPM_TIMEOUT:
      bot.sendMessage(CHAT_ID, "Timeout!");
      return p;
    case FPM_READ_ERROR:
      bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
      return p;
    default:
      bot.sendMessage(CHAT_ID, "Unknown error");
      return p;
  }

  bot.sendMessage(CHAT_ID, "Remove finger");
  delay(2000);
  p = 0;
  while (p != FPM_NOFINGER) {
    p = finger.getImage();
    yield();
  }

  p = -1;
  bot.sendMessage(CHAT_ID, "Place same finger again");
  bot.sendChatAction(CHAT_ID, "typing");
  while (p != FPM_OK) {
    p = finger.getImage();
    switch (p) {
      case FPM_OK:
        bot.sendMessage(CHAT_ID, "Image taken");
        break;
      case FPM_NOFINGER:
        //Serial.print(".");
        break;
      case FPM_PACKETRECIEVEERR:
        bot.sendMessage(CHAT_ID, "Communication error");
        break;
      case FPM_IMAGEFAIL:
        bot.sendMessage(CHAT_ID, "Imaging error");
        break;
      case FPM_TIMEOUT:
        bot.sendMessage(CHAT_ID, "Timeout!");
        break;
      case FPM_READ_ERROR:
        bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
        break;
      default:
        bot.sendMessage(CHAT_ID, "Unknown error");
        break;
    }
    yield();
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FPM_OK:
      bot.sendMessage(CHAT_ID, "Image converted");
      break;
    case FPM_IMAGEMESS:
      bot.sendMessage(CHAT_ID, "Image too messy");
      return p;
    case FPM_PACKETRECIEVEERR:
      bot.sendMessage(CHAT_ID, "Communication error");
      return p;
    case FPM_FEATUREFAIL:
      bot.sendMessage(CHAT_ID, "Could not find fingerprint features");
      return p;
    case FPM_INVALIDIMAGE:
      bot.sendMessage(CHAT_ID, "Could not find fingerprint features");
      return p;
    case FPM_TIMEOUT:
      bot.sendMessage(CHAT_ID, "Timeout!");
      return false;
    case FPM_READ_ERROR:
      bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
      return false;
    default:
      bot.sendMessage(CHAT_ID, "Unknown error");
      return p;
  }


  // OK converted!
  p = finger.createModel();
  if (p == FPM_OK) {
    bot.sendMessage(CHAT_ID, "Prints matched!");
  } else if (p == FPM_PACKETRECIEVEERR) {
    bot.sendMessage(CHAT_ID, "Communication error");
    return p;
  } else if (p == FPM_ENROLLMISMATCH) {
    bot.sendMessage(CHAT_ID, "Fingerprints did not match");
    return p;
  } else if (p == FPM_TIMEOUT) {
    bot.sendMessage(CHAT_ID, "Timeout!");
    return p;
  } else if (p == FPM_READ_ERROR) {
    bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
    return p;
  } else {
    bot.sendMessage(CHAT_ID, "Unknown error");
    return p;
  }

  bot.sendMessage(CHAT_ID, "ID ");
  bot.sendMessage(CHAT_ID, String(fid));
  p = finger.storeModel(fid);
  if (p == FPM_OK) {
    bot.sendMessage(CHAT_ID, "Stored!");
    return 0;
  } else if (p == FPM_PACKETRECIEVEERR) {
    bot.sendMessage(CHAT_ID, "Communication error");
    return p;
  } else if (p == FPM_BADLOCATION) {
    bot.sendMessage(CHAT_ID, "Could not store in that location");
    return p;
  } else if (p == FPM_FLASHERR) {
    bot.sendMessage(CHAT_ID, "Error writing to flash");
    return p;
  } else if (p == FPM_TIMEOUT) {
    bot.sendMessage(CHAT_ID, "Timeout!");
    return p;
  } else if (p == FPM_READ_ERROR) {
    bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
    return p;
  } else {
    bot.sendMessage(CHAT_ID, "Unknown error");
    return p;
  }
}
// видалити відбиток
int deleteFingerprint(int fid) {
  int p = -1;

  p = finger.deleteModel(fid);

  if (p == FPM_OK) {
    bot.sendMessage(CHAT_ID, "Deleted!");
  } else if (p == FPM_PACKETRECIEVEERR) {
    bot.sendMessage(CHAT_ID, "Communication error");
    return p;
  } else if (p == FPM_BADLOCATION) {
    bot.sendMessage(CHAT_ID, "Could not delete in that location");
    return p;
  } else if (p == FPM_FLASHERR) {
    bot.sendMessage(CHAT_ID, "Error writing to flash");
    return p;
  } else if (p == FPM_TIMEOUT) {
    bot.sendMessage(CHAT_ID, "Timeout!");
    return p;
  } else if (p == FPM_READ_ERROR) {
    bot.sendMessage(CHAT_ID, "Got wrong PID or length!");
    return p;
  } else {
    bot.sendMessage(CHAT_ID, "Unknown error: 0x");
    bot.sendMessage(CHAT_ID, String(p));
    return p;
  }
}
