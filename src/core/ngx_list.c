
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/**
 * 本链表在内存池中申请内存;
 * 链表容量可以自动增加;
 * 只有向链表尾中添加新元素的操作,@see ngx_list_push();
 * 
 */

#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 创建链表
 *
 * @param pool 链表
 * @param n 链表的容量
 * @param size 链表节点所需内存大小
 *
 * @return 成功返回新创建的链表，否则返回NULL
 *
 * @note 链表的容量不是固定，如果当前链表块满了，就扩容
 */
ngx_list_t *
ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    ngx_list_t  *list;

    list = ngx_palloc(pool, sizeof(ngx_list_t));
    if (list == NULL) {
        return NULL;
    }

    list->part.elts = ngx_palloc(pool, n * size);
    if (list->part.elts == NULL) {
        return NULL;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return list;
}


/**
 * @brief 向链表中申请一个节点
 *
 * @param l 链表
 *
 * @return 成功返回新节点，否则返回NULL
 */
void *
ngx_list_push(ngx_list_t *l)
{
    void             *elt;
    ngx_list_part_t  *last;

    last = l->last;

    if (last->nelts == l->nalloc) {

        /* the last part is full, allocate a new list part */
		/* 当前链表块满了，需要扩容 */

        last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL) {
            return NULL;
        }

        last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL) {
            return NULL;
        }

        last->nelts = 0;
        last->next = NULL;

		/* 尾插法 */
        l->last->next = last;
        l->last = last;
    }

    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}
