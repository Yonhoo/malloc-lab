/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))





/*-------------------------------------------------------*/
#define WSIZE 	4
#define DSIZE 	8
#define CHUNKSIZE 	(1<<12)	//一块大小为4096

#define PACK(size,alloc)  ((size) |(alloc))	//将字节大小和标记位合并
#define MAX(x,y)	((x>y)?x:y)
#define GET(p)	(*(unsigned int *)(p))	//表示一个块的大小，里面包含了标记位
#define PUT(p,val) (*(unsigned int *)(p)=(val))	//将val字节的给p 
#define GET_ALLOC(p)	(GET(p) & 0x1)
#define GET_SIZE(p)		(GET(p) & ~0x7)
#define HDRP(bp)	((char *)(bp)-WSIZE)	//此bp是从有效载荷开始的，因此指向块的头部，得减去一个4字节
//只有从块的头部才能获得此块的大小，然后加上整个块的大小，减去DSIZE，移动到尾部的开始处;
#define FTRP(bp)	((char *)(bp)+GET_SIZE(HDRP(bp))-DSIZE)
#define NEXT_BLKP(bp)		((char *)(bp)+GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp)		((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))


int mm_init(void);
static void *extend_heap(size_t words);
void mm_free(void *bp);
static void *coalesce(void *bp);
/*
 *
 *
 * 创建开始空闲链表
 */
//指向第一个块(序言块)
static char *heap_listp;
//指向上一次操作的块
static char *rec;


//	  头 空头空尾 尾
//	| 4 | 4 | 4 | 4 |
int mm_init(void)
{
	/*创建初始空堆*/
	if((heap_listp=(char *)mem_sbrk(4*WSIZE))==(void*) -1)
		return -1;
	PUT(heap_listp,0);	//哨头
	PUT((heap_listp+(WSIZE)),PACK(DSIZE,1));	//序言头块，在此地址上写上块大小的数据
	PUT((heap_listp+(2*WSIZE)),PACK(DSIZE,1));	//序言尾块，在此地址上写上块大小的数据
	PUT((heap_listp+(3*WSIZE)),PACK(0,1));	//序言头块，在次地址上写上块大小的数据
	heap_listp+=(2*WSIZE);	//从有效载荷处开始
	//首次适配块记录的位置
	rec=heap_listp;
	if(extend_heap(CHUNKSIZE/WSIZE)==NULL)
		return -1;
	return 0;
}

static void * extend_heap(size_t words)
{
	char *bp;
	size_t size;

	size=(words%2)?(words+1)*WSIZE:words*WSIZE;
	//mem_sbrk表示地址已经堆向后拓展了size，返回的指针指向这个结尾块的头部
	//返回-1 说明，堆的空间已经满了
	if((long)(bp=(char *)mem_sbrk(size))==-1)
		return NULL;
	//这里的bp指向一个块的有效载荷处，不能理解
	PUT(HDRP(bp),PACK(size,0));
	PUT(FTRP(bp),PACK(size,0));
	PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

	//如果前面一个块是空闲的，则合并
	return coalesce(bp);
}
void mm_free(void *bp)
{
	size_t size=GET_SIZE(bp);

	//使已分配块变为0
	PUT(HDRP(bp),PACK(size,0));
	PUT(FTRP(bp),PACK(size,0));
	//看能否和前面一个块合并
	coalesce(bp);
}



static void *coalesce(void *bp)
{
	//获得上一个块的已分配位

	size_t prev_alloc=GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc=GET_ALLOC(NEXT_BLKP(bp));
	size_t size=GET_SIZE(HDRP(bp));
	//前后都是已分配的
	if(prev_alloc&&next_alloc){	
		return bp;
}
	else if (prev_alloc&&!next_alloc){
		size+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp),PACK(size,0));
   	    PUT(FTRP(bp),PACK(size,0));
		}
	else if (!prev_alloc&&next_alloc){
                    size+=GET_SIZE(HDRP(PREV_BLKP(bp)));
                    PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
                    PUT(FTRP(bp),PACK(size,0));
                    bp=PREV_BLKP(bp);
                    }
	else{
		size+=GET_SIZE(HDRP(PREV_BLKP(bp)))+GET_SIZE(FTRP(NEXT_BLKP(bp)));
                       PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
                       PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
                       bp=PREV_BLKP(bp);
            }

	return bp;
}
static void *find_fit(size_t asize)
{

	void *bp;
	//序言块的下一个头块
	for(bp=heap_listp;GET_SIZE(HDRP(bp))>0;bp=NEXT_BLKP(bp))
	{	
		if(GET_ALLOC(HDRP(bp))==0&&GET_SIZE(HDRP(bp))>=asize){	
			return bp;
		}
	}
	return NULL;

}




static void place(void *bp,size_t asize)
{
	size_t size=GET_SIZE(HDRP(bp));
	if((size-asize)>=16){
		PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
		bp=NEXT_BLKP(bp);
		PUT(HDRP(bp),PACK(size-asize,0));

		PUT(FTRP(bp),PACK(size-asize,0));
	}
	else {
		PUT(HDRP(bp),PACK(size,1));
        PUT(FTRP(bp),PACK(size,1));

  }
		


}

void *mm_malloc(size_t size)
{
	size_t asize;
	size_t extendsize;
	char *bp;
	printf("需要分配大小： %d\n ",size);
	if(size==0)
		return NULL;
	if(size<=DSIZE)
		asize=2*DSIZE;
	else 
		asize=8*((size+15)/8);
	if((bp=(char *)find_fit(asize))!=NULL){
		place(bp,asize);
		return bp;
	}

	extendsize=MAX(asize,CHUNKSIZE);
	
	if((bp=(char *)extend_heap(extendsize/WSIZE))==NULL)
		return NULL;
	place(bp,asize);
	return bp;
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  I'm too lazy
 *      to do better.
 */





/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














