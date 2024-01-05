#include <ArduinoWiFiServer.h>

#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

struct HttpResult{
    int httpCode;
    String payload;
};

class SpotifyClient {
public:
    SpotifyClient();
    void Init(String clientId, String clientSecret, String deviceName, String refreshToken);
    void FetchToken();
    int Play( String context_uri );
    int Shuffle();
    int Next();
    String GetDevices();

private:
    
    String clientId;
    String clientSecret;
    String redirectUri;
    String accessToken;
    String refreshToken;
    String deviceId;
    String deviceName;

    String ParseJson(String key, String json );
    HttpResult CallAPI( String method, String url, String body );

    String GetDeviceId(String json);

   
};

 
