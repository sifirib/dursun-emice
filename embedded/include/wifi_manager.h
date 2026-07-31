#pragma once

class WifiManager
{
public:
    void begin();

    void update();

private:
    void reconnect();
};