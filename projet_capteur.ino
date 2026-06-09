#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define rxPin 0
#define txPin 1
#define swPin 3
#define clkPin 6
#define dtPin 4
#define VCC 5

#define largeur 128
#define hauteur 64
#define OLED_RESET -1
#define adresse 0x3C
Adafruit_SSD1306 ecranOLED(largeur, hauteur, &Wire, OLED_RESET);

#include <SPI.h>

// Define the MCP41100 OP command bits (only one POT)
// Note: command byte format xxCCxxPP, CC command, PP pot number (01 if selected) 
#define MCP_NOP 0b00000000
#define MCP_WRITE 0b00010001
#define MCP_SHTDWN 0b00100001

const int ssMCPin = 10; // Define the slave select for the digital pot

#define WAIT_DELAY 5000
float valeurr;
const int R5=1000;
const int R3=10000;
const int R4=98000;
const int R1=98000;
float valeur;
const int analog_flex_sensor_out  = A3;
const int analog_paper_sensor_out = A0;
int valeur_resistance=100;
bool setup_R= false;
int choix_read  = 1;
int position    = 0;
int lastCLK     = 0;
int mouvement_accumulated = 0;  // 
int R2=0;
unsigned long lastTime = 0;
float R0;
int16_t encoded ;

// ────────────────────────────────────────────
// Lecture capteur
// ────────────────────────────────────────────
float readCapteur(int choix) {
  if (choix == 0) {
    int ADCflex = analogRead(analog_flex_sensor_out);
    float Vflex = (ADCflex * VCC) / 1023.0;
    float Rflex = 27000 * (VCC / Vflex - 1.0);
    return (Rflex/1000.0);
  } else {
    float resistance = ((1.0 + (R4 / R2)) * (R1 * (1024.0 / analogRead(analog_paper_sensor_out))) - R1 - R3) / 1000.0;
    return (resistance);
  }
}

// ────────────────────────────────────────────
// Encodeur : appelé en continu dans loop()
// Accumule les mouvements sans bloquer
// ────────────────────────────────────────────
void read_encodeur() {
  static int prev_CLK = HIGH;
  int new_CLK = digitalRead(clkPin);
  if (new_CLK == HIGH && new_CLK != prev_CLK) {
    mouvement_accumulated += 1;
  }
  prev_CLK = new_CLK;
  
}

void calibration() {
  while(setup_R== false){
    delay(50);
    valeur = analogRead(analog_paper_sensor_out);
  if (valeur<=508){
    valeur_resistance--;
  }
  else if(valeur>=516){
    valeur_resistance++;
  }
  if((valeur>508&&valeur<516) || valeur_resistance<2 || valeur_resistance>254){
    setup_R=true;
  }
    SPIWrite(MCP_WRITE, valeur_resistance, ssMCPin);
  }
  R2=valeur_resistance*(10000.0/255.0);
  float a = readCapteur(1);
  R0 = a;
  Serial.println(R0);
  }
// ────────────────────────────────────────────
// Affichage OLED + bouton (appelé 1x/seconde)
// ────────────────────────────────────────────
void ecran_oled() {
  // Applique les mouvements accumulés depuis la dernière seconde
  if (mouvement_accumulated != 0) {
    position = (position + mouvement_accumulated % 2 + 3) % 3;
    mouvement_accumulated = 0;  // reset après application
  }

  ecranOLED.clearDisplay();
  ecranOLED.setTextSize(1);
  ecranOLED.setCursor(16,16);
  ecranOLED.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  if (choix_read==1){
    ecranOLED.println("mode papier");
  }
  else {
    ecranOLED.println("mode flex");
  }
  // Option 0 : Capteur flex

  if (position == 0) {
    ecranOLED.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // surligné
  } else {
    ecranOLED.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  }
  ecranOLED.setCursor(0, 0);
  ecranOLED.println("Capteur flex");

  // Option 1 : Capteur papier
  if (position == 1) {
    ecranOLED.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // surligné
  } 
  // Option 2 : Calibration
  if (position == 2) {
    ecranOLED.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  } else {
    ecranOLED.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  }
  ecranOLED.setCursor(0, 48);
  ecranOLED.println("Calibration");
  ecranOLED.setCursor(0, 32);
  ecranOLED.println("Capteur papier");

  // Bouton validation
if (digitalRead(swPin) == LOW) {
    if (position == 2) {
      // Lance calibration
      setup_R = false;   // reset pour relancer
      calibration();
    } else {
      choix_read = position;
    }
    delay(200);
  }
  ecranOLED.display();
}



void SPIWrite(uint8_t cmd, uint8_t data, uint8_t ssPin) // SPI write the command and data to the MCP IC connected to the ssPin
{
  SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0)); //https://www.arduino.cc/en/Reference/SPISettings
  
  digitalWrite(ssPin, LOW); // SS pin low to select chip
  
  SPI.transfer(cmd);        // Send command code
  SPI.transfer(data);       // Send associated value
  
  digitalWrite(ssPin, HIGH);// SS pin high to de-select chip
  SPI.endTransaction();
  }

void setup() {
  Serial.begin(9600);   // 
  pinMode(clkPin, INPUT_PULLUP);
  pinMode(dtPin,  INPUT_PULLUP);
  pinMode(swPin,  INPUT_PULLUP);
  pinMode (ssMCPin, OUTPUT); //select pin output
  digitalWrite(ssMCPin, HIGH); //SPI chip disabled
  SPI.begin(); 

  
  

  if (!ecranOLED.begin(SSD1306_SWITCHCAPVCC, adresse)) {
    while (1);}
 

  lastCLK = digitalRead(clkPin);
  calibration();
}

// ────────────────────────────────────────────
void loop() {
  read_encodeur();   

  if (millis() - lastTime >= 300) {
    lastTime = millis();
    ecran_oled();                     // applique mouvement et affiche
    valeur = readCapteur(choix_read);
    if (choix_read==0) {
      valeur = readCapteur(0);
    } else {
      valeur = readCapteur(1);
      valeurr =valeur;
      valeur = ((valeur - R0) / R0 ) * 100 ;
      uint16_t encoded = (uint16_t)(valeur * 1000 + 1000);
    }
    //Serial.println(valeurr);
    Serial.write(int(valeurr) >> 8);
    Serial.write(int(valeurr) & 0xFF);

  }
}