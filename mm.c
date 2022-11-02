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
 * 
 * This code is from https://github.com/ryanfarr01/Malloc-Lab/blob/master/mm.c
 * Some of the codes are modified 
 * The below description is on the reference
 * 
 * This implementation uses segregated explicit free lists. That is, there 
 * is an array that contains pointers to the heads of free lists. The array
 * is FREE_LIST_SIZE(32) long, and each entry points to free lists with memory
 * [n^2, n^(2+1)) relative to the previous entry.
 *
 * Each block requires at least 4 words. One word is used for the header,
 * one word for the next pointer, one word for the prev pointer, and one
 * word for the footer. The header and footer are identical and use the first
 * three bits to indicate if the memory is free or allocated (1 is allocated)
 * with the remaining bits indicating the size of the block. Each next and prev
 * pointers point to the headers of the next and prev blocks, respectively.
 * If a block is allocated, then there is no next and prev pointer.
 * 
 * 
 * 
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
#define FREE_LIST_SIZE 32       /* Free_list_size를 32으로 설정 */
#define REALLOCATION_SIZE (1 << 9) /* 재 할당 사이즈?? */

#define MAX(x, y) ((x) > (y)? (x) : (y)) // 최대값 구하는 함수
#define MIN(x, y) ((x) < (y)? (x) : (y)) // 최소값 구하는 함수

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

/* Address of next and prev respecitvely. PTR must be header address */
#define GET_NEXT(ptr) (*(char **) (ptr + WSIZE))
#define GET_PREV(ptr) (*(char **) (ptr + DSIZE))

/* Address that stores the pointers to next and prev respectively */
#define GET_NEXTP(ptr) ((char *) ptr + WSIZE)
#define GET_PREVP(ptr) ((char *) ptr + 2 * WSIZE)

/* Free List 상에서 이전과 이후 블록의 포인터를 return */
#define PREC_FREEP(bp) (*(void**)(bp))         // void를 가르키는 이중포인터의 역참조 ; 이전 블록의 bp
#define SUCC_FREEP(bp) (*(void**)(bp + WSIZE)) // void를 가르키는 이중포인터의 역참조 ; 이후 블록의 bp


static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void removeBlock(void *ptr);
static void find_place(void *ptr);
int get_list_idx(size_t size);
void** is_head(void* ptr);


void* heap_listp;  // Prologue block을 가르키는 정적 전역 변수 설정
void **free_list;  // Array of pointers to the free list


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //Allocate the memory for the free list
    free_list = mem_sbrk(FREE_LIST_SIZE * WSIZE);

    //Create the initial empty heap
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0); //padding so that we have offset for 8-byte alignment
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); //header setting allocated bit to true
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); //footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); //epilogue header

    int i;
    //Set every head to NULL initially
    for(i = 0; i < FREE_LIST_SIZE; i++) { free_list[i] = NULL; }

    //extend the empty heap with a free block of CHUNKSIZE bytes
    if((extend_heap(CHUNKSIZE) ) == NULL)
        return -1;

    //Set up the heap_listp so that we can successfully check memory in mm_check
    heap_listp += WSIZE;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *             블록 할당을 brk 포인터를 증가시킴으로써 실행해줌
 *             사이즈가 0인 malloc의 경우 무시되고 free list 중 알맞은 곳에 가서 할당
 *             적절한 free space가 없을시 heap을 extend
 */
void *mm_malloc(size_t size)
{
    // if(mm_check())
    // {
    //     exit(1);
    // }

    size_t asize; //adjusted block size
    size_t extendsize; //amount to extend heap if no fit
    void *dest = NULL;

    //Ignore 0 mallocs
    if(size == 0) { return NULL; }

    //adjust block size to include overhead and alignment requirements
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    //Find the best fit
    dest = find_fit(asize);

    //If NULL is returned, we need to extend the heap
    if(dest == NULL)
    {
        extendsize = MAX(asize, CHUNKSIZE);
        if((dest = extend_heap(extendsize/WSIZE)) == NULL)
            return NULL;
    }

    //Updates the pointers
    place(dest, asize);

    return dest + WSIZE; //offset back from header
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));   // size를 얻음

    PUT(ptr, PACK(size, 0));             // bp의 헤더에 (size, 0(미할당)) 추가
    PUT(ptr, PACK(size, 0));             // bp의 footer에 (size, 0(미할당)) 추가
    void* newPtr = coalesce(HDRP(ptr));  // 앞, 뒤 블록 중 연결할 곳이 있으면 연결 

    //Place in a free list
    find_place(newPtr);
}











/*
 * coalesce - 연결
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // prev 블록의 할당여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  // next 블록의 할당여부 확인
    size_t size = GET_SIZE(HDRP(bp));                    // 현재 블록의 사이즈 확인
    void* ptr = HDRP(bp);

    /* Case 1 */   
    /* prev 할당 & next 할당 */
    /* 해당 블록만 free list에 넣음 */

    /* Case 2 */   
    if (prev_alloc && !next_alloc) {                
        removeBlock(HDRP(NEXT_BLKP(bp)));                      // free 상태였던 직후 블록을 free list에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));           /* prev 할당 & next 미할당 */
        PUT(HDRP(bp), PACK(size, 0));                    // 현재 헤더에 새로운 size 넣고
        PUT(FTRP(bp), PACK(size, 0));                    // 새로운 사이즈를 기준으로 새로운 footer에도 정보 저장
    }

    else if (!prev_alloc && next_alloc) {                /* Case 3*/
        removeBlock(HDRP(PREV_BLKP(bp)));                // 직전 블록을 free list에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));           /* prev 미할당 & next 할당 */
        // bp = PREV_BLKP(bp);                           // bp는 이전 값에 저장해놓음
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));          // 새로운 사이즈를 prev header에 넣고 저장
        PUT(FTRP(bp), PACK(size, 0));                    // 현재 footer에 새로운 size 넣고
        ptr = HDRP(PREV_BLKP(bp));                                  // ptr은 이전 값의 헤더에 저장
    }

    else if (!prev_alloc && !next_alloc) {               /* Case 4 */
        removeBlock(HDRP(PREV_BLKP(bp)));                // 직전 블록을 free list에서 제거
        removeBlock(HDRP(NEXT_BLKP(bp)));                // 다음 블록을 free list에서 제거

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +   
            GET_SIZE(HDRP(NEXT_BLKP(bp)));               /* prev 미할당 & next 미할당 */
            
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));          // 이전 header에 size 넣고
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));          // 이후 footer에 size 넣음


        ptr = HDRP(PREV_BLKP(bp));      
    }

    PUT(GET_NEXTP(ptr), NULL);
    PUT(GET_PREVP(ptr), NULL);
    
    return ptr;
}

/* 
 * Find_fit
 */
static void *find_fit(size_t asize){
    /* First-fit search */
    void *bp;
    void *ptr;

    // free list의 맨 마지막은 Prologue Block -> HDR : (16/1)
    for (bp = free_list; GET_ALLOC(HDRP(bp)) !=  1; bp = SUCC_FREEP(bp)){ 
        if (asize <= GET_SIZE(HDRP(bp))) { // Implicit와 달리 할당되어있는지 여부는 따로 체크할 필요 없음
            ptr = HDRP(bp);
            return bp; // 조건에 맞는 블록이 있다면 다음 bp값을 할당해줌
        }
    }


    return NULL; /* No Fit */
}

void** is_head(void* ptr)
{
    void* head;
    int i;
    for(i = 0; i < FREE_LIST_SIZE; i++)
    {
        head = free_list[i];
        if(ptr == head) { return &free_list[i]; }
    }

    return NULL;
}



/* 
 * place 함수 : asize를 저장해주는 함수
 */
static void place(void *ptr, size_t asize)      
{   
    size_t csize = GET_SIZE(ptr);         // fit에서 찾아진 bp의 사이즈를 csize에 할당 (현재 할당되어 있지 않은 상태)
    removeBlock(ptr);                           // 할당되어질 블록이기 때문에 free_list에서 삭제

    // 블록을 자를 수 있다면 
    if((csize - asize) >= (2*DSIZE)) {            // (비어있는 블록 - 저장해야하는 사이즈) >= 16 : 필요한 메모리 주소만큼 사용하고 나머지 여분의 주소 사용 가능
        PUT(ptr, PACK(asize, 1));                 // HDRP(bp)에 asize만큼 저장
        PUT(FTRP(ptr + WSIZE), PACK(asize, 1));   // FTRP(bp)에 asize만큼 저장 (헤더 자리에)
        
        
        void *next_bp = NEXT_BLKP(ptr + WSIZE);         // bp는 다음 bp를 뜻함 (다음 bp가 될 자리) : #define 참고
        
        // 다음 블록을 설정
        PUT(HDRP(next_bp), PACK(csize-asize, 0));      // 다음 bp는 0을 뜻함
        PUT(FTRP(next_bp), PACK(csize-asize, 0));      // 다음 bp의 footer는 0을 뜻함 

        // NEXT와 PREV 값에 NULL을 넣음
        PUT(next_bp, NULL);
        PUT(next_bp + WSIZE, NULL);

        void* next_ptr = HDRP(next_bp);

        // next_ptr이 들어갈 수 있는 free_list를 찾음
        find_place(next_ptr);
    }

    else {
        PUT(ptr, PACK(csize, 1));
        PUT(FTRP(ptr + WSIZE), PACK(csize, 1)); 
    }
}

/*
 * find_place : 현재의 ptr이 놓일 적절한 위치를 찾는 함수 (free_list를 위해서)
 */

void find_place(void *ptr){
  int listIdx = get_list_idx(GET_SIZE(ptr));
  void *current = free_list[listIdx];

  // Case 1 : Current is NULL => ptr 값이 free_list의 최초값이 됨
  if(current == NULL){
    free_list[listIdx] = ptr;

    return;
  }

  // Case 2 : 바로 뒤에 하나의 값 밖에 없을 때 & 바로 뒤의 값이 ptr보다 클 때
  if(ptr < current)
  {
    PUT(GET_NEXTP(ptr), current);  // ptr의 다음 포인터는 current
    // PUT(GET_NEXT(ptr), *current);  // ptr의 다음 값은 current의 역참조 
    PUT(GET_PREVP(current), ptr);  // current의 이전 포인터는 ptr
    // PUT(GET_PREV(current), *ptr);  // current의 이전 값은 ptr의 역참조
    free_list[listIdx] = ptr;          

    return;
  }

  // Case 3: ptr이 Free list의 중간에 있어야할 때 (current < ptr < next)
  void *next; 
  while ((next = GET_NEXT(current)) != NULL)
  {
    if (ptr < next) {
      PUT(GET_NEXTP(current), (uint)ptr);
      // PUT(GET_NEXT(current), *ptr);

      PUT(GET_PREVP(next), (uint)ptr);
      // PUT(GET_PREV(next), *ptr);

      PUT(GET_PREVP(ptr), (uint)current);
      // PUT(GET_PREV(ptr), *current);

      PUT(GET_NEXTP(ptr), (uint)next);
      // PUT(GET_NEXT(ptr), *next);

      return;
    }

    current = GET_NEXT(current);
  }

  // 가장 마지막에 위치시킴
  PUT(GET_NEXTP(current), (uint)ptr);
  // PUT(GET_NEXT(current), *ptr);

  PUT(GET_PREVP(ptr), (uint)current);
  // PUT(GET_PREV(ptr), *current);
}

/* 
 * removeBlock : 할당되거나 연결되는 가용 블록을 free list에서 없앰
 */
static void removeBlock(void *ptr){
    // PREV가 NULL이 아닐 때 이전 포인터의 다음 값은 다음 포인터로 잡음
    if(GET_PREV(ptr) != NULL){
      PUT(GET_NEXTP(GET_PREV(ptr)), (uint)GET_NEXT(ptr));
    }

    // PREV가 NULL일 때는 free_list의 첫번째 값을 GET_NEXT로 잡음
    else
    {
        void** h = is_head(ptr);
        PUT(h, (uint)GET_NEXT(ptr)); //no previous implies it was a head
    }

    // NEXT가 NULL이 아닐 때 NEXT의 PREV는 PREV로 잡음
    if(GET_NEXT(ptr) != NULL){
      PUT(GET_PREVP(GET_NEXT(ptr)), (uint)GET_PREV(ptr));
    }

    PUT(GET_NEXTP(ptr), (uint)NULL); // 현재 ptr의 다음 포인터 NULL으로 처리
    PUT(GET_PREVP(ptr), (uint)NULL); // 현재 ptr의 이전 포인터 NULL으로 처리
}


/* extend_heap */
/* 새 가용 블록으로 힙 확장하기 (워드 단위 메모리를 인자로 받아 힙을 늘려줌) */
static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment (double-words aligned)*/
    /* 요청한 크기를 인접 2워드의 배수(8바이트)로 반올림 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 1 : True -> odd number, 2: False -> even number
    if ((long)(bp = mem_sbrk(size)) == -1) // mem_sbrk : incr까지 늘리고 block의 최초 값을 return 해줌 (mem_brk : void pointer)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));            /* Free block header */
    PUT(bp, (uint)NULL);                           /* NEXT */
    PUT(bp + WSIZE, (uint)NULL);                   /* PREV */
    PUT(FTRP(bp), PACK(size, 0));            /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));    /* New epilogue header */


    /* 포인터를 각 블록의 맨 앞으로 배치 */
    /* 매크로의 영향 : #define GET_NEXT(ptr) (*(char **) (ptr + WSIZE)) */
    void *ptr = HDRP(bp);

    ptr = coalesce(bp); // -> coalesce로 이동
    
    // Find place for ptr
    find_place(ptr);

    return ptr;
}

/*
 * get_list_idx :  Free_list의 idx를 찾기 위한 함수
 */
int get_list_idx(uint size){
  int idx = 0; // idx가 0부터 시작

  // 비트 연산을 활용해서 size를 2의 0승까지 줄임
  while ((size = size >> 1) > 1)
  {
    idx++;
  }

  return idx;
}


/* ==================== 보류 ==================== */
/*
 * 
 * mm_realloc - Changes the size of a previously allocated block
 * 포인터가 NULL 값일 때   = malloc
 * 포인터의 사이즈가 0일 때  = free
 *   
 * current block이 충분히 크다면 그대로 할당 및 pointer를 return
 * current block이 충분히 크지 못하다면 다음 블록이 비어있는지 확인 후 연결하여 할당
 * 
 * 위의 모든 작업들이 수행 불가능할 시 새로운 메모리를 할당하고 old ptr과 대체 후 새롭게 할당함
 * 새로운 메모리의 경우 충분한 buffer를 가지고 있음
 * 
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t asize; //adjusted size to include header and footer
    void *oldptr = ptr;
    void *newptr;
    void *ptrH = HDRP(ptr); //pointer to the header

    //If ptr is null, we simply allocate
    if(ptr == NULL) { return mm_malloc(size); }
    
    //Free the pointer if the size is 0
    if(size == 0)
    {
        mm_free(ptr);
        return ptr;
    }

    //adjust block size to include overhead and alignment requirements
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    size_t new_size;
    size_t old_size = GET_SIZE(ptrH);
    void* next = HDRP(NEXT_BLKP(ptr));

    //Check to see if we can fit in the current block
    if(old_size >= asize)
    {
        return ptr;
    }

    //If the next block is empty, check and see if it's big enough 
    if(!GET_ALLOC(next))
    {
        new_size = old_size + GET_SIZE(next);
        if(new_size >= asize) //Can simply use the next block for allocation
        {
            //Set the next pointer to allocated and take out of free list
            removeBlock(next);

            PUT(ptrH, PACK(new_size, 1));
            PUT(FTRP(next + WSIZE), PACK(new_size, 1));

            return ptr;
        }
    }

    //Otherwise we have to allocate new memory
    new_size = asize + REALLOCATION_SIZE;
    newptr = mm_malloc(new_size);
    if(newptr == NULL) { return NULL; }

    //Copy the new memory
    memcpy(newptr, oldptr, MIN(GET_SIZE(HDRP(oldptr)), size));

    //Free the old memory
    mm_free(oldptr);
    return newptr;
}

/*
 * mm_check - Function that goes through the entire heap, checks that every block that is free
 *            is in a free list, every block that is allocated is not in a free list, and makes sure
 *            that, if it is free, the next and prev pointers are correct by looking at the values 
 *            of the prev and next and making sure they are pointing to the current ptr.
 *
 *            Note: this is only used for debugging. All occurances in the code that is run
 *            should be commented out.
 */
int mm_check(void)
{
    void* ptr = heap_listp;

    //Go until we reach the epilogue header
    while(GET_SIZE(ptr) != 0)
    {
        //IF the current block is unallocated
        if(GET_ALLOC(ptr) == 0) 
        {
            //Make sure it's in the free list
            if(!in_free_list(ptr)) 
            { 
                printf("%p not in free list but is unallocated\n", ptr);
                return 1; 
            }

            //Make sure that its prev is pointing to this ptr
            if(GET_PREV(ptr) != NULL)
            {
                if(GET_NEXT(GET_PREV(ptr)) != ptr)
                {
                    printf("The next's previous isn't this\n");
                    return 1;
                }
            }

            //Make sure that its next is pointing to this ptr
            if(GET_NEXT(ptr) != NULL)
            {
                if(GET_PREV(GET_NEXT(ptr)) != ptr)
                {
                    printf("The prev's next isn't this\n");
                    return 1;
                }
            }
        }

        //Otherwise it's allocated
        else
        {
            //Make sure it isn't in a free list
            if(in_free_list(ptr)) 
            { 
                printf("%p in free list but is allocated\n", ptr);
                return 1; 
            }
        }

        //Increment to the next block
        ptr = HDRP(NEXT_BLKP(ptr + WSIZE));
    }

    return 0;
}

/*
 * in_free_list - Checks to ensure that the passed in pointer is contained 
 *                within the free list.
 *
 */
int in_free_list(void* ptr)
{
    void* current;

    int i;
    for(i = 0; i < FREE_LIST_SIZE; i++)
    {
        current = free_list[i];

        while(current != NULL)
        {
            if(current == ptr) { return 1; }
            current = GET_NEXT(current);
        }
    }

    return 0;
}
