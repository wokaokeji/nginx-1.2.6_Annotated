
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 创建数组
 *
 * @param p 内存池
 * @param n 欲申请元素的个数
 * @param size 每个元素的大小
 *
 * @return 成功返回创建的数组，失败返回NULL
 */
ngx_array_t *
ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size)
{
    ngx_array_t *a;

    a = ngx_palloc(p, sizeof(ngx_array_t));
    if (a == NULL) {
        return NULL;
    }

    a->elts = ngx_palloc(p, n * size);
    if (a->elts == NULL) {
        return NULL;
    }

    a->nelts = 0;
    a->size = size;
    a->nalloc = n;
    a->pool = p;

    return a;
}


/**
 * @brief 销毁创建数组时在内存池申请的空间(移动内存池的指针),
 *
 * @param a 数组
 *
 * @note 仅当该数组是当前内存池最近申请的空间时，内存才会释放,
 *		 并且ngx_array_push()和ngx_array_push_n()保证了一个
 *		 数组在内存池中是连续的
 */
void
ngx_array_destroy(ngx_array_t *a)
{
    ngx_pool_t  *p;

    p = a->pool;

    if ((u_char *) a->elts + a->size * a->nalloc == p->d.last) {
        p->d.last -= a->size * a->nalloc;
    }

    if ((u_char *) a + sizeof(ngx_array_t) == p->d.last) {
        p->d.last = (u_char *) a;
    }
}


/**
 * @brief 向数组中添加一个新元素(仅仅返回新元素的地址)
 *
 * @param a 数组
 *
 * @return 返回新元素的地址, 否则返回NULL
 */
void *
ngx_array_push(ngx_array_t *a)
{
    void        *elt, *new;
    size_t       size;
    ngx_pool_t  *p;

    if (a->nelts == a->nalloc) {

        /* the array is full */

        size = a->size * a->nalloc;

        p = a->pool;

        if ((u_char *) a->elts + size == p->d.last
            && p->d.last + a->size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
			 * 该数组是当前内存池中最近申请的空间，并且最新的这个
			 * 内存池中还有足够空间再存放一个数组元素的空间
             */

            p->d.last += a->size;
            a->nalloc++;

        } else {
            /* allocate a new array */

			/* 
			 * 否则重新内存，并把原来的数组数组复制到新申请的内存,
			 * 这样才能保证一个数组在内存池中是连续的
			 */
            new = ngx_palloc(p, 2 * size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}


/**
 * @brief 向数组中添加多个新元素(仅仅返回第一个新元素的地址)
 *
 * @param a 数组
 *
 * @return 返回新申请的第一个元素的地址, 否则返回NULL
 *
 * @note  操作过程同ngx_array_push()
 */
void *
ngx_array_push_n(ngx_array_t *a, ngx_uint_t n)
{
    void        *elt, *new;
    size_t       size;
    ngx_uint_t   nalloc;
    ngx_pool_t  *p;

    size = n * a->size;

    if (a->nelts + n > a->nalloc) {

        /* the array is full */

        p = a->pool;

        if ((u_char *) a->elts + a->size * a->nalloc == p->d.last
            && p->d.last + size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += size;
            a->nalloc += n;

        } else {
            /* allocate a new array */

            nalloc = 2 * ((n >= a->nalloc) ? n : a->nalloc);

            new = ngx_palloc(p, nalloc * a->size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, a->nelts * a->size);
            a->elts = new;
            a->nalloc = nalloc;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts += n;

    return elt;
}
