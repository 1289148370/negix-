#include "ngx_mem_pool.h"
#include<cstdlib>
void* ngx_mem_pool::ngx_creat_pool(size_t size) {
	ngx_pool_s  *p;
	//�����û�ָ���Ĵ�С�����ڴ��
	p = (ngx_pool_s*)malloc(size);
	if (p == nullptr) {
		return nullptr;
	}
	//lastָ���ڴ��ͷ��Ϣ������ڴ����ʼ��ַ 
	p->d.last = (u_char *)p + sizeof(ngx_pool_s);
	//endָ���ڴ�ص�ĩβ
	p->d.end = (u_char *)p + size;

	p->d.next = nullptr;
	p->d.failed = 0;
	//ʵ�����ڴ��������ʹ�õ��ڴ�
	size = size - sizeof(ngx_pool_s);

	//NGX_MAX_ALLOC_FROM_POOL һ��ҳ��4096-1
	//max��ʾС���ڴ��������ֵ,ʣ�µ��ڴ��һ��ҳ����Ǿ���һ��ҳ��-1�Ĵ�С
	p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;
	//currentָ��ǰС���ڴ�����ָ��
	p->current = p;
	p->large = nullptr;
	p->cleanup = nullptr;
	
	pool_ = p;
	//���ص�ǰ���ٵ��ڴ���ͷָ�룬����ͨ���鿴���ָ���Ƿ�Ϊ�ղ鿴�ڴ��Ƿ񿪱ٳɹ�
	return p;
}


//�����ڴ��ֽڶ��룬���ڴ����Ϊ�ڴ�����size��С���ڴ�
void *ngx_mem_pool::ngx_palloc(size_t size) {
	//���Ҫ������ڴ�С�ڵ�ǰmax(��ǰ�ڴ����ʣ����ڴ�(��4095С)����4095)
	//��ô����С���ڴ�����
	if (size <= pool_->max) {//max���ܳ���һ��ҳ�棬���Ϊ4095
		//С�ڴ��ķ��䣬1��ʾ�����ڴ����
		return ngx_palloc_small(size, 1);
	}
	//����������ڴ������
	return ngx_palloc_large(size);
}

#define NGX_ALIGNMENT   sizeof(unsigned long)    /* platform word */
//��ָ��p������a���ڽ��ı���
#define ngx_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))
//����С���ڴ�
void *ngx_mem_pool::ngx_palloc_small(size_t size, ngx_uint_t align) {
	u_char      *m;
	ngx_pool_s  *p;
	//current����ָ�����ĸ��ڴ�أ��ʹ��Ǹ��ڴ���н����ڴ����
	p = pool_->current;

	do {
		//mָ��ɷ����ڴ����ʼ��ַ
		m = p->d.last;

		if (align) {
			//��mָ���ַ������ƽ̨��ص�unsigned long(4/8�ֽ�)��������
			m = ngx_align_ptr(m, NGX_ALIGNMENT);
		}
		//�����ڴ�ռ�>=size,������ָ�����������תΪsize_t����
		if ((size_t)(p->d.end - m) >= size) {
			//p->d.lastָ��m + size���Ǿͱ�ʾ֮ǰ��size���ռ䱻ռ��
			p->d.last = m + size;

			return m;
		}
		//next��ʼ��Ϊ�գ�����һ��˵��֮ǰ���ڴ���ڴ�û�з���ɹ�
		//����˵�ֽڶ����Ժ�ʣ�µ��ڴ治�������С�ڴ���з����ˣ�һֱ��������ڴ��
		p = p->d.next;

	} while (p);
	//���˳��������ѭ������ô����Ҫ�����µ��ڴ��
	return ngx_palloc_block(size);
}

//�����µ��ڴ��
void *ngx_mem_pool::ngx_palloc_block(size_t size) {
	u_char      *m;
	size_t       psize;
	ngx_pool_s  *p, *ne;
	//������һ���ڴ�صĴ�С������ǰ�󿪱ٵ��ڴ�ش�С����һ����
	psize = (size_t)(pool_->d.end - (u_char *)pool_);
	//mָ���µ��ڴ�ص���ʼλ��
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
	ne->d.last = m + size;//����ͨ��ָ��ƫ�ư�Ҫ�����size�ڴ�����ȥ
	//���ڵ�һ���͵ڶ����ڴ�أ�next֮�仹û�н�����ϵ������forѭ��
	//������ǰ���е��ڴ�飬ÿ����һ���µ��ڴ�飬ǰ�����е��ڴ���failed����+1
	for (p = pool_->current; p->d.next; p = p->d.next) {
		if (p->d.failed++ > 4) {
			pool_->current = p->d.next;
		}
	}
	//����һ���ڴ��͵ڶ����ڴ��������,current����ָ���һ���ڴ���
	p->d.next = ne;

	return m;
}

//����ڴ������
void *ngx_mem_pool::ngx_palloc_large(size_t size) {
	void              *p;
	ngx_uint_t         n;
	ngx_pool_large_s  *large;

	p = malloc(size);//ֱ�ӵ���malloc
	if (p == nullptr) {
		return nullptr;
	}

	n = 0;
	//����large����
	for (large = pool_->large; large; large = large->next) {
		if (large->alloc == nullptr) {//������ڴ�free��ʱ��allocΪ��
			large->alloc = p;//�Ȳ��ż���������ڴ���ڴ�ͷ�������֮ǰ������ڴ��ͷŹ�����ô���������ڴ�ͷ����
			return p;
		}
		//��ǰ�������ɣ���ǰ�洴�������л��ᱻ�ͷţ����������Ǻܺķ����ܵ�
		if (n++ > 3) {
			break;
		}
	}
	//��С���ڴ���������ڴ�ͷ�����ַ������ڴ�ͷ����ָ����һ������ڴ�ͷ��ָ��Ϳ��ٵĴ���ڴ����ʼ��ַ
	large =(ngx_pool_large_s  *)ngx_palloc_small(sizeof(ngx_pool_large_s), 1);
	if (large == nullptr) {//��С���ڴ��϶��Ҳ�������ڴ�ͷ��ô����ڴ棬����ֻ����ϵͳ�ڴ�������
		free(p);//�޷���¼��ֱ��free
		return nullptr;
	}

	large->alloc = p;//alloc��¼����ڴ����ʼ��ַ
	large->next = pool_->large;//ͷ�巨���ڴ�����е�large���
	pool_->large = large;

	return p;
}

//�ṩ�ͷŴ���ڴ�ĺ���,ע��С���ڴ�������ͨ��ָ��ƫ���������ڴ棬���������ڴ治��ֻ�ͷ��м�ģ�
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
//�������ڴ��ֽڶ��룬���ڴ��������size��С���ڴ�
//�ײ���õĻ���ngx_palloc_small����
void *ngx_mem_pool::ngx_pnalloc(size_t size) {
	if (size <= pool_->max) {
		return ngx_palloc_small(size, 0);
	}
	return ngx_palloc_large(size);
}

//buf����������
#define ngx_memzero(buf, n)		  (void) memset(buf, 0, n)
//���ڴ��ʼ��Ϊ0���ڴ��������
void *ngx_mem_pool::ngx_pcalloc(size_t size) {
	void *p;
	p = ngx_palloc(size);
	if (p) {
		ngx_memzero(p, size);
	}

	return p;
}


//�����ڴ��
void ngx_mem_pool::ngx_reset_pool() {
		ngx_pool_s        *p;//����С���ڴ�ص����ָ��
		ngx_pool_large_s  *l;//����ڴ������ں���

		//��������ڴ�
		for (l = pool_->large; l; l = l->next) {
			if (l->alloc) {
				free(l->alloc);//�Ѵ���ڴ�free��
			}
		}
		//����С���ڴ�
		/*for (p = pool; p; p = p->d.next) {
			p->d.last = (u_char *) p + sizeof(ngx_pool_t);
			p->d.failed = 0;
		}*/
		//�����һ���ڴ�,��������������ͷ��Ϣ
		p = pool_;
		p->d.last = (u_char *)p + sizeof(ngx_pool_s);
		p->d.failed = 0;
		//����֮����ڴ棬ֻ��������С���ڴ������ͷ����Ϣ������current,large,cleanup�ȣ�ֻ��һ��
		for (p = p->d.next; p; p = p->d.next) {
			p->d.last = (u_char *)p + sizeof(ngx_pool_data_t);
			p->d.failed = 0;
		}

		pool_->current = pool_;
		pool_->large = nullptr;//��resetС���ڴ��ʱ�����ڴ���ڴ�ͷ���Ѿ��ͷ���
	}


//�����ڴ��
void ngx_mem_pool::ngx_destroy_pool() {
	//1�����������ڴ�ָ����ⲿ��Դ
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

//��ӻص������������
ngx_pool_cleanup_s *ngx_mem_pool::ngx_pool_cleanup_add(size_t size) {
	ngx_pool_cleanup_s  *c;
	//��С���ڴ����濪���ڴ��
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
	//ͷ���������������ͷ��Ϣ����cleanup��������
	c->next = pool_->cleanup;
	pool_->cleanup = c;

	return c;
}