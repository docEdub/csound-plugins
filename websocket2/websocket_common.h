#ifndef WEBSOCKET_COMMON_H
#define WEBSOCKET_COMMON_H

#include "csdl.h"

#include <libwebsockets.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Websocket {
    CSOUND *csound;
    CS_HASH_TABLE *pathFloatsHashTable; // key = path string, value = WebsocketPath containing a MYFLT array.
    CS_HASH_TABLE *pathStringHashTable; // key = path string, value = WebsocketPath containing a string
    int refCount;
    struct lws_context *context;
    struct lws_protocols *protocols;
    struct lws_context_creation_info info;
    char *receiveBuffer;
    int receiveBufferSize;
    int receiveBufferIndex;
    void *processThread;
    bool isRunning;
} Websocket;

typedef struct PortKey
{
    MYFLT port;
    int nullTerminator;
} PortKey;

#ifdef __cplusplus
}
#endif

#endif // #ifndef WEBSOCKET_COMMON_H
