#include <Ethernet.h>
#include <SPI.h>
#include <SD.h>
#include <avr/wdt.h>
#define time 1000

byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x0E, 0x96};
IPAddress ip(192, 168, 0, 100);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
EthernetServer server(8080);
EthernetClient client;
byte mailServer[] = {200, 147, 99, 132}; // Colocar o ip do servidor SMTP (no Caso Bol)

String readString;
byte webButton;

const int buttonPin = 2;
int buttonState = 0;

byte sensorPin = 3;
byte sensorInterrupt = 1;
volatile int pulseCount; //pulso do sensor de fluxo
const int pinRelay = 5;
int statusSolen = 0;
float calibrationFactor = 6.9;
float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
unsigned long oldTime;
int arrayFlow[100];
int flowCount;
unsigned long maxFlow;

File myFile;
const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;
int dataNumber = 0;


void setup() {
  wdt_disable();
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.print("Initializing SD card...");

  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();
  pinMode(buttonPin, INPUT);

  pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);
  pinMode(pinRelay, OUTPUT);
  digitalWrite(pinRelay, LOW); //inicia modulo rele desligado

  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  oldTime = 0;
  flowCount = 0;
  maxFlow = 0;
  webButton = 0;

  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);

}

void loop() {
  client = server.available();
  readSavedFlow();
  flow();

  if (client) {
    Serial.println("new client");

    boolean currentLineIsBlank = true;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);

        if (readString.length() < 100) {

          //store characters to string
          readString += c;
          //Serial.print(c);
        }

        //if (c == 'n' && currentLineIsBlank) {
        if (c == '\n') {
          Serial.println(readString);

          client.println(F("HTTP/1.1 200 OK"));
          client.println(F("Content-Type: text/html; charset=utf-8"));
          client.println(F("Connection: close"));
          client.println(F("Refresh: 2"));
          client.println(F(""));
          client.println(F("<!DOCTYPE HTML>"));
          client.println(F("<html>"));
          client.println(F("<head>"));
          client.println(F("<body style=background-color:#FFF>"));
          client.println(F("<title>Monitoramento hídrico residencial</title>"));
          client.println(F("</head>"));

          client.println(F("<h1>Monitoramento hídrico residencial</h1>"));
          client.println(F("<hr>"));

          client.println(F("<br>"));
          client.print(F("Fluxo atual: "));
          client.print(totalMilliLitres);
          client.println(F("ml"));

          client.println(F("<br>"));
          client.println(F("<br>"));

          client.println(F("Se o fluxo for maior que "));
          client.print(dataNumber * 3);
          client.println(F(" o abastecimento de água será interrompido!"));


          client.println(F("<br>"));
          client.println(F("<hr>"));

          //client.print("<input type=submit value='DESLIGAR ÁGUA' style=width:150px;height:45px onClick=location.href='/?on8;'>");
          client.print("<input type=submit value='LIGAR ÁGUA' style=width:100px;height:45px onClick=location.href='/?off5;'>");


          client.println(F("</body>"));
          client.println(F("</html>"));
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != 'r') {
          currentLineIsBlank = false;
        }

      }
    }
    delay(1);
    client.stop();

    /*if (readString.indexOf('8') > 0) //checks for 8
    {
      digitalWrite(pinRelay, HIGH);    // set pin 8 high
      //Serial.println("Led 8 On");
    }*/
    if (readString.indexOf('5') > 0) //checks for 9
      {
      digitalWrite(pinRelay, LOW);    // set pin 5 low
      }

    //clearing string for next read
    readString = "";
  }

  if (totalMilliLitres > (dataNumber * 3)) {
  //if (totalMilliLitres > (dataNumber)) {
    activeSolen();
    //sendMail();
  }

  
    if (millis() > 86400000) {
      // millis 86.400.000 = 1 dia
      // 10 minutos 600000 millis
      //1min 60000
      //30seg 10000
      findMaxFlow();
      //Serial.println(maxFlow);
      //writeMaxFlow();
      if ((maxFlow > (dataNumber + (dataNumber / 2))) && (maxFlow < dataNumber * 3)) {
        writeMaxFlow();
      }
      if (maxFlow < (dataNumber - (dataNumber / 2))) {
        writeMaxFlow();
      }
      delay(10000);
      resetArduino();
    }

}

void flow() {

  if ((millis() - oldTime) > 1000) {
    detachInterrupt(sensorInterrupt);
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
    oldTime = millis();
    flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;

    unsigned int frac;

    if (flowRate > 0) {
      Serial.print("Flow rate: ");
      Serial.print(int(flowRate));
      Serial.print("L/min");
      Serial.print("\t");
      Serial.print("Output Liquid Quantity: ");
      Serial.print(totalMilliLitres);
      Serial.println("mL");
      Serial.print("\t");
      Serial.print(totalMilliLitres / 1000);
      Serial.print("L");
      Serial.print("\n");

    } else {
      if (flowCount < 100) {
        arrayFlow [flowCount] = totalMilliLitres;
        flowRate = 0;
        flowMilliLitres = 0;
        totalMilliLitres = 0;
        flowCount++;
      }

    }
    pulseCount = 0;

    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
    return;
  }
}


void pulseCounter() {
  pulseCount++;
}

void findMaxFlow() {
  for (byte i = 0; i < 100; i = i + 1) {
    if (arrayFlow[i] > 0) {
      maxFlow = max(maxFlow, arrayFlow[i]);
      Serial.println(maxFlow);
    }
  }
}

void writeMaxFlow() {

  myFile = SD.open("flows1.txt", FILE_WRITE);
  if (myFile) {
    //maxFlow = arrayFlow[0];
    Serial.print("Writing to test.txt...");
    //myFile.println("1");
    //myFile.println("2");
    myFile.println(maxFlow);
    //myFile.println(totalMilliLitres);
    //close the file:
    myFile.close();
    Serial.println("maior fluxo gravado.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("erro na gravação do maior fluxo");
  }
}

void readSavedFlow() {
  // re-open the file for reading:
  myFile = SD.open("flows1.txt");
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;
  if (myFile) {
    Serial.println("flows1.txt:");
    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      //Serial.write(myFile.read());
      rc = myFile.read();
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        receivedChars[ndx] = '\0'; // terminate the string
        ndx = 0;
        newData = true;
      }
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("erro na leitura do fluxo salvo");
  }

  if (newData == true) {
    //dataNumber = 0;             // new for this version
    dataNumber = atoi(receivedChars);   // new for this version
    Serial.print("This just in ... ");
    Serial.println(receivedChars);
    Serial.print("Data as Number ... ");    // new for this version
    Serial.println(dataNumber);     // new for this version
    newData = false;
  }
}

void resetArduino() {
  wdt_reset ();
  reset();
}

void reset() {
  while (true) ; // fica aqui até resetar
}

void activeSolen() {
  if (statusSolen == 0) {
    digitalWrite(pinRelay, HIGH); //aciona o relé (ativa o solenoide)
    statusSolen = 1;
  }
}

void desableSolen() {
  if (statusSolen == 1) {
    digitalWrite(pinRelay, LOW); //aciona o relé (ativa o solenoide)
    statusSolen = 0;
  }
}

void sendMail() {
  delay(time);
  Serial.println("conectando...");
  if (client.connect(mailServer, 587)) {
    Serial.println("conectado!");
    Serial.println("enviando email...");
    Serial.println();
    client.println("HELO localhost");
    recebe();
    delay(time);
    client.println("AUTH LOGIN");
    recebe();
    delay(time);
    client.println("YXJkdWlub2xlb0Bib2wuY29tLmJy"); // Email de login em base de 64:
    recebe();
    delay(time);
    client.println("bTQxNmthcjk4"); // Senha do email em base de 64:
    recebe();
    delay(time);
    client.println("mail from: <arduinoleo@bol.com.br>"); //Email remetente
    recebe();
    delay(time);
    client.println("rcpt to: <leonardoprestes@gmail.com>"); // Email destinatário
    recebe();
    delay(time);
    client.println("data");
    recebe();
    delay(time);
    client.println("Subject: Monitoramento hídrico residencial"); // Assunto
    recebe();
    delay(time);
    client.println("Atenção! Foi identificado um vazamento de água na sua residência e o abastecimento foi interrompido! Acesse a página interna do Arduino para restabeler o abastecimento."); // Corpo
    recebe();
    delay(time);
    client.println("."); // Indica fim do email.
    recebe();
    delay(time);
    client.println();
    recebe();
    delay(time);
    Serial.println("email enviado!");
    delay(time);
    if (client.connected()) {
      Serial.println();
      Serial.println("desconectando...");
      client.stop();
      Serial.println();
      Serial.println();
    }
  } else {
    Serial.println("connection failed");
  }
}

void recebe() {
  while (client.available()) {
    char conteudo = client.read();
    Serial.print(conteudo);
  }
}
