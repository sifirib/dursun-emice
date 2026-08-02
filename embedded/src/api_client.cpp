#include "api_client.h"

#include "config.h"

bool ApiClient::health()
{
    client.setInsecure();

    http.begin(client, String(SERVER_BASE_URL) + HEALTH_ENDPOINT);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int status = http.GET();

    http.end();

    return status == HTTP_CODE_OK;
}

bool ApiClient::begin_chat(const uint8_t* audio, size_t size)
{
    content_length_ = 0;

    client.setInsecure();

    http.begin(client, String(SERVER_BASE_URL) + CHAT_ENDPOINT);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", "audio/wav");

    int status = http.POST(const_cast<uint8_t*>(audio), size);

    if (status != HTTP_CODE_OK)
    {
        http.end();
        return false;
    }

    String content_type = http.header("Content-Type");

    if (!content_type.startsWith("audio/mpeg"))
    {
        http.end();
        return false;
    }

    // StreamSource (player.cpp) akisi bitirmek icin bilinen bir boyuta
    // ihtiyac duyar. Sunucu her zaman tam mp3 govdesini tek seferde
    // dondurdugu icin (chunked degil) bu deger guvenilir sekilde gelir.
    content_length_ = http.getSize();

    if (content_length_ <= 0)
    {
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
}
