//
// Created by sayan on 11/21/18.
//

#include "hclasses.h"
#include "khash.h"
#include "amhandlers.h"
#include "kvec.h"

#define DEFAULT_MAX_SOCKET_COUNT 1024
int max_socket_count;
int sock_status_vec_sz;

const char *HCTX_TAGSTR = "CTXT";
KHASH_MAP_INIT_INT(i32, int)

struct _hctx_t
{
    uint32_t tag;
    gasnet_node_t node_id;
    gasnet_node_t num_nodes;
    size_t segsize;                    ///< size of segment used for long active messages
    gasnet_seginfo_t *seginfo_table;   ///< an array of all remote segment addresses and sizes
    void *local_seg_addr;              ///< address of my registered segment accessible to peers
    hctx_t **peer_ctx_table;           ///< addresses of all peer context objects
    khash_t(i32) *server_map;          ///< a hash-map of servers with key = server post and value = server id
    hfunnel_t **mailbox;               ///< an array of multi-producer queues to local servers
    uint32_t *sockid_freelist;         ///< an array to store status (used/unused) of socket ids
    gasnet_hsl_t lock;
};

hctx_t *hctx_new(int argc, char **argv)
{
    /// bootstrap GASNet and initialize parameters
    GASNET_SAFE(gasnet_init(&argc, &argv));
    hctx_t *self = hmalloc(hctx_t, 1);
    self->tag = *((uint32_t *)HCTX_TAGSTR);
    self->node_id = gasnet_mynode();
    self->num_nodes = gasnet_nodes();
    self->segsize = gasnet_getMaxGlobalSegmentSize();
    size_t localsegsz = gasnet_getMaxLocalSegmentSize();

    /// initialize primary network resources
    gasnet_handlerentry_t htable[] = {
            {HSOCK_CLIENT_CONNECTION_REQUEST_HANDLER_IDX, &client_connection_request_handler},
            {HSOCK_CLIENT_CONNECTION_REPLY_HANDLER_IDX, &client_connection_reply_handler},
            {HSOCK_PICTURE_TRANSMISSION_REQ_HANDLER_IDX, &picture_transmission_request_handler},
            {HSOCK_TRANSMISSION_REP_HANDLER_IDX, &transmission_reply_handler},
            {HSOCK_PAYLOAD_TRANSMISSION_REQ_HANDLER_IDX, &payload_transmission_request_handler},
            {HSOCK_FLOW_TERMINATION_REQ_HANDLER_IDX, flow_termination_request_handler}
    };
    GASNET_SAFE(gasnet_attach(htable, sizeof(htable)/ sizeof(gasnet_handlerentry_t), self->segsize, GASNET_PAGESIZE));
    if (self->node_id == 0) {
        LOG_INFO("Max local segsize = %zd bytes.", localsegsz);
        LOG_INFO("Attached segsize = %zd bytes.", self->segsize);
        LOG_INFO("Max args = %zd, Max medium = %zd bytes, Max long request / reply = %zd / %zd bytes.",
                 gasnet_AMMaxArgs(), gasnet_AMMaxMedium(), gasnet_AMMaxLongRequest(), gasnet_AMMaxLongReply());
    }
    self->seginfo_table = hmalloc(gasnet_seginfo_t, self->num_nodes);
    GASNET_SAFE(gasnet_getSegmentInfo(self->seginfo_table, self->num_nodes));

    self->local_seg_addr = self->seginfo_table[self->node_id].addr;
    assert(self->segsize == self->seginfo_table[self->node_id].size);

    /// place myself in the registered address for everyone else to discover
    memcpy(self->local_seg_addr, &self, sizeof(hctx_t *));
    gasnet_barrier_notify(0,GASNET_BARRIERFLAG_ANONYMOUS);

    /// wait for all peers to finish and get the addresses of all other node contexts
    gasnet_barrier_wait(0,GASNET_BARRIERFLAG_ANONYMOUS);
    self->peer_ctx_table = hmalloc(hctx_t*, self->num_nodes);
    for (int i = 0; i < self->num_nodes; ++i)
    {
        gasnet_get_bulk(self->peer_ctx_table + i, i, self->seginfo_table[i].addr, sizeof(hctx_t *));
    }
    gasnet_barrier_notify(0,GASNET_BARRIERFLAG_ANONYMOUS);
    gasnet_barrier_wait(0,GASNET_BARRIERFLAG_ANONYMOUS);

    max_socket_count = DEFAULT_MAX_SOCKET_COUNT;
    kv_roundup32(max_socket_count);
    assert(max_socket_count <= 1<<16);
    sock_status_vec_sz = max_socket_count/32;
    self->mailbox = hmalloc(hfunnel_t*, max_socket_count);
    self->sockid_freelist = hmalloc(uint32_t , sock_status_vec_sz);
    for (int i = 0; i < sock_status_vec_sz; ++i)
        self->sockid_freelist[i] = UINT32_MAX;

    /// create a map for storing sockets
    self->server_map = kh_init(i32);

    gasnet_hsl_init(&self->lock);

    return self;
}

bool hctx_is(void *self)
{
    assert(self);
    return ((hctx_t*)self)->tag == *((uint32_t *)HCTX_TAGSTR);
}

void hctx_destroy(hctx_t **self_p)
{
    if (self_p && *self_p)
    {
        hctx_t *self = *self_p;
        assert(hctx_is(self));

        hfree(self->seginfo_table);
        hfree(self->peer_ctx_table);
        hfree(self->mailbox);
        hfree(self->sockid_freelist);
        kh_destroy(i32, self->server_map);
        gasnet_hsl_destroy(&self->lock);
        hfree(self);
        *self_p = NULL;

        gasnet_barrier_notify(0,GASNET_BARRIERFLAG_ANONYMOUS);
        gasnet_barrier_wait(0,GASNET_BARRIERFLAG_ANONYMOUS);
        gasnet_exit(0);
    }
}

int hctx_mynode(hctx_t *self)
{
    assert(hctx_is(self));
    return self->node_id;
}

int hctx_num_nodes(hctx_t *self)
{
    assert(hctx_is(self));
    return self->num_nodes;
}

size_t hctx_segsize(hctx_t *self)
{
    assert(hctx_is(self));
    return self->segsize;
}

void* hctx_segaddr(hctx_t *self)
{
    assert(hctx_is(self));
    return self->local_seg_addr;
}

int hctx_register(hctx_t *self, hfunnel_t *mailbox)
{
    assert(hctx_is(self));
    assert(hfunnel_is(mailbox));
    int i, pos = -1;
    gasnet_hsl_lock(&self->lock);
    for (i = 0; i < sock_status_vec_sz; ++i) {
        pos = __builtin_ffs(self->sockid_freelist[i])-1;
        if (pos >= 0) break;
    }
    gasnet_hsl_unlock(&self->lock);
    if (i == sock_status_vec_sz) {
        errno = EDQUOT;
        return -1;
    }
    else {
        self->sockid_freelist[i] &= ~(1<<pos);
        int id = i * 32 + pos;
        self->mailbox[id] = mailbox;
        return id;
    }
}

void hctx_unregister(hctx_t *self, uint16_t sock_id)
{
    assert(hctx_is(self));
    self->mailbox[sock_id] = NULL;
    int off = sock_id/32;
    int pos = sock_id%32;
    gasnet_hsl_lock(&self->lock);
    self->sockid_freelist[off] |= (1<<pos);
    gasnet_hsl_unlock(&self->lock);
}

bool hctx_bind(hctx_t *self, uint16_t endpoint, uint16_t socket_id)
{
    assert(hctx_is(self));
    int absent;
    bool rc = false;
    gasnet_hsl_lock(&self->lock);
    khint_t it = kh_put(i32, self->server_map, endpoint, &absent);
    if (absent >= 1) {
        kh_val(self->server_map, it) = socket_id;
        rc = true;
    }
    else if (absent == 0) {
        if (kh_val(self->server_map, it) < 0) {
            kh_val(self->server_map, it) = socket_id;
            rc = true;
        }
    }
    else
        LOG_WARN("Could not register socket at endpoint %x. Unkown error.", endpoint);
    gasnet_hsl_unlock(&self->lock);
    return rc;
}

void hctx_unbind(hctx_t *self, uint16_t endpoint)
{
    assert(hctx_is(self));
    gasnet_hsl_lock(&self->lock);
    khint_t it = kh_get(i32, self->server_map, endpoint);
    if (it != kh_end(self->server_map))
        kh_val(self->server_map, it) = -1;
    gasnet_hsl_unlock(&self->lock);
}

void* hctx_mailbox_at(hctx_t *self, uint16_t endpoint)
{
    assert(hctx_is(self));
    hfunnel_t *mailbox = NULL;
    gasnet_hsl_lock(&self->lock);
    khint_t it = kh_get(i32, self->server_map, endpoint);
    if (it != kh_end(self->server_map) && kh_val(self->server_map, it) >= 0) {
        mailbox = self->mailbox[kh_val(self->server_map, it)];
    }
    else LOG_WARN("Null mailbox.");
    gasnet_hsl_unlock(&self->lock);
    return mailbox;
}

hctx_t* hctx_peer_ctxptr(hctx_t *self, uint16_t node)
{
    assert(hctx_is(self));
    return self->peer_ctx_table[node];
}