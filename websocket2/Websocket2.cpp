#include "Websocket2.h"

#include <libwebsockets.h>

static const int WebsocketBufferCount = 1024;
static const int WebsocketInitialMessageSize = 1024;

enum {
    StringType = 1,
    Float64ArrayType = 2
};

struct Websocket {
    struct lws_context *context;
    struct lws *websocket;
    struct lws_protocols *protocols;
    void *processThread;
    struct lws_context_creation_info info;
    STRINGDAT messages[WebsocketBufferCount];
    int messageIndex;
    void *messageIndexBuffer;
    bool isRunning;
};

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
    WSget *p = (WSget*) protocol->user;
    Websocket *ws = p->websocket;
    CSOUND *csound = p->csound;

    if (inputData && 0 < inputDataSize) {
        const int messageSize = inputDataSize + 1;

        STRINGDAT *s = &ws->messages[ws->messageIndex];

        if (0 < s->size && s->size < messageSize) {
            csound->Free(csound, s->data);
            s->size = 0;
        }
        if (s->size == 0) {
            s->size = sizeof(char) * messageSize;
            s->data = (char*) csound->Calloc(csound, s->size);
        }
        memcpy(s->data, inputData, inputDataSize);
        s->data[inputDataSize] = '\0';

        while (true) {
            const int written = csound->WriteCircularBuffer(csound, ws->messageIndexBuffer, &ws->messageIndex, 1);
            if (written != 0) {
                break;
            }
            csound->Sleep(1);
        }

        ws->messageIndex++;
        ws->messageIndex %= WebsocketBufferCount;
    }

    return OK;
}

uintptr_t WS_processThread(void *vp)
{
    WSget *p = (WSget*) vp;
    Websocket *ws = p->websocket;
    ws->isRunning = true;

    while (ws->isRunning) {
        lws_service(ws->context, 0);
    }

    return 0;
}

int32_t WS_destroyWebsocket(CSOUND *csound, void *vp)
{
    WSget *p = (WSget*) vp;
    Websocket *ws = p->websocket;
    ws->isRunning = false;

    for (int i = 0; i < WebsocketBufferCount; i++) {
        const STRINGDAT *s = &ws->messages[i];
        if (s->data && 0 < s->size) {
            csound->Free(csound, s->data);
        }
    }

    csound->JoinThread(ws->processThread);

    lws_cancel_service(ws->context);
    lws_context_destroy(ws->context);

    csound->DestroyCircularBuffer(csound, ws->messageIndexBuffer);
    csound->Free(csound, ws->protocols);
    csound->Free(csound, ws);

    return OK;
}

Websocket *WS_createWebsocket(CSOUND *csound, const char *channelName, MYFLT port, WSget *p)
{
    Websocket *ws = (Websocket*) csound->Calloc(csound, sizeof(Websocket));

    // Allocate 2 protocols; the actual protocol, and a null protocol at the end
    // (idk why, but this is how the original websocket opcode does it and the call to lws_service sometimes crashes
    // without it).
    ws->protocols = (lws_protocols*) csound->Calloc(csound, sizeof(struct lws_protocols) * 2);
    memset(ws->protocols, 0, sizeof(struct lws_protocols) * 2);
    
    ws->protocols[0].callback = WS_callback;
    ws->protocols[0].id = 1000;
    ws->protocols[0].name = channelName;
    ws->protocols[0].per_session_data_size = sizeof(void*);
    ws->protocols[0].user = p;

    ws->info.port = port;
    ws->info.protocols = ws->protocols;
    ws->info.gid = -1;
    ws->info.uid = -1;

    lws_set_log_level(LLL_DEBUG, NULL);
    ws->context = lws_create_context(&ws->info);
    if (UNLIKELY(ws->context == NULL)) {
        csound->Die(csound, "%s", Str("websocket: could not initialize websocket. Exiting"));
    }

    for (int i = 0; i < WebsocketBufferCount; i++) {
        STRINGDAT *s = &ws->messages[i];
        s->size = WebsocketInitialMessageSize + 1;
        s->data = (char*) csound->Calloc(csound, s->size);
    }

    ws->messageIndex = 0;
    ws->messageIndexBuffer = csound->CreateCircularBuffer(csound, WebsocketBufferCount, sizeof(int));
    ws->processThread = csound->CreateThread(WS_processThread, p);

    csound->RegisterDeinitCallback(csound, p, WS_destroyWebsocket);

    return ws;
}

int32_t WSget_init(CSOUND *csound, WSget *p)
{
    p->csound = csound;
    p->output->data = (char*) csound->Calloc(csound, 11);
    p->i = 0;

    p->websocket = WS_createWebsocket(csound, "csound", *p->port, p);

    return OK;
}

int32_t WSget_perf(CSOUND *csound, WSget *p) {
    if (p->i == 0) {
      memset(p->output->data, 0, 11);
    }
    p->i++;
    // for (int j = 0; j < p->i; ++j) {
    //     p->output->data[j] = '.';
    // }
    // p->output->size = p->i;
    p->i %= 10;

    const Websocket *const ws = p->websocket;

    while (true) {
        int messageIndex = 0;
        const int read = csound->ReadCircularBuffer(csound, ws->messageIndexBuffer, &messageIndex, 1);
        if (read == 1) {
            char *data = ws->messages[messageIndex].data;
            char *d = data;

            csound->Message(csound, Str("path = %s, "), d);
            d += strlen(data) + 1;

            const int type = *d;
            csound->Message(csound, Str("type = %d, "), type);
            d++;

            if (StringType == type) {
                csound->Message(csound, Str("data = %s\n"), d);
            }
            else if (Float64ArrayType == type) {
                d += (4 - ((d - data) % 4)) % 4;
                const uint32_t *length = (const uint32_t*) d;
                csound->Message(csound, Str("length = %d, "), *length);
                d += 4;

                d += (8 - ((d - data) % 8)) % 8;
                const double *data = (const double *) d;
                csound->Message(csound, Str("data = %s"), "[ ");
                csound->Message(csound, Str("%.3f"), data[0]);
                for (int i = 1; i < *length; i++) {
                    csound->Message(csound, Str(", %.3f"), data[i]);
                }
                csound->Message(csound, Str("%s"), " ]\n");
            }
        }
        else {
            break;
        }
    }

    return OK;
}

static OENTRY localops[] = {
    {
        .opname = (char*) "WSget",
        .dsblksiz = sizeof(WSget),
        .thread = 3,
        .outypes = (char*) "S",
        .intypes = (char*) "Sc",
        .iopadr = (SUBR) WSget_init,
        .kopadr = (SUBR) WSget_perf,
        .aopadr = nullptr
    }
};

LINKAGE
