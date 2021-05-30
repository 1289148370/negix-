#include"ngx_mem_pool.h"
#include<stdio.h>
#include<string.h>

typedef struct Data stData;
struct Data
{
	char *ptr;
	FILE *pfile;
};

void func1(void *p1) //void (*) (void*)
{
	char *p = (char*)p1;
	printf("free ptr mem!");
	free(p);
}
//void func2(void *pf1)
//{
//	FILE *pf = (FILE*)pf1;
//	printf("close file!");
//	fclose(pf);
//}
int main()
{
	// 512 - sizeof(ngx_pool_s) - 4095   =>   max
	ngx_mem_pool mempool; 
	if (nullptr == mempool.ngx_creat_pool(512))
	{
		printf("ngx_create_pool fail...");
		return -1;
	}

	void *p1 = mempool.ngx_palloc(128); // ��С���ڴ�ط����
	if (p1 == nullptr)
	{
		printf("ngx_palloc 128 bytes fail...");
		return -1;
	}

	stData *p2 = (stData *)mempool.ngx_palloc(512); // �Ӵ���ڴ�ط����
	if (p2 == nullptr)
	{
		printf("ngx_palloc 512 bytes fail...");
		return -1;
	}
	p2->ptr = (char*)malloc(12);
	strcpy_s(p2->ptr, strlen("hello world")+1,"hello world");
	/*FILE *fp;
	p2->pfile = (FILE*)fopen_s(&fp,"data.txt", "w");*/

	ngx_pool_cleanup_s *c1 = mempool.ngx_pool_cleanup_add(sizeof(char*));
	c1->handler = func1;
	c1->data = p2->ptr;

	/*ngx_pool_cleanup_s *c2 = mempool.ngx_pool_cleanup_add(sizeof(FILE*));
	c2->handler = func2;
	c2->data = p2->pfile;*/

	//����ֱ��ʵ����������������
	mempool.ngx_destroy_pool(); // 1.�������е�Ԥ�õ������� 2.�ͷŴ���ڴ� 3.�ͷ�С���ڴ�������ڴ�

	return 0;
}