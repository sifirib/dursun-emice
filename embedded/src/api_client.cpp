#include "api_client.h"

#include <Arduino.h>

#include "config.h"


namespace
{
    const char* RESPONSE_HEADERS[] =
    {
        "Content-Type",
        "Content-Length"
    };
}


bool ApiClient::is_https_url() const
{
    return String(
        SERVER_BASE_URL
    ).startsWith(
        "https://"
    );
}


bool ApiClient::health()
{
    const String url =
        String(SERVER_BASE_URL) +
        HEALTH_ENDPOINT;

    bool started = false;

    if (is_https_url())
    {
        secure_client.setInsecure();

        started =
            http.begin(
                secure_client,
                url
            );
    }
    else
    {
        started =
            http.begin(
                client,
                url
            );
    }

    if (!started)
    {
        return false;
    }

    http.setTimeout(
        HEALTH_TIMEOUT_MS
    );

    const int status_code =
        http.GET();

    http.end();

    return (
        status_code ==
        HTTP_CODE_OK
    );
}


bool ApiClient::begin_chat(
    const uint8_t* audio,
    size_t size
)
{
    const String url =
        String(SERVER_BASE_URL) +
        CHAT_ENDPOINT;

    response_size = -1;

    bool started = false;

    if (is_https_url())
    {
        secure_client.setInsecure();

        started =
            http.begin(
                secure_client,
                url
            );
    }
    else
    {
        started =
            http.begin(
                client,
                url
            );
    }

    if (!started)
    {
        Serial.println(
            "HATA: HTTP begin basarisiz."
        );

        return false;
    }

    http.setTimeout(
        HTTP_TIMEOUT_MS
    );

    http.collectHeaders(
        RESPONSE_HEADERS,
        2
    );

    http.addHeader(
        "Content-Type",
        "audio/wav"
    );

    Serial.print(
        "WAV gonderiliyor: "
    );

    Serial.print(size);

    Serial.println(" byte");

    const int status_code =
        http.POST(
            const_cast<uint8_t*>(
                audio
            ),
            size
        );

    Serial.print(
        "HTTP status: "
    );

    Serial.println(
        status_code
    );

    if (
        status_code !=
        HTTP_CODE_OK
    )
    {
        Serial.print(
            "HTTP POST hatasi: "
        );

        Serial.println(
            status_code
        );

        if (status_code > 0)
        {
            const String error_body =
                http.getString();

            if (!error_body.isEmpty())
            {
                Serial.println(
                    "Server hata cevabi:"
                );

                Serial.println(
                    error_body
                );
            }
        }

        http.end();

        return false;
    }

    const String content_type =
        http.header(
            "Content-Type"
        );

    Serial.print(
        "Content-Type: "
    );

    Serial.println(
        content_type
    );

    response_size =
        http.getSize();

    Serial.print(
        "Content-Length: "
    );

    Serial.println(
        response_size
    );

    if (
        !content_type.startsWith(
            "audio/mpeg"
        )
    )
    {
        Serial.println(
            "HATA: Sunucu audio/mpeg donmedi."
        );

        http.end();

        return false;
    }

    if (response_size <= 0)
    {
        Serial.println(
            "HATA: Sunucu gecersiz Content-Length dondu."
        );

        http.end();

        return false;
    }

    return true;
}


Stream* ApiClient::get_stream()
{
    return http.getStreamPtr();
}


void ApiClient::end_chat()
{
    http.end();

    response_size = -1;
}


int ApiClient::content_length() const
{
    return response_size;
}