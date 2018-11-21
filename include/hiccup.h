//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HICCUP_H
#define HICCUP_HICCUP_H

#include "hprelude.h"

/** Opaque structures */
typedef struct _hqueue_t hqueue_t;
typedef struct _hfunnel_t hfunnel_t;
typedef struct _hmsg_t hmsg_t;
typedef struct _hpair_t hpair_t;
typedef struct _hpipe_t hpipe_t;
typedef struct _hactor_t hactor_t;
typedef struct _hctx_t hctx_t;
typedef struct _hsock_t hsock_t;

/** Public classes */
#include "hqueue.h"
#include "hfunnel.h"
#include "hpair.h"
#include "hpipe.h"
#include "hactor.h"
#include "hctx.h"
#include "hsock.h"

#endif //HICCUP_HICCUP_H
