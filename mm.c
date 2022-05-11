/*
implicit -> next_fit
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
    "team_4",
    /* First member's full name */
    "ChoJinWoo",
    /* First member's email address */
    "whwlsdn96@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

// 블록을 8의 배수로 할당하므로 자주 연산에 사용되기 때문에 상수를 정의해서 사용
#define ALIGNMENT 8

// 블록이 8의 배수로 할당되도록 연산해준다. size가 어떤 것이 들어오든 8의 배수를 반환한다.
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// sizeof는 데이터 타입의 크기를 반환하고 size_t 는 int형과 같이 4를 반환한다.
// 이 매크로는 블록의 사이즈를 명시하며 결과적으로 8의 값을 나타낸다.
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 블록의 사이즈를 가르치고 있는 포인터에 접근한다.
#define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))

#define WSIZE           4		// word 크기를 지정.
#define DSIZE           8		// wouble word의 크기 지정.
#define CHUNKSIZE       (1<<12)	// 초기 Heap의 크기를 설정해 준다.(4096)
#define OVERHEAD        8		// header + footer의 사이즈.

#define MAX(x, y) ((x) > (y)? (x) : (y))

// size와 alloc(a)의 값을 한 word로 묶는다.
// 편리하게 header와 footer에 저장하기 위함.
#define PACK(size, alloc) ((size) | (alloc))

//포인터 p가 가리키는 곳의 한 word의 값을 읽어온다.
#define GET(p)          ( *(unsigned int *)(p) )

//포인터 p가 가르키는 곳의 한 word의 값에 val을 저장한다.
#define PUT(p, val)     ( *(unsigned int *)(p) = (val) )

//포인터 p가 가리키는 곳에서 한 word를 읽고 하위 3bit를 버린다.
//Header에서 block size를 읽기위함.
#define GET_SIZE(p)     (GET(p) & ~0x7)

//포인터 p가 가리키는 곳에서 한 word를 읽고 최하위 1bit를 가져온다.
//block의 할당여부 체크에 사용된다.
//할당된 블록이라면 1, 아니라면 0.
#define GET_ALLOC(p)    (GET(p) & 0x1)

//주어진 포인터 bp의 header의 주소를 계산한다.
#define HDRP(bp)        ((char *)(bp) - WSIZE)

//주어진 포인터 bp의 footer의 주소를 계산한다.
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//주어진 포인터 bp를 이용하여 다음 블록의 주소를 얻어 온다.
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE( ((char *)(bp) - WSIZE)) )

//주어진 포인터 bp를 이용하여 이전 블록의 주소를 얻어 온다.
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE( ((char *)(bp) - DSIZE)) )


static char *heap_listp = 0;	// 처음 first block의 포인터를 선언.
void *heap_cur;		// 현재 block의 위치를 가리키는 포인터.


int mm_init(void);
void *mm_malloc(size_t size);
void *mm_realloc(void *oldptr, size_t size);
void free(void *bp);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // 초기 empty heap 생성
    // heap_lis - 새로 생성되는 heap 영역의 시작 address
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                          // 정렬을 위한 의미없는 값
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += DSIZE;                      

    heap_cur = heap_listp;

    // 생성된 empty heap을 free block으로 확장

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)   // WSIZE로 align 되어있지 않았으면 에러
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
 // 블록 할당
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      

    /* $begin mmmalloc */
    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)                                          
        asize = 2*DSIZE;                                        // footer 4, header 4, pay 8 총 16 size
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //line:vm:mm:sizeadjust3

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {  
        place(bp, asize);                  // asize 만큼 현재 bp 가르키는 곳에 메모리 위치
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);                 
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
        return NULL;                                 
    place(bp, asize);           // asize 만큼 현재 bp 가르키는 곳에 메모리 위치
    heap_cur = NEXT_BLKP(bp);    // 현재 힙 NEXT                      
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
 // 메모리 해제
void mm_free(void *bp)
{
    if (bp == 0)
        return;
    size_t size = GET_SIZE(HDRP(bp));   // bp의 헤더에서 block size를 읽어옴

    // 실제로 데이터를 지우는 것이 아니라 header와 footer의 최하위 1비트(할당된 상태)만을 수정
    PUT(HDRP(bp), PACK(size, 0));       // bp의 header에 block size와 alloc = 0 저장
    PUT(FTRP(bp), PACK(size, 0));       // bp의 footer에 block size와 alloc = 0 저장
    coalesce(bp);                       // 주위 빈 블럭이 있을 시 병합
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;
    
    if (size == 0){
        free(ptr);
        return 0;
    }

    if (ptr == NULL)
        return mm_malloc(size);
    
    newptr = mm_malloc(size);

    if (!newptr)
        return 0;
    
    oldsize = GET_SIZE(HDRP(ptr));

    if (size < oldsize)
        oldsize = size;
    memcpy(newptr, ptr, oldsize);
    mm_free(ptr);

    return newptr;
}

// free block 검색
static void *find_fit(size_t asize)
{
    char *p;

    // 힙의 현재 위치에서 부터 find
    for (p = heap_cur; GET_SIZE(HDRP(p)) > 0; p = NEXT_BLKP(p))
        if (!(GET_ALLOC(HDRP(p))) && (asize <= GET_SIZE(HDRP(p))))
            return p;
    
    return NULL;
}


// 빈 공간을 매꾸는 함수
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     // 이전블럭 할당 여부 0 = No, 1 = YES
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     // 다음블럭 할당 여부 0 = No, 1 = YES
    size_t size = GET_SIZE(HDRP(bp));       // 현재 블럭의 size

/*  case 1 : 이전 블럭, 다음 블럭 최하 bit 둘 다 1 (할당)
    블럭 병합 없이 bp return
*/
    if (prev_alloc && next_alloc) {            
        return bp;
    }
/*  case 2 : 이전 블럭 최하위 bit 1(할당, 다음 블럭 최하위 bit 0 (비할당))
    다음 블럭과 병합한 뒤 bp return
*/
    else if (prev_alloc && !next_alloc) {      
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }
/*  case 3 : 이전 블럭 최하위 bit 0(비할당), 다음 블럭 최하위 bit 1 (할당)
    이전 블럭과 병합한 뒤 새로운 bp return
*/
    else if (!prev_alloc && next_alloc) {      
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
/*  case 4 : 이전 블록 최하위 bit 0(비할당), 다음 블럭 최하위 bit 0(비할당)
    이전 블럭, 현재 블럭, 다음 블럭 모두 병합한 뒤 새로운 bp return
*/
    else {                                     
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    if (heap_cur > bp)
        heap_cur = bp;

    return bp;  // 병합된 블럭의 주소 bp return
}

// 요청받은 size의 빈 블럭을 만든다
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1)  
        return NULL;                                        

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 

    /* Coalesce if the previous block was free */
    return coalesce(bp);                                          
}

// 해당 위치에 사이즈만큼 메모리를 위치 시킴
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   

    if ((csize - asize) >= (2*DSIZE)) { 
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}