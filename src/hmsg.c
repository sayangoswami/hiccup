//
// Created by sayan on 11/21/18.
//

#include "hclasses.h"
#include "hmsg.h"

const char* HMSG_TAGSTR = "MSSG";

struct _hmsg_t
{
    uint32_t tag;
    void *picture;
    size_t picture_len;
};

hmsg_t *hmsg_new()
{
    hmsg_t *self = hmalloc(hmsg_t, 1);
    self->tag = *((uint32_t *)HMSG_TAGSTR);
    self->picture = NULL;
    self->picture_len = 0;
    return self;
}

void hmsg_destroy(hmsg_t **self_p)
{
    if (self_p && *self_p)
    {
        hmsg_t *self = *self_p;
        if (self->picture && self->picture_len)
            hfree(self->picture);
        self->tag = 0xdeadbeef;
        hfree(self);
        *self_p = NULL;
    }
}

bool hmsg_is(void *self)
{
    assert(self);
    return ((hmsg_t *)self)->tag == *((uint32_t *)HMSG_TAGSTR);
}

void hmsg_set(hmsg_t *self, const char *picture, ...)
{
    assert(hmsg_is(self));
    va_list argptr;
    va_start(argptr, picture);
    hmsg_vset(self, picture, argptr);
    va_end(argptr);
}

void hmsg_vset(hmsg_t *self, const char *picture, va_list argptr)
{
    assert(hmsg_is(self));

    /// pessimistically allocate 8 bytes per picture element
    size_t picture_sz = strlen(picture) * 8;
    char *msg = hmalloc(char, picture_sz);
    size_t msg_sz = 0;

    /// get all the picture elements and pack into a single frame
    while (*picture)
    {
        switch (*picture)
        {
            case 'i':
                /// signed integer
                *((int *)(msg + msg_sz)) = va_arg(argptr, int);
                msg_sz += sizeof(int);
                break;
            case '1':
                /// unsigned char
                *((int *)(msg + msg_sz)) = va_arg(argptr, int);
                msg_sz += sizeof(int);
                break;
            case '2':
                /// unsigned short
                *((int *)(msg + msg_sz)) = va_arg(argptr, int);
                msg_sz += sizeof(int);
                break;
            case '4':
                /// unsigned int
                *((uint32_t*)(msg + msg_sz)) = va_arg(argptr, uint32_t);
                msg_sz += sizeof(uint32_t);
                break;
            case '8':
                /// unsigned long
                *((uint64_t*)(msg + msg_sz)) = va_arg(argptr, uint64_t);
                msg_sz += sizeof(uint64_t);
                break;
            case 'd':
                /// double
                *((double *)(msg + msg_sz)) = va_arg(argptr, double);
                msg_sz += sizeof(double);
                break;
            case 'p':
                /// pointer
                *((uintptr_t *)(msg + msg_sz)) = (uintptr_t)va_arg(argptr, void*);
                msg_sz += sizeof(uintptr_t);
                break;
            default:
                LOG_ERR("Invalid picture element '%c'", *picture);
        }
        ++picture;
    }
    hmsg_set_picture(self, msg, msg_sz);
}

void hmsg_get(hmsg_t *self, const char *picture, ...)
{
    assert(hmsg_is(self));
    va_list argptr;
    va_start(argptr, picture);
    hmsg_vget(self, picture, argptr);
    va_end(argptr);
}

void hmsg_vget(hmsg_t *self, const char *picture, va_list argptr)
{
    assert(hmsg_is(self));
    size_t ppos = 0;
    while (*picture)
    {
        if (*picture == 'i')
        {
            int *int_p = va_arg(argptr, int*);
            if (int_p)
            {
                *int_p = *((int*)(self->picture + ppos));
                ppos += sizeof(int);
            }
        }
        else if (*picture == '1')
        {
            uint8_t *u8_p = va_arg(argptr, uint8_t*);
            if (u8_p)
            {
                *u8_p = (uint8_t)(*((int *)(self->picture + ppos)));
                ppos += sizeof(int);
            }
        }
        else if (*picture == '2')
        {
            uint16_t *u16_p = va_arg(argptr, uint16_t*);
            if (u16_p)
            {
                *u16_p = (uint16_t)(*((int *)(self->picture + ppos)));
                ppos += sizeof(int);
            }
        }
        else if (*picture == '4')
        {
            uint32_t *u32_p = va_arg(argptr, uint32_t*);
            if (u32_p)
            {
                *u32_p = *((uint32_t *)(self->picture + ppos));
                ppos += sizeof(uint32_t);
            }
        }
        else if (*picture == '8')
        {
            uint64_t *u64_p = va_arg(argptr, uint64_t*);
            if (u64_p)
            {
                *u64_p = *((uint64_t *)(self->picture + ppos));
                ppos += sizeof(uint64_t);
            }
        }
        else if (*picture == 'd')
        {
            double *d_p = va_arg(argptr, double*);
            if (d_p)
            {
                *d_p = *((double *)(self->picture + ppos));
                ppos += sizeof(double);
            }
        }
        else if (*picture == 'p')
        {
            void **void_p_p = va_arg(argptr, void **);
            if (void_p_p)
            {
                *void_p_p = (void *)(*((uintptr_t *)(self->picture + ppos)));
                ppos += sizeof(uintptr_t);
            }
        }
        else LOG_ERR("Invalid picture element '%c'", *picture);
        ++picture;
    }
    hfree(self->picture);
    self->picture_len = 0;
}

void hmsg_picture(hmsg_t *self, void **data_p, size_t *size_p)
{
    assert(hmsg_is(self));
    *data_p = self->picture;
    *size_p = self->picture_len;
}

void hmsg_set_picture(hmsg_t *self, void *data, size_t size)
{
    assert(hmsg_is(self));
    self->picture = data;
    self->picture_len = size;
}