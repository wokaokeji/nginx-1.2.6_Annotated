
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


static void *ngx_palloc_block(ngx_pool_t *pool, size_t size);
static void *ngx_palloc_large(ngx_pool_t *pool, size_t size);


/**
 * @brief 创建内存池(以NGX_POOL_ALIGNMENT对齐)
 *
 * @param size 内存池大小
 * @param log
 *
 * @return 成功返回所创建内存池的指针，失败返回NULL
 */
ngx_pool_t *
ngx_create_pool(size_t size, ngx_log_t *log)
{
    ngx_pool_t  *p;

    /* 申请已对齐的内存 */
    p = ngx_memalign(NGX_POOL_ALIGNMENT, size, log);
    if (p == NULL) {
        return NULL;
    }

    p->d.last = (u_char *) p + sizeof(ngx_pool_t);
    p->d.end = (u_char *) p + size;
    p->d.next = NULL;
    p->d.failed = 0;

    size = size - sizeof(ngx_pool_t);
    /* 从内存池中单次申请内存的最大值 */
    p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;

    p->current = p;
    p->chain = NULL;
    p->large = NULL;
    p->cleanup = NULL;
    p->log = log;

    return p;
}


/**
 * @brief 销毁内存池
 *
 * @param pool 内存池
 */
void
ngx_destroy_pool(ngx_pool_t *pool)
{
    ngx_pool_t          *p, *n;
    ngx_pool_large_t    *l;
    ngx_pool_cleanup_t  *c;

    /* 调用清理函数表 */
    for (c = pool->cleanup; c; c = c->next) {
        if (c->handler) {
            ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                           "run cleanup: %p", c);
            c->handler(c->data);
        }
    }

    /* 释放所有大内存 */
    for (l = pool->large; l; l = l->next) {

        ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0, "free: %p", l->alloc);

        if (l->alloc) {
            ngx_free(l->alloc);
        }
    }

#if (NGX_DEBUG)

    /*
     * we could allocate the pool->log from this pool
     * so we cannot use this log while free()ing the pool
     */

    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
        ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                       "free: %p, unused: %uz", p, p->d.end - p->d.last);

        if (n == NULL) {
            break;
        }
    }

#endif

    /* 释放内存块链表 */
    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
        ngx_free(p);

        if (n == NULL) {
            break;
        }
    }
}


/**
 * @brief 重置内存池
 *
 * @param pool 内存池
 */
void
ngx_reset_pool(ngx_pool_t *pool)
{
    ngx_pool_t        *p;
    ngx_pool_large_t  *l;

    /* 释放大内存 */
    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            ngx_free(l->alloc);
        }
    }

    pool->large = NULL;

    /* 重置所有内存块的正在使用地址 */
    for (p = pool; p; p = p->d.next) {
        p->d.last = (u_char *) p + sizeof(ngx_pool_t);
    }
}


/**
 * @brief 从内存池中申请内存(所申请的内存起始地址以NGX_ALIGNMENT对齐)
 *
 * @param pool 内存池
 * @param size 申请大小为size的内存
 *
 * @return 成功返回所申请的内存起始地址,失败返回NULL
 */
void *
ngx_palloc(ngx_pool_t *pool, size_t size)
{
    u_char      *m;
    ngx_pool_t  *p;

    if (size <= pool->max) {
        /* 允许从内存块中申请大小为size的内存 */

        p = pool->current;

        do {
            /*  内存地址对齐 */
            m = ngx_align_ptr(p->d.last, NGX_ALIGNMENT);

            if ((size_t) (p->d.end - m) >= size) {
                /* 能够当前内存块申请,更新内存池当前指针 */
                p->d.last = m + size;

                return m;
            }

            /* 否则在下一个内存块中申请 */
            p = p->d.next;

        } while (p);

        /*
         * 如果没有能够分配的内存块，内存池重新申请一个内存块,
         * 并在新申请的内存块中，申请大小为size的内存给用户
         */
        return ngx_palloc_block(pool, size);
    }

    /* 此时size超过内存池允许申请内存大小的最大值，申请一个大内存给用户 */
    return ngx_palloc_large(pool, size);
}


/**
 * @brief 为用户从内存池pootl中申请大小为size的内存
 *        操作过程与@see ngx_palloc()相同，但是本函数申请
 *        内存时不强制内存对齐
 *
 * @param pool 内存
 * @param size 用户要申请的内存大小
 *
 * @return 成功返回所申请的内存起始地址,失败返回NULL
 */
void *
ngx_pnalloc(ngx_pool_t *pool, size_t size)
{
    u_char      *m;
    ngx_pool_t  *p;

    if (size <= pool->max) {

        p = pool->current;

        do {
            m = p->d.last;

            if ((size_t) (p->d.end - m) >= size) {
                p->d.last = m + size;

                return m;
            }

            p = p->d.next;

        } while (p);

        return ngx_palloc_block(pool, size);
    }

    return ngx_palloc_large(pool, size);
}


/**
 * @brief 为内存池申请一个新内存块,并在这个内存块中
 *        为用户申请大小为size的内存
 *
 * @param pool 内存池
 * @param size 用户要申请的内存大小
 *
 * @return 成功返回给用户新申请内存的起始地址，失败返回NULL
 */
static void *
ngx_palloc_block(ngx_pool_t *pool, size_t size)
{
    u_char      *m;
    size_t       psize;
    ngx_pool_t  *p, *new, *current;

    /* 一个内存块的大小 */
    psize = (size_t) (pool->d.end - (u_char *) pool);

    /* 申请一块新的内存块来扩充当前内存池 */
    m = ngx_memalign(NGX_POOL_ALIGNMENT, psize, pool->log);
    if (m == NULL) {
        return NULL;
    }

    new = (ngx_pool_t *) m;

    new->d.end = m + psize;
    new->d.next = NULL;
    new->d.failed = 0;

    m += sizeof(ngx_pool_data_t);
    /* 此时,m指向内块的数据段 */
    m = ngx_align_ptr(m, NGX_ALIGNMENT);
    
    new->d.last = m + size;/* 更新内存池当前指针 */

    current = pool->current;

    for (p = current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            /*
             * 如果从一个内存块中申请内存，失败超过4次，
             * 就将内存池的正在使用内存块指向当前内存块的下一个
             * 内存块 
             */
            current = p->d.next;
        }
    }

    p->d.next = new;

    /* 更新内存池的正在使用内存块 */
    pool->current = current ? current : new;

    return m;
}


/**
 * @brief 申请大内存
 *
 * @param pool 内存池
 * @param size 大内存大小
 *
 * @return 成功返回申请的大内存起始地址，失败返回NULL
 */
static void *
ngx_palloc_large(ngx_pool_t *pool, size_t size)
{
    void              *p;
    ngx_uint_t         n;
    ngx_pool_large_t  *large;

    p = ngx_alloc(size, pool->log);
    if (p == NULL) {
        return NULL;
    }

    n = 0;

    /*
     * 将新申请的大内存加入到大内存链表.
     * 在大内存链表的前三个节点中查找alloc=NULL的节点,
     * 若找到，则将新申请的大内存地址存入该节点，否则插入到链表头
     */
    for (large = pool->large; large; large = large->next) {
        if (large->alloc == NULL) {
            large->alloc = p;
            return p;
        }

        if (n++ > 3) {
            break;
        }
    }

    /*
     * 大内存是由malloc申请的，但是
     * 大内存链表的节点信息存储在内存池的内存块中.
     */
    large = ngx_palloc(pool, sizeof(ngx_pool_large_t));
    if (large == NULL) {
        ngx_free(p);
        return NULL;
    }

    /* 头插法 */
    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


/**
 * @brief 在内存池中申请大小为size的并且已经地址对齐的内存
 *
 * @param pool 内存池
 * @param size 申请内存的大小
 * @param alignment 申请的地址以alignment对齐
 *
 * @return 成功返回申请的地址，失败返回NULL
 *
 * @note 新申请的这段内存存储在内存池的大内存链表中
 */
void *
ngx_pmemalign(ngx_pool_t *pool, size_t size, size_t alignment)
{
    void              *p;
    ngx_pool_large_t  *large;

    p = ngx_memalign(alignment, size, pool->log);
    if (p == NULL) {
        return NULL;
    }

    large = ngx_palloc(pool, sizeof(ngx_pool_large_t));
    if (large == NULL) {
        ngx_free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


/**
 * @brief 释放内存池中，地址为p的大内存
 *
 * @param pool 内存池
 * @param p 大内存地址
 *
 * @return 成功返回NGX_OK, 失败返回NGX_DECLINED
 */
ngx_int_t
ngx_pfree(ngx_pool_t *pool, void *p)
{
    ngx_pool_large_t  *l;

    /* 遍历大内存链表, 查找地址为p的大内存 */
    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                           "free: %p", l->alloc);
            ngx_free(l->alloc);
            l->alloc = NULL;

            return NGX_OK;
        }
    }

    return NGX_DECLINED;
}


/**
 * @brief 在内存池pool中申请大小为size的内存，
 *        并把这段内存用0填充
 *
 * @param pool 内存池
 * @param size 要申请的内存的大小
 *
 * @return 成功返回新申请内存的地址，失败返回NULL
 */
void *
ngx_pcalloc(ngx_pool_t *pool, size_t size)
{
    void *p;

    p = ngx_palloc(pool, size);/* 申请内存 */
    if (p) {
        ngx_memzero(p, size);/* 用0填充 */
    }

    return p;
}


ngx_pool_cleanup_t *
ngx_pool_cleanup_add(ngx_pool_t *p, size_t size)
{
    ngx_pool_cleanup_t  *c;

    c = ngx_palloc(p, sizeof(ngx_pool_cleanup_t));
    if (c == NULL) {
        return NULL;
    }

    if (size) {
        c->data = ngx_palloc(p, size);
        if (c->data == NULL) {
            return NULL;
        }

    } else {
        c->data = NULL;
    }

    /* 头插法 */
    c->handler = NULL;
    c->next = p->cleanup;

    p->cleanup = c;

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, p->log, 0, "add cleanup: %p", c);

    return c;
}


void
ngx_pool_run_cleanup_file(ngx_pool_t *p, ngx_fd_t fd)
{
    ngx_pool_cleanup_t       *c;
    ngx_pool_cleanup_file_t  *cf;

    for (c = p->cleanup; c; c = c->next) {
        if (c->handler == ngx_pool_cleanup_file) {

            cf = c->data;

            if (cf->fd == fd) {
                c->handler(cf);
                c->handler = NULL;
                return;
            }
        }
    }
}


void
ngx_pool_cleanup_file(void *data)
{
    ngx_pool_cleanup_file_t  *c = data;

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, c->log, 0, "file cleanup: fd:%d",
                   c->fd);

    if (ngx_close_file(c->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", c->name);
    }
}


void
ngx_pool_delete_file(void *data)
{
    ngx_pool_cleanup_file_t  *c = data;

    ngx_err_t  err;

    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, c->log, 0, "file cleanup: fd:%d %s",
                   c->fd, c->name);

    if (ngx_delete_file(c->name) == NGX_FILE_ERROR) {
        err = ngx_errno;

        if (err != NGX_ENOENT) {
            ngx_log_error(NGX_LOG_CRIT, c->log, err,
                          ngx_delete_file_n " \"%s\" failed", c->name);
        }
    }

    if (ngx_close_file(c->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", c->name);
    }
}


#if 0

static void *
ngx_get_cached_block(size_t size)
{
    void                     *p;
    ngx_cached_block_slot_t  *slot;

    if (ngx_cycle->cache == NULL) {
        return NULL;
    }

    slot = &ngx_cycle->cache[(size + ngx_pagesize - 1) / ngx_pagesize];

    slot->tries++;

    if (slot->number) {
        p = slot->block;
        slot->block = slot->block->next;
        slot->number--;
        return p;
    }

    return NULL;
}

#endif
