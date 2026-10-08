#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "esp_sleep.h"
#include "esp_system.h"
#include "secrets.h"
// ============================================================
//                    USER CONFIGURATION
// ============================================================



// Device name
const char* DEVICE_NAME = "Garage Door 1";


// ============================================================
//                         PINS
// ============================================================

#define DOOR_PIN    D2
#define SETUP_PIN   D1
#define BATTERY_PIN A0

// Change this to HIGH if your tilt switch operates backwards
//#define DOOR_CLOSED_STATE LOW //orig for tilt switch
#define DOOR_CLOSED_STATE HIGH // for momentary button


// ============================================================
//                       GLOBALS
// ============================================================

WebServer server(80);
Preferences preferences;

unsigned long doorOpenedAt = 0;

bool alertSent = false;
bool lowBatterySent = false;

int alertMinutes = 15;

bool notifyClosed = true;
bool notifyLowBattery = true;

const unsigned long DOOR_DEBOUNCE_MS = 1000;

int rawDoorState;
int stableDoorState;

unsigned long doorStateChangedAt = 0;

const unsigned long SLEEP_DELAY_MS = 3000;
unsigned long doorClosedAt = 0;

bool doorHasBeenOpened = false;

unsigned long lastWiFiRetry = 0;
const unsigned long WIFI_RETRY_MS = 10000;

unsigned long lastPushAttempt = 0;
const unsigned long PUSH_RETRY_MS = 10000;

bool setupMode = false;
unsigned long setupModeStartedAt = 0;

// 1 minute for testing. Change to 10UL * 60UL * 1000UL when finished.
const unsigned long SETUP_TIMEOUT_MS = 10UL * 60UL * 1000UL;

// for testsing
RTC_DATA_ATTR uint64_t savedWakeStatus = 0;
RTC_DATA_ATTR int savedWakeCause = 0;
// ============================================================
//                   BATTERY MEASUREMENT
// ============================================================

float readBatteryVoltage()
{
    // Average several ADC readings

     // rem for no battery connected during test
    uint32_t totalMv = 0;

    for (int i = 0; i < 16; i++)
    {
        totalMv += analogReadMilliVolts(BATTERY_PIN);
        delay(5);
    }

    float adcVoltage =
        (totalMv / 16.0) / 1000.0;
Serial.print("ADC voltage = ");
Serial.println(adcVoltage, 3);

    // 1M / 1M divider = voltage divided by 2
    return adcVoltage * 2.0;

    

      // return 0.0;
}


// ------------------------------------------------------------

int batteryPercent(float voltage)
{
    // Approximation suitable for our purposes
//  rem for testing without battery
    if (voltage >= 4.15) return 100;
    if (voltage >= 4.05) return 90;
    if (voltage >= 3.95) return 80;
    if (voltage >= 3.85) return 70;
    if (voltage >= 3.80) return 60;
    if (voltage >= 3.75) return 50;
    if (voltage >= 3.70) return 40;
    if (voltage >= 3.65) return 30;
    if (voltage >= 3.55) return 20;
    if (voltage >= 3.45) return 10;

    return 5;
}


// ============================================================
//                      DOOR STATUS
// ============================================================

bool doorIsClosed()
{
    return stableDoorState == DOOR_CLOSED_STATE;
}


// ============================================================
//                        PUSHOVER
// ============================================================

String urlEncode(const String &str)
{
    String encoded = "";

    char c;
    char code0;
    char code1;

    for (unsigned int i = 0; i < str.length(); i++)
    {
        c = str.charAt(i);

        if (isalnum(c))
        {
            encoded += c;
        }
        else if (c == ' ')
        {
            encoded += '+';
        }
        else
        {
            code1 = (c & 0xF) + '0';

            if ((c & 0xF) > 9)
                code1 = (c & 0xF) - 10 + 'A';

            c = (c >> 4) & 0xF;

            code0 = c + '0';

            if (c > 9)
                code0 = c - 10 + 'A';

            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }

    return encoded;
}

bool sendPush(String title, String message)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Pushover: WiFi not connected.");
        return false;
    }

    HTTPClient http;

    http.begin(
        "https://api.pushover.net/1/messages.json"
    );

    http.addHeader(
        "Content-Type",
        "application/x-www-form-urlencoded"
    );

    String data =
        "token=" + String(PUSHOVER_TOKEN) +
        "&user=" + String(PUSHOVER_USER) +
        "&title=" + urlEncode(title) +
        "&message=" + urlEncode(message);

    int httpCode = http.POST(data);

    String response =
        http.getString();

    Serial.print("Pushover HTTP response: ");
    Serial.println(httpCode);

    Serial.print("Pushover response body: ");
    Serial.println(response);

    http.end();

    return httpCode == 200;
}


// ============================================================
//                       WIFI
// ============================================================

void maintainWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
        return;

    if (millis() - lastWiFiRetry < WIFI_RETRY_MS)
        return;

    lastWiFiRetry = millis();

    Serial.println("WiFi disconnected - attempting reconnect...");

    WiFi.reconnect();
}

bool connectWiFi()
{
    Serial.println("Starting WiFi...");

    delay(500);

    WiFi.mode(WIFI_STA);
    delay(100);

    Serial.print("Connecting to SSID: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        Serial.print("WiFi status = ");
        Serial.println(WiFi.status());

        if (millis() - start > 30000)
        {
            Serial.println("WiFi connection timed out.");
            return false;
        }
    }

    Serial.println("WiFi connected!");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());

    return true;
}


// ============================================================
//                    CONFIGURATION PAGE
// ============================================================

void handleRoot()
{
    Serial.println("handleRoot() called");

    // Battery temporarily disabled until battery hardware is connected
   // float battery = 0.0;
    float battery = readBatteryVoltage();

    String html;
    html.reserve(2500);   // Allocate once instead of repeatedly resizing

html = F(
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Garage Door 1</title>"
    "</head>"
    "<body style='font-family:Arial;max-width:500px;margin:auto;padding:20px'>"
    "<h1>Garage Door 1</h1>"
);

    // Door status
    html += F("<h2>Door: ");

    if (doorIsClosed())
        html += F("<span style='color:green'>CLOSED</span>");
    else
        html += F("<span style='color:red'>OPEN</span>");

    html += F("</h2>");

        // Battery

    html += "<p>Battery: ";

    html += String(battery, 2);

    html += " V (";

    html += String(
        batteryPercent(battery)
    );

    html += "%)</p>";

    // WiFi
    html += F("<p>WiFi RSSI: ");
    html += String(WiFi.RSSI());
    html += F(" dBm</p>");

    // Settings
    html += F(
        "<hr>"
        "<h2>Settings</h2>"
        "<form method='POST' action='/save'>"
        "Alert if door open longer than:<br>"
        "<input type='number' name='minutes' min='1' max='240' value='"
    );

    html += String(alertMinutes);

    html += F(
        "'> minutes"
        "<br><br>"
        "<label>"
        "<input type='checkbox' name='closed' "
    );

    if (notifyClosed)
        html += F("checked");

    html += F(
        "> Notify when door closes"
        "</label>"
        "<br><br>"
        "<label>"
        "<input type='checkbox' name='battery' "
    );

    if (notifyLowBattery)
        html += F("checked");

    html += F(
        "> Low battery warning"
        "</label>"
        "<br><br>"
        "<input type='submit' value='Save Settings'>"
        "</form>"
        "<hr>"
        "<p><a href='/testpush'>Send Test Notification</a></p>"
        "<p><a href='/sleep'>Sleep Now</a></p>"
        "</body>"
        "</html>"
    );

    server.send(
        200,
        "text/html",
        html
    );

    Serial.print("Web page sent. HTML length = ");
    Serial.println(html.length());
}


// ============================================================
//                       SAVE SETTINGS
// ============================================================

void handleSave()
{
    if (server.hasArg("minutes"))
    {
        alertMinutes =
            server.arg("minutes").toInt();

        if (alertMinutes < 1)
            alertMinutes = 1;

        if (alertMinutes > 240)
            alertMinutes = 240;
    }


    notifyClosed =
        server.hasArg("closed");

    notifyLowBattery =
        server.hasArg("battery");


    // Save permanently

    preferences.putInt(
        "alertMin",
        alertMinutes
    );

    preferences.putBool(
        "closed",
        notifyClosed
    );

    preferences.putBool(
        "lowBatt",
        notifyLowBattery
    );


    server.sendHeader(
        "Location",
        "/"
    );

    server.send(
        303,
        "text/plain",
        ""
    );
}


// ============================================================
//                    TEST PUSH MESSAGE
// ============================================================

void handleTestPush()
{
    sendPush(
        DEVICE_NAME,
        "Test notification"
    );

    server.sendHeader(
        "Location",
        "/"
    );

    server.send(
        303,
        "text/plain",
        ""
    );
}


// ============================================================
//                       JSON STATUS
// ============================================================

void handleStatus()
{
    float battery =
        readBatteryVoltage();

    String json = "{";

    json += "\"door\":\"";

    json +=
        doorIsClosed()
        ? "closed"
        : "open";

    json += "\",";

    json += "\"battery\":";
    json += String(battery, 2);

    json += ",";

    json += "\"batteryPercent\":";
    json += String(
        batteryPercent(battery)
    );

    json += ",";

    json += "\"rssi\":";
    json += String(
        WiFi.RSSI()
    );

    json += "}";


    server.send(
        200,
        "application/json",
        json
    );
}
//============================================================
//                      Sleep Now
//============================================================
void goToSleep();
void handleSleepNow()
{
    server.send(
        200,
        "text/html",
        "<html><body style='font-family:Arial'>"
        "<h2>Going to sleep...</h2>"
        "</body></html>"
    );

    delay(500);
    goToSleep();
}

// ============================================================
//                    START WEB SERVER
// ============================================================

void startWebServer()
{


    server.on("/", HTTP_GET, handleRoot);

    server.on("/save", HTTP_POST, handleSave);

    server.on("/testpush", HTTP_GET, handleTestPush);

    server.on("/status", HTTP_GET, handleStatus);

    server.on("/sleep", HTTP_GET, handleSleepNow);

    server.begin();

    Serial.println("Web server started.");
}






// ============================================================
//                       DEEP SLEEP
// ============================================================

void goToSleep()
{

Serial.println("Door confirmed closed - preparing for deep sleep.");

    Serial.flush();
    delay(100);

    // Momentary-button test:
    // CLOSED = HIGH
    // OPEN   = LOW
    // Therefore wake when D2 goes LOW.

uint64_t wakePins =
    (1ULL << digitalPinToGPIONumber(DOOR_PIN)) |
  (1ULL << digitalPinToGPIONumber(SETUP_PIN));

esp_deep_sleep_enable_gpio_wakeup(
    wakePins,
    ESP_GPIO_WAKEUP_GPIO_LOW


/*  orig working
    esp_deep_sleep_enable_gpio_wakeup(
        1ULL << digitalPinToGPIONumber(DOOR_PIN),
        ESP_GPIO_WAKEUP_GPIO_LOW
        */
    );

    Serial.println("Entering deep sleep...");
    Serial.flush();

    esp_deep_sleep_start();



    delay(100);


}

void updateDoorState()
{
    int currentState = digitalRead(DOOR_PIN);

    // Raw input changed - restart debounce timer
    if (currentState != rawDoorState)
    {
        rawDoorState = currentState;
        doorStateChangedAt = millis();
    }

    // Input has remained unchanged long enough
    if ((rawDoorState != stableDoorState) &&
        (millis() - doorStateChangedAt >= DOOR_DEBOUNCE_MS))
    {
        stableDoorState = rawDoorState;

        Serial.print("Stable door state = ");
        Serial.println(stableDoorState);

        if (stableDoorState == DOOR_CLOSED_STATE)
            Serial.println("Door CLOSED");
        else
            Serial.println("Door OPEN");
    }
}
// ============================================================
//                         SETUP
// ============================================================

void setup()
{
Serial.begin(115200);
//rem this once completely done to save battery

delay(500);

delay(1000);

Serial.println();
Serial.println("***********************");
Serial.println("NEW BOOT");
Serial.print("Reset reason = ");
Serial.println(esp_reset_reason());
Serial.println("***********************");

    delay(500);

    Serial.println();
    Serial.println("=======================");
    Serial.println(DEVICE_NAME);
    Serial.println("=======================");


    // Inputs

    pinMode(
        DOOR_PIN,
        INPUT_PULLUP
    );
//rawDoorState = digitalRead(DOOR_PIN);  these 2 were orig 
//stableDoorState = rawDoorState;

delay(50);  // allow pull-up/input to settle

rawDoorState = digitalRead(DOOR_PIN);
stableDoorState = rawDoorState;
doorStateChangedAt = millis();

Serial.print("Initial raw door pin = ");
Serial.println(rawDoorState);

Serial.print("Initial door state = ");

if (stableDoorState == DOOR_CLOSED_STATE)
{
    Serial.println("CLOSED");
}
else
{
    Serial.println("OPEN");
}
    pinMode(
        SETUP_PIN,
        INPUT_PULLUP
    );


    // ADC

    analogReadResolution(12);


    // Preferences

    preferences.begin(
        "garage",
        false
    );


    alertMinutes =
        preferences.getInt(
            "alertMin",
            15
        );

    notifyClosed =
        preferences.getBool(
            "closed",
            true
        );

    notifyLowBattery =
        preferences.getBool(
            "lowBatt",
            true
        );


    // Determine why we woke

    esp_sleep_wakeup_cause_t wakeCause =
        esp_sleep_get_wakeup_cause();


    Serial.print(
        "Wake cause: "
    );

    Serial.println(
        wakeCause
    );


bool setupPressed =
    digitalRead(SETUP_PIN) == LOW;

// Determine which GPIO caused the deep-sleep wake
uint64_t wakeStatus = esp_sleep_get_gpio_wakeup_status();
Serial.print("GPIO wake status: 0x");
Serial.println((unsigned long)wakeStatus, HEX);

savedWakeCause = (int)esp_sleep_get_wakeup_cause();
savedWakeStatus = esp_sleep_get_gpio_wakeup_status();

if (wakeStatus & (1ULL << digitalPinToGPIONumber(SETUP_PIN)))
{
    setupMode = true;
    setupModeStartedAt = millis();

    Serial.println("SETUP BUTTON WAKE - entering setup mode.");
}
    /*
       If door is closed and setup button
       wasn't pressed, we don't need WiFi.

       Immediately return to sleep.
    */


   

    // We're awake because either:
    //
    // 1. Door opened
    // 2. Setup button was pressed


    if (!connectWiFi())
    {
        /*
           If WiFi failed but door is open,
           stay awake and keep trying later.
        */

        Serial.println(
            "Continuing without WiFi."
        );
    }


    startWebServer();  // remd for crash debugging
//Serial.println("Web server skipped.");
Serial.println("Setup continuing.");




delay(5000);

Serial.print("SAVED wake cause: ");
Serial.println(savedWakeCause);

Serial.print("SAVED GPIO wake status: 0x");
Serial.println((unsigned long)savedWakeStatus, HEX);





    // If door is already open,
    // start timer now

if (!doorIsClosed())
{
    doorHasBeenOpened = true;
    doorOpenedAt = millis();

    Serial.println("Door OPEN - timer started.");
}
}


// ============================================================
//                          LOOP
// ============================================================

void loop()
{
    updateDoorState();

    // added for web page drop testing
static int lastWiFiStatus = -1;

int currentWiFiStatus = WiFi.status();

if (currentWiFiStatus != lastWiFiStatus)
{
    Serial.print("WiFi status changed: ");
    Serial.print(lastWiFiStatus);
    Serial.print(" -> ");
    Serial.println(currentWiFiStatus);

    lastWiFiStatus = currentWiFiStatus;
}

// enn add for tessting 


       maintainWiFi();

    server.handleClient();
if (setupMode &&
    millis() - setupModeStartedAt >= SETUP_TIMEOUT_MS)
{
    Serial.println("Setup mode timeout - going to sleep.");
    goToSleep();
}
    // ========================================================
    // DOOR OPEN
    // ========================================================

if (!doorIsClosed())
{
    doorHasBeenOpened = true;
    doorClosedAt = 0;

    if (doorOpenedAt == 0)
    {
        doorOpenedAt = millis();
        alertSent = false;

        Serial.println("Door opened - timer started.");
    }

        unsigned long openTime =
            millis() - doorOpenedAt;

        unsigned long alertTime =
            (unsigned long)alertMinutes *
            60UL *
            1000UL;

if (
    openTime >= alertTime &&
    !alertSent &&
    millis() - lastPushAttempt >= PUSH_RETRY_MS
)
{
    lastPushAttempt = millis();

    String message =
        "Garage door has been open for " +
        String(alertMinutes) +
        " minutes.";

    if (sendPush(DEVICE_NAME, message))
    {
        alertSent = true;

        Serial.println("Open-door alert sent.");
    }
}
    }

// ========================================================
// DOOR CLOSED
// ========================================================

else
{
    // Do absolutely nothing on a normal boot while closed.
    // We only sleep after this wake cycle has actually seen OPEN.
    if (!doorHasBeenOpened)
    {
        doorClosedAt = 0;
    }
    else
    {
        // First confirmed CLOSED reading
        if (doorClosedAt == 0)
        {
            doorClosedAt = millis();

            Serial.println(
                "Door closed - waiting 3 seconds before sleep."
            );
        }



        // Remained closed for 3 seconds
        if (!setupMode &&
            millis() - doorClosedAt >= SLEEP_DELAY_MS)
        {
            Serial.println(
                "Door has remained closed for 3 seconds."
            );

//Serial.println("3 SEC REACHED");

if (
    notifyClosed &&
    alertSent &&
    WiFi.status() == WL_CONNECTED
)
{
   // Serial.println("ABOUT TO SEND CLOSED PUSH");

    sendPush(
        DEVICE_NAME,
        "Garage door closed."
    );

    //Serial.println("CLOSED PUSH RETURNED");
}

//Serial.println("ABOUT TO SLEEP");
goToSleep();
        }
    }
}

    delay(20);
}

