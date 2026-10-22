#pragma once

extern "C" int sceNetSend(int socket, const void *data, unsigned int length, int flags);
extern "C" int sceNetSocketClose(int socket);
