#define debug 0
#define localip 0    // 0:nicstte.no-ip.org 1:192.168.1.177
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <DNS.h>
#include <LiquidCrystal.h>
#define analogButtonPin 3
#define siren 0

void(* resetFunc) (void) = 0;

byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xAD };
IPAddress ip(192, 168, 1, 177);
IPAddress nicsteeIP(192, 168, 1, 177);
unsigned int localPort = 9876;
char packetBuffer[48]; //buffer to hold incoming packet,

EthernetUDP Udp;
DNSClient dnsClient;
volatile long lastAck = 0;

LiquidCrystal lcd(9, 8, 7, 6, 5, 4);
boolean sirenFlag;
boolean commFlag;
int buttonState = 0;
const int buzzerPin =  3;
const int sirenPin = 2;
int val = 0;
long startSiren;
int count;
volatile long lastCheck;

void setup() {
  lcd.begin(16,2);              // columns, rows.  use 16,2 for a 16x2 LCD, etc.
  lcd.clear();                  // start with a blank screen
  lcd.setCursor(0,0);           // set cursor to column 0, row 0 (the first row)
  lcd.print("SCAN ALARMS");    // change this text to whatever you like. keep it clean.
  mesLCD("starting ...");
  // initialize the Siren pin as an output:
  pinMode(sirenPin, OUTPUT);
  // initialize the Buzzer pin as an output:
  pinMode(buzzerPin, OUTPUT);
  sirenFlag = false;
  startSiren = 0;
  digitalWrite(buzzerPin, LOW);
  digitalWrite(sirenPin, LOW);
  #if debug == 1
  Serial.begin(9600);
  #endif
  while (Ethernet.begin(mac) != 1)
  {
    mesLCD("IP/DHCP error");
    digitalWrite(buzzerPin, HIGH);
    delay(5000);
    digitalWrite(buzzerPin, LOW);
  }
  mesLCD(" ...           ");
  delay(1000);
  #if localip == 0
  dnsClient.begin(Ethernet.dnsServerIP());
  dnsClient.getHostByName("nicstee.no-ip.org", nicsteeIP);
  #endif 
  printLCDIp(nicsteeIP);
  Udp.begin(localPort);
  count = 0;
  commFlag = true;
}

void loop() {
  if( sirenFlag){
      val = analogRead(analogButtonPin);
      #if debug == 1
      Serial.println("Siren/buzzer on");
      Serial.println(val);
      #endif
      if(millis() - startSiren > 60000 || val > 512){
      // turn SIREN off:    
      sirenFlag = false;
      startSiren = 0;
      #if debug == 1  
      Serial.println("Siren/buzzer off");
      #endif
      digitalWrite(buzzerPin, LOW);
      digitalWrite(sirenPin, LOW);
    }
  }
  else{
       if( millis()  - lastCheck > 3600000*25  )resetFunc();
  }
  if( millis() - lastAck > 30000  ) {
       #if debug == 1
       Serial.println("Hello sent to nicstee Arduino Alarm server");
       #endif
       lastAck = millis();
       Udp.beginPacket(nicsteeIP, 8888);
       Udp.print("Hello");
       Udp.endPacket();
       count++;
       if(count > 10){
        count = 0;
        commFlag = false;
        mesLCD("Comm. down !!!");
        digitalWrite(buzzerPin, HIGH);
        delay(5000);
        resetFunc();
      }
  }
  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    Udp.read(packetBuffer,48);
    count = 0;
    if(!commFlag){
      commFlag = true;
      mesLCD("Comm. reset");
    }
    #if debug == 1
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    printIp(Udp.remoteIP());
    Serial.print(", port ");
    Serial.println(Udp.remotePort());
    Serial.println("Contents:");
    for(int i = 0; i < packetSize ; i++){
      Serial.print(packetBuffer[i]);
    }
    Serial.println("");
    #endif
    String sDate;
    for(int i = 0; i < 5 ; i++){
        sDate = sDate + packetBuffer[i];
    }
    String sHour;
    for(int i = 5; i < 14 ; i++){
        sHour = sHour + packetBuffer[i];
    }
//  restart
    if( (millis()  - lastCheck > 3600000) && (sHour.substring(1,3) == "00") )resetFunc();
//
    String line1;
    for(int i = 15; i < packetSize ; i++){
      line1 = line1 + packetBuffer[i];
    }
    String s_hit = line1.substring(line1.indexOf("hit=")+4,line1.indexOf(" "));
    #if debug == 1
    Serial.print("hit => ");Serial.println(s_hit);
    #endif
    String subLine1 = line1.substring(line1.indexOf("alarm="),line1.length());
    String s_alarm = subLine1.substring(subLine1.indexOf("alarm=")+6,subLine1.indexOf(" "));
    #if debug == 1
    Serial.print("alarm => ");Serial.println(s_alarm);
    #endif
    int alarm = s_alarm.toInt();
    int hit = s_hit.toInt();
    if(hit == 15){
      lcd.setCursor(0,0);           // set cursor to column 0, row 0 (the first row)        
      lcd.print(sDate+sHour);
    }else{
      lcd.clear();
      lcd.setCursor(0,0);           // set cursor to column 0, row 0 (the first row)        
      lcd.print(sDate+sHour);
      lcd.setCursor(0,1);           // set cursor to column 0, row 1 (the first row)
      if(alarm > 0 ) lcd.print("alrm=" + s_alarm +sHour);
      else lcd.print("hit=" + s_hit+sHour);
    }
    if(alarm > 0){
      #if debug == 1
      Serial.print("Activer la sirÃ¨ne/buzzer !!!");
      #endif
      #if siren == 0
      digitalWrite(buzzerPin, HIGH);
      #endif
      #if siren == 1
      digitalWrite(sirenPin, HIGH);
      #endif
      sirenFlag = true;
      if(startSiren == 0)startSiren = millis();
    }
  }
  delay(10);
}

void printLCDIp(IPAddress ip)
{
  lcd.setCursor(0,1);           // set cursor to column 0, row 1
  for (int iii=0;iii<4;iii++) {
    if (iii!=0)
      lcd.print(".");
      lcd.print(ip[iii]);
  }
}

void printIp(IPAddress ip)
{
  for (int iii=0;iii<4;iii++) {
    if (iii!=0)
      Serial.print(".");
      Serial.print(ip[iii]);
  }
}

void mesLCD(String message){
   lcd.setCursor(0,1);           // set cursor to column 0, row 1
   lcd.print(message); 

}
