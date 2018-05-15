// -----------------  Аналоговый вход A0
void initA0() {
  static uint16_t t = readArgsInt();
  static uint8_t averageFactor = readArgsInt();
  if (t < 500) t = 1000;
  if (averageFactor == 0) averageFactor = 1023;
  analogRead(A0);
  sendStatus(stateA0S, analogRead(A0));
  sendOptions(alarmA0S, 0);
  alarmLoad(stateA0S, highalarmA0S, lowalarmA0S);
  ts.add(3, t, [&](void*) {
    uint32_t a = 0;
    for (uint8_t i = 1; i <= 10; i++) {
      a += analogRead(A0);
    }
    a = a / 10;
    sendStatus(stateA0S, a);
    alarmTest(stateA0S, highalarmA0S, lowalarmA0S, alarmA0S);
  }, nullptr, true);
  HTTP.on("/analog.json", HTTP_GET, []() {
    String data = graf3(getStatusInt(stateA0S), getOptionsInt(highalarmA0S), getOptionsInt(lowalarmA0S), 10, t, "low:0");
    httpOkJson(data);
  });
  modulesReg("analog");
}
// ----------------- OneWire -------------------------------------------------------------------------------
void initOneWire() {
  uint8_t pin = readArgsInt();
  static uint16_t t = readArgsInt();
  static uint8_t averageFactor = readArgsInt();
  if (t < 750) t = 1000;
  //Serial.println(t);
  oneWire =  new OneWire(pin);
  sensors.setOneWire(oneWire);
  sensors.begin();
  byte num = sensors.getDS18Count();

  for (byte i = 0; i < num; i++) {
    sendStatus(temperatureS + (String)(i + 1), (String)sensors.getTempCByIndex(i));
    modulesReg(temperatureS + (String)(i + 1));
    sendOptions(alarmtempS + (String)(i + 1), 0);
    sendOptions(highalarmtempS + (String)(i + 1), 0);
    sendOptions(lowalarmtempS + (String)(i + 1), 0);
    alarmLoad(temperatureS + (String)(i + 1), highalarmtempS + (String)(i + 1), lowalarmtempS + (String)(i + 1));
  }
  sendOptions(temperatureS + "num", num);
  ts.add(4, t, [&](void*) {
    static float oldTemp = 0;
    float temp = 0;
    sensors.requestTemperatures();
    for (byte i = 0; i < sensors.getDS18Count(); i++) {
      temp = sensors.getTempCByIndex(i);
      String num = "";
      num = (String)(i + 1);
      sendStatus(temperatureS + num, (String)temp);
      alarmTest(temperatureS + num, highalarmtempS + num, lowalarmtempS + num, alarmtempS + num);
    }
  }, nullptr, true);

  HTTP.on("/temperature.json", HTTP_GET, []() {
    String num = HTTP.arg("n");
    String data = graf3(getStatusFloat(temperatureS + num), getOptionsFloat(highalarmtempS + num), getOptionsFloat(lowalarmtempS + num), 10, t, "low:0");
    httpOkJson(data);
  });
}
// -----------------  DHT
void initDHT() {
  uint8_t pin = readArgsInt();
  dht.setup(pin);
  delay(1000);
  static uint16_t t = readArgsInt();
  static uint16_t test = dht.getMinimumSamplingPeriod();
  if (t < test) t = test;
  //Serial.println(t);
  String temp = "";
  temp += dht.getTemperature();
  //  Serial.println(temp);
  if (temp != "nan") {
    sendStatus(temperatureS, dht.getTemperature());
    sendOptions(alarmtempS, 0);
    alarmLoad(temperatureS, highalarmtempS, lowalarmtempS);
    sendStatus(humidityS, dht.getHumidity());
    sendOptions(alarmhumS, 0);
    alarmLoad(humidityS, highalarmhumS, lowalarmhumS);
    ts.add(5, test, [&](void*) {
      sendStatus(temperatureS, dht.getTemperature());
      sendStatus(humidityS, dht.getHumidity());
      alarmTest(temperatureS, highalarmtempS, lowalarmtempS, alarmtempS);
      alarmTest(humidityS, highalarmhumS, lowalarmhumS, alarmhumS);
    }, nullptr, true);
    HTTP.on("/temperature.json", HTTP_GET, []() {
      //      String data = graf(getStatusInt(temperatureS), 10, t, "low:0");
      String data = graf3(getStatusFloat(temperatureS), getOptionsFloat(highalarmtempS), getOptionsFloat(lowalarmtempS), 10, t, "low:0");
      httpOkJson(data);
    });
    HTTP.on("/humidity.json", HTTP_GET, []() {
      //      String data = graf(getStatusInt(humidityS), 10, t, "low:0");
      String data = graf3(getStatusFloat(humidityS), getOptionsFloat(highalarmhumS), getOptionsFloat(lowalarmhumS), 10, t, "low:0");
      httpOkJson(data);
    });
    modulesReg(temperatureS);
    modulesReg(humidityS);
  }
}
// ----------------------- Загрузка данных уровней сработки ------------------------------------------
void alarmLoad(String sName, String high, String low) {
  String configSensor = readFile(ScenaryS, 4096);
  configSensor.replace("\r\n", "\n");
  configSensor.replace("\n\n", "\n");
  configSensor += "\n";
  do {
    String test = selectToMarker(configSensor, "\n");
    // if temperature1 > 26.50
    String del = "if " + sName + " >";
    if (test.indexOf(del) != -1) {
      //      Serial.println(test);
      test.replace(del, "");
      sendOptionsF(high, test.toFloat());
    }
    del = "if " + sName + " <";
    if (test.indexOf(del) != -1) {
      //      Serial.println(test);
      test.replace(del, "");
      sendOptionsF(low, test.toFloat());
    }
    configSensor = deleteBeforeDelimiter(configSensor, "\n"); //Откидываем обработанную строку
    configSensor = deleteBeforeDelimiterTo(configSensor, "if"); // откидываем все до следующего if
  } while (configSensor.length() != 0);
}
// ------------------------------- Проверка уровней ------------------------------------------------
// Текущее значение сенсора уровни признак датчика
void alarmTest(String value, String high, String low, String sAlarm ) {
  if (getOptionsFloat(high) == 0 || getOptionsFloat(low) == 0){ // нужно добавить флаг остановки теста
    if (getStatusFloat(value) > getOptionsFloat(high) && getOptionsInt(sAlarm) == LOW) {
      sendOptions(sAlarm, HIGH);
      flag = sendStatus(value, getStatusFloat(value));
    }
  if (!flag) {
    if (getStatusFloat(value) < getOptionsFloat(low) && getOptionsInt(sAlarm) == HIGH) {
      sendOptions(sAlarm, LOW);
      flag = sendStatus(value, getStatusFloat(value));
    }
  }
}
}
// ----------------------Приемник ИK
void irReceived() {
  byte pin = readArgsInt();
  if (pin == 1 || pin == 3)  Serial.end();
  irReceiver = new IRrecv(pin);  // Create a new IRrecv object. Change to what ever pin you need etc.
  irReceiver->enableIRIn(); // Start the receiver
  // задача опрашивать RC код
  ts.add(6, 100, [&](void*) {
    handleIrReceiv();
  }, nullptr, true);
  sendStatus(irReceivedS, "ffffffff");
  modulesReg(irReceivedS);
}
void handleIrReceiv() {
  if (irReceiver->decode(&results)) {
    //serialPrintUint64(results.value, HEX);
    //Serial.println("");
    dump(&results);
    flag = sendStatus(irReceivedS, String((uint32_t) results.value, HEX));
    irReceiver->resume();  // Continue looking for IR codes after your finished dealing with the data.
  }
}
void dump(decode_results *results) {
  // Dumps out the decode_results structure.
  // Call this after IRrecv::decode()
  uint16_t count = results->rawlen;
  if (results->decode_type == UNKNOWN) {
    sendOptions(irDecodeTypeS, "UNKNOWN");
  } else if (results->decode_type == NEC) {
    sendOptions(irDecodeTypeS, "NEC");
  } else if (results->decode_type == SONY) {
    sendOptions(irDecodeTypeS, "SONY");
  } else if (results->decode_type == RC5) {
    sendOptions(irDecodeTypeS, "RC5");
  } else if (results->decode_type == RC5X) {
    sendOptions(irDecodeTypeS, "RC5X");
  } else if (results->decode_type == RC6) {
    sendOptions(irDecodeTypeS, "RC6");
  } else if (results->decode_type == RCMM) {
    sendOptions(irDecodeTypeS, "RCMM");
  } else if (results->decode_type == PANASONIC) {
    sendOptions(irDecodeTypeS, "PANASONIC");
  } else if (results->decode_type == LG) {
    sendOptions(irDecodeTypeS, "LG");
  } else if (results->decode_type == JVC) {
    sendOptions(irDecodeTypeS, "JVC");
  } else if (results->decode_type == AIWA_RC_T501) {
    sendOptions(irDecodeTypeS, "AIWA_RC_T501");
  } else if (results->decode_type == WHYNTER) {
    sendOptions(irDecodeTypeS, "WHYNTER");
  } else if (results->decode_type == NIKAI) {
    sendOptions(irDecodeTypeS, "NIKAI");
  }
}
// ----------------------Приемник на 433мГ
void rfReceived() {
  byte pin = readArgsInt();
  if (pin == 1 || pin == 3)  Serial.end();
  mySwitch.enableReceive(pin);
  // задача опрашивать RC код
  ts.add(7, 5, [&](void*) {
    // handleRfReceiv();
  }, nullptr, true);
  sendStatus(rfReceivedS, 0);
  sendStatus(rfBitS, 0);
  sendStatus(rfProtocolS, 0);
  modulesReg(rfReceivedS);
}
void handleRfReceiv() {
  if (mySwitch.available()) {
    int value = mySwitch.getReceivedValue();
    if (value == 0) {
      sendStatus(rfReceivedS, 0);
      sendStatus(rfBitS, 0);
      sendStatus(rfProtocolS, 0);
    } else {
      uint32_t temp = mySwitch.getReceivedValue() ;
      flag = sendStatus(rfReceivedS, temp);
      temp = mySwitch.getReceivedBitlength();
      sendStatus(rfBitS, temp);
      temp = mySwitch.getReceivedProtocol();
      sendStatus(rfProtocolS, temp);
    }
    mySwitch.resetAvailable();
  }
}
// -----------------  Кнопка
void initTach() {
  uint8_t pin = readArgsInt(); // первый аргумент pin
  String num = readArgsString(); // второй аргумент прификс реле 0 1 2
  uint16_t bDelay = readArgsInt(); // третий время нажатия
  sendStatus(stateTachS + num, 0);
  buttons[num.toInt()].attach(pin);
  buttons[num.toInt()].interval(bDelay);
  but[num.toInt()] = true;
  boolean inv = readArgsInt(); // четвертый аргумент инверсия входа
  sendOptions("invTach" + num, inv);
  String m = readArgsString(); // подключаем движение
  if (m == "m") {
    sendOptions(movementTimeS, readArgsInt()); // время тригера
    sendStatus(stateMovementS, 0);
    modulesReg("movement");
    HTTP.on("/movement.json", HTTP_GET, []() {
      String data = graf(getStatusInt(stateMovementS), 10, 1000, "low:0");
      httpOkJson(data);
    });
    sCmd.addCommand("motion",     motionOn); //
    commandsReg("motion");
  }
}
void handleButtons() {
  static uint8_t num = 0;
  if (but[num]) {
    buttons[num].update();
    //if (buttons[num].fell() != getStatusInt(stateTachS + String(num, DEC))) {
    if (buttons[num].fell()) {
      flag = sendStatus(stateTachS + String(num, DEC), !getOptionsInt("invTach" + String(num)));
    }
    //if (buttons[num].rose() != getStatusInt(stateTachS + String(num, DEC))) {
    if (buttons[num].rose()) {
      flag = sendStatus(stateTachS + String(num, DEC), getOptionsInt("invTach" + String(num)));
    }
    //buttons[num].rose()
  }
  num++;
  if (num == 8) num = 0;
}
// -----------------  Движение
void motionOn() {
  uint16_t t = getOptionsInt(movementTimeS);
  motion.attach(t, motionOff);
  if (!getStatusInt(stateMovementS)) {
    flag = sendStatus(stateMovementS, 1);
  }
}
void motionOff() {
  motion.detach();
  if (getStatusInt(stateMovementS)) {
    flag = sendStatus(stateMovementS, 0);
  }
}
