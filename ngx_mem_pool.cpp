#include "ngx_mem_pool.h"
#include<cstdlib>
void* ngx_mem_pool::ngx_creat_pool(size_t size) {
	ngx_pool_s  *p;
	//根据用户指定的大小开辟内存池
	p = (ngx_pool_s*)malloc(size);
	if (p == nullptr) {
		return nullptr;
	}
	//last指向内存池头信息以外的内存的起始地址 
	p->d.last = (u_char *)p + sizeof(ngx_pool_s);
	//end指向内存池的末尾
	p->d.end = (u_char *)p + size;

	p->d.next = nullptr;
	p->d.failed = 0;
	//实际上内存池上所能使用的内存
	size = size - sizeof(ngx_pool_s);

	//NGX_MAX_ALLOC_FROM_POOL 一个页面4096-1
	//max表示小块内存分配的最大值,剩下的内存比一个页面大，那就是一个页面-1的大小
	p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;
	//current指向当前小块内存池入口指针
	p->current = p;
	p->large = nullptr;
	p->cleanup = nullptr;
	
	pool_ = p;
	//返回当前开辟的内存块的头指针，可以通过查看这个指针是否为空查看内存是否开辟成功
	return p;
}


//考虑内存字节对齐，从内存池中为内存申请size大小的内存
void *ngx_mem_pool::ngx_palloc(size_t size) {
	//如果要申请的内存小于当前max(当前内存池中剩余的内存(比4095小)或者4095)
	//那么就是小块内存申请
	if (size <= pool_->max) {//max不能超过一个页面，最多为4095
		//小内存块的分配，1表示考虑内存对齐
		return ngx_palloc_small(size, 1);
	}
	//否则进入大块内存的申请
	return ngx_palloc_large(size);
}

#define NGX_ALIGNMENT   sizeof(unsigned long)    /* platform word */
//把指针p调整到a的邻近的倍数
#define ngx_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))
//分配小块内存
void *ngx_mem_pool::ngx_palloc_small(size_t size, ngx_uint_t align) {
	u_char      *m;
	ngx_pool_s  *p;
	//current现在指向了哪个内存池，就从那个内存池中进行内存分配
	p = pool_->current;

	do {
		//m指向可分配内存的起始地址
		m = p->d.last;

		if (align) {
			//把m指针地址调整成平台相关的unsigned long(4/8字节)的整数倍
			m = ngx_align_ptr(m, NGX_ALIGNMENT);
		}
		//可用内存空间>=size,将两个指针类型相减后转为size_t类型
		if ((size_t)(p->d.end - m) >= size) {
			//p->d.last指向m + size，那就表示之前的size个空间被占用
			p->d.last = m + size;

			return m;
		}
		//next初始化为空，到这一步说明之前的内存块内存没有分配成功
		//或者说字节对齐以后剩下的内存不够给这个小内存进行分配了，一直往后遍历内存块
		p = p->d.next;

	} while (p);
	//当退出了上面的循环，那么就需要开辟新的内存池
	return ngx_palloc_block(size);
}

//开辟新的内存池
void *ngx_mem_pool::ngx_palloc_block(size_t size) {
	u_char      *m;
	size_t       psize;
	ngx_pool_s  *p, *ne;
	//计算上一个内存池的大小，所以前后开辟的内存池大小都是一定的
	psize = (size_t)(pool_->d.end - (u_char *)pool_);
	//m指向新的内存池的起始位置
	m = (u_char*)malloc(psize);
	if (m == nullptr) {
		return nullptr;
	}

	ne = (ngx_pool_s *)m;

	ne->d.end = m + psize;
	ne->d.next = nullptr;
	ne->d.failed = 0;

	m += sizeof(ngx_pool_data_t);
	m = ngx_align_ptr(m, NGX_ALIGNMENT);
	ne->d.last = m + size;//这里通过指针偏移把要分配的size内存分配出去
	//对于第一个和第二个内存池，next之间还没有建立联系，跳过for循环
	//遍历当前所有的内存块，每开辟一个新的内存块，前面所有的内存块的failed次数+1
	for (p = pool_->current; p->d.next; p = p->d.next) {
		if (p->d.failed++ > 4) {
			pool_->current = p->d.next;
		}
	}
	//将第一个内存块和第二个内存块连起来,current还是指向第一个内存块的
	p->d.next = ne;

	return m;
}

//大块内存的申请
void *ngx_mem_pool::ngx_palloc_large(size_t size) {
	void              *p;
	ngx_uint_t         n;
	ngx_pool_large_s  *large;

	p = malloc(size);//直接调用malloc
	if (p == nullptr) {
		return nullptr;
	}

	n = 0;
	//遍历large链表
	for (large = pool_->large; large; large = large->next) {
		if (large->alloc == nullptr) {//当大块内存free的时候alloc为空
			large->alloc = p;//先不着急创建大块内存的内存头，如果有之前大块内内存释放过，那么利用他的内存头即可
			return p;
		}
		//找前三个即可，最前面创建的最有机会被释放，遍历链表是很耗费性能的
		if (n++ > 3) {
			break;
		}
	}
	//在小块内存池里给大块内存头分配地址，大块内存头包含指向下一个大块内存头的指针和开辟的大块内存的起始地址
	large =(ngx_pool_large_s  *)ngx_palloc_small(sizeof(ngx_pool_large_s), 1);
	if (large == nullptr) {//在小块内存上都找不到大块内存头这么大的内存，这里只能是系统内存用完了
		free(p);//无法记录，直接free
		return nullptr;
	}

	large->alloc = p;//alloc记录大块内存的起始地址
	large->next = pool_->large;//头插法，内存池中中的large入口
	pool_->large = large;

	return p;
}

//提供释放大块内存的函数,注（小块内存由于其通过指针偏移来分配内存，所以连续内存不能只释放中间的）
void ngx_mem_pool::ngx_pfree(void *p) {
	ngx_pool_large_s  *l;

	for (l = pool_->large; l; l = l->next) {
		if (p == l->alloc) {
			free(l->alloc);
			l->alloc = nullptr;

			return;
		}
	}
}
//不考虑内存字节对齐，从内存池中申请size大小的内存
//底层调用的还是ngx_palloc_small函数
void *ngx_mem_pool::ngx_pnalloc(size_t size) {
	if (size <= pool_->max) {
		return ngx_palloc_small(size, 0);
	}
	return ngx_palloc_large(size);
}

//buf缓冲区清零
#define ngx_memzero(buf, n)		  (void) memset(buf, 0, n)
//将内存初始化为0的内存申请操作
void *ngx_mem_pool::ngx_pcalloc(size_t size) {
	void *p;
	p = ngx_palloc(size);
	if (p) {
		ngx_memzero(p, size);
	}

	return p;
}


//重置内存池
void ngx_mem_pool::ngx_reset_pool() {
		ngx_pool_s        *p;//分配小块内存池的入口指针
		ngx_pool_large_s  *l;//大块内存分配入口函数

		//遍历大块内存
		for (l = pool_->large; l; l = l->next) {
			if (l->alloc) {
				free(l->alloc);//把大块内存free掉
			}
		}
		//遍历小块内存
		/*for (p = pool; p; p = p->d.next) {
			p->d.last = (u_char *) p + sizeof(ngx_pool_t);
			p->d.failed = 0;
		}*/
		//处理第一块内存,包含完整的数据头信息
		p = pool_;
		p->d.last = (u_char *)p + sizeof(ngx_pool_s);
		p->d.failed = 0;
		//处理之后的内存，只包含分配小块内存的数据头部信息，少了current,large,cleanup等，只存一份
		for (p = p->d.next; p; p = p->d.next) {
			p->d.last = (u_char *)p + sizeof(ngx_pool_data_t);
			p->d.failed = 0;
		}

		pool_->current = pool_;
		pool_->large = nullptr;//在reset小块内存的时候大块内存的内存头就已经释放了
	}


//销毁内存池
void ngx_mem_pool::ngx_destroy_pool() {
	//1、先清理大块内存指向的外部资源
	ngx_pool_s          *p, *n;
	ngx_pool_large_s    *l;
	ngx_pool_cleanup_s  *c;

	for (c = pool_->cleanup; c; c = c->next) {
		if (c->handler) {
			c->handler(c->data);
		}
	}
	for (l = pool_->large; l; l = l->next) {
		if (l->alloc) {
			free(l->alloc);
		}
	}

	for (p = pool_, n = pool_->d.next; /* void */; p = n, n = n->d.next) {
			free(p);
		if (n == nullptr) {
			break;
		}
	}
}

//添加回调清理操作函数
ngx_pool_cleanup_s *ngx_mem_pool::ngx_pool_cleanup_add(size_t size) {
	ngx_pool_cleanup_s  *c;
	//在小块内存上面开辟内存池
	c = (ngx_pool_cleanup_s*)ngx_palloc(sizeof(ngx_pool_cleanup_s));
	if (c == nullptr) {
		return nullptr;
	}

	if (size) {
		c->data = ngx_palloc(size);
		if (c->data == nullptr) {
			return nullptr;
		}

	}
	else {
		c->data = nullptr;
	}

	c->handler = nullptr;
	//头擦法把清除函数的头信息串在cleanup的链表上
	c->next = pool_->cleanup;
	pool_->cleanup = c;

	return c;
}