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
 * 코드 참고 : https://github.com/ryanfarr01/Malloc-Lab/blob/master/mm.c
 * 코드 설명 :
 * - Segregated Explicit Lists (분리 가용)
 * - Free List의 배열 길이는 32
 * - 각 블록은 최소 4워드로 지정되어 있으며 헤더, NEXT, PREV, FOOTER로 설정되어 있음
 * - NEXT와 PREV는 다음 혹은 이전의 헤더를 가르키며 할당된 블록은 헤더가 없음  
 * 
 * ----------------------------------------------------------------------------
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
#define HDRP(bp)    ((char *)(bp) - WSIZE)                          // 헤더의 포인터(주소) 반환
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)     // 헤더의 포인터(주소) 반환

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 다음블록의 헤더값 리턴 (bp + bp - 헤더)  : | header + payload + footer | | header + payload + .. |
                                                                       // 블록의 사이즈는 header로 가서 알아봄(bp - WSIZE)     ^
                                                                       //                                   -> | header + payload + footer | | header + payload + .. |
                                                                       //                                                                             ^
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 이전블록의 헤더값 리턴 (bp - (bp - DSIZE)) : | header + payload + footer | | header + payload + .. |
                                                                       //                                                                                ^
                                                                       //                                      -> | header + payload + footer | | header + payload + .. |
                                                                       //                                                  ^                                                                                                      ^

/* 다음과 이전의 주소값. PTR must be header address */
#define GET_NEXT(ptr) (*(char **) (ptr + WSIZE))
#define GET_PREV(ptr) (*(char **) (ptr + DSIZE))

/* 다음과 이전을 가르키는 포인터의 주소값 */
#define GET_NEXTP(ptr) ((char *) ptr + WSIZE)
#define GET_PREVP(ptr) ((char *) ptr + DSIZE)

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
 * mm_init - malloc 패키지를 초기화 해주는 함수
 */
int mm_init(void)
{   
    /* Allocate the memory for the free list */
    // 32 사이즈의 free_list를 설정 (가장 첫번째 값에 대한 pointer가 return 됨)
    free_list = mem_sbrk(FREE_LIST_SIZE * WSIZE); 

    /* Create the initial empty heap */
    /* 초기 설정은 4 words로 설정 => Padding | Prologue header | Prologue footer | Epilogue block header */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) 
        return -1;
    
    PUT(heap_listp, 0);                           // Padding         | 0 
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));  // Prologue Header | 8 / 1
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));  // Prologue Footer | 8 / 1
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));      // Epilogue Header | 1 / 0

    // free_list의 첫 값을 모두 NULL 값으로 만들기
    for (int i = 0; i < FREE_LIST_SIZE; i++){
      free_list[i] = NULL;
    }

    // Extend the Empty Heap with a free block of CHUNKSIZE bytes
    if((extend_heap(CHUNKSIZE)) == NULL)
      return -1;

    // Set up the head_listp to the location of the next block of the initial block
    heap_listp += WSIZE;

    return 0;
}   

/* 
 * mm_malloc - 블록 할당을 brk 포인터를 증가시킴으로써 실행해줌
 *             사이즈가 0인 malloc의 경우 무시되고 free list 중 알맞은 곳에 가서 할당
 *             적절한 free space가 없을시 heap을 extend
 */
void *mm_malloc(size_t size){
    size_t asize;         /* Adjusted block size : 조정된 사이즈 */
    size_t extendsize;    /* Amount to extend heap if no fit */
    void *dp = NULL;      


    /* Fake Request 걸러내기 */
    if (size == 0)
        return NULL;
    
    /* 블록을 최소 16바이트로 늘려줌 (CSAPP 823page) */
    if (size <= DSIZE)
        asize = 2*DSIZE; // 최소 16바이트, 8바이트는 header + footer 만의 최소 블록 크기이므로, 그 다음 8의 배수인 16바이트로 설정
    else                 // 크면
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); // 7을 더함으로써 size = 1일 때 최소 16바이트 나올 수 있음 
                                                                  // 32비트에서는 8의 배수인 블록을 리턴

    // find the best fit
    dp = find_fit(asize);

    /* asize에 맞는 bp 찾기  */
    if (dp == NULL) {  

    /* No fit found Get more memory and place the block (asize 또는 CHUNKSIZE 만큼 가용 리스트 범위 넓혀줌) */
    // 맞는 크기의 메모리가 없으면 asize 혹은 CHUNKSIZE 보다 크게 heap의 크기를 늘려줌
    extendsize = MAX(asize, CHUNKSIZE); // asize가 CHUNKSIZE보다 큰 경우는 size가 요구하는 8바이트 기준의 크기가 매우 큼을 의미
    if ((dp = extend_heap(extendsize / WSIZE)) == NULL) // (extend / 8 word)
        return NULL;
    }

    place(dp, asize);
    return dp + WSIZE;
}



/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));         // size를 얻음

    PUT(HDRP(ptr), PACK(size, 0));             // bp의 헤더에 (size, 0(미할당)) 추가
    PUT(HDRP(ptr), PACK(size, 0));             // bp의 footer에 (size, 0(미할당)) 추가
    void* newPtr = coalesce(HDRP(ptr));  // 앞, 뒤 블록 중 연결할 곳이 있으면 연결 

    //Place in a free list
    find_place(newPtr);
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


/* 
 * Find_fit
 */
static void *find_fit(size_t asize)
{
    //Start at its memory bracket
    int listIndex = get_list_idx(asize);
    void *bp;

    //Search over list to find 
    int i;
    for(i = listIndex; i < FREE_LIST_SIZE; i++)
    {
        //get head of current list
        bp = free_list[i];

        //Go through list until we find a block
        while(bp != NULL)
        {
            if(GET_SIZE(bp) >= asize)
            {
                return bp;
            }

            bp = GET_NEXT(bp);
        }
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
 * coalesce - 연결
 */
static void *coalesce(void *ptr) {
    //Get the headers of next and previous
    void* nextH = HDRP(NEXT_BLKP(ptr + WSIZE));
    void* prevH = HDRP(PREV_BLKP(ptr + WSIZE));

    //Get teh sizes of all three
    size_t prev_alloc = GET_ALLOC(prevH);
    size_t next_alloc = GET_ALLOC(nextH);
    size_t size = GET_SIZE(ptr);

    if(prev_alloc && next_alloc) { /*Case 1*/ }
    else if(prev_alloc && !next_alloc) 
    { //Case 2
        removeBlock(nextH);

        size += GET_SIZE(nextH);
        PUT(ptr, PACK(size, 0));
        PUT(FTRP(ptr + WSIZE), PACK(size, 0));
    } 
    else if(!prev_alloc && next_alloc) 
    { //Case 3
        removeBlock(prevH);
        
        size += GET_SIZE(prevH);

        PUT(HDRP(PREV_BLKP(ptr + WSIZE)), PACK(size, 0));
        PUT(FTRP(ptr + WSIZE), PACK(size, 0));

        ptr = HDRP(PREV_BLKP(ptr + WSIZE));
    }
    else 
    { //Case 4
        removeBlock(prevH);
        removeBlock(nextH);

        size += GET_SIZE(prevH) + GET_SIZE(nextH);

        PUT(HDRP(PREV_BLKP(ptr + WSIZE)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr + WSIZE)), PACK(size, 0));

        ptr = HDRP(PREV_BLKP(ptr + WSIZE));
    }

    PUT(GET_NEXTP(ptr), (uint)NULL); //place null for next
    PUT(GET_PREVP(ptr), (uint)NULL); //place null for prev

    return ptr;
}

/*
 * is_head(void *ptr) : free_list의 가장 head 값을 보기 위한 함수
 */
void** is_head(void* ptr){
  void* head;
  int i;
  for(i = 0; i < FREE_LIST_SIZE; i++){
    head = free_list[i];
    if(ptr == head){
      return &free_list[i];
    }
  }

  return NULL;

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
      PUT(GET_NEXTP(GET_PREV(ptr)), GET_NEXT(ptr));
    }

    // PREV가 NULL일 때는 free_list의 첫번째 값을 GET_NEXT로 잡음
    else
    {
        void** h = is_head(ptr);
        PUT(h, (uint)GET_NEXT(ptr)); //no previous implies it was a head
    }

    // NEXT가 NULL이 아닐 때 NEXT의 PREV는 PREV로 잡음
    if(GET_NEXT(ptr) != NULL){
      PUT(GET_PREVP(GET_NEXT(ptr)), GET_PREV(ptr));
    }

    PUT(GET_NEXTP(ptr), NULL); // 현재 ptr의 다음 포인터 NULL으로 처리
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

    ptr = coalesce(ptr); // -> coalesce로 이동
    
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
