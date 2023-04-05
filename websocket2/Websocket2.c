#include "Websocket2.h"

#include <libwebsockets.h>

static const size_t WriteBufferBytesCount = 2048;

 struct Websocket {
    struct lws_context *context;
    struct lws *websocket;
    struct lws_protocols *protocols;
    void *processThread;
    unsigned char *messageBuffer;
    struct lws_context_creation_info info;
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
    CSOUND *csound = p->csound;
    
    return OK;
}

uintptr_t WS_processThread(void *vp)
{
    WSget *p = vp;
    p->websocket->isRunning = true;

    while (p->websocket->isRunning) {

      // https://libwebsockets.org/lws-api-doc-main/html/group__service.html#gaf95bd0c663d6516a0c80047d9b1167a8
      //
      // Service any pending websocket activity.
      //  context:  Websocket context
      //  timeout_ms:  Set to 0; ignored; for backward compatibility
      //
      int returnValue = lws_service(p->websocket->context, 0);

      // https://libwebsockets.org/lws-api-doc-main/html/group__callback-when-writeable.html#gabbe4655c7eeb3eb1671b2323ec6b3107
      //
      // Request a callback for all connections using the given protocol when it becomes possible to write to each socket without blocking in turn.
      //  context:  Websocket context
      //  protocol:  Protocol whose connections will get callbacks
      lws_callback_on_writable_all_protocol(p->websocket->context, &p->websocket->protocols[0]);
    }

    return 0;
}

int32_t WS_deinitializeWebsocket(CSOUND *csound, void *vp)
{
    WSget *p = vp;
    p->websocket->isRunning = false;

    Websocket *ws = p->websocket;
    csound->JoinThread(ws->processThread);
    lws_cancel_service(ws->context);
    lws_context_destroy(ws->context);
    csound->Free(csound, ws->messageBuffer);
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
    ws->messageBuffer = csound->Calloc(
        csound,
        LWS_SEND_BUFFER_PRE_PADDING +
        (sizeof(char) * WriteBufferBytesCount) +
        LWS_SEND_BUFFER_POST_PADDING
    );
    if (UNLIKELY(ws->context == NULL)) {
        csound->Die(csound, "%s", Str("websocket: could not initialize websocket. Exiting"));
    }

    ws->processThread = csound->CreateThread(WS_processThread, p);
    csound->RegisterDeinitCallback(csound, p, WS_deinitializeWebsocket);

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
    for (int j = 0; j < p->i; ++j) {
        p->output->data[j] = '.';
    }
    p->output->size = p->i;
    p->i %= 10;
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