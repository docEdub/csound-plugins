#ifndef WEBSOCKET_SET_H
#define WEBSOCKET_SET_H

#include "websocket_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WS_set
{
    OPDS h;
    MYFLT *port;
    STRINGDAT *path;
    void *input; // Must be STRINGDAT* or ARRAYDAT*
    PortKey portKey;
    CSOUND *csound;
    Websocket *websocket;
} WS_set;

#ifdef __cplusplus
}
#endif

#endif // #ifndef WEBSOCKET_SET_H
