#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif
#define WIFI_OFF WIFI_MODE_NULL
#define WIFI_STA WIFI_MODE_STA

#define WHITE 0xFF
#define LGREY 0xBB
#define GREY 0x88
#define DGREY 0x44
#define BLACK 0x00

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "epd_driver.h"
#include "esp_adc_cal.h"
#include <Wire.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <SPI.h>
#include <time.h>

#include "roboto8.h"
#include "roboto10.h"
#include "roboto12.h"
#include "roboto18.h"
#include "roboto18r.h"
#include "roboto24r.h"

#define BATT_PIN 36
enum alignment
{
    LEFT,
    RIGHT,
    CENTER
};

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWRD";

/*
 * Keep this at 4. The long currency name is used for the api, so check the spelling.
 */
const int AMOUNT_CURR = 4;
const String currency_short[] = {"BTC", "ETH", "LTC", "XMR"};
const String currency_names[] = {"Bitcoin", "Ethereum", "Litecoin", "Monero"};

int currency_index;

uint8_t *framebuffer;
int vref = 1100;

int wifi_signal;

uint8_t start_wifi()
{
    Serial.println("Connecting to: " + String(ssid));
    IPAddress dns(8, 8, 8, 8);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA); // switch off AP
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("STA: Failed!");
        WiFi.disconnect(false);
        delay(500);
        WiFi.begin(ssid, password);
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
        Serial.println("WiFi connected as " + WiFi.localIP().toString());
    }
    else
        Serial.println("WiFi connection failed: " + String(WiFi.status()));
    return WiFi.status();
}

void stop_wifi()
{
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

float c_price = -1.0f;
float c_price_btc = -1.0f;
float c_price_24h = 0.0f;
float c_price_1h = 0.0f;

int c_rank = 0;

double c_cap = -1.0;
double c_vol = -1.0;

/**
 * Read https://api.coinstats.app API data into fields
 **/
bool decode_web_data(WiFiClient &json)
{
    DynamicJsonDocument doc(4 * 1024);
    DeserializationError error = deserializeJson(doc, json);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return false;
    }

    JsonObject coin = doc["coin"];
    c_price = coin["price"].as<float>();
    c_price_btc = coin["priceBtc"].as<float>();
    c_rank = coin["rank"].as<int>();
    c_cap = coin["marketCap"].as<double>();
    c_vol = coin["volume"].as<double>();

    c_price_24h = coin["priceChange1d"].as<float>();
    c_price_1h = coin["priceChange1h"].as<float>();
    return true;
}

/**
 * https://api.coinstats.app certificate chain.
 **/
const char *root_ca = "-----BEGIN CERTIFICATE-----\r\nMIIFZzCCBE+gAwIBAgIQDTlrDyYrb/AxYW1KAPrBujANBgkqhkiG9w0BAQsFADBGMQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRUwEwYDVQQLEwxTZXJ2ZXIgQ0EgMUIxDzANBgNVBAMTBkFtYXpvbjAeFw0yMTAyMjgwMDAwMDBaFw0yMjAzMjkyMzU5NTlaMBgxFjAUBgNVBAMTDWNvaW5zdGF0cy5hcHAwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC6z+21LImyDsItMYKeGKIFUNFHdb4zys40Pn7Ge2MeZYYiV4zNC2bZXM35WFeYYMG6i6PSvk3+q3zQlJzdjAknAs7CnHicYECMqrnE57uIQynQhouCe6XAj+i4w4ze5NvZFj47Tpq24/jUBkljIt/6BlQMi90t4FBn1OFD7pddPV/hrgxRBeM3MWCydIiUF9yHuubhGqVBOKubQHWYXkaWFzzF8vswTfZ/nsZUzG1beAVrbHbRkKmjY+tlBVDmexox/GuYg71vO0e5TXQUP9Z0UhfBitq7YzjOjcel70jry2Q66ce3BaGmZXCcV1Wj8HqwrJvapL9JeUSXmxzmMH3VAgMBAAGjggJ9MIICeTAfBgNVHSMEGDAWgBRZpGYGUqB7lZI8o5QHJ5Z0W/k90DAdBgNVHQ4EFgQUXhZsTg172MLYrVNuKrC51fOakW0wKQYDVR0RBCIwIIINY29pbnN0YXRzLmFwcIIPKi5jb2luc3RhdHMuYXBwMA4GA1UdDwEB/wQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwOwYDVR0fBDQwMjAwoC6gLIYqaHR0cDovL2NybC5zY2ExYi5hbWF6b250cnVzdC5jb20vc2NhMWIuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMHUGCCsGAQUFBwEBBGkwZzAtBggrBgEFBQcwAYYhaHR0cDovL29jc3Auc2NhMWIuYW1hem9udHJ1c3QuY29tMDYGCCsGAQUFBzAChipodHRwOi8vY3J0LnNjYTFiLmFtYXpvbnRydXN0LmNvbS9zY2ExYi5jcnQwDAYDVR0TAQH/BAIwADCCAQQGCisGAQQB1nkCBAIEgfUEgfIA8AB2ACl5vvCeOTkh8FZzn2Old+W+V32cYAr4+U1dJlwlXceEAAABd+ZZXDQAAAQDAEcwRQIhAJ81JDwark4rNE554mChYn+UxT9FcXPKHCEfHpMEtK3OAiBPWDJ8kXekttFVZOtOtEcHfx8E4ZaoD6nz/TNwfluVvwB2ACJFRQdZVSRWlj+hL/H3bYbgIyZjrcBLf13Gg1xu4g8CAAABd+ZZXIQAAAQDAEcwRQIhAICPYbJ1FqYG10jBi6m0gLub7iP8Y78psDvY4IOleOU1AiBVXEnDsCGxPzTc0cEaXTTX5LaJUsPIDCUXAyVt6QL/KDANBgkqhkiG9w0BAQsFAAOCAQEAawPBNUkrC9BCbtQLCC9ZDpKPTphU6d7MkMTRtazi2Vi/lzYSw3zCDV1Y8O1iv5OeyTmf8AidUr7B3OYlVbhWzlenY7ox9asbfRjI/s72ZuvdV08M7oybAOFxLjQ/UgXKAHNeEExsNv/NEhxROxEvWmLoPKaxr9BAknEN0o9owkdiugbo7E5gJiGtW3hG8ysYvhQ4f2XgwVzRS9xK6rEnM6p+cmPdjM7OPK2oPV8fcBwE/b6VotbA8UfjLJGH9eLJ2Kmrx/l1rQegIn+0pS6e822KXzxFYchKeEkYXqrkWEMwWaqh1yI1yitkTLg0Lf7LZl1PEpxoByVHb9oIJovxIg==\r\n-----END CERTIFICATE-----\r\n-----BEGIN CERTIFICATE-----\r\nMIIESTCCAzGgAwIBAgITBntQXCplJ7wevi2i0ZmY7bibLDANBgkqhkiG9w0BAQsFADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6b24gUm9vdCBDQSAxMB4XDTE1MTAyMTIyMjQzNFoXDTQwMTAyMTIyMjQzNFowRjELMAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEVMBMGA1UECxMMU2VydmVyIENBIDFCMQ8wDQYDVQQDEwZBbWF6b24wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDCThZn3c68asg3Wuw6MLAd5tES6BIoSMzoKcG5blPVo+sDORrMd4f2AbnZcMzPa43j4wNxhplty6aUKk4T1qe9BOwKFjwK6zmxxLVYo7bHViXsPlJ6qOMpFge5blDP+18x+B26A0piiQOuPkfyDyeR4xQghfj66Yo19V+emU3nazfvpFA+ROz6WoVmB5x+F2pV8xeKNR7u6azDdU5YVX1TawprmxRC1+WsAYmz6qP+z8ArDITC2FMVy2fw0IjKOtEXc/VfmtTFch5+AfGYMGMqqvJ6LcXiAhqG5TI+Dr0RtM88k+8XUBCeQ8IGKuANaL7TiItKZYxK1MMuTJtV9IblAgMBAAGjggE7MIIBNzASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUWaRmBlKge5WSPKOUByeWdFv5PdAwHwYDVR0jBBgwFoAUhBjMhTTsvAyUlC4IWZzHshBOCggwewYIKwYBBQUHAQEEbzBtMC8GCCsGAQUFBzABhiNodHRwOi8vb2NzcC5yb290Y2ExLmFtYXpvbnRydXN0LmNvbTA6BggrBgEFBQcwAoYuaHR0cDovL2NybC5yb290Y2ExLmFtYXpvbnRydXN0LmNvbS9yb290Y2ExLmNlcjA/BgNVHR8EODA2MDSgMqAwhi5odHRwOi8vY3JsLnJvb3RjYTEuYW1hem9udHJ1c3QuY29tL3Jvb3RjYTEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqGSIb3DQEBCwUAA4IBAQAfsaEKwn17DjAbi/Die0etn+PEgfY/I6s8NLWkxGAOUfW2o+vVowNARRVjaIGdrhAfeWHkZI6q2pI0x/IJYmymmcWaZaW/2R7DvQDtxCkFkVaxUeHvENm6IyqVhf6Q5oN12kDSrJozzx7I7tHjhBK7V5XoTyS4NU4EhSyzGgj2x6axDd1hHRjblEpJ80LoiXlmUDzputBXyO5mkcrplcVvlIJiWmKjrDn2zzKxDX5nwvkskpIjYlJcrQu4iCX1/YwZ1yNqF9LryjlilphHCACiHbhIRnGfN8j8KLDVmWyTYMk8V+6j0LI4+4zFh2upqGMQHL3VFVFWBek6vCDWhB/b\r\n-----END CERTIFICATE-----\r\n-----BEGIN CERTIFICATE-----\r\nMIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsFADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTELMAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJvb3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXjca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qwIFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQmjgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUAA4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDIU5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUsN+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vvo/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpyrqXRfboQnoZsG4q5WTP468SQvvG5\r\n-----END CERTIFICATE-----";

bool read_web_data()
{
    HTTPClient http;
    String cur = currency_names[currency_index];
    cur.toLowerCase();
    String uri = "/public/v1/coins/" + cur + "?currency=EUR";
    http.begin("https://api.coinstats.app" + uri, root_ca);
    int httpCode = http.GET();
    bool success = false;
    if (httpCode == HTTP_CODE_OK)
    {
        if (decode_web_data(http.getStream()))
            success = true;
        else
            Serial.println("Web data decoding failed");
    }
    else
    {
        Serial.printf("Connection to API failed, error: %s\r\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    return success;
}

void draw_str(const GFXfont font, const String txt, int x, int y, alignment align)
{
    const char *string = (char *)txt.c_str();
    int x1, y1;
    int w, h;
    int xx = x, yy = y;
    get_text_bounds(&font, string, &xx, &yy, &x1, &y1, &w, &h, NULL);
    if (align == RIGHT)
        x = x - w;
    if (align == CENTER)
        x = x - w / 2;
    int cursor_y = y + h;
    writeln(&font, string, &x, &cursor_y, framebuffer);
}

void start_deep_sleep()
{
    epd_poweroff_all();
    esp_sleep_enable_timer_wakeup(1000L * 60000L * 5L); // sleep 5 min (API limit)

    Serial.println("Starting deep-sleep period...");

    esp_deep_sleep_start();
}

void draw_framebuf(bool clear_buf)
{
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    if (clear_buf)
    {
        memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    }
}

void correct_adc_ref_volt()
{
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        Serial.printf("eFuse Vref:%u mV\r\n", adc_chars.vref);
        vref = adc_chars.vref;
    }
}
void setup()
{
    Serial.begin(115200);

    correct_adc_ref_volt();
    epd_init();

    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer)
    {
        Serial.println("alloc memory failed.");
        while (1)
            ;
    }
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    epd_poweron();
    epd_clear();

    uint16_t v = analogRead(BATT_PIN);
    float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
    String voltage = String(battery_voltage) + "V";
    Serial.println(voltage);

    int x_pos = (int)(EPD_WIDTH * 0.02f);
    int y_pos = (int)(EPD_HEIGHT * 0.96f);

    draw_str(Roboto8, (char *)voltage.c_str(), x_pos, y_pos, LEFT);

    x_pos = (int)(EPD_WIDTH * 0.98f);

    draw_str(Roboto8, "https://github.com/GraxCode - v1.0", x_pos, y_pos, RIGHT);

    x_pos = (int)(EPD_WIDTH * 0.5f);
    y_pos = (int)(EPD_HEIGHT * 0.45f);
    draw_str(Roboto24R, "Connecting to " + String(ssid), x_pos, y_pos, CENTER);

    draw_framebuf(true);
    if (start_wifi() != WL_CONNECTED)
    {
        epd_clear();
        draw_str(Roboto24R, "WLAN failed. Check credentials.", x_pos, y_pos, CENTER);
    }
    else
    {
        epd_clear();
        for (currency_index = 0; currency_index < AMOUNT_CURR; currency_index++)
        {
            int y_off = (currency_index / float(AMOUNT_CURR)) * EPD_HEIGHT;
            int y_off_end = ((currency_index + 1) / float(AMOUNT_CURR)) * EPD_HEIGHT;

            epd_draw_line(0, y_off_end, EPD_WIDTH, y_off_end, BLACK, framebuffer);

            x_pos = (int)(EPD_WIDTH * 0.02f);
            y_pos = (int)(EPD_HEIGHT * 0.04f);

            draw_str(Roboto24R, (char *)currency_names[currency_index].c_str(), x_pos, y_off + y_pos, LEFT);
            y_pos = (int)(EPD_HEIGHT * 0.15f);
            draw_str(Roboto18R, (char *)currency_short[currency_index].c_str(), x_pos, y_off + y_pos, LEFT);
            if (!read_web_data())
            {
                x_pos = (int)(EPD_WIDTH * 0.5f);
                y_pos = (int)(EPD_HEIGHT * 0.1f);
                draw_str(Roboto18R, "Wifi read failed", x_pos, (y_off + y_off_end) / 2, CENTER);
            }
            else
            {
                x_pos = (int)(EPD_WIDTH * 0.30f);
                y_pos = (int)(EPD_HEIGHT * 0.05f);
                draw_str(Roboto18R, String(c_price, 2) + " €", x_pos, y_off + y_pos, LEFT);
                y_pos = (int)(EPD_HEIGHT * 0.125f);
                draw_str(Roboto12, "= " + String(c_price_btc, 8) + " BTC", x_pos, y_off + y_pos, LEFT);
                y_pos = (int)(EPD_HEIGHT * 0.2f);
                draw_str(Roboto8, "RANK #" + String(c_rank), x_pos, y_off + y_pos, LEFT);

                x_pos = (int)(EPD_WIDTH * 0.59f);
                y_pos = (int)(EPD_HEIGHT * 0.05f);
                draw_str(abs(c_price_24h) > 10 ? Roboto18R : Roboto18, String(c_price_24h, 2) + "%", x_pos, y_off + y_pos, LEFT);
                y_pos = (int)(EPD_HEIGHT * 0.11f);
                draw_str(Roboto12, "24h CHANGE", x_pos, y_off + y_pos, LEFT);
                y_pos = (int)(EPD_HEIGHT * 0.2f);
                draw_str(Roboto8, String(c_cap / 1000000.0f, 2) + " M CAP", x_pos, y_off + y_pos, LEFT);

                x_pos = (int)(EPD_WIDTH * 0.94f);
                y_pos = (int)(EPD_HEIGHT * 0.05f);
                draw_str(abs(c_price_1h) > 5 ? Roboto18R : Roboto18, String(c_price_1h, 2) + "%", x_pos, y_off + y_pos, RIGHT);
                y_pos = (int)(EPD_HEIGHT * 0.11f);
                draw_str(Roboto12, "1h CHANGE", x_pos, y_off + y_pos, RIGHT);
                y_pos = (int)(EPD_HEIGHT * 0.2f);
                draw_str(Roboto8, String(c_vol / 1000000.0f, 2) + " M VOL", x_pos, y_off + y_pos, RIGHT);

                // min to 100%, put in range 0 - 255
                int intensity = (int)(255.0f - (min(abs(c_price_1h) + 2.0f, 10.0f) / 10.0f) * 255.0f);
                int left_x = (int)(EPD_WIDTH * 0.96f);
                int right_x = (int)(EPD_WIDTH * 0.99f);
                int mid_x = (left_x + right_x) / 2;

                int low_y_up = y_off + (int)(EPD_HEIGHT * 0.08f);
                int high_y_up = y_off + (int)(EPD_HEIGHT * 0.04f);
                int low_y_down = y_off + (int)(EPD_HEIGHT * (0.25f - 0.08f));
                int high_y_down = y_off + (int)(EPD_HEIGHT * (0.25f - 0.04f));

                if (c_price_1h > 0)
                {
                    epd_fill_triangle(left_x, low_y_up, right_x, low_y_up, mid_x, high_y_up, intensity, framebuffer);
                }
                else
                {
                    epd_fill_triangle(left_x, low_y_down, right_x, low_y_down, mid_x, high_y_down, intensity, framebuffer);
                }
                epd_draw_triangle(left_x, low_y_up, right_x, low_y_up, mid_x, high_y_up, BLACK, framebuffer);
                epd_draw_triangle(left_x, low_y_down, right_x, low_y_down, mid_x, high_y_down, BLACK, framebuffer);
            }
            draw_framebuf(true);
        }
    }
    stop_wifi();
    start_deep_sleep();
}

void loop()
{
}
