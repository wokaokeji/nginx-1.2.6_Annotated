
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ARRAY_H_INCLUDED_
#define _NGX_ARRAY_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


struct ngx_array_s {
    void        *elts;      /**< 数组头指针 */
    ngx_uint_t   nelts;     /**< 元素的个数 */
    size_t       size;      /**< 一个元素的大小 */
    ngx_uint_t   nalloc;    /**< 已经预申请了能够存储了nalloc个元素的内存 */
    ngx_pool_t  *pool;      /**< 对应的内存池 */
};


ngx_array_t *ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size);
void ngx_array_destroy(ngx_array_t *a);
void *ngx_array_push(ngx_array_t *a);
void *ngx_array_push_n(ngx_array_t *a, ngx_uint_t n);


/**
 * @brief 初始化数组
 *
 * @param array 数组
 * @param p 内存池
 * @param n 欲申请元素的个数
 * @param size 每个元素的大小
 *
 * @return 成功返回NGX_OK(0)，失败返回NGX_ERROR(-1)
 */
static ngx_inline ngx_int_t
ngx_array_init(ngx_array_t *array, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    array->elts = ngx_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


#endif /* _NGX_ARRAY_H_INCLUDED_ */
