const byte switchPin = 2;
const byte ledPin = 7;
volatile byte state = digitalRead(switchPin);
volatile byte setupComplete = false;

void setup() {
  Serial.begin(9600);
  pinMode(switchPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(switchPin), switchFlipped, CHANGE);

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW); // turn off built in led
}

void loop() {
  delay(100);
    if (Serial.available() > 0) {
    char byte = Serial.read();
    if (byte == '0') {
      digitalWrite(ledPin, LOW);
    } else if (byte == '1') {
      digitalWrite(ledPin, HIGH);
    }
  }
  Serial.print(state);
}

void switchFlipped() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200) {
    state = !state;
  }
  last_interrupt_time = interrupt_time;
}