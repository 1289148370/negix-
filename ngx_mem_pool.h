#pragma once
/*
��ֲnginx�ڴ�صĴ��룬��oop��ʵ��
*/
#include<iostream>
#include<stdlib.h>
#include<memory.h>
using namespace std;

//�����ض���
using u_char = unsigned char;
using ngx_uint_t = unsigned int;



//����ǰ������
struct ngx_pool_s;
//�����������ͣ��ص�����
typedef void(*ngx_pool_cleanup_pt)(void *data);
struct ngx_pool_cleanup_s {
	ngx_pool_cleanup_pt   handler;//����Ԥ�����õĻص�����
	void                  *data;//�ͷ���Դʱ��Դ�ĵ�ַ
	ngx_pool_cleanup_s    *next;//����
};

/*
����ڴ���ڴ�ͷ��Ϣ
*/
struct ngx_pool_large_s {
	ngx_pool_large_s	*next;//ָ����һ��ͬ�����ڴ��
	void				*alloc;//����ڴ����ʼ��ַ
};
/*
����С���ڴ���ڴ�ص�ͷ��������Ϣ
���������ͷ ÿ���ڴ�鶼��
*/
struct ngx_pool_data_t {
	u_char				*last;//�ڴ���п����ڴ����ʼ��ַ
	u_char				*end;//�ڴ�ؿ����ڴ��ĩβ��ַ
	ngx_pool_s			*next;//ָ����һ�����ٵ��ڴ�ص�ַ����ֵΪ��
	ngx_uint_t			failed;//�ڵ�ǰ�ڴ���з����ڴ�ʧ�ܵĴ���
} ;
/*
nginx�ڴ�ص�ͷ����Ϣ�͹����Ա��Ϣ
*/
struct ngx_pool_s {
	ngx_pool_data_t		d;//�ڴ�ص�ʹ�������ͷ��Ϣ
	//max��creat�����У��Ǽ�¼��ǰ�ڴ��������С�ڴ���������ڴ��С
	//�����ǰ�ڴ����ʣ����ڴ�size<4095,�Ǿ�ֻ�ܼ���size���������4095
	size_t				max;
	ngx_pool_s			*current;//ָ��ǰ�ɷ����ڴ���ڴ��
	ngx_pool_large_s	*large;//����ڴ�������ں���
	ngx_pool_cleanup_s	*cleanup;//������handler����ں���
};


//����ֵd�������ڽ���a�ı���
#define ngx_align(d, a)     (((d) + (a - 1)) & ~(a - 1))

//�ܴӳ�������С���ڴ������ڴ棬һ��ҳ�棬4K,4096
const int ngx_pagesize = 4096;//Ĭ��һ������ҳ��Ĵ�С
const int NGX_MAX_ALLOC_FROM_POOL = ngx_pagesize - 1;
//Ĭ�ϵĿ��ٵ�ngx�ڴ�صĴ�С
const int NGX_DEFAULT_POOL_SIZE = 16 * 1024;
//�ڴ�����ƥ�������ֽ���
const int NGX_POOL_ALIGNMENT = 16;
//�ܹ��������С�ĳصĴ�С����һ������ͷ��Ϣ��С��2*2��ָ�����ʹ�С
//�ٽ���ת��Ϊ16�ı���
const int NGX_MIN_POOL_SIZE = \
			ngx_align((sizeof(ngx_pool_s) + 2 * sizeof(ngx_pool_large_s)), \
			NGX_POOL_ALIGNMENT);
class ngx_mem_pool {
public:
	//����ָ��size���ڴ��
	void* ngx_creat_pool(size_t size);

	//�����ڴ��ֽڶ��룬���ڴ����ΪС���ڴ�����size��С���ڴ�
	void *ngx_palloc(size_t size);

	//�������ڴ��ֽڶ��룬���ڴ��������size��С���ڴ�
	void *ngx_pnalloc(size_t size);

	//���ڴ��ʼ��Ϊ0���ڴ��������
	void *ngx_pcalloc(size_t size);

	//�ṩ�ͷŴ���ڴ�ĺ���,ע��С���ڴ�������ͨ��ָ��ƫ���������ڴ棬���������ڴ治��ֻ�ͷ��м�ģ�
	void ngx_pfree(void *p);

	//�����ڴ��
	void ngx_reset_pool();
	
	//�����ڴ��
	void ngx_destroy_pool();
	
	//��ӻص������������
	ngx_pool_cleanup_s *ngx_pool_cleanup_add(size_t size);

private:
	
	ngx_pool_s			*pool_;//ָ��nginx�ڴ�ص����ָ��
	//�ӳ��з���С���ڴ�
	void *ngx_palloc_small(size_t size, ngx_uint_t align);
	//�������ڴ�
	void *ngx_palloc_large(size_t size);
	//�����µ��ڴ��
	void *ngx_palloc_block(size_t size);
};