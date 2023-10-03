#ifndef WEBSOCKET2_H
#define WEBSOCKET2_H

#include "csdl.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Websocket Websocket;

typedef struct PortKey
{
    MYFLT port;
    int nullTerminator;
} PortKey;

typedef struct WS_get
{
    OPDS h;
    void *output; // Will be STRINGDAT* or ARRAYDAT*
    MYFLT *port;
    STRINGDAT *path;
    PortKey portKey;
    CSOUND *csound;
    Websocket *websocket;
} WS_get;

#ifdef __cplusplus
}
#endif

#endif // #ifndef WEBSOCKET2_H
