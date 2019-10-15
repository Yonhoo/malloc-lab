#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>


#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))



/*-------------------------------------------------------*/
#define WSIZE   4
#define DSIZE   8
#define CHUNKSIZE   (1<<12) //一块大小为4096

#define PACK(size,alloc)  ((size) |(alloc)) //将字节大小和标记位合并
#define MAX(x,y)    ((x>y)?x:y)
#define GET(p)  (*(unsigned int *)(p))  //表示一个块的大小，里面包含了标记位
#define PUT(p,val) (*(unsigned int *)(p)=(val)) //将val字节的给p 
#define GET_ALLOC(p)    (GET(p) & 0x1)
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define HDRP(bp)    ((char *)(bp)-WSIZE)    //此bp是从有效载荷开始的，因此指向块的头部，得减去一个4字节
//只有从块的头部才能获得此块的大小，然后加上整个块的大小，减去DSIZE，移动到尾部的开始处;
#define FTRP(bp)    ((char *)(bp)+GET_SIZE(HDRP(bp))-DSIZE)
#define NEXT_BLKP(bp)       ((char *)(bp)+GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp)       ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))


/*---------------------------*/
#define PREV_FREE(bp)   ((char *)(bp))
#define NEXT_FREE(bp)   ((char *)(bp)+WSIZE)
/*---------------------------*/
int mm_init(void);
static void *extend_heap(size_t words);
void mm_free(void *bp);
static void *coalesce(void *bp);
static void insert_free_list(void *bp);
/*
 *
 * 创建开始空闲链表
 */
//指向第一个块(序言块)
static char *heap_listp=0;
//显示空闲链表
static char * free_list_first=0;
//    头 空头  空尾 尾
//  | 4 | 4 | 4 | 4 |

/*

这里要求块大小是16的倍数，所以在分配头块的时候，我分配了16个字节

        /   头    /
        --------
        / prev   /
        --------
        / next   /
        --------
        / 序言头  /
        --------
        / 序言尾  /
        --------
        /   结束  /
        --------

---------------------------------
正常块则为：
        /   头    /
        --------
        / prev   /
        --------
        / next   /
        --------
        /  ...   /
        --------
        /   尾   /


*/
int mm_init(void)
{
    /*创建初始空堆*/
    if((heap_listp=mem_sbrk(6*WSIZE))==(void*) -1)
        return -1;
    PUT(heap_listp,0);  //哨头
    PUT((heap_listp+(WSIZE)),0);    //序言头的上一个空闲为0
    PUT((heap_listp+(2*WSIZE)),0);  //序言头的下一个空闲暂时还不知道
    PUT((heap_listp+(3*WSIZE)),PACK(DSIZE,1));  
    PUT((heap_listp+(4*WSIZE)),PACK(DSIZE,1));  
    PUT((heap_listp+(5*WSIZE)),1);      //结束尾
    heap_listp+=(4*WSIZE);  //从有效载荷处开始
    //安排空闲链表
    free_list_first=(char *)heap_listp-2*WSIZE;
    //初始化的时候，使得空闲链表为0
    PUT(free_list_first, 0);
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
    if((long)(bp=mem_sbrk(size))==(void *)-1)
        return NULL;
    printf("%x\n",bp );
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));
    insert_free_list(bp);
    return bp;
}

static void insert_free_list(void *bp)
{
    //把bp这个块变成空闲块的数据结构
    char *next=(char *)GET(free_list_first);//free_list 总是指向链表的开头
    PUT(free_list_first,bp);
    PUT(PREV_FREE(bp),0);
    if(next!=0){
        PUT(NEXT_FREE(bp), next);
    }
    else 
        PUT(NEXT_FREE(bp), 0);
        
    
}


void mm_free(void *bp)
{
    if(bp == 0)
       return;
    size_t size=GET_SIZE(HDRP(bp));
    //printf("free :  %d\n",size);
    //使已分配块变为0
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    //插入链表
    insert_free_list(bp);
    
}


//我觉的这里并不需要重新合并，因为每一次free之后，都将这一块加入到链表的开头
static void *coalesce(void *bp)
{
    //获得上一个块的已分配位
    //printf("合并\n" );
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc=GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size=GET_SIZE(HDRP(bp));
    //前后都是已分配的
    if(prev_alloc&&next_alloc){
        //printf("进来了，不合并  \n");      
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
                    bp = PREV_BLKP(bp);
                    }
    else{
        size+=GET_SIZE(HDRP(PREV_BLKP(bp)))+GET_SIZE(FTRP(NEXT_BLKP(bp)));
                       PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
                       PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
                       bp=PREV_BLKP(bp);
            }

    return bp;
}

//这一步就相当于链表的删除
static void fix_list(void *bp,size_t asize)
{
    
    char *pr_node=(char *)GET(PREV_FREE(bp));
    char *ne_node=(char *)GET(NEXT_FREE(bp));
    size_t size=GET_SIZE(HDRP(bp));
    if(size-asize>=16){
        //这是这个空闲块大于的情况，我将进行分割
        char *next_bp=(char *)bp+asize;
        PUT(HDRP(next_bp),PACK(size-asize,0));
        PUT(FTRP(next_bp),PACK(size-asize,0));
        if(pr_node&&ne_node){
                PUT(NEXT_FREE(pr_node), next_bp);
                PUT(PREV_FREE(ne_node), next_bp);

                PUT(NEXT_FREE(next_bp), ne_node);
                PUT(PREV_FREE(next_bp), pr_node);
        }
        else if(!pr_node&&ne_node){
            PUT(free_list_first,next_bp);
            PUT(PREV_FREE(next_bp), 0);

            PUT(NEXT_FREE(next_bp), ne_node);
            PUT(PREV_FREE(ne_node), next_bp);
        }
        else if (pr_node&&!ne_node){
            PUT(NEXT_FREE(pr_node), next_bp);

            PUT(PREV_FREE(next_bp), pr_node);
            PUT(NEXT_FREE(next_bp), 0);
        }
        else {
            PUT(free_list_first,next_bp);
            PUT(NEXT_FREE(next_bp), 0);
            PUT(PREV_FREE(next_bp), 0);
        }
    }
    else{
        //否则这肯定是16,直接分配给它
        if(pr_node&&ne_node){
                    PUT(NEXT_FREE(pr_node), ne_node);
                    PUT(PREV_FREE(ne_node), pr_node);
            }
            else if(!pr_node&&ne_node){
                PUT(free_list_first,ne_node);
                PUT(PREV_FREE(ne_node), 0);
            }
            else if (pr_node&&!ne_node){
                PUT(NEXT_FREE(pr_node), 0);
            }
            else PUT(free_list_first,0);
    }

        
    
    //将所有空闲的块加进来都不符合，于是这些空闲块就放里面不动，
    //或者将所有剩下的空闲块全部合并，万一下次还是需要这么大的块怎么办
    //不对，这里空闲链表没法合并，因为这些块并不是连续的，
    
}



static void *find_fit(size_t asize)
{
    //printf("进来找了   \n ");
    //printf("需要这么大 %d\n ",asize);
    char *bp;
    //序言块的下一个头块
    for(bp=(char *)GET(free_list_first);bp!=0;bp=(char *)GET(NEXT_FREE(bp)))
    {   
        if((GET_SIZE(HDRP(bp))>=asize)){
            fix_list(bp,asize);
            return bp;
        }
    }
    //如果找了一圈都没找到，则说明需要的size太大，链表里都不符合
    //因此就需要进行合并,修改链表--->错，如果链表中的块不是连续的，怎么合并
    return NULL;
    
}


static void place(void * bp,size_t asize)
{
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
}

void *mm_malloc(size_t size)
{
    printf("什么情况？  \n");
    char *pp=0;
    for(pp=(char *)GET(free_list_first);pp!=0;pp=(char *)GET(NEXT_FREE(pp)))
    {   
        printf("%d\n",GET_SIZE(HDRP(pp)) );
        
    }
    size_t asize;
    size_t extendsize;
    char *bp;
    //printf("需要分配大小： %d\n ",size);
    if(size==0)
        return NULL;
    if(size<=DSIZE)
        asize=2*DSIZE;
    else 
        asize=8*((size+15)/8);
    if((bp=find_fit(asize))!=NULL){
        place(bp,asize);
        return bp;
    }
    //printf("此时尺寸不合适  %d\n",asize);
    printf("重新分配4096  ");
    extendsize=MAX(asize,CHUNKSIZE);
    
    if((bp=extend_heap(extendsize/WSIZE))==NULL)
        return NULL;
    place(bp,asize);
    return bp;
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
printf("realloc 你被调用过？？\n");
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    printf("dddddd\n");
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



int main(int argc, char const *argv[])
{
    mem_init();
    mm_init();
    void *bp=mm_malloc(2048);
    void *bp1=mm_malloc(48);
    mm_free(bp);
    return 0;
}


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
#define WSIZE   4
#define DSIZE   8
#define CHUNKSIZE   (1<<12) //一块大小为4096

#define PACK(size,alloc)  ((size) |(alloc)) //将字节大小和标记位合并
#define MAX(x,y)    ((x>y)?x:y)
#define GET(p)  (*(unsigned int *)(p))  //表示一个块的大小，里面包含了标记位
#define PUT(p,val) (*(unsigned int *)(p)=(val)) //将val字节的给p 
#define GET_ALLOC(p)    (GET(p) & 0x1)
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define HDRP(bp)    ((char *)(bp)-WSIZE)    //此bp是从有效载荷开始的，因此指向块的头部，得减去一个4字节
//只有从块的头部才能获得此块的大小，然后加上整个块的大小，减去DSIZE，移动到尾部的开始处;
#define FTRP(bp)    ((char *)(bp)+GET_SIZE(HDRP(bp))-DSIZE)
#define NEXT_BLKP(bp)       ((char *)(bp)+GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp)       ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))


/*---------------------------*/
#define PREV_FREE(bp)   ((char *)(bp))
#define NEXT_FREE(bp)   ((char *)(bp)+WSIZE)
/*---------------------------*/
int mm_init(void);
static void *extend_heap(size_t words);
void mm_free(void *bp);
static void *coalesce(void *bp);
static void insert_free_list(void *bp);
static void insert_list_srot(void *bp);
static void merge_bp(void * bp);
static void merge_free_list(void);
/*
 *
 * 创建开始空闲链表
 */
//指向第一个块(序言块)
static char *heap_listp=0;
//显示空闲链表
static char * free_list_first=0;
//    头 空头  空尾 尾
//  | 4 | 4 | 4 | 4 |

/*

这里要求块大小是16的倍数，所以在分配头块的时候，我分配了16个字节

        /   头    /
        --------
        / prev   /
        --------
        / next   /
        --------
        / 序言头  /
        --------
        / 序言尾  /
        --------
        /   结束  /
        --------

---------------------------------
正常块则为：
        /   头    /
        --------
        / prev   /
        --------
        / next   /
        --------
        /  ...   /
        --------
        /   尾   /


*/
int mm_init(void)
{
    /*创建初始空堆*/
    if((heap_listp=mem_sbrk(6*WSIZE))==(void*) -1)
        return -1;
    PUT(heap_listp,0);  //哨头
    PUT((heap_listp+(WSIZE)),0);    //序言头的上一个空闲为0
    PUT((heap_listp+(2*WSIZE)),0);  //序言头的下一个空闲暂时还不知道
    PUT((heap_listp+(3*WSIZE)),PACK(DSIZE,1));  
    PUT((heap_listp+(4*WSIZE)),PACK(DSIZE,1));  
    PUT((heap_listp+(5*WSIZE)),1);      //结束尾
    heap_listp+=(4*WSIZE);  //从有效载荷处开始
    //安排空闲链表
    free_list_first=(char *)heap_listp-2*WSIZE;
    //初始化的时候，使得空闲链表为0
    PUT(free_list_first, 0);
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
    if((long)(bp=mem_sbrk(size))==(void *)-1)
        return NULL;

    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));
    /*
    *   每一次都直接将空闲链表插入到表头
    *
     */
    //insert_free_list(bp);



    /*
    *   每一次都将空闲链表按照地址顺序插入到链表中
    *
     */
    insert_list_srot(bp);
    return bp;
}

static void insert_free_list(void *bp)
{
    //把bp这个块变成空闲块的数据结构
    char *next=(char *)GET(free_list_first);//free_list 总是指向链表的开头
    PUT(free_list_first,bp);
    PUT(PREV_FREE(bp),0);
    if(next!=0){
        PUT(NEXT_FREE(bp), next);
        PUT(PREV_FREE(next), bp);
    }
    else 
        PUT(NEXT_FREE(bp), 0);

        
    
}

//检查是否可以合并，（可惜c语言没有标准库，因此不能用c++的set等数据结构，除非自己编写。。）
//因为我是直接free块，加入到链表的开头，因此，如果要合并前后块，
//则需要每一次搜索整个链表，才能找到是否有前后块，
//但是如果我的空闲链表的插入是按照地址的顺序进行排序的，那么合并就变的容易很多;
//因此这里，我可不可以维护一个额外的位图呢？用来记载空闲链表的顺序;
//那我可不可以用struct来记载？
//我觉得还是改成按地址顺序排列比较好

static void insert_list_srot(void *bp)
{
    char *fp;
    char *next;
    //序言块的下一个头块
    for(fp=(char *)GET(free_list_first);fp!=0;fp=(char *)GET(NEXT_FREE(fp)))
    {   
        next=(char *)GET(NEXT_FREE(fp));
        if((char *)bp>fp&&(((char *)bp<next)||next==0))
        {
            if(next!=0){
                PUT(NEXT_FREE(fp), bp);
                PUT(PREV_FREE(next), bp);

                PUT(NEXT_FREE(bp), next);
                PUT(PREV_FREE(bp), fp);
            }
            else{
                PUT(NEXT_FREE(fp), bp);

                PUT(NEXT_FREE(bp), 0);
                PUT(PREV_FREE(bp), fp);
            }
            return ;
        }
    }
    PUT(free_list_first,bp);
    PUT(PREV_FREE(bp),0);
    PUT(NEXT_FREE(bp),0);
}

static void merge_bp(void * bp){
    size_t asize=GET_SIZE(HDRP(bp));
    char *pr_free=(char *)GET(PREV_FREE(bp));
    char *ne_free=(char *)GET(NEXT_FREE(bp));

    char *PREV=PREV_BLKP(bp);
    char *SUCC=NEXT_BLKP(bp);
    if(SUCC!=0){
        if(pr_free==PREV&&ne_free==SUCC){
        asize+=GET_SIZE(HDRP(PREV))+GET_SIZE(HDRP(SUCC));
        PUT(HDRP(PREV),asize);
        PUT(FTRP(PREV),asize);

        PUT(NEXT_FREE(pr_free), ne_free);
        if(ne_free!=0)  PUT(PREV_FREE(ne_free), pr_free);
        }
        else if(pr_free!=PREV&&ne_free==SUCC){
            asize+=GET_SIZE(HDRP(SUCC));
            PUT(HDRP(bp),asize);
            PUT(FTRP(bp),asize);
            char *nn=(char *)GET(NEXT_FREE(ne_free));
            PUT(NEXT_FREE(bp), nn);
            if(nn!=0)  PUT(PREV_FREE(nn), bp);
        }
        else if(pr_free==PREV&&ne_free!=SUCC){
            //强调一下，这里为什么不判断pr_free为0的情况，
            //因为PREV此时为序言头，因此二者不可能相等，
            //相等也就是存在空闲块
            asize+=GET_SIZE(HDRP(PREV));
            PUT(HDRP(pr_free),asize);
            PUT(FTRP(pr_free),asize);


            PUT(NEXT_FREE(pr_free), ne_free);
            if(ne_free!=0)  PUT(PREV_FREE(ne_free), pr_free);
        }
    }
    else{
        if(pr_free==PREV){
            asize+=GET_SIZE(HDRP(PREV));
            PUT(HDRP(pr_free),asize);
            PUT(FTRP(pr_free),asize);

            PUT(NEXT_FREE(pr_free), ne_free);
            if(ne_free!=0)  PUT(PREV_FREE(ne_free), pr_free);
        }
        
    }
 
}


static void merge_free_list(void)
{
    char * bp=(char *)GET(free_list_first);
    
    for(;bp!=0;bp=(char *)GET(NEXT_FREE(bp)))
    {   
        merge_bp(bp);
    }
}



void mm_free(void *bp)
{
    if(bp == 0)
       return;
    size_t size=GET_SIZE(HDRP(bp));
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    /*
    *   每一次都直接将空闲链表插入到表头
    *
     */
    //insert_free_list(bp);



    /*
    *   每一次都将空闲链表按照地址顺序插入到链表中
    *
     */
    insert_list_srot(bp);
    merge_free_list();
    
}



//这一步就相当于链表的删除
static void fix_list(void *bp,size_t asize)
{
    
    char *pr_node=(char *)GET(PREV_FREE(bp));
    char *ne_node=(char *)GET(NEXT_FREE(bp));
    size_t size=GET_SIZE(HDRP(bp));
    if(size-asize>=16){
        //这是这个空闲块大于的情况，我将进行分割
        char *next_bp=(char *)bp+asize;
        PUT(HDRP(next_bp),PACK(size-asize,0));
        PUT(FTRP(next_bp),PACK(size-asize,0));
        if(pr_node&&ne_node){
                PUT(NEXT_FREE(pr_node), next_bp);
                PUT(PREV_FREE(ne_node), next_bp);

                PUT(NEXT_FREE(next_bp), ne_node);
                PUT(PREV_FREE(next_bp), pr_node);
        }
        else if(!pr_node&&ne_node){
            PUT(free_list_first,next_bp);
            PUT(PREV_FREE(next_bp), 0);

            PUT(NEXT_FREE(next_bp), ne_node);
            PUT(PREV_FREE(ne_node), next_bp);
        }
        else if (pr_node&&!ne_node){
            PUT(NEXT_FREE(pr_node), next_bp);

            PUT(PREV_FREE(next_bp), pr_node);
            PUT(NEXT_FREE(next_bp), 0);
        }
        else {
            PUT(free_list_first,next_bp);
            PUT(NEXT_FREE(next_bp), 0);
            PUT(PREV_FREE(next_bp), 0);
        }
    }
    else{
        //否则这肯定是16,直接分配给它
        if(pr_node&&ne_node){
                    PUT(NEXT_FREE(pr_node), ne_node);
                    PUT(PREV_FREE(ne_node), pr_node);
            }
            else if(!pr_node&&ne_node){
                PUT(free_list_first,ne_node);
                PUT(PREV_FREE(ne_node), 0);
            }
            else if (pr_node&&!ne_node){
                PUT(NEXT_FREE(pr_node), 0);
            }
            else PUT(free_list_first,0);
    }

        
    
    //将所有空闲的块加进来都不符合，于是这些空闲块就放里面不动，
    //或者将所有剩下的空闲块全部合并，万一下次还是需要这么大的块怎么办
    //不对，这里空闲链表没法合并，因为这些块并不是连续的，
    
}



static void *find_fit(size_t asize)
{

    char *bp;
    for(bp=(char *)GET(free_list_first);bp!=0;bp=(char *)GET(NEXT_FREE(bp)))
    {   
        if((GET_SIZE(HDRP(bp))>=asize)){
            fix_list(bp,asize);
            return bp;
        }
    }
    //如果找了一圈都没找到，则说明需要的size太大，链表里都不符合
    //因此就需要进行合并,修改链表--->错，如果链表中的块不是连续的，怎么合并
    //现在可以合并了，因为我按照地址顺序排的链表

    return NULL;
    
}


static void place(void * bp,size_t asize)
{
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
}

void *mm_malloc(size_t size)
{
    
    size_t asize;
    size_t extendsize;
    char *bp;
    if(size==0)
        return NULL;
    if(size<=DSIZE)
        asize=2*DSIZE;
    else 
        asize=8*((size+15)/8);
    if((bp=find_fit(asize))!=NULL){
        place(bp,asize);
        return bp;
    }
    extendsize=MAX(asize,CHUNKSIZE);
    
    if((bp=extend_heap(extendsize/WSIZE))==NULL)
        return NULL;
    place(bp,asize);
    fix_list(bp,asize);
    return bp;
}

明天修改realloc
目前思路是，把所有之前分配的全复制到新的内存中去，然后删除原有内存，
实现压缩，
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
printf("realloc 你被调用过？？\n");
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    printf("dddddd\n");
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