
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


ngx_uint_t  ngx_pagesize;
ngx_uint_t  ngx_pagesize_shift;
ngx_uint_t  ngx_cacheline_size;


/**
 * @brief 申请内存(对malloc()封装)
 *
 * @param size 要申请内存的大小
 * @param log 日志
 *
 * @return 成功返回新申请的内存首地址 失败返回NULL
 */
void *
ngx_alloc(size_t size, ngx_log_t *log)
{
    void  *p;

    p = malloc(size);
    if (p == NULL) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "malloc(%uz) failed", size);
    }

    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, log, 0, "malloc: %p:%uz", p, size);

    return p;
}


/**
 * @brief 申请内存, 并用0填充内存
 *
 * @param size 要申请内存的大小
 * @param log 日志
 *
 * @return 成功返回新申请的内存首地址 失败返回NULL
 *
 * @see ngx_alloc()
 */
void *
ngx_calloc(size_t size, ngx_log_t *log)
{
    void  *p;

    p = ngx_alloc(size, log);

    if (p) {
		/* 用0填充 */
        ngx_memzero(p, size);
    }

    return p;
}


#if (NGX_HAVE_POSIX_MEMALIGN)

/**
 * @brief 以alignment对齐申请内存(对posix_memalign()封装)
 *
 * @param alignment
 * @param size
 * @param log
 *
 * @return 成功返回新申请的内存首地址 失败返回NULL
 */
void *
ngx_memalign(size_t alignment, size_t size, ngx_log_t *log)
{
    void  *p;
    int    err;

    err = posix_memalign(&p, alignment, size);

    if (err) {
        ngx_log_error(NGX_LOG_EMERG, log, err,
                      "posix_memalign(%uz, %uz) failed", alignment, size);
        p = NULL;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_ALLOC, log, 0,
                   "posix_memalign: %p:%uz @%uz", p, size, alignment);

    return p;
}

#elif (NGX_HAVE_MEMALIGN)

/**
 * @brief 以alignment对齐申请内存(对memalign()封装)
 *
 * @param alignment
 * @param size
 * @param log
 *
 * @return 成功返回新申请的内存首地址 失败返回NULL
 */
void *
ngx_memalign(size_t alignment, size_t size, ngx_log_t *log)
{
    void  *p;

    p = memalign(alignment, size);
    if (p == NULL) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "memalign(%uz, %uz) failed", alignment, size);
    }

    ngx_log_debug3(NGX_LOG_DEBUG_ALLOC, log, 0,
                   "memalign: %p:%uz @%uz", p, size, alignment);

    return p;
}

#endif
