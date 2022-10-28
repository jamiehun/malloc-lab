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
    "Jungle_week06_team06",
    /* First member's full name */
    "jamiehun",
    /* First member's email address */
    "jamiesunghun@gmail.com",
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

/* Basic constants and macros */
#define WSIZE       4           /* Word and header/footer size (bytes)*/ /* the size of word == the size of header/footer */
#define DSIZE       8           /* Double word size (bytes)*/
#define CHUNKSIZE   (1<<12)     /* Extend heap by this amount (bytes)*/ /* 1000 0000 0000(2) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))  // '|' : 비트연산자 OR

/* Read and write a word at address P */
#define GET(p)      (*(unsigned int *)(p))    /* The argument p is typically a (void *) pointer, which cannot be dereferenced directly*/
                                             /* Thus, casting is used on the line */
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)          /* p(bit) & (1111 1000(2)) */
#define GET_ALLOC(p) (GET(p) & 0x1)          /* p(bit) & (0000 0001(2)) */

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* extend_heap */
/* 새 가용 블록으로 힙 확장하기 */
static void * extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment (double-words aligned)*/
    /* 요청한 크기를 인접 2워드의 배수(8바이트)로 반올림 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 1 : True -> odd number, 2: False -> even number
    if ((long)(bp = mem_sbrk(size) == -1)) // mem_sbrk : incr까지 늘리고 block의 최초 값을 return 해줌
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0))            /* Free block header */
    PUT(FTRP(bp), PACK(size, 0))            /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1))    /* New epilogue header */

    /* Coalesce if the previous block was free */
    /* 이전 블록이 free이면 연결*/
    return coalesce(bp);

}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)  // go to page 823 in CSAPP (4 word 할당 후 에러체크)
        return -1;
    PUT(heap_listp, 0);                                 /* Alignment padding     | 0 저장     */   
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));        /* Prologue header       | (8/1) 저장 */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));        /* Epilogue header       | (8/1) 저장 */ 
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));            /* Epilogue block header | (0/1) 저장 */
    heap_listp += (2*WSIZE);                              

    /* Extend the empty heap with a free block of CHUNKSIZE bytes*/
    /* CHUNKSIZE 바이트의 free block인 비어있는 힙으로 늘림 */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) // (2 ^ 11 / 4)
        return -1;
    return 0;
}   

/* 
 * find_fit
 */
static void *find_fit(size_t asize) // asize는 8의 배수
{
    char *bp;

    // i = 1일 때 ~ CHUNKSIZE까지 검색하며 mem_brk를 찾기
    for (i = 0; i < CHUNKSIZE; i++)
    {
        bp = 
    }
}

/* 
 * Find_fit
 */
static void *find_fit(size_t asize){
    char *bp
    bp = mem_brk; // 새로 시작하는 주소

    while  ((bp < mem_max_addr) &&               // bp가 마지막 점을 넘지 않을 때까지
           (((GET_ALLOC(HDRP(bp))) ||            // already allocated
             (GET_SIZE(HDRP(bp)))) <= asize))    // too small
        { bp = NEXT_BLKP(bp); }
    
    return bp;
}

/* 
 * place 함수
 */
static void place(void *bp, size_t asize){
    
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    size_t asize;       /* Adjusted block size */ // 조정된 
    size_t extendsize;  /* Amount to extend heap if no fit */
    char *bp;

    /* Fake Request 걸러내기 */
    if (size == 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE)
        asize = 2*DSIZE; // 최소 16바이트
    else                 // 크면
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); // 인접 8의 배수로 반올림(?)

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) { // 맞는 블록을 찾으면
        place(bp, asize);               // 해당 블록에 저장
        return bp;
    }

    /* No fit found Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) // (extend / 8 word)
        return NULL;
    place(bp, asize);
    return bp;

}

/*
 * coalesce - 연결
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // prev 블록의 할당여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  // next 블록의 할당여부 확인
    size_t size = GET_SIZE(HDRP(bp));                    // 현재 블록의 사이즈 확인

    if (prev_alloc && next_alloc) {                      /* Case 1 */   
        return bp;                                       /* prev 할당 & next 할당 */
    }

    else if (prev_alloc && !next_alloc) {                /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));           /* prev 할당 & next 미할당 */
        PUT(HDRP(bp), PACK(size, 0));                    // 현재 헤더에 새로운 size 넣고
        PUT(FTRP(bp), PACK(size, 0));                    // 새로운 사이즈를 기준으로 새로운 footer에도 정보 저장
    
    else if (!prev_alloc && next_alloc) {                /* Case 3*/
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));           /* prev 미할당 & next 할당 */
        PUT(FTRP(bp), PACK(size, 0));                    // 현재 footer에 새로운 size 넣고
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));         // 새로운 사이즈를 prev header에 넣고 저장
        bp = PREV_BLKP(bp);                              // bp는 prev에 저장

    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +          /* Case 4 */
            GET_SIZE(FTRP(NEXT_BLKP(bp)));               /* prev 미할당 & next 미할당 */   
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));         // 이전 header에 size 넣고
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));         // 이후 footer에 size 넣음
        bp = PREV_BLKP(bp);
    }
    return bp;
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));   // bp의 size를 얻음

    PUT(HDRP(bp), PACK(size, 0));       // bp의 헤더에 (size, 0(미할당)) 추가
    PUT(FTRP(bp), PACK(size, 0));       // bp의 footer에 (size, 0(미할당)) 추가
    coalesce(bp);                       // 연결
}

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














