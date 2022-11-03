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
#include <stdint.h>

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
// 7을 더하고 size를 해주는 이유는 가장 가까운 8의 배수로 갈 수 있음
// ex) size = 7 => 7 + 7 = 14 | 14 & ~0x7 = 8
// ex) size = 13 => 13 + 7 = 20 | 20 & ~0x7 = 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE       4           /* Word and header/footer size (bytes)*/ /* the size of word == the size of header/footer */
#define DSIZE       8           /* Double word size (bytes)*/
#define CHUNKSIZE   (1<<12)     /* Extend heap by this amount (bytes)*/ /* 1000 0000 0000(2) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))  // '|' : 비트연산자 OR => header 또는 footer에 들어갈 size의 크기 및 할당 유무 비트 연산

/* Read and write a word at address P */
#define GET(p)      (*(unsigned int *)(p))    /* The argument p is typically a (void *) pointer, which cannot be dereferenced directly*/
                                             /* Thus, casting is used on the line */
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)          /* p(bit) & (1111 1000(2)) */
#define GET_ALLOC(p) (GET(p) & 0x1)          /* p(bit) & (0000 0001(2)) */

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)  // 헤더의 포인터(주소) 반환
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 헤더의 포인터(주소) 반환

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *next_fit(size_t asize);
static void *best_fit(size_t asize);
static void place(void *bp, size_t asize);


static char *heap_listp;

/* 
 * cur_bp 변경 지점
 * 1) 처음에 Prologue Block의 heap_listp로 지정
 * 2) fit 될 경우 next_bp 업데이트 
 * 3) coalescing 경우에도 next_bp 업데이트
 */
static char *cur_bp;

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
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));        /* Prologue footer       | (8/1) 저장 */ 
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));            /* Epilogue block header | (0/1) 저장 */
    heap_listp += (2*WSIZE);                              
    cur_bp = heap_listp;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes*/
    /* CHUNKSIZE 바이트의 free block인 비어있는 힙으로 늘림 */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) // (2 ^ 11 / 4)
        return -1;
    return 0;
}   

/* extend_heap */
/* 새 가용 블록으로 힙 확장하기 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment (double-words aligned)*/
    /* 요청한 크기를 인접 2워드의 배수(8바이트)로 반올림 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 1 : True -> odd number, 2: False -> even number
    if ((long)(bp = mem_sbrk(size)) == -1) // mem_sbrk : incr까지 늘리고 block의 최초 값을 return 해줌
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));            /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));            /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));    /* New epilogue header */

    /* Coalesce if the previous block was free */
    /* 이전 블록이 free이면 연결*/
    return coalesce(bp);
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
    }

    else if (!prev_alloc && next_alloc) {                /* Case 3*/
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));           /* prev 미할당 & next 할당 */
        PUT(FTRP(bp), PACK(size, 0));                    // 현재 footer에 새로운 size 넣고
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));         // 새로운 사이즈를 prev header에 넣고 저장
        bp = PREV_BLKP(bp);                              // bp는 prev에 저장
    }

    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +          /* Case 4 */
            GET_SIZE(FTRP(NEXT_BLKP(bp)));               /* prev 미할당 & next 미할당 */   
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));         // 이전 header에 size 넣고
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));         // 이후 footer에 size 넣음
        bp = PREV_BLKP(bp);
    }
    cur_bp = bp;
    return bp;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 * 블록 할당을 brk 포인터를 증가시킴으로써 실행해줌
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
        asize = 2*DSIZE; // 최소 16바이트, 8바이트는 header + footer 만의 최소 블록 크기이므로, 그 다음 8의 배수인 16바이트로 설정
    else                 // 크면
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); // DSIZE는 8바이트로 바꿔주는 것임 

    /* Search the free list for a fit */
    if ((bp = best_fit(asize)) != NULL) { // 맞는 블록을 찾으면
        place(bp, asize);               // 해당 블록에 저장
        return bp;
    }

    /* No fit found Get more memory and place the block (asize 또는 CHUNKSIZE 만큼 가용 리스트 범위 넓혀줌) */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) // (extend / 8 word)
        return NULL;
    place(bp, asize);
    return bp;

}





/* 
 * Find_fit
 */
static void *find_fit(size_t asize)
{
    // char *bp
    // bp = mem_brk; // 새로 시작하는 주소

    // while  ((bp < mem_max_addr) &&               // bp가 마지막 점을 넘지 않을 때까지
    //        (((GET_ALLOC(HDRP(bp))) ||            // already allocated
    //          (GET_SIZE(HDRP(bp)))) <= asize))    // too small
    //     { bp = NEXT_BLKP(bp); }
    
    // return bp;

    /* First-fit search */
    char *bp;


    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){ // GET_SIZE(HDRP(bp)) > 0 ; 에필로그 가기 전까지
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) { // 0 : False ,  1 : True
            return bp; // 조건에 맞는 블록이 있다면 다음 bp값을 할당해줌
        }
    }


    return NULL; /* No Fit */
}

/*
 * Next Fit 함수 
 */
static void *next_fit(size_t asize){
    char *bp;

    // cur_bp를 최종값으로 설정해놓고 있으니 cur_bp ~ epilogue 값까지 검색
    for(bp = cur_bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if( !GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))) ) {
            cur_bp = bp; // cur_bp 최신값으로 설정
            return bp;
        }
    }

    // 위에서 return을 못하면 처음부터 cur_bp까지 search
    for(bp = heap_listp; bp < cur_bp; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))) ) {
            cur_bp = bp;
            return bp;
        }
    }

    // 그래도 없으면 NULL 반환
    return NULL;
}

/*
 * Best Fit 함수
 */
static void *best_fit(size_t asize){
    char *bp;
    char *best_bp = NULL;
    size_t best_size;
    size_t min = SIZE_MAX;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){ // GET_SIZE(HDRP(bp)) > 0 ; 에필로그 가기 전까지
        

        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            best_size = asize - GET_SIZE(HDRP(bp));

            if (best_size == 0){
                return bp;
            }    

            if (best_size < min) {
                min = best_size;
                best_bp = bp;
            } 
        }
    }

    if (best_bp == NULL) {
        return NULL;
    }
    
    return best_bp;
}


/* 
 * place 함수
 */
static void place(void *bp, size_t asize)      // asize를 저장해주는 함수
{   
    size_t csize = GET_SIZE(HDRP(bp));         // 해당 bp의 사이즈를 csize에 할당 (현재 할당되어 있지 않은 상태)

    if((csize - asize) >= (2*DSIZE)) {         // (비어있는 블록 - 저장해야하는 사이즈) >= 16
        PUT(HDRP(bp), PACK(asize, 1));         // HDRP(bp)에 asize만큼 저장
        PUT(FTRP(bp), PACK(asize, 1));         // FTRP(bp)에 asize만큼 저장
        bp = NEXT_BLKP(bp);                    // bp는 다음 bp를 뜻함
        PUT(HDRP(bp), PACK(csize-asize, 0));   // 다음 bp는 0을 뜻함
        PUT(FTRP(bp), PACK(csize-asize, 0));   // 다음 bp의 footer는 0을 뜻함 
    }

    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1)); 
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

    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    /* copy size는 oldptr의 사이즈를 copy하는 것인데 (temp와 같은 역할) */
    /* oldptr에 WSIZE(1워드 - 헤더)를 한번 빼주고 할당된 값 (마지막 1)을 빼줌 => free된 상태 */
    // copySize = *(size_t *)((char *)oldptr - WSIZE);

    /* 혹은 oldptr의 헤더에 가서 size를 받아줌 */
    copySize = GET_SIZE(HDRP(oldptr)) - 8; // 8을 빼주는 이유는 헤더와 footer를 제거하기 위함 

    if (size < copySize)    // 만약 copySize(old ptr)이 realloc하려는 (newptr) size보다 크다면
      copySize = size;      // copySize = size
    memcpy(newptr, oldptr, copySize);  // memcpy(목적지 포인터, 원본포인터, 크기) => 원본 포인터의 내용을 목적지 포인터에 저장
    mm_free(oldptr);                   // oldptr은 free
    return newptr;                     // 새로운 포인터를 리턴
}