#include "websocket_get.h"

#include <libwebsockets.h>

static const char *SharedWebsocketDataGlobalVariableName = "SharedWebsocketData";
static const int WebsocketBufferCount = 2;//1024;
static const int WebsocketInitialMessageSize = 1024;

typedef struct {
    CS_HASH_TABLE *portWebsocketHashTable; // key = port float as string, value = Websocket
} SharedWebsocketData;

struct Websocket {
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
};

typedef struct WebsocketMessage {
    char *buffer;
    size_t size;
} WebsocketMessage;

typedef struct WebsocketPath {
    WebsocketMessage messages[WebsocketBufferCount];
    int messageIndex;
    void *messageIndexCircularBuffer;
} WebsocketPath;

enum {
    StringType = 1,
    Float64ArrayType = 2
};

static int32_t WS_resetSharedData(CSOUND *csound, void *vshared)
{
    SharedWebsocketData *shared = vshared;
    csound->DestroyHashTable(csound, shared->portWebsocketHashTable);
    csound->DestroyGlobalVariable(csound, SharedWebsocketDataGlobalVariableName);
    return OK;
}

static SharedWebsocketData *WS_getSharedData(CSOUND *csound)
{
    SharedWebsocketData *shared = csound->QueryGlobalVariable(csound, SharedWebsocketDataGlobalVariableName);
    if (shared) {
        return shared;
    }
    csound->CreateGlobalVariable(csound, SharedWebsocketDataGlobalVariableName, sizeof(SharedWebsocketData));
    shared = csound->QueryGlobalVariable(csound, SharedWebsocketDataGlobalVariableName);
    if (!shared) {
        csound->ErrorMsg(csound, "Websocket: failed to allocate globals");
    }
    shared->portWebsocketHashTable = csound->CreateHashTable(csound);
    csound->RegisterResetCallback(csound, shared, WS_resetSharedData);
    return shared;
}

char *initPortKeyString(MYFLT port, PortKey *portKey)
{
    portKey->port = port;
    portKey->nullTerminator = 0;

    char *s = (char*)(&portKey->port);
    for (size_t i = 0; i < sizeof(MYFLT); i++) {
        if (s[i] == 0) {
            s[i] = '~';
        }
    }

    return s;
}

WebsocketPath *initPathData(CSOUND *csound)
{
    WebsocketPath *pathData = csound->Calloc(csound, sizeof(WebsocketPath));
    pathData->messageIndexCircularBuffer = csound->CreateCircularBuffer(csound, WebsocketBufferCount, sizeof(pathData->messageIndex));
    return pathData;
}

static int32_t WS_callback(
    struct lws *websocket,
    enum lws_callback_reasons reason,
    void *user,
    void *inputData,
    size_t inputDataSize)
{
    if (reason != LWS_CALLBACK_ESTABLISHED && reason != LWS_CALLBACK_SERVER_WRITEABLE && reason != LWS_CALLBACK_RECEIVE) {
        return OK;
    }

    if (inputData && 0 < inputDataSize) {
        const struct lws_protocols *protocol = lws_get_protocol(websocket);
        Websocket *ws = protocol->user;
        CSOUND *csound = ws->csound;

        char *data = NULL;

        int isFinalFragment = lws_is_final_fragment(websocket);
        if (isFinalFragment && ws->receiveBufferIndex == 0) {
            data = inputData;
        }
        else {
            // Handle partially received messages.
            const int receivedCount = ws->receiveBufferIndex + inputDataSize;
            if (!ws->receiveBuffer) {
                ws->receiveBuffer = csound->Calloc(csound, receivedCount);
                ws->receiveBufferSize = receivedCount;
            }
            else if (ws->receiveBufferSize < receivedCount) {
                char *newReceiveBuffer = csound->Calloc(csound, receivedCount);
                memcpy(newReceiveBuffer, ws->receiveBuffer, ws->receiveBufferSize);
                csound->Free(csound, ws->receiveBuffer);
                ws->receiveBuffer = newReceiveBuffer;
                ws->receiveBufferSize = receivedCount;
            }
            char *receiveBufferDest = ws->receiveBuffer + ws->receiveBufferIndex;
            memcpy(receiveBufferDest, inputData, inputDataSize);
            if (!isFinalFragment) {
                ws->receiveBufferIndex += inputDataSize;
                return OK;
            }
            ws->receiveBufferIndex = 0;

            data = ws->receiveBuffer;
        }

        // Get the path. It should be a null terminated string at the beginning of the received data.
        char *d = data;
        char *path = d;

        size_t pathLength = strlen(path);
        if (pathLength == 0) {
            csound->Message(csound, Str("%s"), "WARNING: websocket path is empty\n");
            return OK;
        }

        // csound->Message(csound, Str("path = %s, "), path);
        d += pathLength + 1;

        // Get the data type. It should be either 1 or 2 for string or doubles array.
        const int type = *d;
        // csound->Message(csound, Str("type = %d, "), type);
        d++;

        char *bufferData = NULL;
        size_t bufferSize = 0;
        WebsocketPath *pathData = NULL;

        // Write the data to the path's messages circular buffer.
        if (Float64ArrayType == type) {
            d += (4 - ((d - data) % 4)) % 4;
            const uint32_t *length = (uint32_t*)d;
            d += 4;
            // csound->Message(csound, Str("length = %d, "), *length);

            d += (8 - ((d - data) % 8)) % 8;
            const double *values = (double*)d;
            // csound->Message(csound, Str("data = %s"), "[ ");
            // csound->Message(csound, Str("%.3f"), values[0]);
            // for (int i = 1; i < *length; i++) {
            //     csound->Message(csound, Str(", %.3f"), values[i]);
            // }
            // csound->Message(csound, Str("%s"), " ]\n");

            char *pathKey = csound->GetHashTableKey(csound, ws->pathFloatsHashTable, path);
            if (pathKey) {
                pathData = csound->GetHashTableValue(csound, ws->pathFloatsHashTable, pathKey);
            }
            else {
                pathData = initPathData(csound);
                csound->SetHashTableValue(csound, ws->pathFloatsHashTable, path, pathData);
            }
            bufferData = d;
            bufferSize = *length * sizeof(double);
        }
        else if (StringType == type) {
            // csound->Message(csound, Str("data = %s\n"), d);

            char *pathKey = csound->GetHashTableKey(csound, ws->pathStringHashTable, path);
            if (pathKey) {
                pathData = csound->GetHashTableValue(csound, ws->pathStringHashTable, pathKey);
            }
            else {
                pathData = initPathData(csound);
                csound->SetHashTableValue(csound, ws->pathStringHashTable, path, pathData);
            }
            bufferData = d;
            bufferSize = strlen(d) + 1;
        }
        else {
            csound->Message(csound, Str("WARNING: Unknown websocket data type %d received\n"), type);
            return OK;
        }

        WebsocketMessage *msg = &pathData->messages[pathData->messageIndex];

        if (0 < msg->size && msg->size < bufferSize) {
            csound->Free(csound, msg->buffer);
            msg->size = 0;
        }
        if (msg->size == 0) {
            msg->buffer = csound->Calloc(csound, bufferSize);
            msg->size = bufferSize;
        }
        memcpy(msg->buffer, bufferData, bufferSize);

        while (true) {
            int written = csound->WriteCircularBuffer(csound, pathData->messageIndexCircularBuffer, &pathData->messageIndex, 1);
            if (written != 0) {
                break;
            }

            // Message buffer is full. Read 1 item from it to free up room for the incoming message.
            // csound->Message(csound, Str("WARNING: port %d path %s message buffer full\n"), ws->info.port, path);
            int index;
            csound->ReadCircularBuffer(csound, pathData->messageIndexCircularBuffer, &index, 1);
        }

        pathData->messageIndex++;
        pathData->messageIndex %= WebsocketBufferCount;
    }

    return OK;
}

uintptr_t WS_processThread(void *vws)
{
    Websocket *ws = vws;
    ws->isRunning = true;

    while (ws->isRunning) {
        lws_service(ws->context, 0);
    }

    return 0;
}

Websocket *WS_initWebsocket(CSOUND *csound, MYFLT port, char *portKey)
{
    Websocket *ws = NULL;

    SharedWebsocketData *shared = WS_getSharedData(csound);

    char *hashTablePortKey = csound->GetHashTableKey(csound, shared->portWebsocketHashTable, portKey);
    if (hashTablePortKey) {
        ws = csound->GetHashTableValue(csound, shared->portWebsocketHashTable, hashTablePortKey);
        ws->refCount++;
        return ws;
    }

    ws = csound->Calloc(csound, sizeof(Websocket));
    ws->csound = csound;
    ws->pathStringHashTable = csound->CreateHashTable(csound);
    ws->pathFloatsHashTable = csound->CreateHashTable(csound);
    ws->refCount = 1;

    csound->SetHashTableValue(csound, shared->portWebsocketHashTable, portKey, ws);

    // Allocate 2 protocols; the actual protocol, and a null protocol at the end
    // (idk why, but this is how the original websocket opcode does it and the call to lws_service sometimes crashes
    // without it).
    ws->protocols = csound->Calloc(csound, sizeof(struct lws_protocols) * 2);

    ws->protocols[0].callback = WS_callback;
    ws->protocols[0].id = 1000;
    ws->protocols[0].name = "csound";
    ws->protocols[0].per_session_data_size = sizeof(void*);
    ws->protocols[0].user = ws;

    ws->info.port = port;
    ws->info.protocols = ws->protocols;
    ws->info.gid = -1;
    ws->info.uid = -1;

    lws_set_log_level(LLL_DEBUG, NULL);
    ws->context = lws_create_context(&ws->info);
    if (UNLIKELY(ws->context == NULL)) {
        csound->InitError(csound, Str("cannot start websocket on port %d\n"), port);
    }

    ws->processThread = csound->CreateThread(WS_processThread, ws);

    return ws;
}

void WS_deinitPathHashTable(CSOUND *csound, CS_HASH_TABLE *pathHashTable)
{
    CONS_CELL *pathItem = csound->GetHashTableValues(csound, pathHashTable);
    while (pathItem) {
        WebsocketPath *path = pathItem->value;

        for (int i = 0; i < WebsocketBufferCount; i++) {
            csound->Free(csound, path->messages[i].buffer);
            path->messages[i].size = 0;
        }

        csound->DestroyCircularBuffer(csound, path->messageIndexCircularBuffer);

        csound->Free(csound, path);

        pathItem = pathItem->next;
    }

    csound->DestroyHashTable(csound, pathHashTable);
}

void WS_deinitWebsocket(CSOUND *csound, Websocket *ws)
{
    ws->refCount--;
    if (0 < ws->refCount) {
        return;
    }

    ws->isRunning = false;
    lws_cancel_service(ws->context);

    csound->JoinThread(ws->processThread);

    lws_context_destroy(ws->context);

    WS_deinitPathHashTable(csound, ws->pathFloatsHashTable);
    WS_deinitPathHashTable(csound, ws->pathStringHashTable);

    csound->Free(csound, ws->receiveBuffer);
    csound->Free(csound, ws->protocols);
    csound->Free(csound, ws);
}

int32_t websocket_get_deinit(CSOUND *csound, void *vp)
{
    WS_get *p = vp;
    WS_deinitWebsocket(csound, p->websocket);
    return OK;
}

int32_t websocket_get_init(CSOUND *csound, WS_get *p)
{
    p->csound = csound;

    char *portKey = initPortKeyString(*p->port, &p->portKey);
    p->websocket = WS_initWebsocket(csound, *p->port, portKey);

    csound->RegisterDeinitCallback(csound, p, websocket_get_deinit);

    return OK;
}

int32_t websocket_getString_perf(CSOUND *csound, WS_get *p) {
    const Websocket *const ws = p->websocket;

    CS_HASH_TABLE *hashTable = ws->pathStringHashTable;

    WebsocketPath *wsPath = csound->GetHashTableValue(csound, hashTable, p->path->data);
    if (!wsPath) {
        return OK;
    }

    STRINGDAT *output = p->output;

    while (true) {
        int messageIndex = -1;
        const int read = csound->ReadCircularBuffer(csound, wsPath->messageIndexCircularBuffer, &messageIndex, 1);
        if (read == 1) {
            // Make sure we're reading the most recent message sent to the websocket.
            int unused = -1;
            if (csound->PeekCircularBuffer(csound, wsPath->messageIndexCircularBuffer, &unused, 1)) {
                continue;
            }

            char *d = wsPath->messages[messageIndex].buffer;

            // csound->Message(csound, Str("data = %s\n"), d);
            size_t length = strlen(d);
            if (output->size < length) {
                csound->Free(csound, output->data);
                output->data = csound->Malloc(csound, 2 * length + 1);
                output->size = 2 * length + 1;
            }
            memset(output->data, 0, output->size);
            memcpy(output->data, d, length);
        }
        else {
            break;
        }
    }

    return OK;
}

int32_t websocket_getArray_perf(CSOUND *csound, WS_get *p) {
    const Websocket *const ws = p->websocket;

    CS_HASH_TABLE *hashTable = ws->pathFloatsHashTable;

    WebsocketPath *wsPath = csound->GetHashTableValue(csound, hashTable, p->path->data);
    if (!wsPath) {
        return OK;
    }

    ARRAYDAT *output = p->output;

    while (true) {
        int messageIndex = -1;
        const int read = csound->ReadCircularBuffer(csound, wsPath->messageIndexCircularBuffer, &messageIndex, 1);
        if (read == 1) {
            // Make sure we're reading the most recent message sent to the websocket.
            int unused = -1;
            if (csound->PeekCircularBuffer(csound, wsPath->messageIndexCircularBuffer, &unused, 1)) {
                continue;
            }

            MYFLT *d = (MYFLT*) wsPath->messages[messageIndex].buffer;

            size_t size = wsPath->messages[messageIndex].size;
            int arrayLength = size / 4;
            if (output->allocated < arrayLength) {
                csound->Free(csound, output->data);
                output->data = csound->Malloc(csound, 2 * size);
                output->allocated = 2 * arrayLength;
            }
            memcpy(output->data, d, size);
        }
        else {
            break;
        }
    }

    return OK;
}

static OENTRY localops[] = {
    {
        .opname = "websocket_getString",
        .dsblksiz = sizeof(WS_get),
        .thread = 3,
        .outypes = "S",
        .intypes = "cS",
        .iopadr = (SUBR) websocket_get_init,
        .kopadr = (SUBR) websocket_getString_perf,
        .aopadr = NULL
    },
    {
        .opname = "websocket_getString",
        .dsblksiz = sizeof(WS_get),
        .thread = 3,
        .outypes = "S",
        .intypes = "iS",
        .iopadr = (SUBR) websocket_get_init,
        .kopadr = (SUBR) websocket_getString_perf,
        .aopadr = NULL
    },
    {
        .opname = "websocket_getArray",
        .dsblksiz = sizeof(WS_get),
        .thread = 3,
        .outypes = "k[]",
        .intypes = "cS",
        .iopadr = (SUBR) websocket_get_init,
        .kopadr = (SUBR) websocket_getArray_perf,
        .aopadr = NULL
    },
    {
        .opname = "websocket_getArray",
        .dsblksiz = sizeof(WS_get),
        .thread = 3,
        .outypes = "k[]",
        .intypes = "iS",
        .iopadr = (SUBR) websocket_get_init,
        .kopadr = (SUBR) websocket_getArray_perf,
        .aopadr = NULL
    }
};

LINKAGE
