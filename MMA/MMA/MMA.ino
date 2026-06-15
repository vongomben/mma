/*******************************************************************
   ESP8266 Wemos D1 mini + Telegram Bot + Relay

   Comandi:
   acqua          -> attiva il relay per il tempo impostato
   /t 10          -> imposta il tempo del relay a 10 secondi
   /s 23          -> imposta l'orario serale alle 23:00
   /m 6           -> imposta l'orario mattutino alle 06:00
   /stato         -> mostra lo stato attuale
   /serale si     -> abilita acqua automatica serale
   /serale no     -> disabilita acqua automatica serale
   /mattina si    -> abilita acqua automatica mattutina
   /mattina no    -> disabilita acqua automatica mattutina

   Relay Shield Wemos D1 mini:
   D1 = GPIO5
 *******************************************************************/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <time.h>

// =======================
// WIFI MULTIPLI
// =======================

const char *WIFI_SSIDS[] = {
  "wifi1",
  "wifi2"
};

const char *WIFI_PASSWORDS[] = {
  "wifi2",
  "wifi2p"
};

const int NUM_WIFI = sizeof(WIFI_SSIDS) / sizeof(WIFI_SSIDS[0]);

// =======================
// TELEGRAM BOT
// =======================

#define BOT_TOKEN "token"

// =======================
// RELAY
// =======================

#define RELAY_PIN D1  // D1 = GPIO5

// =======================
// CHAT ID AUTORIZZATI
// =======================

const String AUTHORIZED_CHAT_IDS[] = {
  "user1",
  "user2"
};

const int NUM_AUTHORIZED_CHAT_IDS = sizeof(AUTHORIZED_CHAT_IDS) / sizeof(AUTHORIZED_CHAT_IDS[0]);

// =======================
// VARIABILI GLOBALI
// =======================

const unsigned long BOT_MTBS = 1000;
unsigned long bot_lasttime = 0;

// tempo di apertura relay in secondi
int tempoAcqua = 3;

// orari automazioni
int oraSerale = 23;
int oraMattutina = 6;

// booleani automazioni
bool acquaSerale = true;
bool acquaMattutina = true;

// per evitare che l'automazione parta più volte nello stesso giorno
int ultimoGiornoSerale = -1;
int ultimoGiornoMattutino = -1;

// =======================
// TELEGRAM CLIENT
// =======================

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// =======================
// FUNZIONE CONNESSIONE WIFI
// =======================

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(500);

  while (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < NUM_WIFI; i++) {
      Serial.println();
      Serial.print("Provo a connettermi a: ");
      Serial.println(WIFI_SSIDS[i]);

      WiFi.begin(WIFI_SSIDS[i], WIFI_PASSWORDS[i]);

      unsigned long startAttemptTime = millis();

      while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 12000) {
        Serial.print(".");
        delay(500);
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("WiFi connesso a: ");
        Serial.println(WIFI_SSIDS[i]);

        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        return;
      }

      Serial.println();
      Serial.print("Connessione fallita a: ");
      Serial.println(WIFI_SSIDS[i]);

      WiFi.disconnect();
      delay(1000);
    }

    Serial.println("Nessuna rete disponibile. Riprovo tra 5 secondi...");
    delay(5000);
  }
}

// =======================
// FUNZIONE AUTORIZZAZIONE
// =======================

bool isAuthorized(String chat_id) {
  for (int i = 0; i < NUM_AUTHORIZED_CHAT_IDS; i++) {
    if (chat_id == AUTHORIZED_CHAT_IDS[i]) {
      return true;
    }
  }

  return false;
}

// =======================
// FUNZIONE RELAY
// =======================

void apriAcqua(int secondi) {
  Serial.print("Relay ON per ");
  Serial.print(secondi);
  Serial.println(" secondi");

  digitalWrite(RELAY_PIN, HIGH);
  delay((unsigned long)secondi * 1000UL);
  digitalWrite(RELAY_PIN, LOW);

  Serial.println("Relay OFF");
}

// =======================
// INVIA MESSAGGIO A TUTTI GLI AUTORIZZATI
// =======================

void sendMessageToAuthorizedUsers(String message) {
  for (int i = 0; i < NUM_AUTHORIZED_CHAT_IDS; i++) {
    bot.sendMessage(AUTHORIZED_CHAT_IDS[i], message, "");
  }
}

// =======================
// CONTROLLO ORARIO AUTOMATICO
// =======================

void controllaAutomazioniOrarie() {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  int ora = timeinfo->tm_hour;
  int minuto = timeinfo->tm_min;
  int giornoAnno = timeinfo->tm_yday;

  // Acqua mattutina all'orario impostato
  if (acquaMattutina == true && ora == oraMattutina && minuto == 0 && ultimoGiornoMattutino != giornoAnno) {
    ultimoGiornoMattutino = giornoAnno;

    Serial.println("Avvio automatico acqua mattutina");

    sendMessageToAuthorizedUsers(
      "Avvio automatico acqua mattutina alle " + String(oraMattutina) + ":00 per " + String(tempoAcqua) + " secondi."
    );

    apriAcqua(tempoAcqua);

    sendMessageToAuthorizedUsers("Acqua mattutina conclusa.");
  }

  // Acqua serale all'orario impostato
  if (acquaSerale == true && ora == oraSerale && minuto == 0 && ultimoGiornoSerale != giornoAnno) {
    ultimoGiornoSerale = giornoAnno;

    Serial.println("Avvio automatico acqua serale");

    sendMessageToAuthorizedUsers(
      "Avvio automatico acqua serale alle " + String(oraSerale) + ":00 per " + String(tempoAcqua) + " secondi."
    );

    apriAcqua(tempoAcqua);

    sendMessageToAuthorizedUsers("Acqua serale conclusa.");
  }
}

// =======================
// TEMPO MANCANTE
// =======================

String tempoMancanteA(int targetHour, int targetMinute) {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  int oraAttuale = timeinfo->tm_hour;
  int minutoAttuale = timeinfo->tm_min;
  int secondoAttuale = timeinfo->tm_sec;

  int secondiAttuali = oraAttuale * 3600 + minutoAttuale * 60 + secondoAttuale;
  int secondiTarget = targetHour * 3600 + targetMinute * 60;

  int differenza = secondiTarget - secondiAttuali;

  if (differenza < 0) {
    differenza += 24 * 3600;
  }

  int ore = differenza / 3600;
  int minuti = (differenza % 3600) / 60;
  int secondi = differenza % 60;

  String risultato = "";
  risultato += String(ore);
  risultato += "h ";
  risultato += String(minuti);
  risultato += "m ";
  risultato += String(secondi);
  risultato += "s";

  return risultato;
}

// =======================
// GESTIONE MESSAGGI
// =======================

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    text.trim();
    text.toLowerCase();

    Serial.println("----- NUOVO MESSAGGIO -----");
    Serial.print("Chat ID: ");
    Serial.println(chat_id);
    Serial.print("Testo ricevuto: ");
    Serial.println(text);
    Serial.println("---------------------------");

    // =======================
    // CONTROLLO AUTORIZZAZIONE
    // =======================

    if (!isAuthorized(chat_id)) {
      Serial.println("Utente NON autorizzato");
      Serial.print("Chat ID non autorizzato: ");
      Serial.println(chat_id);

      String message = "Accesso non autorizzato.\n";
      message += "Il tuo Chat ID è:\n";
      message += chat_id;

      bot.sendMessage(chat_id, message, "");
      continue;
    }

    Serial.println("Utente autorizzato");

    // =======================
    // COMANDO /start
    // =======================

    if (text == "/start") {
      String message = "Comandi disponibili:\n";
      message += "acqua - apre il relay\n";
      message += "/t 5 - imposta il tempo di apertura a 5 secondi\n";
      message += "/s 23 - imposta l'orario serale alle 23:00\n";
      message += "/m 6 - imposta l'orario mattutino alle 06:00\n";
      message += "/stato - mostra lo stato attuale\n";
      message += "/serale si - abilita acqua automatica serale\n";
      message += "/serale no - disabilita acqua automatica serale\n";
      message += "/mattina si - abilita acqua automatica mattutina\n";
      message += "/mattina no - disabilita acqua automatica mattutina";

      bot.sendMessage(chat_id, message, "");
    }

    // =======================
    // COMANDO /t NUMERO
    // =======================

    else if (text.startsWith("/t ")) {
      String secondsText = text.substring(3);
      secondsText.trim();

      int nuovoTempo = secondsText.toInt();

      if (nuovoTempo > 0 && nuovoTempo <= 300) {
        tempoAcqua = nuovoTempo;

        bot.sendMessage(
          chat_id,
          "Tempo acqua aggiornato a " + String(tempoAcqua) + " secondi.",
          ""
        );
      } else {
        bot.sendMessage(
          chat_id,
          "Valore non valido. Usa per esempio: /t 5\nValore minimo: 1\nValore massimo: 300",
          ""
        );
      }
    }

    // =======================
    // COMANDO /s NUMERO
    // =======================

    else if (text.startsWith("/s ")) {
      String oraText = text.substring(3);
      oraText.trim();

      int nuovaOra = oraText.toInt();

      if (nuovaOra >= 0 && nuovaOra <= 23) {
        oraSerale = nuovaOra;

        bot.sendMessage(
          chat_id,
          "Orario acqua serale aggiornato alle " + String(oraSerale) + ":00.",
          ""
        );
      } else {
        bot.sendMessage(
          chat_id,
          "Valore non valido. Usa un numero da 0 a 23. Esempio: /s 23",
          ""
        );
      }
    }

    // =======================
    // COMANDO /m NUMERO
    // =======================

    else if (text.startsWith("/m ")) {
      String oraText = text.substring(3);
      oraText.trim();

      int nuovaOra = oraText.toInt();

      if (nuovaOra >= 0 && nuovaOra <= 23) {
        oraMattutina = nuovaOra;

        bot.sendMessage(
          chat_id,
          "Orario acqua mattutina aggiornato alle " + String(oraMattutina) + ":00.",
          ""
        );
      } else {
        bot.sendMessage(
          chat_id,
          "Valore non valido. Usa un numero da 0 a 23. Esempio: /m 6",
          ""
        );
      }
    }

    // =======================
    // COMANDO acqua
    // =======================

    else if (text == "acqua") {
      bot.sendMessage(
        chat_id,
        "Apro l'acqua per " + String(tempoAcqua) + " secondi.",
        ""
      );

      apriAcqua(tempoAcqua);

      bot.sendMessage(chat_id, "Acqua chiusa.", "");
    }

    // =======================
    // COMANDO /serale si
    // =======================

    else if (text == "/serale si") {
      acquaSerale = true;
      bot.sendMessage(
        chat_id,
        "Acqua serale abilitata. Partirà ogni giorno alle " + String(oraSerale) + ":00.",
        ""
      );
    }

    // =======================
    // COMANDO /serale no
    // =======================

    else if (text == "/serale no") {
      acquaSerale = false;
      bot.sendMessage(chat_id, "Acqua serale disabilitata.", "");
    }

    // =======================
    // COMANDO /mattina si
    // =======================

    else if (text == "/mattina si") {
      acquaMattutina = true;
      bot.sendMessage(
        chat_id,
        "Acqua mattutina abilitata. Partirà ogni giorno alle " + String(oraMattutina) + ":00.",
        ""
      );
    }

    // =======================
    // COMANDO /mattina no
    // =======================

    else if (text == "/mattina no") {
      acquaMattutina = false;
      bot.sendMessage(chat_id, "Acqua mattutina disabilitata.", "");
    }

    // =======================
    // COMANDO /stato
    // =======================

    else if (text == "/stato") {
      time_t now = time(nullptr);
      struct tm *timeinfo = localtime(&now);

      char bufferOra[30];
      strftime(bufferOra, sizeof(bufferOra), "%d/%m/%Y %H:%M:%S", timeinfo);

      String message = "Stato attuale:\n";
      message += "Ora ESP8266: ";
      message += String(bufferOra);
      message += "\n";

      message += "WiFi connesso: ";
      message += WiFi.SSID();
      message += "\n";

      message += "IP: ";
      message += WiFi.localIP().toString();
      message += "\n";

      message += "Tempo acqua: ";
      message += String(tempoAcqua);
      message += " secondi\n";

      message += "Acqua serale ";
      message += String(oraSerale);
      message += ":00: ";
      message += acquaSerale ? "si" : "no";
      message += "\n";

      if (acquaSerale) {
        message += "Manca alla serale: ";
        message += tempoMancanteA(oraSerale, 0);
        message += "\n";
      } else {
        message += "Manca alla serale: disattivata\n";
      }

      message += "Acqua mattutina ";
      message += String(oraMattutina);
      message += ":00: ";
      message += acquaMattutina ? "si" : "no";
      message += "\n";

      if (acquaMattutina) {
        message += "Manca alla mattutina: ";
        message += tempoMancanteA(oraMattutina, 0);
      } else {
        message += "Manca alla mattutina: disattivata";
      }

      bot.sendMessage(chat_id, message, "");
    }

    // =======================
    // COMANDO NON RICONOSCIUTO
    // =======================

    else {
      bot.sendMessage(
        chat_id,
        "Comando non riconosciuto. Usa /start per vedere i comandi.",
        ""
      );
    }
  }
}

// =======================
// SETUP
// =======================

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  connectToWiFi();

  secured_client.setTrustAnchors(&cert);

  Serial.print("Retrieving time: ");

  // Recupera l'orario via NTP in UTC
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);

  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }

  Serial.println();

  // Timezone Roma: CET/CEST con ora legale automatica
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();

  now = time(nullptr);

  struct tm *timeinfo = localtime(&now);

  char bufferOra[30];
  strftime(bufferOra, sizeof(bufferOra), "%d/%m/%Y %H:%M:%S", timeinfo);

  Serial.print("Ora italiana recuperata: ");
  Serial.println(bufferOra);

  Serial.println("Bot pronto.");

  Serial.print("Tempo acqua: ");
  Serial.print(tempoAcqua);
  Serial.println(" secondi");

  Serial.print("Acqua serale: ");
  Serial.println(acquaSerale ? "si" : "no");

  Serial.print("Ora serale: ");
  Serial.print(oraSerale);
  Serial.println(":00");

  Serial.print("Acqua mattutina: ");
  Serial.println(acquaMattutina ? "si" : "no");

  Serial.print("Ora mattutina: ");
  Serial.print(oraMattutina);
  Serial.println(":00");
}

// =======================
// LOOP
// =======================

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnesso. Riconnessione...");
    connectToWiFi();
  }

  controllaAutomazioniOrarie();

  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("Got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
}
