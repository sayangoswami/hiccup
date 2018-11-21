//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_AMHANDLERS_H
#define HICCUP_AMHANDLERS_H

#define GASNET_REPLY_ATOMIC_SET 1
#define GASNET_REPLY_ATOMIC_INCR 2

typedef struct _picture_transmission_request_packet_t
{
    uint16_t payloadcount;          ///< number of frames in the payload
    hfunnel_t *dest_mailbox;        ///< address of mailbox at destination node where the new message will be placed
    uint16_t src_node;
    gasnett_atomic64_t *atomicobj;  ///< address of atomic object at source node used for ACK
    hfunnel_t *src_mailbox;         ///< address of mailbox at source socket to be used for replies
} picture_transmission_request_packet_t;

void picture_transmission_request_handler(gasnet_token_t token, void *buf, size_t nbytes);
#define HSOCK_PICTURE_TRANSMISSION_REQ_HANDLER_IDX 128

typedef struct _payload_transmission_request_packet_t
{
    void *frames;                   ///< address of payload array created by the picture transmission handler
    uint32_t frame_id;              ///< which frame should the data be written to
    uint32_t offset;                ///< offset within the payload where the data is to be written
    gasnett_atomic64_t *atomicobj;  ///< address of atomic object at source node used for ACK
} payload_transmission_request_packet_t;

void payload_transmission_request_handler(gasnet_token_t token, void *buf, size_t nbytes,
                                          gasnet_handlerarg_t a0, gasnet_handlerarg_t a1, gasnet_handlerarg_t a2,
                                          gasnet_handlerarg_t a3, gasnet_handlerarg_t a4, gasnet_handlerarg_t a5);
#define HSOCK_PAYLOAD_TRANSMISSION_REQ_HANDLER_IDX 129

typedef struct _transmission_reply_packet_t
{
    uint64_t atomicval;
    gasnett_atomic64_t *atomicobj;
    int opcode;
} transmission_reply_packet_t;

void transmission_reply_handler(gasnet_token_t token, void *buf, size_t nbytes);
#define HSOCK_TRANSMISSION_REP_HANDLER_IDX 130

typedef struct _flow_termination_request_packet_t
{
    void *frames;                   ///< address of payload array created by the picture transmission handler
    hfunnel_t *mailbox;             ///< mailbox at destination node where the new message will be placed
    gasnett_atomic64_t *atomicobj;  ///< address of atomic object at source node used for ACK
} flow_termination_request_packet_t;

void flow_termination_request_handler(gasnet_token_t token, void *buf, size_t nbytes);
#define HSOCK_FLOW_TERMINATION_REQ_HANDLER_IDX 131

typedef struct _client_connection_request_packet_t {
    hctx_t *server_ctx;             ///< address of context at destination (server's node)
    uint16_t server_endpoint;       ///< endpoint of the server
    gasnett_atomic64_t *atomicobj;  ///< address of atomic object at source node used for ACK
} client_connection_request_packet_t;

void client_connection_request_handler(gasnet_token_t token, void *buf, size_t nbytes);
#define HSOCK_CLIENT_CONNECTION_REQUEST_HANDLER_IDX 132

typedef struct _client_connection_reply_packet_t {
    hfunnel_t *server_mailbox;      ///< pointer to mailbox of the server at destination node
    gasnett_atomic64_t *atomicobj;  ///< address of atomic object used for ACK
} client_connection_reply_packet_t;

void client_connection_reply_handler(gasnet_token_t token, void *buf, size_t nbytes);
#define HSOCK_CLIENT_CONNECTION_REPLY_HANDLER_IDX 133

#endif //HICCUP_AMHANDLERS_H
