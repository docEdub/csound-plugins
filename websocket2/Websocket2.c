#include "Websocket2.h"

#include <libwebsockets.h>

// TODO: Try getting the buffer working with an odd sized circular buffer.
// - Idea is to write to the circular buffer twice on each websocket message received.
//      - 1st write is the actual pointer to the buffer.
//      - 2nd write is a null pointer. If it fails then we know the buffer is full.
//          - If buffer is full, set a flag and wait for WSget_perf to clear it.

static const size_t WebsocketMessageCountMax = 10;

 struct Websocket {
    struct lws_context *context;
    struct lws *websocket;
    struct lws_protocols *protocols;
    void *processThread;
    struct lws_context_creation_info info;
    void *messageBuffer;
    STRINGDAT messages[2 * WebsocketMessageCountMax];
    int messageIndex;
    int messageIndex2;
    bool isMessageBufferFull;
    bool wasMessageBufferFull;
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
    WSget *p = protocol->user;
    Websocket *ws = p->websocket;
    CSOUND *csound = p->csound;
    
    if (inputData && 0 < inputDataSize) {
        while (ws->isMessageBufferFull) {
            csound->Sleep(0);
        }

        const int messageSize = inputDataSize + 1;

        STRINGDAT *s = &ws->messages[ws->messageIndex];
        if (0 < s->size && s->size < messageSize) {
            csound->Free(csound, s->data);
            s->size = 0;
        }
        if (s->size == 0) {
            s->size = sizeof(char) * messageSize;
            s->data = csound->Calloc(csound, s->size);
        }
        memcpy(s->data, inputData, inputDataSize);
        s->data[inputDataSize] = '\0';

        if (ws->wasMessageBufferFull) {
            // If the message buffer was full then the 2nd message in the even/odd pair wasn't written, yet.
            // Write it now so we're back on the correct even step in the circular buffer.
            if (!csound->WriteCircularBuffer(csound, ws->messageBuffer, &ws->messageIndex, 1)) {
                csound->Message(csound, Str("%s\n"), "Warning: Websocket failed to get back on even step");
            }
            ws->wasMessageBufferFull = false;
        }
        const int n = csound->WriteCircularBuffer(csound, ws->messageBuffer, &ws->messageIndex, 2);
        if (n != 2) {
            ws->isMessageBufferFull = true;
            ws->wasMessageBufferFull = true;
        }

        ws->messageIndex += 2;
        ws->messageIndex %= 2 * WebsocketMessageCountMax;
        ws->messageIndex2 = ws->messageIndex + 1;

        if (ws->isMessageBufferFull == WebsocketMessageCountMax) {
            lws_cancel_service(ws->context);
        }
    }

    return OK;
}

uintptr_t WS_processThread(void *vp)
{
    WSget *p = vp;
    Websocket *ws = p->websocket;
    ws->isRunning = true;

    while (p->websocket->isRunning) {
        if (!ws->isMessageBufferFull) {
            lws_service(p->websocket->context, 0);
        }
    }

    return 0;
}

int32_t WS_destroyWebsocket(CSOUND *csound, void *vp)
{
    WSget *p = vp;
    Websocket *ws = p->websocket;
    ws->isRunning = false;

    for (int i = 0; i < WebsocketMessageCountMax; i++) {
        const STRINGDAT *s = &ws->messages[i];
        if (s->data && 0 < s->size) {
            csound->Free(csound, s->data);
        }
    }
    ws->messageIndex = 0;

    csound->JoinThread(ws->processThread);

    lws_cancel_service(ws->context);
    lws_context_destroy(ws->context);

    csound->DestroyCircularBuffer(csound, ws->messageBuffer);
    csound->Free(csound, ws->protocols);
    csound->Free(csound, ws);

    return OK;
}

Websocket *WS_createWebsocket(CSOUND *csound, const char *channelName, MYFLT port, WSget *p)
{
    Websocket *ws = csound->Calloc(csound, sizeof(Websocket));

    // Allocate 2 protocols; the actual protocol, and a null protocol at the end
    // (idk why, but this is how the original websocket opcode does it and the call to lws_service sometimes crashes
    // without it).
    ws->protocols = csound->Calloc(csound, sizeof(struct lws_protocols) * 2);
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

    ws->messageIndex = 0;
    ws->messageIndex2 = 0;
    ws->isMessageBufferFull = false;
    ws->wasMessageBufferFull = false;
    memset(ws->messages, 0, sizeof(STRINGDAT*) * 2 * WebsocketMessageCountMax);
    ws->messageBuffer = csound->CreateCircularBuffer(csound, sizeof(int) * WebsocketMessageCountMax, sizeof(int));
    ws->processThread = csound->CreateThread(WS_processThread, p);

    csound->RegisterDeinitCallback(csound, p, WS_destroyWebsocket);

    return ws;
}

int32_t WSget_init(CSOUND *csound, WSget *p)
{
    p->csound = csound;
    p->output->data = csound->Calloc(csound, 11);
    p->i = 0;

    p->websocket = WS_createWebsocket(csound, p->channelName->data, *p->port, p);

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

    int32_t n = 0;
    int messageIndex[2];
    while (true) {
        n = csound->ReadCircularBuffer(csound, p->websocket->messageBuffer, messageIndex, 2);
        if (n != 0) {
            int i = messageIndex[0];
            if (i % 2) {
                i--;
            }
            csound->Message(csound, Str("[%d]: %s\n"), i, p->websocket->messages[i].data);
        }
        else {
            break;
        }
    }
    p->websocket->isMessageBufferFull = false;

    return OK;
}

static OENTRY localops[] = {
  {
    .opname = "WSget",
    .dsblksiz = sizeof(WSget),
    .thread = 3,
    .outypes = "S",
    .intypes = "Sc",
    .iopadr = (SUBR)WSget_init,
    .kopadr = (SUBR)WSget_perf,
    .aopadr = NULL
  }

};

LINKAGE
