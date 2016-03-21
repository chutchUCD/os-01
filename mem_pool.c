/*
 * Created by Ivo Georgiev on 2/9/16.
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h> // for perror()

#include "mem_pool.h"

/*************/
/*           */
/* Constants */
/*           */
/*************/
/*
GCC- Would not accept non-constant initializers, so I made these values constant initializers.
*/

static const float      MEM_FILL_FACTOR                 = 0.75;
static const unsigned   MEM_EXPAND_FACTOR               = 2;

static const unsigned   MEM_POOL_STORE_INIT_CAPACITY    = 20;
static const float      MEM_POOL_STORE_FILL_FACTOR      = 0.75;
static const unsigned   MEM_POOL_STORE_EXPAND_FACTOR    = 2;

static const unsigned   MEM_NODE_HEAP_INIT_CAPACITY     = 40;
static const float      MEM_NODE_HEAP_FILL_FACTOR       = 0.75;
static const unsigned   MEM_NODE_HEAP_EXPAND_FACTOR     = 2;

static const unsigned   MEM_GAP_IX_INIT_CAPACITY        = 40;
static const float      MEM_GAP_IX_FILL_FACTOR          = 0.75;
static const unsigned   MEM_GAP_IX_EXPAND_FACTOR        = 2;

/*
#define     MEM_FILL_FACTOR                   0.75
#define     MEM_EXPAND_FACTOR                 2

#define     MEM_POOL_STORE_INIT_CAPACITY      20
#define     MEM_POOL_STORE_FILL_FACTOR        MEM_FILL_FACTOR
#define     MEM_POOL_STORE_EXPAND_FACTOR      MEM_EXPAND_FACTOR

#define     MEM_NODE_HEAP_INIT_CAPACITY       40
#define     MEM_NODE_HEAP_FILL_FACTOR         MEM_FILL_FACTOR
#define     MEM_NODE_HEAP_EXPAND_FACTOR       MEM_EXPAND_FACTOR

#define     MEM_GAP_IX_INIT_CAPACITY          40
#define     MEM_GAP_IX_FILL_FACTOR            MEM_FILL_FACTOR
#define     MEM_GAP_IX_EXPAND_FACTOR          MEM_EXPAND_FACTOR
*/

/*********************/
/*                   */
/* Type declarations */
/*                   */
/*********************/
typedef struct _node {
    alloc_t alloc_record;
    unsigned used;
    unsigned allocated;
    struct _node *next, *prev; // doubly-linked list for gap deletion
} node_t, *node_pt;

typedef struct _gap {
    size_t size;
    node_pt node;
} gap_t, *gap_pt;

typedef struct _pool_mgr {
    pool_t pool;
    node_pt node_heap;
    unsigned total_nodes;
    unsigned used_nodes;
    gap_pt gap_ix;
    unsigned gap_ix_capacity;
} pool_mgr_t, *pool_mgr_pt;

/***************************/
/*                         */
/* Static global variables */
/*                         */
/***************************/
static pool_mgr_pt *pool_store = NULL; // an array of pointers, only expand
static unsigned pool_store_size = 0;//current pool strorage size.
static unsigned pool_store_capacity = 0;//pool capacity



/********************************************/
/*                                          */
/* Forward declarations of static functions */
/*                                          */
/********************************************/
static alloc_status _mem_resize_pool_store();
static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr);
static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr);
static alloc_status
        _mem_add_to_gap_ix(pool_mgr_pt pool_mgr,
                           size_t size,
                           node_pt node);
static alloc_status
        _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr,
                                size_t size,
                                node_pt node);
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr);
void _print_node( node_pt n);
void _print_gap_ix( pool_mgr_pt, char);

void _print_gap_ix(pool_mgr_pt p, char c){
    size_t i = 0;
    while(i < p->pool.num_gaps){
        printf( "%c%u : %u\n" , c, (unsigned int)i, (unsigned int)p->gap_ix[i].size );
        i++;
    }
}

/*
    Definition of private functions
*/
//Simply returns a pointer of the correct type from malloc of the proper length
/*
pool_mgr_pt* allocatePoolStorePointer( unsigned length_of_pool){
    return (pool_mgr_pt*)malloc(sizeof(pool_mgr_pt) * length_of_pool);
}
*/

/****************************************/
/*                                      */
/* Definitions of user-facing functions */
/*                                      */
/****************************************/
//precondition: mem_init has not been called, therefore pool_store refers to a NULL value.
//postcondition: if mem_init is called, allocate fail is returned if it has been called mor
alloc_status mem_init() {
    // ensure that it's called only once until mem_free
    // allocate the pool store with initial capacity
    // note: holds pointers only, other functions to allocate/deallocate
    if (pool_store == NULL){
        pool_store_capacity = MEM_POOL_STORE_INIT_CAPACITY;
        pool_store = (pool_mgr_pt*)malloc( sizeof(pool_mgr_pt) * MEM_POOL_STORE_INIT_CAPACITY);;//initialize the pool to its initial capacity.
        if(pool_store != NULL)
        {
            return ALLOC_OK;
        }else{
            return ALLOC_FAIL;
        }
    }

    return ALLOC_CALLED_AGAIN;
}

alloc_status mem_free() {
    // ensure that it's called only once for each mem_init
    // make sure all pool managers have been deallocated
    // can free the pool store array
    // update static variables
    if (pool_store != NULL){
        //delete all
        //future optimization: put a specific instruction to check and remove cell zero
        //then an instruction to remove all all other cells by decrementing pool_store_size to exactly zero.
        //eliminates allocation of i and assignment of pool store size to zero.
        free((void*)pool_store);
        pool_store = NULL;
        return ALLOC_OK;
    }
    pool_store_size = 0;
    pool_store_capacity = 0;
    return ALLOC_CALLED_AGAIN;
}

pool_pt mem_pool_open(size_t size, alloc_policy policy) {
    // make sure there the pool store is allocated
    if (pool_store == NULL){
        return NULL;
    }

    // expand the pool store, if necessary
    size_t insertion_point = 0;
    //finds the first pool address in the pool store that is non-null
    //In theory, there should only be a small number of possible pools
    //Or if there are a great number of pools, finding them individually is preferable
    //to running out of them prematurely because you don't recycle them.
    while (insertion_point < pool_store_size && pool_store[insertion_point] != NULL){
        insertion_point += 1;
    }

    if (insertion_point >= pool_store_size){
        insertion_point = pool_store_size;
        pool_store_size+=1;
    }

    if( _mem_resize_pool_store() == ALLOC_FAIL){
        pool_store_size-=1;//correct pool size.
        return NULL;//If an unrecoverable error occurs, return nothing.
    }

    // allocate a new mem pool mgr
    pool_mgr_pt pool_mgr = (pool_mgr_pt)malloc( sizeof( pool_mgr_t ) );
    if (pool_mgr == NULL){
        return NULL;
    }

    // allocate a new memory pool
    pool_mgr->pool.mem = (char*)malloc( sizeof(char)*size );//allocate raw memory.
    // check success, on error deallocate mgr and return null
    if (pool_mgr->pool.mem == NULL){
        free ( ( void* ) pool_mgr);
        pool_mgr = NULL;
        return NULL;
    }

    pool_mgr->pool.total_size = size;//lets say that total size is the total size allocated
    pool_mgr->pool.alloc_size = 0;//lets say that the current size allocated
    pool_mgr->pool.num_allocs = 0;//None of this has been allocated by the user
    pool_mgr->pool.num_gaps = 1; //you start having allocated one gap of space.
    pool_mgr->pool.policy = policy;

    // allocate a new node heap
    pool_mgr->node_heap = malloc(sizeof(node_t) * MEM_NODE_HEAP_INIT_CAPACITY);
    // check success, on error deallocate mgr/pool and return null
    if(pool_mgr->node_heap == NULL){
        free( (void*) pool_mgr->pool.mem);
        pool_mgr->pool.mem=NULL;
        free( (void*) pool_mgr);
        pool_mgr = NULL;
        return NULL;
    }

    // allocate a new gap index
    pool_mgr->gap_ix = malloc(sizeof(gap_t) * MEM_GAP_IX_INIT_CAPACITY);
    // check success, on error deallocate mgr/pool/heap and return null
    if(pool_mgr->gap_ix == NULL){
        free( (void*) pool_mgr->pool.mem);
        pool_mgr->pool.mem=NULL;
        free( (void*) pool_mgr->node_heap);
        pool_mgr->pool.mem=NULL;
        free( (void*) pool_mgr);
        pool_mgr = NULL;
        return NULL;
    }

    // assign all the pointers and update meta data:
    //for node heap:
    pool_mgr->node_heap->used = 1;//means it s part of the list
    pool_mgr->node_heap->allocated = 0;//means it is a gap.
    pool_mgr->node_heap->alloc_record.mem = pool_mgr->pool.mem;
    pool_mgr->node_heap->alloc_record.size = size;
    //   initialize top node of node heap
    // - treating this as a linked list.
    pool_mgr->node_heap->next = pool_mgr->node_heap;//point the node heap to itself
    pool_mgr->node_heap->prev = pool_mgr->node_heap;//and make it so the circular reference points to itself
    //   initialize top node of gap index
    pool_mgr->gap_ix->node = pool_mgr->node_heap;
    pool_mgr->gap_ix->size = size;//needs to be the size of the new gap.
    //   initialize pool mgr
    pool_mgr->gap_ix_capacity = MEM_GAP_IX_INIT_CAPACITY;
    pool_mgr->total_nodes = MEM_NODE_HEAP_INIT_CAPACITY;
    pool_mgr->used_nodes = 1;
    //   link pool mgr to pool store
    pool_store[pool_store_size-1] = pool_mgr;
    pool_mgr = NULL;
    // return the address of the mgr, cast to (pool_pt)
    return (pool_pt)pool_store[pool_store_size-1];

}


alloc_status mem_pool_close(pool_pt pool) {
    // check if this pool is allocated - not that I don't disagree with this, but want to sdo it first, Cecil
    //especially since the pool_mgr and the pool will have the same address. So there is no reason not to do this first.
    if(pool == NULL){
        printf("YHIP!\n");
        return ALLOC_FAIL;
    }

    // check if pool has only one gap
    // check if it has zero allocations

    if( pool->num_gaps != 1 || pool->num_allocs != 0){
            printf("HIP!\n");
            printf("%u:%u", pool->num_gaps, pool->num_allocs);
        return ALLOC_NOT_FREED;
    }//Then, return a failing error code if either of those checks happens to be true.

    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt pool_mgr = (pool_mgr_pt) pool;

    // - serves no purpose. Pool always has at least one gap unless it is malformed.
    // - Furthermore, checking for zero allocations says nothing as to the quality of those allocated sectors.
    // Instead checking if the pool is malformed - i.e. if its node points to nothing.

    // free memory pool
    if( pool->mem != NULL){
        free(pool->mem);
        pool->mem=NULL;//All references to the memory pool now need to be pointed at NULL.
    }
    // free node heap
    if ( pool_mgr->node_heap != NULL){
        node_pt node_i = pool_mgr->node_heap->next;//this sets the pool to point to the next node
        //Which is the node heap itself or another node.
        //if it is another node, then this while loop runs until it is not. At termination, every node but
        //the list head will have had its links broken.
        while(pool_mgr->node_heap != node_i){
            node_i->prev = NULL;//set the previous link to null
            node_i->alloc_record.mem = NULL;//set the allocation record to null just in case. The allocation record has already been deleted.
            node_i= node_i->next;//shift the node iterator one unit forward
            node_i->prev->next = NULL;//break final link
        }
        //then the individual node heap is deallocated.
        node_i = NULL;
        pool_mgr->node_heap->next = NULL;
        pool_mgr->node_heap->prev = NULL;
        pool_mgr->node_heap->alloc_record.mem=NULL;
        free((void*)pool_mgr->node_heap);
        pool_mgr->node_heap = NULL;
    }
    // free gap index
    size_t i=0;//set initializer
    if(pool_mgr->gap_ix != NULL){
        while (i < pool_mgr->pool.num_gaps){
            pool_mgr->gap_ix[i].node = NULL;//these where already freed.
            i+=1;
        }
        free((void*)pool_mgr->gap_ix);
        pool_mgr->gap_ix = NULL;
    };
    // find mgr in pool store and set to null
    // free mgr
    pool_mgr->total_nodes=0;
    pool_mgr->used_nodes=0;
    // note: don't decrement pool_store_size, because it only grows
    i = 0;
    while (i < pool_store_size){
        if(pool_store[i] == pool_mgr){
            pool_store[i] = NULL;
            free(pool_mgr);
            return ALLOC_OK;
        }
        i+=1;
    }
            printf("FAIL!\n");
    return ALLOC_NOT_FREED;
}

alloc_pt mem_new_alloc(pool_pt pool, size_t size) {
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt pool_mgr = (pool_mgr_pt) pool;
    // check if any gaps, return null if none
    if(pool_mgr->pool.num_gaps == 0){
        return NULL;
    }
    // expand heap node, if necessary, quit on error
    if( _mem_resize_node_heap(pool_mgr) != ALLOC_OK){
        return NULL;
    }
    // check used nodes fewer than total nodes, quit on error
    if ( pool_mgr->used_nodes > pool_mgr->total_nodes){
        return NULL;
    }
    //check to make sure allocation is smaller than total size allocated.
    if ( (size + pool->alloc_size) > pool->total_size ){
        return NULL;
    }
    // get a node for allocation:
    // if FIRST_FIT, then find the first sufficient node in the node heap
    // if BEST_FIT, then find the first sufficient node in the gap index

    node_pt insert_node = NULL;

    if (pool->policy == FIRST_FIT)
    {

        //if nothing is allocated, return the node heap
        if(pool->num_allocs == 0 ){
            insert_node = pool_mgr->node_heap;
        }
        else{
        //otherwise, cycle through all other nodes, until the node heap is found.
            char is_size_unused = 0;
            insert_node = pool_mgr->node_heap;
            //check size
            while( is_size_unused == 0 ){//and if that isn't allocated, spin till you find one that is allocated.
                    if( insert_node->alloc_record.size >= size && insert_node->allocated==0){
                        is_size_unused = 1;
                    }
                    else{
                        insert_node = insert_node->next;
                    }
            }
        }
        //check the first node in the list.
        //if it is not a fit, query list for for first fit.
        //If no node of the correct size is found return null.
    }
    else if (pool->policy == BEST_FIT)
    {
        insert_node = NULL;
        size_t gap_i = 0;
        while (gap_i < pool->num_gaps && insert_node == NULL){
            if(pool_mgr->gap_ix[gap_i].size >= size){
            insert_node = pool_mgr->gap_ix[gap_i].node;
            }
            gap_i+=1;
        }
        //simply move through the array of gaps until one is found.
        //Remove that gap using gap removal function call.
    }else{
    //no recognizable policy provided? assert false
        assert(pool->policy == BEST_FIT || pool->policy == FIRST_FIT );
    }

    // check if node found
    if (insert_node == NULL){
        return NULL;
    }

    // update metadata (num_allocs, alloc_size)
    pool->num_allocs+=1;
    pool->alloc_size+=size;
    // calculate the size of the remaining gap, if any

    size_t rem_gap = insert_node->alloc_record.size - size;

    assert(rem_gap <= insert_node->alloc_record.size);//overflow catch
    // remove node from gap index
    assert(_mem_remove_from_gap_ix(pool_mgr, size, insert_node) == ALLOC_OK);
    // convert gap_node to an allocation node of given size
    insert_node->allocated = 1;
    insert_node->used = 1;
    insert_node->alloc_record.size = size;

    //The insert_nodes allocation record pointer is adjusted.
    // adjust node heap:
    //   if remaining gap, need a new node
    node_pt new_gap = NULL;
    if(rem_gap != 0){
    //   needs to either exist in the heap as an unused node, or needs to be created
        new_gap = pool_mgr->node_heap;

        new_gap = new_gap->next;
        while(new_gap != pool_mgr->node_heap && new_gap->used != 0){
            new_gap = new_gap->next;
        }
        if(new_gap == pool_mgr->node_heap){
        if(_mem_resize_node_heap(pool_mgr) != ALLOC_OK){
            return NULL;
        }
        new_gap = &pool_mgr->node_heap[pool_mgr->used_nodes];
        //did not find an unused node. Therefore create a new node to refer to it in the node_heap
            }
        new_gap->prev = insert_node;
        new_gap->next = insert_node->next;
        new_gap->next->prev = new_gap;
        insert_node->next = new_gap;
        new_gap->used = 1;
        new_gap->allocated = 0;
        new_gap->alloc_record.size = rem_gap;
        //the starting index of the gap is the next available memory slice.
        new_gap->alloc_record.mem =(char *) (insert_node->alloc_record.mem + size-1);
        //   initialize it to a gap node
        assert( _mem_add_to_gap_ix(pool_mgr, rem_gap, new_gap) == ALLOC_OK );
        //   update metadata (used_nodes)
        pool_mgr->used_nodes+=1;

        //   update linked list (new node right after the node for allocation) <- unnecessary. Either no gap exists. A gap exists but it was already allocated. OR you've added a gap node.
        //   add to gap index <- unnecessary, done in static _mem_add_to_gap_ix code
        //   check if successful <-done with assert.
        new_gap = NULL;//wipe out local reference just in case
    }

    // return allocation record by casting the node to (alloc_pt)
    //or you can just, you know, return the node.
    return &(insert_node->alloc_record);
}

void _print_node( node_pt n){
    printf("ADDR_AT: %p\n", n);
    printf("Alloc-used: %u-%u next->%p prev%p\n", n->allocated, n->used, n->next, n->prev);
    printf("Size: %u Memoryaddr: %p\n", (unsigned int)n->alloc_record.size, n->alloc_record.mem);
}

alloc_status mem_del_alloc(pool_pt pool, alloc_pt alloc) {
    /*Round three.
    *
    Its like a toy puzzle. Put the bolt in, pull it back, merge until you reach the node head or an allocated node.
    Then merge that node into the node head if you reached it.
    Then merge any nodes beyond the node head into the node head.
    */
    pool_mgr_pt pool_mgr = (pool_mgr_pt)pool;//get pool manager
    node_pt init = (node_pt)alloc;// set up the allocation pointer from alloc
    node_pt iter =  pool_mgr->node_heap;//then point it to its next item.
    node_pt head = pool_mgr->node_heap;//don't want to type it, just that simple.


    size_t error_ctr = 0;
    while(iter != init){
        iter = iter->next;
        error_ctr +=1;
        if(error_ctr > pool_mgr->total_nodes){
            return ALLOC_FAIL;
        }
    }//after finding the initial node, clear it out!
    printf("I\n");
    _print_node(head);
    _print_node(init);
    init->allocated = 0;
    pool->num_allocs -=1;//reduce allocations by one.
    pool->alloc_size -= init->alloc_record.size;
    assert(_mem_add_to_gap_ix(pool_mgr, init->alloc_record.size, init) == ALLOC_OK);//that should convert it to a node
    iter = init->prev;
    while(iter->allocated != 1  && iter != init){
        iter = iter->prev;//first back it up
    }
    if(iter->allocated == 1){//then move the pointer forward to the correct starting position.
        init = iter->next;//so the initial node is the one we are lumping things together from.
        iter = init->next;//And the iterator becomes the next node.
    }
    while(iter->allocated != 1 && iter != head && iter != init){

        _print_node(iter);
        init->alloc_record.size+= iter->alloc_record.size;
        _print_node(iter);
        //zero out the allocaton record after adding int the size
        assert(_mem_remove_from_gap_ix(pool_mgr, iter->alloc_record.size, iter) == ALLOC_OK );
        iter->alloc_record.size = 0;
        iter->alloc_record.mem = NULL;
        iter->allocated = 0;
        iter->used = 0;//adjust the iterator
        //then correct its nodes
        init->next = iter->next;
        iter->next->prev = init;
        _print_gap_ix(pool_mgr, 'e');
        iter = init->next;
        //this should force the puzzle one step forward.
        //at the next step, another block is wiped out or becomes the allocation record.
        //Assuming none of them are the node head, life continues on merrily.
    }
    if (iter == head && iter->allocated == 0){
        _print_gap_ix(pool_mgr, 'd');
        init = head;
    // now if the iterator is the head, it needs special attention. First you need to wipe the back out,
    //assuming that the back exists. The algorithm pushes the node all the way back first, then moves it forward. So by the time
    //that the head is reached, only the initial node should exist.
        if (init->next == head){//first, check to be sure you aren't looking at the node head itself.
            printf("visited!\n");
            pool->num_gaps = 1;
            pool->num_allocs = 0;
            size_t i = 0;
            while( i < pool_mgr->pool.num_gaps){
                if( init == pool_mgr->gap_ix[i].node){
                    pool_mgr->gap_ix[i].size = init->alloc_record.size;
                    break;
                }
                i++;
            }
            init = NULL;
            iter = NULL;
            head = NULL;
            return ALLOC_OK;
        }
        if (init->prev->allocated == 0){//if it has a unallocated node behind it, absorb it
                _print_gap_ix(pool_mgr, 'c');
            iter = init->prev;
            init->alloc_record.size+= iter->alloc_record.size;
            //zero out the allocaton record after adding int the size
            assert(_mem_remove_from_gap_ix(pool_mgr, iter->alloc_record.size, iter) == ALLOC_OK);
            iter->alloc_record.size = 0;
            iter->alloc_record.mem = NULL;
            iter->allocated = 0;
            iter->used = 0;//adjust the iterator
            //then correct its nodes
            init->prev = iter->prev;
            iter->prev->next = init;
            }
        iter = init->next;
        //then run it forward again mum.
        while(iter->allocated != 1 && iter != head){
            init->alloc_record.size+= iter->alloc_record.size;
            //zero out the allocaton record after adding int the size
            assert(_mem_remove_from_gap_ix(pool_mgr, iter->alloc_record.size, iter) ==ALLOC_OK);
            iter->alloc_record.size = 0;
            iter->alloc_record.mem = NULL;
            iter->allocated = 0;
            iter->used = 0;//adjust the iterator
            //then correct its nodes
            init->next = iter->next;
            iter->next->prev = init;
            iter = iter->next;
            //this should force the puzzle one step forward.
            //at the next step, another block is wiped out or becomes the allocation record.
            //Assuming none of them are the node head, life continues on merrily.
        }
    }
        int i = 0;
    if( init->next->allocated == 1 && init->prev->allocated == 1){
        while(i < pool_mgr->pool.num_gaps){
            if( init == pool_mgr->gap_ix[i].node){
                pool_mgr->gap_ix[i].size = init->alloc_record.size;
                break;
            }
            i++;
        }
        init = NULL;
        iter = NULL;
        head = NULL;
        return ALLOC_OK;
    }
    if (init == head && init->next == head){
            while(i < pool_mgr->pool.num_gaps){
            if( init == pool_mgr->gap_ix[i].node){
                pool_mgr->gap_ix[i].size = init->alloc_record.size;
                break;
            }
            i++;

        }
        init = NULL;
        iter = NULL;
        head = NULL;
        return ALLOC_OK;
    }
    init = NULL;
    iter = NULL;
    head = NULL;
    return ALLOC_FAIL;
    /*
    pool->alloc_size-=init->alloc_record.size;
    pool->num_allocs-=1;
    iter = iter->next;
    while(iter != init && iter->allocated == 0 && iter != pool_mgr->node_heap){//until it runs into itself or an allocated node:
        if(iter->allocated == 0){//destroy nodes
            init->alloc_record.size+= iter->alloc_record.size;
            iter->alloc_record.size = 0;//zero out for minor caution, remove for minor performance boon.
            assert(_mem_remove_from_gap_ix(pool_mgr, iter->alloc_record.size, iter ) == ALLOC_OK );
            iter->used = 0;
            iter->allocated = 0;
        }
        iter = iter->next;
    }

    init->next = iter;
    iter->prev = init;
    if (init != pool_mgr->node_heap){
        iter = init->prev;//then point it to its next item.
        while(iter != init && iter->allocated == 0 && iter != pool_mgr->node_heap){//until it runs into itself or an allocated node:

            if(iter->allocated == 0){//destroy nodes
                assert(_mem_remove_from_gap_ix(pool_mgr, iter->alloc_record.size, iter ) == ALLOC_OK );
                init->alloc_record.size+= iter->alloc_record.size;
                iter->alloc_record.size = 0;//zero out for minor caution, remove for minor performance boon.
                iter->used = 0;
                iter->allocated = 0;
            }
            iter = iter->prev;
        }
        init->prev = iter;
        iter->next = init;
    }
    if(_mem_add_to_gap_ix(pool_mgr, init->alloc_record.size, init) == ALLOC_OK){
        init->allocated = 0;
        //drain the rest of the nodes if they exist, from the node heap.
        init = pool_mgr->node_heap;
        iter = pool_mgr->node_heap->next;
        if(init ->allocated == 0){
            while(iter->allocated == 0 && init != iter){
                    if(iter->allocated == 0){//destroy nodes
                    assert(_mem_remove_from_gap_ix(pool_mgr, iter->alloc_record.size, iter ) == ALLOC_OK );
                    init->alloc_record.size+= iter->alloc_record.size;
                    iter->alloc_record.size = 0;//zero out for minor caution, remove for minor performance boon.
                    iter->used = 0;
                    iter->allocated = 0;
                }
                iter = iter->next;
                }
            init->next = iter;
            iter->prev = init;
        }
        init = NULL;
        iter = NULL;
        pool_mgr = NULL;
        return ALLOC_OK;
    }
    */
    /*
    if(alloc == NULL){
        return ALLOC_FAIL;
    }
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt pool_mgr= (pool_mgr_pt)pool;
    // get node from alloc by casting the pointer to (node_pt)
    node_pt node = (node_pt) alloc;
    // find the node in the node heap
    // this is node-to-delete
    // make sure it's found
    //check and make sure that its actually allocated in the first place
    // convert to gap node
    // update metadata (num_allocs, alloc_size)
    // if the next node in the list is also a gap, merge into node-to-delete
    //   remove the next node from gap index
    //   check success
    //   add the size to the node-to-delete
    //   update node as unused
    //   update metadata (used nodes)//done when remove from gap ix is called.

    //   update linked list:
                    if (next->next) {
                        next->next->prev = src;
                        src->next = next->next;
                    } else {
                        src->next = NULL;
                    }
                    next->next = NULL;
                    next->prev = NULL;
     */

    // this merged node-to-delete might need to be added to the gap index
    // but one more thing to check...

    // if the previous node in the list is also a gap, merge into previous!
    //   remove the previous node from gap index
    //   check success
    //   add the size of node-to-delete to the previous
    //   update node-to-delete as unused
    //   update metadata (used_nodes)
    //   update linked list
    /*
                    if (node_to_del->next) {
                        prev->next = node_to_del->next;
                        node_to_del->next->prev = prev;
                    } else {
                        prev->next = NULL;
                    }
                    node_to_del->next = NULL;
                    node_to_del->prev = NULL;
     */

    //   change the node to add to the previous node!
    // add the resulting node to the gap index
    // check success

}

//oooh, so you're using pointers as in-out variables. Cute.
void mem_inspect_pool(pool_pt pool,
                      pool_segment_pt *segments,
                      unsigned *num_segments) {
    // get the mgr from the pool
    pool_mgr_pt pool_mgr = (pool_mgr_pt) pool;
    // allocate the segments array with size == used_nodes
    pool_segment_pt arr = (pool_segment_pt)malloc(sizeof(pool_segment_t)*pool_mgr->used_nodes);
    // check successful
    if(arr == NULL){
        segments = NULL;
        num_segments = NULL;
        return;
    }

    node_pt iter = pool_mgr->node_heap;
    size_t i = 0;

    i=0;
    if(iter->used == 1){
        arr[i].allocated = iter->allocated;
        arr[i].size = iter->alloc_record.size;
        i+=1;

    }
    iter = iter->next;
    while(iter != pool_mgr->node_heap){
        if(iter->used == 1){

            //printf("USED: %u\n", iter->used);
            arr[i].allocated = iter->allocated;
            arr[i].size = iter->alloc_record.size;
            i+=1;
        }
        iter = iter->next;
    }
    // loop through the node heap and the segments array

    //    for each node, write the size and allocated in the segment
    // "return" the values:
    *segments = arr;
    *num_segments = pool_mgr->used_nodes;
    pool_mgr = NULL;
    arr = NULL;
    printf("%p\n", segments);
}



/***********************************/
/*                                 */
/* Definitions of static functions */
/*                                 */
/***********************************/
// _mem_resize_pool_store
/*
    pre: There is a static pool_store with a pool_store_size and pool_store capacity.
    There is constants MEM_POOL_STORE_FILL FACTOR, MEM_POOL_STORE_EXPAND_FACTOR

*/
static alloc_status _mem_resize_pool_store() {
    // check if necessary
    if (((float) pool_store_size / (float) pool_store_capacity)
        < (float)MEM_POOL_STORE_FILL_FACTOR)
    {
        return ALLOC_OK;//and if it is not, inform that the allocation is okay.
    }
    unsigned new_capacity = ( unsigned ) ( pool_store_capacity * MEM_POOL_STORE_FILL_FACTOR );
    //Check the new capacity is greater than the pool store capacity, could be less because of overflow
    if (new_capacity > pool_store_capacity)
    {
        pool_mgr_pt* verify_store = ( pool_mgr_pt* ) realloc(
        ( void* ) pool_store ,
        sizeof(pool_mgr_pt) * MEM_POOL_STORE_EXPAND_FACTOR
        );
        //realloc returns a void pointer just to say if pool_store has been allocated.
        //This pointer can be null if it fails. Check that.
        if (verify_store != NULL)
        {
            // don't forget to update capacity variables
            pool_store_capacity = new_capacity;
            pool_store = verify_store;
            verify_store = NULL;
            return ALLOC_OK;
        }
    }

    return ALLOC_FAIL;
}

static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr) {
        // check if necessary
    if (pool_mgr == NULL || pool_mgr->node_heap == NULL){
        return ALLOC_FAIL;
    }
    node_pt node_h = pool_mgr->node_heap;
    //When I realized that the individual arrays and pools had sizes managed from external structures,
    // it made me kind of sad.
    if (((float) pool_mgr->used_nodes / (float) pool_mgr->total_nodes)
        < (float)MEM_NODE_HEAP_FILL_FACTOR)
    {
        return ALLOC_OK;//and if it is not, inform that the allocation is okay.
    }
    unsigned new_capacity = ( unsigned ) ( pool_mgr->total_nodes * MEM_POOL_STORE_FILL_FACTOR );
    //Check the new capacity is greater than the pool store capacity, could be less because of overflow
    if (new_capacity > pool_mgr->total_nodes){
        node_pt verify_store = ( node_pt ) realloc(
        ( void* ) node_h ,
        sizeof(node_pt) * MEM_POOL_STORE_EXPAND_FACTOR
        );
        //realloc returns a void pointer just to say if pool_store has been allocated.
        //This pointer can be null if it fails. Check that.
        if (verify_store != NULL)
        {
            // don't forget to update capacity variables
            pool_mgr->total_nodes = new_capacity;
            pool_mgr->node_heap = verify_store;
            verify_store = NULL;
            node_h = NULL;
            return ALLOC_OK;
        }
    }
    return ALLOC_FAIL;
}

static alloc_status _mem_resize_gap_ix( pool_mgr_pt pool_mgr )
{
        // check if necessary
    if (pool_mgr == NULL || pool_mgr->gap_ix == NULL){
        return ALLOC_FAIL;
    }
    gap_pt gap_arr = pool_mgr->gap_ix;
    //And then when I realized that instead of correctly being stored in a top facing structure,
    //but rather where hidden in multiple substructures, which breaks both re-usability and readability
    //I got offended.
    if (((float) pool_mgr->pool.num_gaps / (float) pool_mgr->gap_ix_capacity)
        < (float)MEM_GAP_IX_FILL_FACTOR)
    {
        return ALLOC_OK;//and if it is not, inform that the allocation is okay.
    }
    unsigned new_capacity = ( unsigned ) ( pool_mgr->gap_ix_capacity * MEM_POOL_STORE_FILL_FACTOR );
    //Check the new capacity is greater than the pool store capacity, could be less because of overflow
    if (new_capacity > pool_mgr->gap_ix_capacity)
    {
        gap_pt verify_gap = ( gap_pt ) realloc(
        ( void* ) gap_arr ,
        sizeof(gap_pt) * MEM_GAP_IX_EXPAND_FACTOR
        );
        //realloc returns a void pointer just to say if pool_store has been allocated.
        //This pointer can be null if it fails. Check that.
        if (verify_gap != NULL)
        {
            // don't forget to update capacity variables
            pool_mgr->gap_ix_capacity = new_capacity;
            pool_mgr->gap_ix = verify_gap;
            verify_gap = NULL;
            gap_arr = NULL;
            return ALLOC_OK;
        }
    }
    return ALLOC_FAIL;
}

static alloc_status _mem_add_to_gap_ix(pool_mgr_pt pool_mgr,
                                       size_t size,
                                       node_pt node) {

    // expand the gap index, if necessary (call the function)
    if ( _mem_resize_gap_ix(pool_mgr) != ALLOC_OK){
        return ALLOC_FAIL;
    }

    // add the entry at the end
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].node = node;
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].size = size;
    // update metadata (num_gaps)
    pool_mgr->pool.num_gaps+=1;
    // sort the gap index (call the function)
    if(_mem_sort_gap_ix(pool_mgr) == ALLOC_OK){
            return ALLOC_OK;
    }
    // check success
    return ALLOC_FAIL;
}

static alloc_status _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr,
                                            size_t size,
                                            node_pt node) {
    // find the position of the node in the gap index
    size_t i = 0;
    char is_del = 0;
    while ( i < pool_mgr->pool.num_gaps && is_del==0){
        if(node == pool_mgr->gap_ix[i].node){
            is_del = 1;
        }else{
            i++;
        }
    }
    if(is_del){
        while( i < pool_mgr->pool.num_gaps - 1){
            pool_mgr->gap_ix[i].size = pool_mgr->gap_ix[i+1].size;
            pool_mgr->gap_ix[i].node = pool_mgr->gap_ix[i+1].node;
            i++;
        }
        pool_mgr->gap_ix[pool_mgr->pool.num_gaps - 1].size = 0;
        pool_mgr->gap_ix[pool_mgr->pool.num_gaps - 1].node = NULL;
        pool_mgr->pool.num_gaps -=1;
        return ALLOC_OK;
    }
    return ALLOC_FAIL;
    // loop from there sto the end of the array:
    //    pull the entries (i.e. copy over) one position up
    //    this effectively deletes the chosen node
    // zero out the element at array end!
    // update metadata (num_gaps)

    /*
    You could also try thinking.
    Additionally, size is unused.
    */
    //Check the last element. Remove it if matches.
    /*size_t i = pool_mgr->pool.num_gaps-1;
    if(pool_mgr->gap_ix[i].node == node){

        pool_mgr->gap_ix[i].node = NULL;
        pool_mgr->gap_ix[i].size = 0;
        pool_mgr->pool.num_gaps -= 1;
        return ALLOC_OK;
    }
    node_pt changed = NULL;
    i = 0;//reset I
    //Check all other elements but the last elements per original comments.


    while ( i < pool_mgr->pool.num_gaps ){
        if( pool_mgr->gap_ix[i].node == node ){
            changed = node;
        }

        if(changed != NULL && i < pool_mgr->pool.num_gaps-1){
            pool_mgr->gap_ix[i].node = pool_mgr->gap_ix[i+1].node;
            pool_mgr->gap_ix[i].size = pool_mgr->gap_ix[i+1].size;
        }
        i++;
    }

    //if it changed, then remove the last element and return alloc_ok.
    if(changed != NULL){
        pool_mgr->gap_ix[pool_mgr->pool.num_gaps-1].node = NULL;
        pool_mgr->gap_ix[pool_mgr->pool.num_gaps-1].size = 0;
        pool_mgr->pool.num_gaps-=1;
        changed = NULL;
        return ALLOC_OK;
    }
    return ALLOC_FAIL;*/
}

// note: only called by _mem_add_to_gap_ix, which appends a single entry
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr) {

    // the new entry is at the end, so "bubble it up"
    if (pool_mgr == NULL){
        return ALLOC_FAIL;
    }
    size_t i = pool_mgr->pool.num_gaps;
    if (i <= 1){
        return ALLOC_OK;
    }
    /*
    i=0;
    node_pt iter = pool_mgr->node_heap;
    while(i < pool_mgr->pool.num_gaps){
        if(iter->used == 1 && iter->allocated ==0){
            pool_mgr->gap_ix[i].size = iter->alloc_record->size;
            pool_mgr->gap_ix[i].node = iter;
        }
    }*/
    //if size one, pool already sorted.
    i=0;//make i point to one
    size_t j = 0;
    size_t top = pool_mgr->pool.num_gaps-1;
    size_t swap;
    node_pt swap_n;
    while ( i < top){
        j=0;
        while (j < top - i ){
        if(pool_mgr->gap_ix[j].size > pool_mgr->gap_ix[j+1].size){
                swap = pool_mgr->gap_ix[j].size;
                swap_n = pool_mgr->gap_ix[j].node;
                pool_mgr->gap_ix[j].size = pool_mgr->gap_ix[j+1].size;
                pool_mgr->gap_ix[j].node = pool_mgr->gap_ix[j+1].node;
                pool_mgr->gap_ix[j+1].size = swap;
                pool_mgr->gap_ix[j+1].node = swap_n;
            }
            j+=1;
        }
        i+=1;
    }
    _print_gap_ix(pool_mgr, 'a');
    i = 0;
    return ALLOC_OK;
    // loop from num_gaps - 1 until but not including 0:
    //    if the size of the current entry is less than the previous (u - 1)
    //       swap them (by copying) (remember to use a temporary variable)

}


