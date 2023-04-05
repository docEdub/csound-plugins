#ifndef WEBSOCKET2_H
#define WEBSOCKET2_H

#include "csdl.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Websocket Websocket;

typedef struct WSget
{
    OPDS h;
    STRINGDAT *output;
    STRINGDAT *channelName;
    MYFLT *port;
    CSOUND *csound;
    Websocket *websocket;
    int i;
} WSget;

#ifdef __cplusplus
}
#endif

#endif // #ifndef WEBSOCKET2_H
