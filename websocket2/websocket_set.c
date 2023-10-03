#include "websocket_set.h"

int32_t websocket_set_deinit(CSOUND *csound, void *vp)
{
    WS_get *p = vp;
    WS_deinitWebsocket(csound, p->websocket);
    return OK;
}

int32_t websocket_set_init(CSOUND *csound, WS_get *p)
{
    p->csound = csound;
    csound->RegisterDeinitCallback(csound, p, websocket_set_deinit);
    return OK;
}

int32_t websocket_setString_perf(CSOUND *csound, WS_set *p) {
    const Websocket *const ws = p->websocket;
    return OK;
}

int32_t websocket_setArray_perf(CSOUND *csound, WS_get *p) {
    const Websocket *const ws = p->websocket;
    return OK;
}
