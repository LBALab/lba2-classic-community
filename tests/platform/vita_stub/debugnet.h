#pragma once

#define DEBUG 3

struct debugNetConfiguration {
    int debugnet_initialized;
    int SocketFD;
    int logLevel;
};

extern "C" int debugNetInit(const char *ip, int port, int level);
extern "C" debugNetConfiguration *debugNetGetConf(void);
