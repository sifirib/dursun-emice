#include "api_client.h"

#include <HTTPClient.h>

#include "config.h"

bool ApiClient::health()
{
    HTTPClient http;

    http.begin(String(SERVER_URL) + "/health");

    int status_code = http.GET();

    http.end();

    return status_code == 200;
}