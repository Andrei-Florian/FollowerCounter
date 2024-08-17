#include <Bridge.h>
#include "secrets.h"
#include <TM1637Display.h>

bool debug = true;

const int CLK = 2;
const int DIO = 3;
TM1637Display display(CLK, DIO);

const char authUrl[] = "https://open.tiktokapis.com/v2/oauth/token/";
const char getUrl[] = "https://open.tiktokapis.com/v2/user/info/?fields=follower_count";

int prevRequestTime = 0;
double requestInterval = 86400000; // 24 hours take 20 seconds

char accessToken[128];
int followers;

void error()
{
    uint8_t error[] = {'0x0A', '0x0A', '0x0A', '0x0A'};
    display.setSegments(error);
    while (true)
        ;
}

void displayNumber(String num)
{
    uint8_t data[4];
    int numLength = num.length();
    int numZeros = 4 - numLength;
    for (int i = 0; i < numZeros; i++)
    {
        data[i] = display.encodeDigit(0);
    }
    for (int i = numZeros; i < 4; i++)
    {
        data[i] = display.encodeDigit(num[i - numZeros] - '0');
    }

    display.setSegments(data);
}

bool parseJSON(char *key, const char *response, char *ret, size_t tokenSize)
{
    char tokenKey[strlen(key) + 10];
    strcpy(tokenKey, "\"");
    strcat(tokenKey, key);
    strcat(tokenKey, "\":");

    const char *tokenStart = strstr(response, tokenKey);
    if (tokenStart)
    {
        tokenStart += strlen(tokenKey);
        if (*tokenStart == '"')
        {
            tokenStart++;
            const char *tokenEnd = strchr(tokenStart, '"');
            if (tokenEnd)
            {
                size_t length = tokenEnd - tokenStart;
                if (length < tokenSize)
                {
                    strncpy(ret, tokenStart, length);
                    ret[length] = '\0';
                    return true;
                }
            }
        }
        else
        {
            const char *tokenEnd = strpbrk(tokenStart, ",}");
            if (tokenEnd)
            {
                size_t length = tokenEnd - tokenStart;
                if (length < tokenSize)
                {
                    strncpy(ret, tokenStart, length);
                    ret[length] = '\0';
                    return true;
                }
            }
        }
    }

    return false;
}

String POST(const char *url, const char *data)
{
    Process p;

    p.begin("curl");
    p.addParameter("-X");
    p.addParameter("POST");

    p.addParameter("-H");
    p.addParameter("Content-Type: application/x-www-form-urlencoded");

    p.addParameter("--data");
    p.addParameter(data);
    p.addParameter(url);
    p.run();

    String res = "";

    while (p.available() > 0)
    {
        char c = p.read();
        res += c;
    }

    int resCode = p.exitValue();

    if (resCode == 0)
    {
        p.close();
        return res;
    }
    else
    {
        p.close();
        return "error";
    }
}

String GET(const char *url, const char *key)
{
    Process p;

    p.begin("curl");
    p.addParameter("-X");
    p.addParameter("GET");

    String authHeader = "Authorization: Bearer ";
    authHeader += key;
    p.addParameter("-H");
    p.addParameter(authHeader);

    p.addParameter(url);
    p.run();

    String res = "";

    while (p.available() > 0)
    {
        char c = p.read();
        res += c;
    }

    int resCode = p.exitValue();

    if (resCode == 0)
    {
        p.close();
        return res;
    }
    else
    {
        p.close();
        return "error";
    }
}

bool getAccessToken()
{
    char body[256];
    snprintf(body, sizeof(body), "client_key=%s&client_secret=%s&grant_type=refresh_token&refresh_token=%s", clientKey, clientSecret, refreshToken);
    String res = POST(authUrl, body);

    if (res != "error")
    {
        const char *response = res.c_str();

        if (parseJSON("access_token", response, accessToken, sizeof(accessToken)))
        {
            Serial.println("Success: getAccessToken(): Access token retrieved");
            return true;
        }
        else
        {
            Serial.println("Error: getAccessToken(): Failed to get access token");
            return false;
        }
    }
    else
    {
        Serial.println("Error: getAccessToken(): POST request failed");
        return false;
    }
}

bool getFollowerCount()
{
    String res = GET(getUrl, accessToken);

    if (res != "error")
    {
        const char *response = res.c_str();
        char followerCount[128];

        if (parseJSON("follower_count", response, followerCount, sizeof(followerCount)))
        {
            Serial.print("Success: getFollowerCount(): Follower count retrieved: ");
            Serial.println(followerCount);
            return true;
        }
        else
        {
            Serial.println("Error: getFollowerCount(): Failed to get access token");
            return false;
        }
    }
    else
    {
        Serial.println("Error: getFollowerCount(): GET request failed");
        return false;
    }
}

void setup()
{
    display.setBrightness(0x0f);
    displayNumber("0000");

    Serial.begin(9600);

    if (debug)
    {
        while (!Serial)
            ;
    }

    Serial.println("Starting Bridge...");
    Bridge.begin();
    Serial.println("Setup complete.");
    Serial.println("");
}

void loop()
{
    if (millis() - prevRequestTime > requestInterval || prevRequestTime == 0)
    {
        prevRequestTime = millis();

        if (!getAccessToken())
        {
            error();
        }
    }

    if (getFollowerCount())
    {
        displayNumber(String(followers));
    }
    else
    {
        error();
    }

    Serial.println("");
    delay(10000);
}