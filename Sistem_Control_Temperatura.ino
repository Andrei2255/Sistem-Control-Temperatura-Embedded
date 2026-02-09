#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#define LCD_ADDR 0x27
const int sensorPin = A0;
const int MOSFET_PIN = 9; // acum controleaza TIP31C pe D9
// Butoane (un picior la pin, celălalt la GND)
const int BTN_UP = 2; // crește valoarea
const int BTN_DOWN = 3; // scade valoarea
const int BTN_OK = 4; // OK / Next / Start
const int BTN_BACK = 5; // Înapoi / Stop / Abort
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
volatile bool tick1s = false;
int secunde = 0, minute = 0, ore = 18;
// control temperatură
float setpoint = 25.0; // T_set, se schimba din meniu
bool heatOn = false; // stare bec (încălzire)
// timpi (în secunde)
int tCool = 60; // timp de răcire SETABIL din meniu
int heatTime = 0; // în câte secunde am ajuns la Tset
int coolTime = 0; // cât a durat răcirea (în practică = tCool, dar îl păstrăm separat)
// stare generală sistem
enum Mode { MODE_MENU, MODE_RUN };
enum Phase { PHASE_IDLE, PHASE_HEAT, PHASE_COOL, PHASE_DONE };
Mode curMode = MODE_MENU;
Phase curPhase = PHASE_IDLE;
// timp petrecut în faza curentă (secunde)
int phaseSec = 0;
// meniu: 0 = T_set, 1 = tCool, 2 = START
int menuItem = 0;
const int MENU_LAST = 2; // 0..2
// TIMERE
void setupTimer1_1Hz() {
 noInterrupts();
 TCCR1A = 0;
 TCCR1B = 0;
 OCR1A = 15624; // 1 s la prescaler 1024
 TCCR1B |= (1 << WGM12);
 TCCR1B |= (1 << CS12) | (1 << CS10);
 TIMSK1 |= (1 << OCIE1A);
 interrupts();
}
ISR(TIMER1_COMPA_vect) {
 tick1s = true;
 secunde++;
 if (secunde >= 60) { secunde = 0; minute++; }
 if (minute >= 60) { minute = 0; ore++; }
 if (ore >= 24) { ore = 0; }
}
// CITIRE TEMPERATURA
float readTemperatureC() {
 (void)analogRead(sensorPin); // prima citire aruncata
 long suma = 0;
 const int N = 32;
 for (int i = 0; i < N; i++) {
 suma += analogRead(sensorPin);
 delayMicroseconds(200);
 }
 float raw = suma / (float)N;
 float voltage = raw * (5.0 / 1023.0);
 float tempC = voltage * 100.0; // LM35: 10 mV/°C
 // debug pe Serial
 Serial.print("raw="); Serial.print(raw);
 Serial.print(" V="); Serial.print(voltage, 3);
 Serial.print(" T="); Serial.print(tempC, 2);
 Serial.println(" C");
 return tempC;
}
//Afisare ceas
void printClock(int h, int m, int s) {
 if (h < 10) lcd.print('0'); lcd.print(h); lcd.print(':');
 if (m < 10) lcd.print('0'); lcd.print(m); lcd.print(':');
 if (s < 10) lcd.print('0'); lcd.print(s);
}
// BUTOANE
bool buttonPressed(int pin, const char* name) {
 if (digitalRead(pin) == LOW) { // INPUT_PULLUP => LOW = apasat
 while (digitalRead(pin) == LOW) {
 delay(10);
 }
 delay(20); // mic debounce
 Serial.print("Buton: ");
 Serial.println(name);
 return true;
 }
 return false;
}
// update meniu pe LCD
void showMenu() {
 lcd.clear();
 switch (menuItem) {
 case 0:
 lcd.setCursor(0, 0);
 lcd.print("Set T: ");
 lcd.print(setpoint, 1);
 lcd.print((char)223);
 lcd.print("C");
 lcd.setCursor(0, 1);
 lcd.print("UP/DN +/- OK>Next");
 break;
 case 1:
 lcd.setCursor(0, 0);
 lcd.print("tCool: ");
 lcd.print(tCool);
 lcd.print("s");
 lcd.setCursor(0, 1);
 lcd.print("UP/DN +/- OK>Next");
 break;
 case 2:
 lcd.setCursor(0, 0);
 lcd.print("START ciclul?");
 lcd.setCursor(0, 1);
 lcd.print("OK=Start BACK=<<");
 break;
 }
}
// logica butoane
void handleButtons() {
 // Buton UP
 if (buttonPressed(BTN_UP, "UP")) {
 if (curMode == MODE_MENU) {
 switch (menuItem) {
 case 0: // T_set
 setpoint += 0.5;
 if (setpoint > 80.0) setpoint = 80.0;
 showMenu();
 break;
 case 1: // tCool
 tCool += 10;
 if (tCool > 999) tCool = 999;
 showMenu();
 break;
 case 2:
 // nimic
 break;
 }
 }
 }
 // Buton DOWN
 if (buttonPressed(BTN_DOWN, "DOWN")) {
 if (curMode == MODE_MENU) {
 switch (menuItem) {
 case 0: // T_set
 setpoint -= 0.5;
 if (setpoint < 10.0) setpoint = 10.0;
 showMenu();
 break;
 case 1: // tCool
 tCool -= 10;
 if (tCool < 10) tCool = 10;
 showMenu();
 break;
 case 2:
 // nimic
 break;
 }
 }
 }
 // Buton OK
 if (buttonPressed(BTN_OK, "OK")) {
 if (curMode == MODE_MENU) {
 if (menuItem < MENU_LAST) {
 menuItem++;
 showMenu();
 } else {
 // Suntem pe "START ciclul?" -> verificam Tset fata de Tcur
 float tC = readTemperatureC();
 if (setpoint <= tC + 0.5) {
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Tset < Tcur!");
 lcd.setCursor(0, 1);
 lcd.print("Tcur=");
 lcd.print(tC, 1);
 lcd.print((char)223);
 lcd.print("C");
 delay(2000);
 showMenu(); // nu pornim ciclul
 } else {
 // PORNIM ciclul
 curMode = MODE_RUN;
 curPhase = PHASE_HEAT;
 phaseSec = 0;
 heatTime = 0;
 coolTime = 0;
 heatOn = true; // la inceput incalzim
 digitalWrite(MOSFET_PIN, HIGH); // TIP31C conduce -> bec aprins
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Ciclu pornit...");
 delay(800);
 }
 }
 }
 }
 // Buton BACK
 if (buttonPressed(BTN_BACK, "BACK")) {
 if (curMode == MODE_MENU) {
 if (menuItem > 0) menuItem--;
 else menuItem = MENU_LAST;
 showMenu();
 } else if (curMode == MODE_RUN) {
 // abort ciclul si revenire la meniu
 curMode = MODE_MENU;
 curPhase = PHASE_IDLE;
 phaseSec = 0;
 heatOn = false;
 digitalWrite(MOSFET_PIN, LOW);
 showMenu();
 }
 }
}
//afisare in mod run
void showRunScreen(float tC) {
 // Linia 1: T actuală + T_set
 lcd.setCursor(0, 0);
 lcd.print("T:");
 lcd.print(tC, 1);
 lcd.print((char)223);
 lcd.print("C ");
 lcd.print("S:");
 lcd.print(setpoint, 1);
 lcd.print(" ");
 // Linia 2: faza + timpul în faza / rezumat
 lcd.setCursor(0, 1);
 switch (curPhase) {
 case PHASE_HEAT:
 lcd.print("HEAT t=");
 lcd.print(phaseSec);
 lcd.print("s ");
 break;
 case PHASE_COOL:
 lcd.print("COOL t=");
 lcd.print(phaseSec);
 lcd.print("s ");
 break;
 case PHASE_DONE:
 lcd.print("DONE Th=");
 lcd.print(heatTime);
 lcd.print("s");
 break;
 default:
 lcd.print("IDLE ");
 break;
 }
}
//setup
void setup() {
 lcd.init();
 lcd.backlight();
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Pornire...");
 pinMode(MOSFET_PIN, OUTPUT);
 digitalWrite(MOSFET_PIN, LOW); // la start bec stins
 pinMode(BTN_UP, INPUT_PULLUP);
 pinMode(BTN_DOWN, INPUT_PULLUP);
 pinMode(BTN_OK, INPUT_PULLUP);
 pinMode(BTN_BACK, INPUT_PULLUP);
 Serial.begin(9600);
 analogReference(DEFAULT);
 setupTimer1_1Hz();
 delay(500);
 showMenu();
}
void loop() {
 // citim butoanele tot timpul
 handleButtons();
 // la fiecare 1s facem logica principala
 if (tick1s) {
 tick1s = false;
 float tC = readTemperatureC();
 if (curMode == MODE_RUN) {
 phaseSec++; // timp in faza curenta
 switch (curPhase) {
 case PHASE_HEAT:
 // în HEAT: becul e APRINS pana ajungem la Tset
 heatOn = true;
 digitalWrite(MOSFET_PIN, HIGH);
 if (tC >= setpoint) {
 // am ajuns la temperatura dorita
 heatTime = phaseSec; // memoram în cat timp
 curPhase = PHASE_COOL; // trecem in racire
 phaseSec = 0;
 heatOn = false;
 digitalWrite(MOSFET_PIN, LOW); // stingem becul IMEDIAT
 }
 break;
 case PHASE_COOL:
 // în COOL: becul este stins, doar cronometram racirea
 heatOn = false;
 digitalWrite(MOSFET_PIN, LOW);
 if (phaseSec >= tCool) {
 coolTime = phaseSec;
 curPhase = PHASE_DONE;

 }
 break;
 case PHASE_DONE:
 // ciclu terminat, bec stins; poti apasa BACK pentru meniu
 heatOn = false;
 digitalWrite(MOSFET_PIN, LOW);
 break;
 default:
 heatOn = false;
 digitalWrite(MOSFET_PIN, LOW);
 break;
 }
 // afis run
 showRunScreen(tC);
 } else {
 // în meniu, becul sta stins (siguranta)
 heatOn = false;
 digitalWrite(MOSFET_PIN, LOW);
 }
 }
}