#include <stdio.h>
#include <stddef.h>

typedef enum {false, true} Bool;

typedef struct memory_block
{
    //Block Header - Metadata
    size_t size;
    struct memory_block* next;
    struct memory_block* prev;

    //User Data
    void* payload;
}Block;

#define HEAPSIZE 8000

//Start of explicit free list.
static Block* start = NULL;

//Latest addition to the free list.
static Block* top = NULL;

static char heap[HEAPSIZE];

static char* heap_top = heap;
static char* heap_end = heap + HEAPSIZE;

void heap_free(void* payload);
void print_free_list();
void print_heap();
void allocate_block(Block* block);
void deallocate_block(Block* block);
void* heap_alloc(size_t size);
void* get_block(void* payload);
void* memory_request(size_t size);
Block* find_block(size_t size);
Block* split_block(Block* block, size_t size);
Block* list_allocate(Block* block, size_t size);
Block* coalesce(Block* block);
Bool can_split(Block* block, size_t size);
Bool can_coalesce(Block* block);
Bool is_allocated(Block* block);
size_t get_block_size(Block* block);
size_t align(size_t size);
size_t alloc_size(size_t size);

void test_one()
{
    void* first = heap_alloc(100);
    void* second = heap_alloc(250);
    heap_free(second);
    print_free_list();
    void* third = heap_alloc(127);
    print_heap();
    print_free_list();
    heap_free(first);
    print_free_list();
    heap_free(third);
    print_free_list();
}   

int main()
{
    test_one();
}

void heap_free(void* payload)
{
    Block* block = get_block(payload);

    //only free the block if its allocated.
    if(is_allocated(block) != false)
    {
        deallocate_block(block);
        //free list is empty.
        if(start == NULL)
        {
            start = block;
            top = block;
            block->next = NULL;
            block->prev = NULL;
        }
        else
        {
            Block* traverser = start;

            //Inserting block at the start of the free list.
            if((traverser != NULL) && (traverser > block))
            {
                block->next = traverser;
                traverser->prev = block;
                block->prev = NULL;
                start = block;
            }
            else
            {
                Bool update_top = false;
                //finding the location to insert the block.
                while((traverser->next != NULL) && (traverser->next < block))
                {
                    traverser = traverser->next;
                }
                //if insertion is to be done at the end of the free list, top needs to point to the new block
                if(traverser->next == NULL)
                    update_top = true;
                if(traverser != NULL)
                {
                    block->next = traverser->next;
                    if(traverser->next != NULL)
                        traverser->next->prev = block;
                    traverser->next = block;
                    block->prev = traverser;
                    if(update_top == true)
                        top = block;
                }
            }
        }
        if(can_coalesce(block) == true)
        {
            coalesce(block);
        }     
    }
}

void* heap_alloc(size_t size)
{
    size = align(size);

    Block* block;
    
    block = find_block(size);
    if(block == NULL)
    {
        block = memory_request(size);
        block->size = size;
        block->payload = (void*)((char*) block + (sizeof(Block)) - (sizeof(void*)));
        block->next = NULL;
        block->prev = NULL;
    }
    
    allocate_block(block);

    return block->payload;
}

//aligns to the size of the machine word. Alignment is done for efficiency.
//The last bit can be used to store the allocation status of the block.
size_t align(size_t size)
{
    return ((size + sizeof(void*) - 1) & ~(sizeof(void*) - 1));
}

size_t alloc_size(size_t size)
{
    return (size + sizeof(Block));
}

void* memory_request(size_t size)
{
    size_t required_size = alloc_size(size);
    void* ret_memory_address = NULL;
    if(((char*)heap_top + required_size) > heap_end)
    {
        printf("Insufficient memory.\n");
    }
    else
    {
        ret_memory_address = heap_top;
        heap_top += required_size;
    }
    return ret_memory_address;
}

//Get pointer to block from user payload pointer.
void* get_block(void* payload)
{
    return (void*)(((char*)(payload) - sizeof(Block)) + sizeof(payload));
}

//Search for a block of suitable size in the free list.
Block* find_block(size_t size)
{
    Block* traverser = start, *block;
    Bool found = false;
    while(traverser != NULL && (found == false))
    {
        //look for a free block of size at least "size".
        if(get_block_size(traverser) >= size)
        {
            block = traverser;
            found = true;
        }
        traverser = traverser->next;
    }
    //If we traverse the entire list and no free block satisfies the requirement, we return NULL.
    if(traverser == NULL && (found == false))
    {
        block = NULL;
    }
    if(block != NULL)
        block = list_allocate(block, size);
    
    return block;
}

//Splits the passed block into 2 parts.
Block* split_block(Block* block, size_t size)
{
    //Making the free block.
    Block* free_block = (Block*)((char*)block + sizeof(Block) + size);
    deallocate_block(free_block);
    free_block->size = get_block_size(block) - size;

    //Generic Case
    if((block->next != NULL) && (block->prev != NULL))
    {
        block->prev->next = free_block;
        block->next->prev = free_block;
    }
    //Block being split is the last block in the free list.
    else if(block->prev != NULL)
    {
        top = free_block;
        block->prev->next = free_block;
    }
    //Block being split is the first block in the free list.
    else if(block->next != NULL)
    {
        start = free_block;
        block->next->prev = free_block;
    }
    //Block being split is the only block in the free list.
    else
    {
        start = free_block;
        top = free_block;

    }
    free_block->next = block->next;
    free_block->prev = block->prev;
    block->next = NULL;
    block->prev = NULL;
    block->size = size;
    allocate_block(block);
    block->payload = (void*)((char*)block + sizeof(Block) - sizeof(void*));
    return block;
}

//Checks if a block can be split into 2 blocks.
Bool can_split(Block* block, size_t size)
{
    Bool retval = false;
    if(get_block_size(block) > (size + sizeof(Block)))
    {
        retval = true;
    }
    return retval;
}

//Called when a block is found in the searching algorithm. Takes care of splitting the block.
Block* list_allocate(Block* block, size_t size)
{
    if(can_split(block, size) == true)
    {
        block = split_block(block, size);
    }
    else
    {
        //Generic Case
        if((block->prev != NULL) && (block->next != NULL))
        {
            block->prev->next = block->next;
            block->next->prev = block->prev;
        }
        //block is the last block in the free list. Need to update top.
        else if(block->prev != NULL)
        {
            top = block->prev;
            block->prev->next = block->next;
        }
        //block is the first block in the free list. Need to update start.
        else if(block->next != NULL)
        {
            start = block->next;
            block->next->prev = block->prev;
        }
        //only one block is present in the free list.
        else
        {
            start = NULL;
            top = NULL;
        }
        block->next = NULL;
        block->prev = NULL;
        allocate_block(block);
    }
    return block;
}

Bool can_coalesce(Block* block)
{
    Bool retval = false;
    if((is_allocated(block) == false) && (((block->next != NULL) && (block->next == (Block*)((char*) block + sizeof(Block) + get_block_size(block))) && (is_allocated(block->next) == false)) || ((block->prev != NULL) && (block == (Block*)((char*)(block->prev) + sizeof(Block) + (get_block_size(block->prev)))) && (is_allocated(block->prev) == false))))
    {
        retval = true;
    }
    return retval;
}

Block* coalesce(Block* block)
{
    Bool update_top = false;
    //both the neighbouring blocks can be coalesced.
    if((block->next != NULL) && (block->prev != NULL))
    {
        if(block->next == top)
        {
            update_top = true;
        }
        block->prev->size += (get_block_size(block) + get_block_size(block->next));
        block->prev->next = block->next->next;
        block = block->prev;
        if(update_top == true)
        {
            top = block;
        }
    }
    //next block can be coalesced.
    else if(block->next != NULL)
    {
        if(block->next == top)
        {
            update_top = true;
        }
        int new_size = get_block_size(block) + get_block_size(block->next);
        block->size = new_size;
        block->next = block->next->next;
        if(update_top == true)
        {
            top = block;
        }
    }
    //prev block can be coalesced.
    else
    {
        if(block == top)
        {
            update_top = true;
        }
        int new_size = get_block_size(block->prev) + get_block_size(block);
        block->prev->size = new_size;
        block->prev->next = block->next;
        block = block->prev;
        if(update_top == true)
        {
            top = block;
        }
    }
    return block;
}

void print_free_list()
{
    printf("\nPrinting free list.\n");
    Block* traverser = start;
    if(traverser == NULL)
        printf("Free List is empty.\n");
    while(traverser != NULL)
    {
        printf("block->allocated : %d, block->size : %d, block address : %p\n", is_allocated(traverser), get_block_size(traverser), traverser);
        traverser = traverser->next;
    }
}

Bool is_allocated(Block* block)
{
    Bool allocated = false;
    if(block->size & 1)
    {
        allocated = true;
    }
    return allocated;
}

void deallocate_block(Block* block)
{
    block->size = (block->size) & ~1;
}

void allocate_block(Block* block)
{
    block->size = (block->size | 1);
}

size_t get_block_size(Block* block)
{
    return (block->size & ~1);
}

void print_heap()
{
    printf("\nPrinting heap.\n");
    Block* traverser = (Block*) heap;
    while(is_allocated(traverser) == true)
    {
        printf("block->allocated : %d, block->size : %d, block address : %p\n", is_allocated(traverser), get_block_size(traverser), traverser);
        traverser = (Block*)((char*)traverser + sizeof(Block) + get_block_size(traverser));
    }
}