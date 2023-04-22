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
    MYFLT *port;
    STRINGDAT *channelName;
    MYFLT portKey;
    int portKeyNullTermiator;
    CSOUND *csound;
    Websocket *websocket;
} WSget;

#ifdef __cplusplus
}
#endif

#endif // #ifndef WEBSOCKET2_H
