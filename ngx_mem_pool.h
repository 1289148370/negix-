#pragma once
/*
移植nginx内存池的代码，用oop来实现
*/
#include<iostream>
#include<stdlib.h>
#include<memory.h>
using namespace std;

//类型重定义
using u_char = unsigned char;
using ngx_uint_t = unsigned int;



//类型前置声明
struct ngx_pool_s;
//清理函数的类型，回调函数
typedef void(*ngx_pool_cleanup_pt)(void *data);
struct ngx_pool_cleanup_s {
	ngx_pool_cleanup_pt   handler;//保存预先设置的回调函数
	void                  *data;//释放资源时资源的地址
	ngx_pool_cleanup_s    *next;//链表
};

/*
大块内存的内存头信息
*/
struct ngx_pool_large_s {
	ngx_pool_large_s	*next;//指向下一个同类型内存块
	void				*alloc;//大块内存的起始地址
};
/*
分配小块内存的内存池的头部数据信息
这里的数据头 每个内存块都有
*/
struct ngx_pool_data_t {
	u_char				*last;//内存池中可用内存的起始地址
	u_char				*end;//内存池可用内存的末尾地址
	ngx_pool_s			*next;//指向下一个开辟的内存池地址，初值为空
	ngx_uint_t			failed;//在当前内存池中分配内存失败的次数
} ;
/*
nginx内存池的头部信息和管理成员信息
*/
struct ngx_pool_s {
	ngx_pool_data_t		d;//内存池的使用情况的头信息
	//max（creat函数中）是记录当前内存块能用于小内存分配的最大内存大小
	//如果当前内存池中剩余的内存size<4095,那就只能记作size，否则记作4095
	size_t				max;
	ngx_pool_s			*current;//指向当前可分配内存的内存块
	ngx_pool_large_s	*large;//大块内存分配的入口函数
	ngx_pool_cleanup_s	*cleanup;//清理函数handler的入口函数
};


//把数值d调整成邻近的a的倍数
#define ngx_align(d, a)     (((d) + (a - 1)) & ~(a - 1))

//能从池里分配的小块内存的最大内存，一个页面，4K,4096
const int ngx_pagesize = 4096;//默认一个物理页面的大小
const int NGX_MAX_ALLOC_FROM_POOL = ngx_pagesize - 1;
//默认的开辟的ngx内存池的大小
const int NGX_DEFAULT_POOL_SIZE = 16 * 1024;
//内存分配的匹配对齐的字节数
const int NGX_POOL_ALIGNMENT = 16;
//能够定义的最小的池的大小包含一个数据头信息大小和2*2个指针类型大小
//再将其转化为16的倍数
const int NGX_MIN_POOL_SIZE = \
			ngx_align((sizeof(ngx_pool_s) + 2 * sizeof(ngx_pool_large_s)), \
			NGX_POOL_ALIGNMENT);
class ngx_mem_pool {
public:
	//创建指定size的内存池
	void* ngx_creat_pool(size_t size);

	//考虑内存字节对齐，从内存池中为小块内存申请size大小的内存
	void *ngx_palloc(size_t size);

	//不考虑内存字节对齐，从内存池中申请size大小的内存
	void *ngx_pnalloc(size_t size);

	//将内存初始化为0的内存申请操作
	void *ngx_pcalloc(size_t size);

	//提供释放大块内存的函数,注（小块内存由于其通过指针偏移来分配内存，所以连续内存不能只释放中间的）
	void ngx_pfree(void *p);

	//重置内存池
	void ngx_reset_pool();
	
	//销毁内存池
	void ngx_destroy_pool();
	
	//添加回调清理操作函数
	ngx_pool_cleanup_s *ngx_pool_cleanup_add(size_t size);

private:
	
	ngx_pool_s			*pool_;//指向nginx内存池的入口指针
	//从池中分配小块内存
	void *ngx_palloc_small(size_t size, ngx_uint_t align);
	//分配大块内存
	void *ngx_palloc_large(size_t size);
	//开辟新的内存池
	void *ngx_palloc_block(size_t size);
};