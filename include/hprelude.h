//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HPRELUDE_H
#define HICCUP_HPRELUDE_H

#define GASNET_PAR 1
#define GASNETT_THREAD_SAFE 1

/// uncomment the following to use gasnet's malloc services
// #define USE_DEBUG_MALLOC

#include <gasnet.h>
#include <gasnet_tools.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <malloc.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <stdlib.h>

/** Logging routines and macros */

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

static char timeStr[9] = {0};

static char *time_str()
{
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    sprintf(timeStr, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return timeStr;
}

#define LOG_ERR(fmt, ...) do {                                      \
      fprintf(stderr, "[%s]" RED "ERROR: " fmt RESET " at %s:%i\n", \
            time_str(), ##__VA_ARGS__, __FILE__, __LINE__);         \
            fflush(stderr);                                         \
            exit(1);                                                \
} while(0)

#define LOG_INFO(fmt, ...) do {                             \
      fprintf(stdout, "[%s]" GRN "INFO: " fmt RESET "\n",   \
            time_str(), ##__VA_ARGS__);                     \
            fflush(stdout);                                 \
} while(0)

#define LOG_WARN(fmt, ...) do {                                      \
      fprintf(stdout, "[%s]" YEL "WARN: " fmt RESET " at %s:%i\n",   \
            time_str(), ##__VA_ARGS__, __FILE__, __LINE__);          \
            fflush(stdout);                                          \
} while(0)

#define GASNET_SAFE(fncall) do {                                \
      int err;                                                  \
      if ((err = fncall) != GASNET_OK) {                        \
         fprintf(stderr, "[%s]" RED "ERROR calling %s" RESET    \
               " at %s:%i\n"                                    \
               "Cause: %s (%s)\n",                              \
               time_str(), #fncall, __FILE__, __LINE__,         \
               gasnet_ErrorName(err),                           \
               gasnet_ErrorDesc(err)                            \
         );                                                     \
         fflush(stderr);                                        \
         gasnet_exit(err);                                      \
      }                                                         \
} while(0)

/** memory allocation/deallcoation utils */

static inline void *
safe_malloc (size_t size, const char *file, unsigned line)
{
    void *mem = calloc(1, size);
    if (mem == NULL) {
        fprintf (stderr, "FATAL ERROR at %s:%u\n", file, line);
        fprintf (stderr, "OUT OF MEMORY (calloc returned NULL)\n");
        fflush (stderr);
        abort ();
    }
    return mem;
}

static inline void*
safe_realloc(void *ptr, size_t size, const char *file, unsigned line)
{
    void *mem = realloc(ptr, size);
    if (mem == NULL)
    {
        fprintf (stderr, "FATAL ERROR at %s:%u\n", file, line);
        fprintf (stderr, "OUT OF MEMORY (realloc returned NULL)\n");
        fflush (stderr);
        abort ();
    }
    return mem;
}

#if GASNET_DEBUGMALLOC && defined USE_DEBUG_MALLOC
#   define hmalloc(type, count) ((type*)gasnett_debug_calloc(count, sizeof(type)))
#else
#   define hmalloc(type, count) ((type*)safe_malloc((count)*sizeof(type), __FILE__, __LINE__))
#endif

#if GASNET_DEBUGMALLOC && defined USE_DEBUG_MALLOC
#   define hrealloc(ptr, type, count) ((type*)gasnett_debug_realloc((ptr), (count)*sizeof(type)))
#else
#   define hrealloc(ptr, type, count) ((type*)safe_realloc((ptr), (count)*sizeof(type), __FILE__, __LINE__))
#endif

#if GASNET_DEBUGMALLOC && defined USE_DEBUG_MALLOC
#   define hfree(x) do {gasnett_debug_free(x); (x) = NULL;} while(0)
#else
#   define hfree(x) do {free((x)); (x) = NULL;} while(0)
#endif

#endif //HICCUP_HPRELUDE_H
