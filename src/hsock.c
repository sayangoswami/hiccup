//
// Created by sayan on 11/21/18.
//

#include "hclasses.h"
#include "kvec.h"
#include "amhandlers.h"

#define HSOCK_BOUND 0x1
#define HSOCK_CONNECTED 0x2

#define MAX_MEDIUM_AM_SZ 4000

const char* HSOCK_TAGSTR = "SOCK";
const char* SERVER_TAGSTR = "SRVR";
const char* CLIENT_TAGSTR = "CLNT";

typedef void    (*send_fn)(void *self, const char *picture, va_list argptr);
typedef void    (*recv_fn)(void *self, const char *picture, va_list argptr);
typedef bool    (*peek_fn)(void *self, const char *picture, va_list argptr);
typedef void    (*destroy_fn)(void **self_p);

typedef struct {
    void *data;
    size_t size;
} frame_t;

typedef kvec_t(frame_t) framevec_t;

struct _hsock_t
{
    uint32_t tag;
    hctx_t *ctx;
    uint16_t node_id;
    uint16_t socket_id;
    hfunnel_t *mailbox;
    int conn_status;
    char *medium_AM_buffer;
    framevec_t frames;
    gasnett_atomic64_t *atomic64;
    void *derived;
    send_fn send;
    recv_fn recv;
    peek_fn peek;
    destroy_fn destroy;
};

typedef struct _server_t
{
    uint32_t tag;
    hsock_t *base;
    uint16_t endpoint;
}
        server_t;

typedef struct _client_t
{
    uint32_t tag;
    hsock_t *base;
    uint16_t s_node_id;     ///< ID of server's node
    hfunnel_t *s_mailbox;   ///< socket's mailbox
}
        client_t;

///** Bare minimum information required by the server to communicate to the client */
//typedef struct _client_portal_t
//{
//    gasnet_node_t c_node_id;   ///< ID of the node at which the client resides
//    hqueue_t *s_q_in;          ///< queue at server's node used by the server to receive messages from client
//    hqueue_t *s_q_out;         ///< queue at client's node used by the server to send messages to client
//}
//client_portal_t;

///////////////////////////// extern function declarations ////////////////////////
bool hctx_bind(hctx_t *self, uint16_t endpoint, uint16_t socket_id);
extern void hctx_unbind(hctx_t *self, uint16_t endpoint);
extern hctx_t* hctx_peer_ctxptr(hctx_t *self, uint16_t node);
extern int hctx_register(hctx_t *self, hfunnel_t *mailbox);
extern void hctx_unregister(hctx_t *self, uint16_t sock_id);
extern void* hctx_mailbox_at(hctx_t *self, uint16_t endpoint);

///////////////////////////// helper functions and utilities /////////////////////////////

static size_t serialize(hsock_t *self, const char *picture, va_list argptr)
{
    assert(hsock_is(self));
    kv_clear(self->frames);

    size_t psz = sizeof(picture_transmission_request_packet_t);
    frame_t frame;
    while (*picture)
    {
        switch (*picture)
        {
            case 'i':
                /// signed integer
                *((int *)(self->medium_AM_buffer + psz)) = va_arg(argptr, int);
                psz += sizeof(int);
                break;
            case '1':
                /// unsigned char
                *((int*)(self->medium_AM_buffer + psz)) = va_arg(argptr, int);
                psz += sizeof(int);
                break;
            case '2':
                /// unsigned short
                *((int*)(self->medium_AM_buffer + psz)) = va_arg(argptr, int);
                psz += sizeof(int);
                break;
            case '4':
                /// unsigned int
                *((uint32_t*)(self->medium_AM_buffer + psz)) = va_arg(argptr, uint32_t);
                psz += sizeof(uint32_t);
                break;
            case '8':
                /// unsigned long
                *((uint64_t*)(self->medium_AM_buffer + psz)) = va_arg(argptr, uint64_t);
                psz += sizeof(uint64_t);
                break;
            case 'd':
                /// double
                *((double *)(self->medium_AM_buffer + psz)) = va_arg(argptr, double);
                psz += sizeof(double);
                break;
            case 'p':
                /// pointer
                *((uintptr_t *)(self->medium_AM_buffer + psz)) = (uintptr_t)va_arg(argptr, void*);
                psz += sizeof(uintptr_t);
                break;
            case 'b':
                /// payload address (must be followed by size)
                /// do not use va_arg macro twice in a single call
                frame.data = va_arg(argptr, void*);
                frame.size = va_arg(argptr, size_t);
                kv_push(frame_t, self->frames, frame);
                break;
            default:
                LOG_ERR("Invalid picture element '%c'", *picture);
        }
        ++picture;
    }

    /// append the payload sizes (if any) at the end of the picture
    size_t framecount = kv_size(self->frames);
    if (framecount)
    {
        uint64_t *payload_sizes = (uint64_t *)(self->medium_AM_buffer + psz);
        for (int i = 0; i < framecount; ++i)
            payload_sizes[i] = kv_A(self->frames, i).size;
        psz += (framecount * sizeof(uint64_t));
    }
    return psz;
}

static void deserialize(frame_t *frames, const char *picture, va_list argptr)
{
    frame_t picture_frame = frames[0];
    char *picturebuff = picture_frame.data;
    picture_transmission_request_packet_t *p = (picture_transmission_request_packet_t*)picturebuff;
    if (*picture && *picture == 's') {
        uint16_t *u16_p = va_arg(argptr, uint16_t*);
        void **void_p_p = va_arg(argptr, void **);
        if (u16_p && void_p_p)
        {
            *u16_p = p->src_node;
            *void_p_p = (void *)p->src_mailbox;
        }
        ++picture;
    }

    picturebuff += sizeof(picture_transmission_request_packet_t);

    size_t ppos = 0;
    int payload_frame_id = 1;

    while (*picture)
    {
        if (*picture == 'i')
        {
            int *int_p = va_arg(argptr, int*);
            if (int_p)
            {
                *int_p = *((int*)(picturebuff + ppos));
                ppos += sizeof(int);
            }
        }
        else if (*picture == '1')
        {
            uint8_t *u8_p = va_arg(argptr, uint8_t*);
            if (u8_p)
            {
                *u8_p = (uint8_t)(*((int *)(picturebuff + ppos)));
                ppos += sizeof(int);
            }
        }
        else if (*picture == '2')
        {
            uint16_t *u16_p = va_arg(argptr, uint16_t*);
            if (u16_p)
            {
                *u16_p = (uint16_t)(*((int *)(picturebuff + ppos)));
                ppos += sizeof(int);
            }
        }
        else if (*picture == '4')
        {
            uint32_t *u32_p = va_arg(argptr, uint32_t*);
            if (u32_p)
            {
                *u32_p = *((uint32_t *)(picturebuff + ppos));
                ppos += sizeof(uint32_t);
            }
        }
        else if (*picture == '8')
        {
            uint64_t *u64_p = va_arg(argptr, uint64_t*);
            if (u64_p)
            {
                *u64_p = *((uint64_t *)(picturebuff + ppos));
                ppos += sizeof(uint64_t);
            }
        }
        else if (*picture == 'd')
        {
            double *d_p = va_arg(argptr, double*);
            if (d_p)
            {
                *d_p = *((double *)(picturebuff + ppos));
                ppos += sizeof(double);
            }
        }
        else if (*picture == 'p')
        {
            void **void_p_p = va_arg(argptr, void **);
            if (void_p_p)
            {
                *void_p_p = (void *)(*((uintptr_t *)(picturebuff + ppos)));
                ppos += sizeof(uintptr_t);
            }
        }
        else if (*picture == 'b')
        {
            void **payload_p = va_arg(argptr, void**);
            size_t *payload_sz_p = va_arg(argptr, size_t*);
            if (payload_p && payload_sz_p)
            {
                frame_t payload_frame = frames[payload_frame_id];
                ++payload_frame_id;
                *payload_p = payload_frame.data;
                *payload_sz_p = payload_frame.size;
            }
        }
        else LOG_ERR("Invalid picture element '%c'", *picture);
        ++picture;
    }
    hfree(frames);
}

static void flood(hsock_t *self, gasnet_node_t dest_node, hfunnel_t *dest_mailbox, size_t picsz)
{
    gasnett_atomic64_set(self->atomic64, 0, GASNETT_ATOMIC_REL); /// cleanup

    /// send picture
    picture_transmission_request_packet_t p = {
            .payloadcount = kv_size(self->frames), .dest_mailbox = dest_mailbox,
            .atomicobj = self->atomic64, .src_mailbox = self->mailbox, .src_node = self->node_id
    };
    memcpy(self->medium_AM_buffer, &p, sizeof(picture_transmission_request_packet_t));
    GASNET_SAFE(gasnet_AMRequestMedium0(
            dest_node, HSOCK_PICTURE_TRANSMISSION_REQ_HANDLER_IDX, self->medium_AM_buffer, picsz));

    if (!p.payloadcount) /// this message has no payload.. nothing to flood
        GASNET_BLOCKUNTIL(gasnett_atomic64_swap(self->atomic64, 0, GASNETT_ATOMIC_NONE) != 0);
    else /// multiframe message
    {
        uintptr_t dst_frames_ptr = 0;
        GASNET_BLOCKUNTIL(
                (dst_frames_ptr = gasnett_atomic64_swap(self->atomic64, 0, GASNETT_ATOMIC_NONE))
                != 0);

        /// send a flood of messages
        payload_transmission_request_packet_t rp = {
                .frames = (frame_t*)dst_frames_ptr, .frame_id = 1, .offset = 0, .atomicobj = self->atomic64
        };
        uint64_t msg_counter = 0;
        for (uint i = 0; i < p.payloadcount; ++i)
        {
            char *data = kv_A(self->frames, i).data;
            size_t size = kv_A(self->frames, i).size;
            assert(size <= UINT32_MAX);
            uint offset = 0;
            while(size)
            {
                size_t nbytes = MIN(size, MAX_MEDIUM_AM_SZ);
                rp.frame_id = i+1;
                rp.offset = offset;
                gasnet_handlerarg_t *a = (gasnet_handlerarg_t*)(&rp);
                GASNET_SAFE(gasnet_AMRequestMedium6(dest_node, HSOCK_PAYLOAD_TRANSMISSION_REQ_HANDLER_IDX,
                                                    data + offset, nbytes, a[0], a[1], a[2], a[3], a[4], a[5]));
                ++msg_counter;
                size -= nbytes;
                offset += nbytes;
            }
        }

        /// wait till all messages have been received
        GASNET_BLOCKUNTIL(
                gasnett_atomic64_compare_and_swap(self->atomic64, msg_counter, 0, GASNETT_ATOMIC_NONE) != 0);

        /// send short AM to terminate flow
        flow_termination_request_packet_t ftrp = {
                .mailbox = dest_mailbox, .atomicobj = self->atomic64, .frames = (frame_t*)dst_frames_ptr
        };
        GASNET_SAFE(gasnet_AMRequestMedium0(dest_node, HSOCK_FLOW_TERMINATION_REQ_HANDLER_IDX, &ftrp, sizeof(ftrp)));
        GASNET_BLOCKUNTIL(gasnett_atomic64_swap(self->atomic64, 0, 0) > 0);
    }
}

///////////////////////////// derived class functions ///////////////////////////

static server_t *server_new(hsock_t *base, uint16_t endpoint)
{
    if (hctx_bind(base->ctx, endpoint, base->socket_id)) {
        server_t *self = hmalloc(server_t, 1);
        self->tag = *((uint32_t *)SERVER_TAGSTR);
        self->base = base;
        self->endpoint = endpoint;
        return self;
    } else {
        errno = EADDRINUSE;
        return NULL;
    }
}

static bool server_is(void *self)
{
    assert(self);
    return ((server_t*)self)->tag == *((uint32_t *)SERVER_TAGSTR);
}

static void server_destroy(void **vself_p)
{
    if (vself_p && *vself_p)
    {
        server_t *self = *vself_p;
        assert(server_is(self));
        hctx_unbind(self->base->ctx, self->endpoint);
        self->tag = 0xDeadBeef;
        hfree(self);
        *vself_p = NULL;
    }
}

void server_send(void *vself, const char *picture, va_list argptr)
{
    server_t *self = (server_t *)vself;
    assert(server_is(self));
    assert(self->base->conn_status == HSOCK_BOUND);

    /// the first argument in the picture must hold the client's mailbox address
    if(*picture != 's') LOG_WARN("No destination specified. Dropping packet.");
    else {
        uint16_t client_node = (uint16_t)va_arg(argptr, int);
        hfunnel_t *client_mailbox = va_arg(argptr, void*);
        size_t psz = serialize(self->base, picture+1, argptr);
        flood(self->base, client_node, client_mailbox, psz);
    }
}

void server_recv(void *vself, const char *picture, va_list argptr)
{
    server_t *self = (server_t *)vself;
    assert(server_is(self));
    assert(self->base->conn_status == HSOCK_BOUND);

    frame_t *frames = NULL;
    GASNET_BLOCKUNTIL((frames = hfunnel_pop(self->base->mailbox)) != NULL);

    deserialize(frames, picture, argptr);
}

client_t *client_new(hsock_t *base, uint16_t node, uint16_t endpoint)
{
    /// initialize values
    client_t *self = hmalloc(client_t, 1);
    self->tag = *((uint32_t *)CLIENT_TAGSTR);
    self->base = base;
    self->s_node_id = node;

    client_connection_request_packet_t p;
    p.server_endpoint = endpoint;

    /// get address of context at server's node
    p.server_ctx = hctx_peer_ctxptr(base->ctx, node);

    gasnett_atomic64_set(self->base->atomic64, UINT64_MAX, GASNETT_ATOMIC_NONE);
    p.atomicobj = self->base->atomic64;

    /// send a connection request
    GASNET_SAFE(gasnet_AMRequestMedium0(node, HSOCK_CLIENT_CONNECTION_REQUEST_HANDLER_IDX, &p, sizeof(p)));

    /// wait until a response is received
    uintptr_t server_mailbox_ptr;
    GASNET_BLOCKUNTIL(
            (server_mailbox_ptr = gasnett_atomic64_swap(self->base->atomic64, UINT64_MAX, GASNETT_ATOMIC_NONE))
            != UINT64_MAX);
    if (server_mailbox_ptr != 0) {
        self->s_mailbox = (hfunnel_t*)server_mailbox_ptr;
        return self;
    } else {
        self->tag = 0xDeadBeef;
        hfree(self);
        errno = ENOTCONN;
        return NULL;
    }
}

bool client_is(void *self)
{
    assert(self);
    return ((client_t*)self)->tag == *((uint32_t *)CLIENT_TAGSTR);
}

void client_destroy(void **vself_p)
{
    if (vself_p && *vself_p)
    {
        client_t *self = (client_t *)(*vself_p);
        assert(client_is(self));
        self->tag = 0xDeadBeef;
        hfree(self);
        *vself_p = NULL;
    }
}

void client_send(void *vself, const char *picture, va_list argptr)
{
    client_t *self = (client_t *)vself;
    assert(client_is(self));
    assert(self->base->conn_status == HSOCK_CONNECTED);

    size_t psz = serialize(self->base, picture, argptr);
    flood(self->base, self->s_node_id, self->s_mailbox, psz);
}

void client_recv(void *vself, const char *picture, va_list argptr)
{
    client_t *self = (client_t *)vself;
    assert(client_is(self));
    assert(self->base->conn_status == HSOCK_CONNECTED);

    frame_t *frames = NULL;
    GASNET_BLOCKUNTIL((frames = hfunnel_pop(self->base->mailbox)) != NULL);

    deserialize(frames, picture, argptr);
}

bool client_peek(void *vself, const char *picture, va_list argptr)
{
    client_t *self = (client_t *)vself;
    assert(client_is(self));
    assert(self->base->conn_status == HSOCK_CONNECTED);

    gasnet_AMPoll();
    frame_t *frames = hfunnel_pop(self->base->mailbox);
    if (frames) {
        deserialize(frames, picture, argptr);
        return true;
    } else return false;
}

//////////////////////////////////// AM handlers ////////////////////////////////////

/** Invoked by client at server's node */
void client_connection_request_handler(gasnet_token_t token, void *buf, size_t nbytes)
{
    assert(nbytes == sizeof(client_connection_request_packet_t));
    client_connection_request_packet_t *p = (client_connection_request_packet_t *)buf;
    hctx_t *ctx = p->server_ctx;
    assert(hctx_is(ctx));
    uint16_t endpoint = p->server_endpoint;
    client_connection_reply_packet_t r;
    r.server_mailbox = hctx_mailbox_at(ctx, endpoint);
    r.atomicobj = p->atomicobj;
    gasnet_AMReplyMedium0(token, HSOCK_CLIENT_CONNECTION_REPLY_HANDLER_IDX, &r, sizeof(r));
}

/** Invoked by server at client's node */
void client_connection_reply_handler(gasnet_token_t token, void *buf, size_t nbytes)
{
    assert(nbytes == sizeof(client_connection_reply_packet_t));
    client_connection_reply_packet_t *p = (client_connection_reply_packet_t*)buf;
    gasnett_atomic64_set(p->atomicobj, (uintptr_t)p->server_mailbox, GASNETT_ATOMIC_NONE);
}

void picture_transmission_request_handler(gasnet_token_t token, void *buf, size_t nbytes)
{
    picture_transmission_request_packet_t *p = (picture_transmission_request_packet_t*)buf;
    //buf = (char*)buf + sizeof(picture_transmission_request_packet_t);
    //nbytes -= sizeof(picture_transmission_request_packet_t);
    int framecount = p->payloadcount + 1;
    frame_t *frames = hmalloc(frame_t, framecount);
    char *picture = hmalloc(char, nbytes);
    memcpy(picture, buf, nbytes);
    frames[0].data = picture, frames[0].size = nbytes;

    if (p->payloadcount) /// this is a multiframe message
    {
        /// allocate memory for payloads (create empty frames)
        uint64_t *payload_sizes = (uint64_t*)((char*)buf + nbytes - (p->payloadcount * sizeof(uint64_t)));
        for (int i = 0; i < p->payloadcount; ++i)
        {
            size_t size = payload_sizes[i];
            char *payload = hmalloc(char, size);
            frames[i+1].data = payload, frames[i+1].size = size;
        }
        /// send address of frame-array back to source
        transmission_reply_packet_t tp = {(uintptr_t)frames, p->atomicobj, GASNET_REPLY_ATOMIC_SET};
        GASNET_SAFE(gasnet_AMReplyMedium0(token, HSOCK_TRANSMISSION_REP_HANDLER_IDX, &tp, sizeof(tp)));
    }
    else /// this is the only frame
    {
        /// add to queue and send ACK
        hfunnel_push(p->dest_mailbox, frames);
        transmission_reply_packet_t tp = {1, p->atomicobj, GASNET_REPLY_ATOMIC_SET};
        GASNET_SAFE(gasnet_AMReplyMedium0(token, HSOCK_TRANSMISSION_REP_HANDLER_IDX, &tp, sizeof(tp)));
    }
}

void transmission_reply_handler(gasnet_token_t token, void *buf, size_t nbytes)
{
    assert(nbytes == sizeof(transmission_reply_packet_t));
    transmission_reply_packet_t *p = (transmission_reply_packet_t*)buf;
    switch (p->opcode)
    {
        case GASNET_REPLY_ATOMIC_SET:
            gasnett_atomic64_set(p->atomicobj, p->atomicval, GASNETT_ATOMIC_NONE);
            break;
        case GASNET_REPLY_ATOMIC_INCR:
            gasnett_atomic64_add(p->atomicobj, p->atomicval, GASNETT_ATOMIC_NONE);
            break;
        default:
            LOG_ERR("Unknown opcode %d.", p->opcode);
    }
}

void payload_transmission_request_handler(gasnet_token_t token, void *buf, size_t nbytes,
                                          gasnet_handlerarg_t a0, gasnet_handlerarg_t a1, gasnet_handlerarg_t a2,
                                          gasnet_handlerarg_t a3, gasnet_handlerarg_t a4, gasnet_handlerarg_t a5)
{
    gasnet_handlerarg_t a[] = {a0,a1,a2,a3,a4,a5};
    payload_transmission_request_packet_t *p = (payload_transmission_request_packet_t*)a;

    /// copy data from message
    frame_t frame = ((frame_t*)p->frames)[p->frame_id];
    memcpy((char*)frame.data + p->offset, buf, nbytes);

    /// increment atomic counter at destination
    transmission_reply_packet_t tp = {1, p->atomicobj, GASNET_REPLY_ATOMIC_INCR};
    GASNET_SAFE(gasnet_AMReplyMedium0(token, HSOCK_TRANSMISSION_REP_HANDLER_IDX, &tp, sizeof(tp)));
}

void flow_termination_request_handler(gasnet_token_t token, void *buf, size_t nbytes)
{
    assert(nbytes == sizeof(flow_termination_request_packet_t));
    flow_termination_request_packet_t *p = (flow_termination_request_packet_t*)buf;
    hfunnel_push(p->mailbox, p->frames);
    transmission_reply_packet_t tp = {1, p->atomicobj, GASNET_REPLY_ATOMIC_SET};
    GASNET_SAFE(gasnet_AMReplyMedium0(token, HSOCK_TRANSMISSION_REP_HANDLER_IDX, &tp, sizeof(tp)));
}

///////////////////////////// user-accessible functions ///////////////////////////

hsock_t *hsock_new(hctx_t *ctx)
{
    hsock_t *self= hmalloc(hsock_t, 1);
    self->tag = *((uint32_t *)HSOCK_TAGSTR);
    self->ctx = ctx;
    self->node_id = (uint16_t) hctx_mynode(ctx);
    self->conn_status = 0;
    self->mailbox = hfunnel_new();
    assert(self->mailbox);
    int id = hctx_register(ctx, self->mailbox);
    if (id < 0) LOG_ERR("Could not create socket because %s.", strerror(errno));
    else self->socket_id = (uint16_t)id;

    self->medium_AM_buffer = hmalloc(char, MAX_MEDIUM_AM_SZ);
    kv_init(self->frames);
    self->atomic64 = hmalloc(gasnett_atomic64_t, 1);

    gasnett_atomic64_set(self->atomic64, 0, GASNETT_ATOMIC_REL);
    self->derived = NULL;
    self->send = NULL;
    self->recv = NULL;
    self->peek = NULL;
    self->destroy = NULL;

    return self;
}

bool hsock_is(void *self)
{
    assert(self);
    return ((hsock_t*)self)->tag == *((uint32_t *)HSOCK_TAGSTR);
}

void hsock_destroy(hsock_t **self_p)
{
    /// if socket is bound or connected, call derived object's destructor. destroy self.
    if (self_p && *self_p)
    {
        hsock_t *self = *self_p;
        assert(hsock_is(self));
        if (self->conn_status == HSOCK_BOUND)
            hsock_unbind(self);
        else if (self->conn_status == HSOCK_CONNECTED)
            hsock_disconnect(self);
        hctx_unregister(self->ctx, self->socket_id);
        hfree(self->medium_AM_buffer);
        hfree(self->atomic64);
        if (hfunnel_pop(self->mailbox) != NULL)
            LOG_WARN("Socket %d at node %d exiting with non-empty mailbox.", self->node_id, self->socket_id);
        hfunnel_destroy(&self->mailbox);
        kv_destroy(self->frames);

        self->tag = 0xDeadBeef;
        hfree(self);
        *self_p = NULL;
    }
}

bool hsock_bind(hsock_t *self, uint16_t endpoint)
{
    assert(hsock_is(self));
    assert(!self->conn_status);
    server_t *server = server_new(self, endpoint);
    if (server)
    {
        self->conn_status = HSOCK_BOUND;
        self->derived = server;
        self->send = server_send;
        self->recv = server_recv;
        self->destroy = server_destroy;
        return true;
    }
    return false;
}

bool hsock_connect(hsock_t *self, uint16_t node, uint16_t endpoint)
{
    assert(hsock_is(self));
    assert(!self->conn_status);
    client_t *client = client_new(self, node, endpoint);
    if (client)
    {
        self->conn_status = HSOCK_CONNECTED;
        self->derived = client;
        self->send = client_send;
        self->recv = client_recv;
        self->peek = client_peek;
        self->destroy = client_destroy;
        return true;
    }
    return false;
}

void hsock_unbind(hsock_t *self)
{
    assert(hsock_is(self));
    assert(self->conn_status == HSOCK_BOUND);
    self->destroy(&(self->derived));
    self->send = NULL;
    self->recv = NULL;
    self->destroy = NULL;
    self->conn_status = 0;
}

void hsock_disconnect(hsock_t *self)
{
    assert(hsock_is(self));
    assert(self->conn_status == HSOCK_CONNECTED);
    self->destroy(&(self->derived));
    self->send = NULL;
    self->recv = NULL;
    self->peek = NULL;
    self->destroy = NULL;
    self->conn_status = 0;
}

void hsock_send(hsock_t *self, const char *picture, ...)
{
    assert(hsock_is(self));
    assert(self->conn_status);
    va_list argptr;
    va_start(argptr, picture);
    self->send(self->derived, picture, argptr);
    va_end(argptr);
}

void hsock_recv(hsock_t *self, const char *picture, ...)
{
    assert(hsock_is(self));
    assert(self->conn_status);
    va_list argptr;
    va_start(argptr, picture);
    self->recv(self->derived, picture, argptr);
    va_end(argptr);
}

bool hsock_peek(hsock_t *self, const char *picture, ...)
{
    assert(hsock_is(self));
    assert(self->conn_status);
    va_list argptr;
    va_start(argptr, picture);
    bool rc = false;
    if (self->conn_status == HSOCK_CONNECTED)
        rc = self->peek(self->derived, picture, argptr);
    else
    {
        self->recv(self->derived, picture, argptr);
        rc =  true;
    }
    va_end(argptr);
    return rc;
}