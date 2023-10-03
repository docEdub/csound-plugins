#ifndef WEBSOCKET_GET_H
#define WEBSOCKET_GET_H

#include "websocket_common.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#endif // #ifndef WEBSOCKET_GET_H