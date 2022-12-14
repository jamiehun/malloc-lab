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
/* 기본 상수와 매크로 */
#define WSIZE       4           /* Word and header/footer size (bytes)*/ /* the size of word == the size of header/footer */
#define DSIZE       8           /* Double word size (bytes)*/
#define MINIMUM     16          /* Initial Prologue block size, header, footer, PREC, SUCC */
#define CHUNKSIZE   (1<<12)     /* Extend heap by this amount (bytes)*/ /* 1 0000 0000 0000(2) => 4096 (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y)) // 최대값 구하는 함수

/* Pack a size and allocated bit into a word */
/* header 및 footer 값 (size + allocated) */
#define PACK(size, alloc)  ((size) | (alloc))  // '|' : 비트연산자 OR => header 또는 footer에 들어갈 size의 크기 및 할당 유무 비트 연산

/* Read and write a word at address P */
#define GET(p)      (*(unsigned int *)(p))    /* void 포인터인 p는 역참조가 불가 => 캐스팅 후 역참조로 진행 */
                                             /* Thus, casting is used on the line */
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
/* header나 footer에서 블록의 size, allocated field를 읽어옴 */
#define GET_SIZE(p) (GET(p) & ~0x7)          /* p(bit) & (1111 1000(2)) */ 
#define GET_ALLOC(p) (GET(p) & 0x1)          /* p(bit) & (0000 0001(2)) */

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)  // 헤더의 포인터(주소) 반환
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 헤더의 포인터(주소) 반환

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 다음블록의 헤더값 리턴 (bp + bp - 헤더)  : | header + payload + footer | | header + payload + .. |
                                                                       // 블록의 사이즈는 header로 가서 알아봄(bp - WSIZE)     ^
                                                                       //                                   -> | header + payload + footer | | header + payload + .. |
                                                                       //                                                                             ^
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 이전블록의 헤더값 리턴 (bp - (bp - DSIZE)) : | header + payload + footer | | header + payload + .. |
                                                                       //                                                                                ^
                                                                       //                                      -> | header + payload + footer | | header + payload + .. |
                                                                       //                                                  ^                                                                                                      ^

/* Free List 상에서 이전과 이후 블록의 포인터를 return */
#define PREC_FREEP(bp) (*(void**)(bp))         // void를 가르키는 이중포인터의 역참조 ; 이전 블록의 bp
#define SUCC_FREEP(bp) (*(void**)(bp + WSIZE)) // void를 가르키는 이중포인터의 역참조 ; 이후 블록의 bp


static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *next_fit(size_t asize);
static void *best_fit(size_t asize);
static void place(void *bp, size_t asize);
void removeBlock(void *bp);
void putFreeBlock(void *bp);


static char *heap_listp; // Prologue block을 가르키는 정적 전역 변수 설정
static char *free_listp; // free list의 맨 첫 블록을 가르키는 포인터

/* 
 * cur_bp의 목적 : 마지막으로 free된 상태의 포인터 위치를 쫓아가기 목적
 * cur_bp 변경 지점
 * 1) 처음에 Prologue Block의 heap_listp로 지정
 * 2) fit 될 경우 cur_bp 업데이트 
 * 3) coalescing 경우에도 next_bp 업데이트
 */
static char *cur_bp;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    /* Create the initial empty heap */
    /* 초기 설정은 implicit free list와는 다르게 6 words로 설정 */
    /* 이유는 padding, prologue_header, prologue_footer, PREC, SUCC, epilogue_header */
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)  // go to page 823 in CSAPP (6 words 할당 후 에러체크)
        return -1;
    
    PUT(heap_listp, 0);                                 /* Alignment padding     | 0 저장     */   
    PUT(heap_listp + (1*WSIZE), PACK(MINIMUM, 1));      /* Prologue header       | (8/1) 저장 */
    PUT(heap_listp + (2*WSIZE), NULL);                  /* Predecessor           | 이전 주소값 저장하기 위함인데 현재는 포인터 없어서 NULL 값으로 초기화 */    
    PUT(heap_listp + (3*WSIZE), NULL);                  /* Successor             | 다음 주소값 저장하기 위함인데 현재는 포인터 없어서 NULL 값으로 초기화 */
    PUT(heap_listp + (4*WSIZE), PACK(MINIMUM, 1));      /* Prologue footer       | (8/1) 저장 */ 
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));            /* Epilogue block header | (0/1) 저장 */
                                                        // 왜 16/1으로 둘까?, 왜 prologue footerfmf pred, succ 뒤에 둘까?
    heap_listp += (2*WSIZE);                            
    free_listp = heap_listp;                            // free_listp를 지속적으로 업데이트해줘야함 (초기값 설정)

    cur_bp = heap_listp;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes*/
    /* CHUNKSIZE 바이트의 free block인 비어있는 힙으로 늘림 */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) // (4096bytes / 4)
        return -1;
    return 0;
}   

/* extend_heap */
/* 새 가용 블록으로 힙 확장하기 (워드 단위 메모리를 인자로 받아 힙을 늘려줌) */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment (double-words aligned)*/
    /* 요청한 크기를 인접 2워드의 배수(8바이트)로 반올림 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 1 : True -> odd number, 2: False -> even number
    if ((long)(bp = mem_sbrk(size)) == -1) // mem_sbrk : incr까지 늘리고 block의 최초 값을 return 해줌 (mem_brk : void pointer)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));            /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));            /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));    /* New epilogue header */

    /* Coalesce if the previous block was free */
    /* 이전 블록이 free이면 연결*/
    return coalesce(bp); // void pointer 함수를 리턴해줌 , 초기에는 coalesce로 바로 이동
}

/*
 * coalesce - 연결
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // prev 블록의 할당여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  // next 블록의 할당여부 확인
    size_t size = GET_SIZE(HDRP(bp));                    // 현재 블록의 사이즈 확인

    /* Case 1 */   
    /* prev 할당 & next 할당 */
    /* 해당 블록만 free list에 넣음 */

    if (prev_alloc && !next_alloc) {                /* Case 2 */
        removeBlock(NEXT_BLKP(bp));                      // free 상태였던 직후 블록을 free list에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));           /* prev 할당 & next 미할당 */
        PUT(HDRP(bp), PACK(size, 0));                    // 현재 헤더에 새로운 size 넣고
        PUT(FTRP(bp), PACK(size, 0));                    // 새로운 사이즈를 기준으로 새로운 footer에도 정보 저장
    }

    else if (!prev_alloc && next_alloc) {                /* Case 3*/
        removeBlock(PREV_BLKP(bp));                      // 직전 블록을 free list에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));           /* prev 미할당 & next 할당 */
        bp = PREV_BLKP(bp);                              // bp는 prev에 저장
        PUT(HDRP(bp), PACK(size, 0));                    // 새로운 사이즈를 prev header에 넣고 저장
        PUT(FTRP(bp), PACK(size, 0));                    // 현재 footer에 새로운 size 넣고
    }

    else if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +          /* Case 4 */
            GET_SIZE(FTRP(NEXT_BLKP(bp)));               /* prev 미할당 & next 미할당 */
        removeBlock(PREV_BLKP(bp));                      // 직전 블록을 free list에서 제거
        removeBlock(NEXT_BLKP(bp));                      // 다음 블록을 free list에서 제거
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));         // 이전 header에 size 넣고
        PUT(FTRP(bp), PACK(size, 0));         // 이후 footer에 size 넣음
 
    }
    cur_bp = bp;

    // 연결된 새 가용 블록을 free list에 추가
    putFreeBlock(bp); // init 시에는 앞 뒤 할당이 되어 있어서 (Prologue, Epilogue) putFreeBlock으로 이동
                      // 나머지의 경우 앞 뒤 연결 후 해당 값 free_listp에 추가
    
    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 * 블록 할당을 brk 포인터를 증가시킴으로써 실행해줌
 */
void *mm_malloc(size_t size)
{
    size_t asize;       /* Adjusted block size : 조정된 사이즈 */
    size_t extendsize;  /* Amount to extend heap if no fit */
    char *bp;

    /* Fake Request 걸러내기 */
    if (size == 0)
        return NULL;
    
    /* 블록을 최소 16바이트로 늘려줌 (CSAPP 823page) */
    if (size <= DSIZE)
        asize = 2*DSIZE; // 최소 16바이트, 8바이트는 header + footer 만의 최소 블록 크기이므로, 그 다음 8의 배수인 16바이트로 설정
    else                 // 크면
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); // 7을 더함으로써 size = 1일 때 최소 16바이트 나올 수 있음 
                                                                  // 32비트에서는 8의 배수인 블록을 리턴

    /* asize에 맞는 bp 찾기  */
    if ((bp = find_fit(asize)) != NULL) {  // 맞는 블록을 찾으면
        place(bp, asize);                // 해당 블록에 할당
        return bp;
    }

    /* No fit found Get more memory and place the block (asize 또는 CHUNKSIZE 만큼 가용 리스트 범위 넓혀줌) */
    // 맞는 크기의 메모리가 없으면 asize 혹은 CHUNKSIZE 보다 크게 heap의 크기를 늘려줌
    extendsize = MAX(asize, CHUNKSIZE); // asize가 CHUNKSIZE보다 큰 경우는 size가 요구하는 8바이트 기준의 크기가 매우 큼을 의미
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
    /* First-fit search */
    char *bp;


    // free list의 맨 마지막은 Prologue Block -> HDR : (16/1)
    for (bp = free_listp; GET_ALLOC(HDRP(bp)) !=  1; bp = SUCC_FREEP(bp)){ 
        if (asize <= GET_SIZE(HDRP(bp))) { // Implicit와 달리 할당되어있는지 여부는 따로 체크할 필요 없음
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
    for(bp = free_listp; bp < cur_bp; bp = NEXT_BLKP(bp)){
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

    for (bp = free_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){ // GET_SIZE(HDRP(bp)) > 0 ; 에필로그 가기 전까지
        

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
 * place 함수 : asize를 저장해주는 함수
 */
static void place(void *bp, size_t asize)      
{   
    size_t csize = GET_SIZE(HDRP(bp));         // fit에서 찾아진 bp의 사이즈를 csize에 할당 (현재 할당되어 있지 않은 상태)
    removeBlock(bp);                           // 할당되어질 블록이기 때문에 free_list에서 삭제

    if((csize - asize) >= (2*DSIZE)) {         // (비어있는 블록 - 저장해야하는 사이즈) >= 16 : 필요한 메모리 주소만큼 사용하고 나머지 여분의 주소 사용 가능
        PUT(HDRP(bp), PACK(asize, 1));         // HDRP(bp)에 asize만큼 저장
        PUT(FTRP(bp), PACK(asize, 1));         // FTRP(bp)에 asize만큼 저장 (헤더 자리에)
        bp = NEXT_BLKP(bp);                    // bp는 다음 bp를 뜻함 (다음 bp가 될 자리) : #define 참고
        PUT(HDRP(bp), PACK(csize-asize, 0));   // 다음 bp는 0을 뜻함
        PUT(FTRP(bp), PACK(csize-asize, 0));   // 다음 bp의 footer는 0을 뜻함 

        // free list 첫번째에 분할된 블럭에 넣음
        putFreeBlock(bp);
    }

    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1)); 
    }
}

/*
 * putFreeBlock(bp) : 새로 반환되거나 생성된 가용 블록을 free list의 첫 부분에 넣음
 */
void putFreeBlock(void *bp){
    SUCC_FREEP(bp) = free_listp; // bp는 헤더 다음, SUCC_FREEP의 경우 bp 다음 워드 = 전에 있는 free 값
    PREC_FREEP(bp) = NULL;       // LIFO이기 때문에 저장되는 값 없음
    PREC_FREEP(free_listp) = bp; // 이전 값의 이전 주소값은 bp(현재 값)로 설정
    free_listp = bp;             // free_listp를 현재의 값으로 갱신

    /* SUCC_FREEP(bp)의 의미 */
    /* bp의 이중포인터의 역참조로 bp를 가르키면 bp는 void pointer임 */
    /* 포인터는 주소를 값으로 가지고 있기 때문에 이중포인터 역참조시 주소값을 SUCC_FREEP에 저장함 */
    /* PREC_FREEP(bp)의 경우에도 똑같은 의미를 가지고 있음 */
}


/* 
 * removeBlock : 할당되거나 연결되는 가용 블록을 free list에서 없앰
 */
void removeBlock(void *bp){
    // freelist의 첫번째 블록을 없앨 때
    if (bp == free_listp) {
        PREC_FREEP(SUCC_FREEP(bp)) = NULL; // 다음 list 해당하는 값의 전의 값은 NULL
        free_listp = SUCC_FREEP(bp);        // free_listp의 값을 다음 해당하는 값으로 옮김
    }

    // free list 안에서 없앨 때
    else{
        SUCC_FREEP(PREC_FREEP(bp)) = SUCC_FREEP(bp); // 현재 이전의 successor를 현재 이후의 bp로 설정
        PREC_FREEP(SUCC_FREEP(bp)) = PREC_FREEP(bp); // 현재 이후의 predecessor를 현재 이전의 bp로 설정
                                                     // 결과적으로 현재값 제외하고 나머지 연결
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
    coalesce(bp);                       // 앞, 뒤 블록 중 연결할 곳이 있으면 연결 
}

/*
 * mm_realloc - Changes the size of a previously allocated block
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;  // 크기를 조절하고 싶은 힙의 시작 포인터
    void *newptr;        // 크기 조절 뒤의 새 힙의 시작 포인터
    size_t copySize;     // 복사할 힙의 크기
    
    newptr = mm_malloc(size); // size에 맞게 mm_malloc 시행
    if (newptr == NULL)
      return NULL;

    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    /* copySize는 oldptr의 사이즈를 copy (temp와 같은 역할) */
    /* oldptr에 DSIZE(헤더, 풋터)를 빼줌 => payload */

    /* 혹은 oldptr의 헤더에 가서 size를 받아줌 */
    copySize = GET_SIZE(HDRP(oldptr)) - 8; // 8을 빼주는 이유는 헤더와 footer를 제거하기 위함 

    if (size < copySize)    // 만약 copySize(oldptr)이 realloc하려는 (newptr) size보다 크다면
      copySize = size;      // 크기에 맞는 메모리만 할당되고 나머지는 안됨

    memcpy(newptr, oldptr, copySize);  // memcpy(목적지 포인터, 원본포인터, 크기) => 원본 포인터의 내용을 목적지 포인터에 copySize만큼 복사해서 저장
    mm_free(oldptr);                   // oldptr은 free
    return newptr;                     // 새로운 포인터를 리턴
}