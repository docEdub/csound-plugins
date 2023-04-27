#include "Websocket2.h"

#include <libwebsockets.h>

static const char *SharedWebsocketDataGlobalVariableName = "SharedWebsocketData";
static const int WebsocketBufferCount = 1024;
static const int WebsocketInitialMessageSize = 1024;

typedef struct {
    CS_HASH_TABLE *portWebsocketHashTable;
} SharedWebsocketData;

struct Websocket {
    CSOUND *csound;
    CS_HASH_TABLE *pathHashTable;
    int refCount;
    struct lws_context *context;
    // struct lws *websocket;
    struct lws_protocols *protocols;
    void *processThread;
    struct lws_context_creation_info info;
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

    const struct lws_protocols *protocol = lws_get_protocol(websocket);
    Websocket *sharedPortData = protocol->user;
    CSOUND *csound = sharedPortData->csound;

    if (inputData && 0 < inputDataSize) {
        // const int messageSize = inputDataSize + 1;

        // STRINGDAT *s = &ws->messages[ws->messageIndex];

        // if (0 < s->size && s->size < messageSize) {
        //     csound->Free(csound, s->data);
        //     s->size = 0;
        // }
        // if (s->size == 0) {
        //     s->size = sizeof(char) * messageSize;
        //     s->data = (char*) csound->Calloc(csound, s->size);
        // }
        // memcpy(s->data, inputData, inputDataSize);
        // s->data[inputDataSize] = '\0';

        // while (true) {
        //     const int written = csound->WriteCircularBuffer(csound, ws->messageIndexBuffer, &ws->messageIndex, 1);
        //     if (written != 0) {
        //         break;
        //     }
        //     csound->Sleep(1);
        // }

        // ws->messageIndex++;
        // ws->messageIndex %= WebsocketBufferCount;
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
    ws->pathHashTable = csound->CreateHashTable(csound);
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

    // for (int i = 0; i < WebsocketBufferCount; i++) {
    //     STRINGDAT *s = &ws->messages[i];
    //     s->size = WebsocketInitialMessageSize + 1;
    //     s->data = (char*) csound->Calloc(csound, s->size);
    // }

    // ws->messageIndex = 0;
    // ws->messageIndexBuffer = csound->CreateCircularBuffer(csound, WebsocketBufferCount, sizeof(int));
    ws->processThread = csound->CreateThread(WS_processThread, ws);

    return ws;
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

    // for (int i = 0; i < WebsocketBufferCount; i++) {
    //     const STRINGDAT *s = &ws->messages[i];
    //     if (s->data && 0 < s->size) {
    //         csound->Free(csound, s->data);
    //     }
    // }

    lws_context_destroy(ws->context);

    csound->DestroyHashTable(csound, ws->pathHashTable);
    csound->Free(csound, ws->protocols);
    csound->Free(csound, ws);
}

int32_t WSget_deinit(CSOUND *csound, void *vp)
{
    WSget *p = vp;

    WS_deinitWebsocket(csound, p->websocket);

    return OK;
}

int32_t WSget_init(CSOUND *csound, WSget *p)
{
    p->csound = csound;

    p->output->data = csound->Calloc(csound, 11);

    p->portKey = *p->port;
    char *portKey = (char*) &p->portKey;
    for (size_t i = 0; i < sizeof(MYFLT); i++) {
        if (portKey[i] == 0) {
            portKey[i] = '~';
        }
    }

    p->websocket = WS_initWebsocket(csound, *p->port, portKey);

    csound->RegisterDeinitCallback(csound, p, WSget_deinit);

    return OK;
}

int32_t WSget_perf(CSOUND *csound, WSget *p) {
    const Websocket *const ws = p->websocket;

    // while (true) {
        // int messageIndex = 0;
        // const int read = csound->ReadCircularBuffer(csound, ws->messageIndexBuffer, &messageIndex, 1);
        // if (read == 1) {
        //     char *data = ws->messages[messageIndex].data;
        //     char *d = data;

        //     csound->Message(csound, Str("path = %s, "), d);
        //     d += strlen(data) + 1;

        //     const int type = *d;
        //     csound->Message(csound, Str("type = %d, "), type);
        //     d++;

        //     if (StringType == type) {
        //         csound->Message(csound, Str("data = %s\n"), d);
        //     }
        //     else if (Float64ArrayType == type) {
        //         d += (4 - ((d - data) % 4)) % 4;
        //         const uint32_t *length = d;
        //         csound->Message(csound, Str("length = %d, "), *length);
        //         d += 4;

        //         d += (8 - ((d - data) % 8)) % 8;
        //         const double *data = d;
        //         csound->Message(csound, Str("data = %s"), "[ ");
        //         csound->Message(csound, Str("%.3f"), data[0]);
        //         for (int i = 1; i < *length; i++) {
        //             csound->Message(csound, Str(", %.3f"), data[i]);
        //         }
        //         csound->Message(csound, Str("%s"), " ]\n");
        //     }
        // }
        // else {
        //     break;
        // }
    // }

    return OK;
}

static OENTRY localops[] = {
    {
        .opname = "WSget",
        .dsblksiz = sizeof(WSget),
        .thread = 3,
        .outypes = "S",
        .intypes = "cS",
        .iopadr = (SUBR) WSget_init,
        .kopadr = (SUBR) WSget_perf,
        .aopadr = NULL
    }
};

LINKAGE
