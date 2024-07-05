#include <Arduino.h>
#include <Adafruit_NeoPixel.h> // Led Strip
#include <GyverNTP.h>          // NTP
#include <WiFi.h>              // Wi-Fi
#include <Preferences.h>       // Preferences
#include <GyverEncoder.h>      // Encoder

#include <Wire.h>   // I2C
#include <BH1750.h> // Light sensor
#include <uRTCLib.h> // Time sensor

#define _DEBUG

#ifdef _DEBUG
#define PRINT(x) Serial.print(x)
#define PRINTLN(x) Serial.println(x)
#endif

#define LED_PIN 26        // пин подключения адресной ленты
#define NUM_PIXELS 30     // количество светодиодов на ленте
#define ENC_S1 25         // пин датчика 1 на энкодере
#define ENC_S2 33         // пин датчика 2 на энкодере
#define ENC_KEY 32        // пин кнопки энкодера
#define AUDIO_PIN 35      // пин подключения датчика звука
#define MOVE_PIN 34       // пин подключения датчика движения
#define SERIAL_TIMEOUT 10 // ms
#define TIMEZONE_GMT 5    // sec (+5 часов)

Preferences preferences;                                               // создание объекта да Использование flash-памяти
WiFiClass WiFi;                                                        // создание объекта WiFi Класса
GyverNTP ntp(TIMEZONE_GMT);                                            // создание объекта ntp сервера для получения реального времени
uRTCLib rtc(0x68);                                                     // создание объекта для подлючение rtc датчика
BH1750 lightMeter(0x23);                                               // создание объекта для подлючение люксометра 0x23 при низком уровне на пине ADDR и 0x5C при высоком
Encoder enc1(ENC_S2, ENC_S1, ENC_KEY);                                 // создание объекта энкодера
Adafruit_NeoPixel NeoPixel(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800); // создание объекта сетодиодной ленты

TaskHandle_t Task1;

u_int8_t hue = 127;        // централтьное положение крутилки и обычный белый свет
u_int8_t buff_color = 0;   // буфер который преобразовывет map(hue, 0, 255, 190, 255);
u_int8_t brightness = 127; // яркость света
u_int8_t temperature = 0;  // температура света

u_int64_t Timer0 = 0; // переменная для задержки датчика движения
u_int64_t Timer1 = 0; // переменная для задержки звукового датчика
u_int64_t Timer2 = 0; // переменная для задержки кнопки энкодера

bool manual = false; // отвечает за ручнуюю или автоматическую настройку
bool flag = false;   // флаг включения ленты
bool wifi_connect_status = false;

char ssid[20] = ""; // настройка wi-fi
char pass[64] = ""; // пароль wi-fi

u_int8_t red, green, blue;

void wifi_settings(void)
{
    Serial.println(F("======================================"));
    Serial.println(F("Setup Wi-Fi credentials:"));

    // Структура credentials для хранения SSID и пароля
    // credentials
    // {
    // ssid:"ssid"
    // pass:"password"
    // }

    // if (!Serial.available())
    //{
    // for (;;)
    //{
    // if (!!Serial.available())
    //{
    //    Serial.print(F("Set SSID: "));
    //    Serial.readBytesUntil('\n', ssid, 20);

    //    Serial.print(F("Set Password: "));
    //    Serial.readBytesUntil('\n', pass, 64);
    //}
    // if (strlen(ssid) > 0 && strlen(pass) > 0)
    //{
    //    break;
    //}
    //}
    Serial.print(F("Saving SSID and Password in flash memory: "));

    // Сохранение настроек в флеш-память
    preferences.begin("credentials", false);
    preferences.putBytes("ssid", ssid, 20);
    preferences.putBytes("pass", pass, 64);
    Serial.println(F("Done"));
    //}
    // else
    //{
    Serial.println(F("No data received"));
    //}
    Serial.println(F("======================================"));
}

void wifi_connect(void)
{
    Serial.println(F("Data"));
    Serial.println(ssid);
    Serial.println(pass);

    // Если ssid или pass не заданы, то выводим сообщение об этом и вызываем функцию wifi_settings
    if (ssid == pass)
    {
        Serial.println(F("No values saved for ssid or password"));
        wifi_settings();
    }

    // Подключаемся к WiFi сети
    Serial.print(F("Connecting to: "));
    Serial.println(ssid);

    WiFi.begin(ssid, pass);
    // Ожидаем подключения к WiFi сети
    for (u_int8_t i = 0; i <= 10; i++)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.print('.');
            delay(1000);
        }
    }
    wifi_connect_status = true;
    // Если не удалось подключиться к WiFi сети, то выводим сообщение об этом и вызываем функцию wifi_settings
    if (WiFi.status() != WL_CONNECTED)
    {
        wifi_connect_status = false;
        Serial.println(F("WiFi connection failed"));
        wifi_settings();
    }
    
    Serial.println(WiFi.localIP());
}

void setup_datetime(void)
{
    ntp.tick();
    ntp.updateNow();
    rtc.refresh();

    uint8_t second = ntp.second();   // получить секунды
    uint8_t minute = ntp.minute();   // получить минуты
    uint8_t hour = ntp.hour();       // получить часы
    uint8_t day = ntp.day();         // получить день месяца
    uint8_t month = ntp.month();     // получить месяц
    uint16_t year = ntp.year();      // получить год
    uint8_t dayWeek = ntp.dayWeek(); // получить день недели (пн.. вс = 1.. 7)

    PRINTLN(F("NTP time"));
    PRINT(second);
    PRINTLN(" sec");
    PRINT(minute);
    PRINTLN(" min");
    PRINT(hour);
    PRINTLN(" h");
    PRINT(day);
    PRINTLN(" days");
    PRINT(month);
    PRINTLN(" month");
    PRINT(year);
    PRINTLN(" year");
    PRINT(dayWeek);
    PRINTLN(" dayWeek");

    // DS3231 seconds, minutes, hours, day, date, month, year
    rtc.set(second, minute, hour, dayWeek, day, month, year - 2000);

    PRINTLN(F("RTC time"));
    PRINT(rtc.second());
    PRINTLN(" sec");
    PRINT(rtc.minute());
    PRINTLN(" min");
    PRINT(rtc.hour());
    PRINTLN(" h");
    PRINT(rtc.day());
    PRINTLN(" days");
    PRINT(rtc.month());
    PRINTLN(" month");
    PRINT(rtc.year());
    PRINTLN(" year");
    PRINT(rtc.dayOfWeek());
    PRINTLN(" dayWeek");
    // setModuleTime(0, 0, 0, 0, 0, 0, 0);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

void core1(void *)
{
    u_int8_t click_count = 0;
    u_int8_t move_count = 0;
    for (;;)
    {
        if (digitalRead(MOVE_PIN) && (millis() - Timer0) >= 3000)
        {
            Timer0 = millis();
            move_count++;
        }
        if (move_count >= 2)
        {
            flag = !flag;
            move_count = 0;
            PRINTLN(F("MOVE target"));
        }

        if (digitalRead(AUDIO_PIN) && (millis() - Timer1) >= 100)
        {
            Timer1 = millis();
            flag = !flag;
            PRINTLN(F("AUDIO target"));
        }
        enc1.tick(); // опрос энкодера

        if (enc1.isRight() && manual)
        {
            if (hue >= 255)
            {
                hue = 255;
            }
            else
            {
                hue++;
            }
            PRINTLN(hue);
            PRINT("r ");
            PRINTLN(red);
            PRINT("g ");
            PRINTLN(green);
            PRINT("b ");
            PRINTLN(blue);
        }

        if (enc1.isLeft() && manual)
        {
            if (hue <= 0)
            {
                hue = 0;
            }
            else
            {
                hue--;
            }
            PRINTLN(hue);
            PRINT("r ");
            PRINTLN(red);
            PRINT("g ");
            PRINTLN(green);
            PRINT("b ");
            PRINTLN(blue);
        }

        if (enc1.isClick())
        {
            click_count++;
            Timer2 = millis();
        }

        if ((millis() - Timer2) > 300)
        {
            Timer2 = millis();
            click_count = 0;
        }

        if (click_count >= 2)
        {
            click_count = 2;
            if (!manual)
            {
                manual = true;
                PRINTLN("MANUAL ON");
            }
        }
        if (click_count == 1)
        {
            if (manual)
            {
                manual = false;
                PRINTLN("MANUAL OFF");
            }
        }
        if (manual)
        {
            buff_color = map(hue, 0, 255, 190, 255);
        }
        else
        {
            buff_color = map(temperature, 0, 255, 190, 255);
        }

        red = map(buff_color, 128, 255, 1, 255);
        green = map(buff_color, 127, 255, 200, 120);
        blue = map(buff_color, 127, 255, 255, 0);

        delay(1);
    }
}

void setup()
{
    // Инициализация Serial
    Serial.begin(9600);
    Serial.println();
    // Инициализация I2C
    Wire.begin();
    // запуск сенсора света
    lightMeter.begin();
    // Установка таймаута для Serial
    Serial.setTimeout(SERIAL_TIMEOUT);
    // Инициализация WiFi
    wifi_connect();
    // Запуск клиента времени
    ntp.begin();
    // Обновление времени
    if (wifi_connect_status)
    {
        setup_datetime();
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    // Запуск ленты
    NeoPixel.begin();
    NeoPixel.clear();

    // Инициализация энкодера
    pinMode(ENC_S1, INPUT);
    pinMode(ENC_S2, INPUT);
    pinMode(ENC_KEY, INPUT);
    pinMode(AUDIO_PIN, INPUT);
    pinMode(MOVE_PIN, INPUT);
    enc1.setType(TYPE2);

    xTaskCreatePinnedToCore(
        core1,
        "Task1",
        10000,
        NULL,
        1,
        &Task1,
        0);
}

void loop()
{
    if (flag)
    { // Выключение
        for (int pixel = 0; pixel <= NUM_PIXELS; pixel++)
        {
            NeoPixel.setPixelColor(pixel, NeoPixel.ColorHSV(hue * 257, 255, 0));
        }
        NeoPixel.show();
    }
    else
    { // Включение
        for (int pixel = 0; pixel <= NUM_PIXELS; pixel++)
        {
            NeoPixel.setPixelColor(pixel, NeoPixel.Color(red, green, blue));
        }
        NeoPixel.setBrightness(brightness);
        NeoPixel.show();
    }

    if (!manual)
    {
        rtc.refresh();
        float lux = 0;
        if (flag)
        {
            float lux = lightMeter.readLightLevel();
            PRINT("Light: ");
            PRINT(lux);
            PRINTLN(" lx");
        }

        if (lux < 150 && flag)
        {
            brightness = 255;
        }
        if (lux < 100 && flag)
        {
            brightness = 128;
        }
        if (lux > 200 && flag)
        {
            brightness = 64;
        }

        uint8_t hour = rtc.hour();   // получить часы
        uint8_t month = rtc.month(); // получить месяц

        switch (month)
        {
        case 12: // если зима
        case 1:
        case 2:
            if (hour > 17)
            {
                temperature = 255;
            }
            break;

        case 3: // если весна
        case 4:
        case 5:
            if (hour > 20)
            {
                temperature = 255;
            }
            break;
        case 6: // если лето
        case 7:
        case 8:
            if (hour > 22)
            {
                temperature = 255;
            }
            break;
        case 9: // если осень
        case 10:
        case 11:
            if (hour > 19)
            {
                temperature = 255;
            }
            break;

        default:
            break;
        }
    }
}
