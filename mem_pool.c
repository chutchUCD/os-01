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


typedef struct _node_head{
    node_pt _nodes;// the data store of all nodes.
    node_pt begin;// the beginning node
    node_pt end;// the ending node
    size_t max_size;
    size_t length;//updated as list operation are performed.
} node_head, *node_head_pt;

/*
    List data structure:
    Invariants:
    All list items are of the node data type.
    There exists a list head that contains the beginning and ending of a list.
    There exists an integer length that contains a count of all initialized nodes.
    There exists an integer max_size that contains the maximum possible size of the list.
    There exists an array that contains all nodes initialized into the list.
    The underlying nodes of the list are accessed from the beginning or end of the list.

*/


//initializes a new node list. Beginning points to nothing. Ending points to nothing.
//nodes are generated.
//a count of all nodes in list is generated.
//returns its own address if successful, null otherwise.
node_head_pt node_head_init( node_head_pt new_list, size_t number_of_nodes){
    if(new_list == NULL || new_list->_nodes != NULL){
        return NULL;
    }
    new_list->_nodes = (node_pt)malloc(sizeof(node_t)*number_of_nodes);
    if (new_list->_nodes == NULL)
    {
        return NULL;
    }
    new_list->length = 0;
    new_list->max_size = number_of_nodes;
    new_list->begin = NULL;
    new_list->end = NULL;
    return new_list;
}

node_head_pt init_to_share_existing_list(node_head_pt existing_list, node_head_pt empty_list ){
    if( empty_list == NULL || empty_list->_nodes != NULL){
        return NULL;
    }
    if(existing_list == NULL || empty_list->_nodes == NULL){
        return NULL;
    }
    empty_list->length = 0;
    empty_list->max_size = existing_list->max_size;
    empty_list->begin = NULL;
    empty_list->end = NULL;
    empty_list->_nodes = existing_list->_nodes;//share memory
    return empty_list;
}

/*
    Post condition: returns a node from an arbitrary offset.
    Used to initialize new nodes.
*/
node_pt node_from_offset( node_head_pt head, size_t offset){
    if (offset > head->max_size || offset > head->length || head == NULL){//make sure to pull from an unused area
        return NULL;
    }
    return &(head->_nodes[offset]);
}

node_pt next_node(node_pt node, node_head_pt head){
    if (node == NULL){
        return NULL;
    }else{
        if (node->next != NULL){
            return node->next;
        }
        return head->begin;
    }
    return NULL;
}

node_pt prev_node(node_pt node, node_head_pt head){
    if(node == NULL){
        return NULL;
    }else{
        if(node->prev != NULL){
            return node->prev;
        }
        return head->end;
    }
    return NULL;
}

//if list has length of 0, returns 1 - otherwise returns 0
char node_list_empty(node_head_pt head){
    if( head->length == 0){
        return 1;
    }else{
        return 0;
    }
}

//if the list has length of 1 or less, returns NULL.
//otherwise returns the address of the next node.
node_pt node_has_next(node_pt node, node_head_pt head){
    if (head->length <= 1){
        return NULL;
    }
    else {
        return next_node(node, head);
    }
}

//if the list has length of 1 or less, returns NULL.
//otherwise returns the address of the next node.
node_pt node_has_prev(node_pt node, node_head_pt head){
    if (head->length <= 1){
        return NULL;
    }
    else {
        return prev_node(node, head);
    }
}

node_pt node_list_insert(node_pt node_to_insert, node_head_pt head, node_pt insert_after){
    if (head == NULL || (head->length+1 > head->max_size)){
        return NULL;
    }
    if (insert_after == NULL){
        ++(head->length);
        node_to_insert->prev = NULL;
        node_to_insert->next = head->begin;
        head->begin = node_to_insert;
        return head->begin;
    }
    node_pt iter = head->begin;
    /*This is very safe, much better performance can be gotten with a simple assignment*/
    while( iter != NULL && iter != insert_after){
        iter = iter->next;
    }
    if ( iter != NULL){
    //you found the iterator! Yay!
        ++(head->length);
        node_to_insert->next = iter->next;
        if(iter->next != NULL){
            iter->next->prev = node_to_insert;
        }//point the node to insert to the next element the iterator points at.
        //if it exists point back.

        node_to_insert->prev = iter;
        //point the node to insert at the iterator, recall it is non-null in this block.
        iter->next = node_to_insert;
    }
    return iter;
}


//remove node:
/*
Precondition: A node contained in the list, a node head
Post condition: Travels through the list from the beginning referred to by the node head.
If it finds the node, points the previous index at the next index, points the next index at the previous index.
Decrements length of list by one.
Returns the node input, which should point to zero. This node can then be modified by other means.
*/
node_pt remove_node(node_pt node, node_head_pt head){
    if(head == NULL){
        return NULL;
    }//make sure the node head exists
    node_pt iter = head->begin;//point to the heads beginning
    while( iter != NULL){
        if(iter == node){
            head->length-=1;
            if(iter->next != NULL){
                iter->next->prev = iter->prev;
            }
            if(iter->prev != NULL){
                iter->prev->next = iter->next;
            }
            if(head->begin == iter){
                head->begin = iter->next;
            }
            if(head->end == iter){
                head->end = iter->prev;
            }
            /*Safety code*/
            if( head->length == 0){
                head->end = NULL;
                head->begin = NULL;
            }
            iter->next = NULL;
            iter->prev = NULL;
            return iter;
        }//end of if statement
        iter = iter->next;//cycle to next node, if it is null this will break.
    }
    return NULL;//iterator not found, return null
}


/*
    reallocates memory by size of multiplier.
*/
node_pt resize_node_head(node_head_pt head, size_t multiplier){

    node_pt res = (node_pt)realloc((void*)head->_nodes, sizeof(node_t)*(multiplier)*(head->max_size));

    if(res != NULL ){
        head->_nodes = res;
        head->max_size *= multiplier;
        res = NULL;
        return head->_nodes;
    }else{
        return NULL;
    }
}

typedef struct _gap {
    size_t size;
    node_pt node;
} gap_t, *gap_pt;

typedef struct _pool_mgr {
    pool_t pool;
    node_head_pt node_heap;//use a proper list head.
    unsigned total_nodes;//what is this?-> no reference to it in test suite...
    unsigned used_nodes;//what is this?-> no reference to it in the test suite... Means it is total number of nodes initialized ever.
    gap_pt gap_ix;
    unsigned gap_ix_capacity;//what is this?-> max possible capacity
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
More list functions
*/
//Return the front and back of a pool from a pool manager, saves typing.
node_pt node_begin(pool_mgr_pt pm){
    return pm->node_heap->begin;
}

node_pt node_end(pool_mgr_pt pm){
    return pm->node_heap->end;
}

node_head_pt clear_node_list(node_head_pt head){
    if (head == NULL){
        return NULL;
    }
    node_pt iter = head->begin;
    /* pretty simple, traverse through all nodes forward, deleting as you go along */
    while(iter != NULL){
        iter->allocated = 0;
        iter->used = 0;
        iter->alloc_record.mem = NULL;
        iter->alloc_record.size = 0;
        iter = iter->next;
        if(iter != NULL){
            iter->prev = NULL;
        }
    }
    iter = NULL;
    head->length = 0;
    return head;
}

node_head_pt delete_node_list(node_head_pt head){
    head = clear_node_list(head);
    if(head != NULL){
        free((void*)(head->_nodes) );
        head->_nodes = NULL;
        head->max_size = 0;
        return head;
    }
    return NULL;
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
    size_t original_size = pool_store_size;
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
        pool_store_size= original_size;//correct pool size.
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
    pool_mgr->node_heap = malloc(sizeof(node_head));//allocate a new node_head
    pool_mgr->node_heap->_nodes = NULL;//make sure this points to nothing.
    pool_mgr->node_heap = node_head_init(pool_mgr->node_heap, MEM_NODE_HEAP_INIT_CAPACITY);

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
    //   initialize top node of node heap
    node_list_insert( node_from_offset(pool_mgr->node_heap, 0), pool_mgr->node_heap, NULL);
    //Updating metadata
    node_begin(pool_mgr)->used = 1;//means it's part of the list
    node_begin(pool_mgr)->allocated = 0;//means it is a gap.
    node_begin(pool_mgr)->alloc_record.mem = pool_mgr->pool.mem;
    node_begin(pool_mgr)->alloc_record.size = size;
    //   initialize top node of gap index
    pool_mgr->gap_ix->node = node_begin(pool_mgr);
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
    // check if this pool is allocated - not that I don't disagree with this, but want to do it first, Cecil
    //especially since the pool_mgr and the pool will have the same address. So there is no reason not to do this first.
    if(pool == NULL){
        return ALLOC_FAIL;
    }

    // check if pool has only one gap
    // check if it has zero allocations

    if( pool->num_gaps != 1 || pool->num_allocs != 0){
        printf("Num_gaps != 1, num_allocs != 0\n");
        printf(" %i : %i\n", pool->num_gaps, pool->num_allocs);
        return ALLOC_NOT_FREED;
    }//Then, return a failing error code if either of those checks happens to be true.

    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt pool_mgr = (pool_mgr_pt) pool;

    // free memory pool
    if( pool->mem != NULL){
        free(pool->mem);
        pool->mem=NULL;//All references to the memory pool now need to be pointed at NULL.
    }
    // free node heap
    if ( pool_mgr->node_heap != NULL){
        if(delete_node_list(pool_mgr->node_heap) == NULL){
        }
        //then the underlying memory is freed.
        free((void*)pool_mgr->node_heap);//now the node heap itself is freed.
        pool_mgr->node_heap = NULL;//everything else is null.
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
            insert_node = node_begin(pool_mgr);
            if(insert_node->alloc_record.size < size || insert_node->allocated == 1){
                printf("YOU REALLY GOOFED 1\n");
                insert_node = NULL;
            }
        }
        else{
        //otherwise, cycle through all other nodes, until the node heap is found.
            insert_node = node_begin(pool_mgr);
            char found = 0;
            //check size
        //check the first node in the list.
        //if it is not a fit, query list for for first fit.
        //If no node of the correct size is found return null.
            while( insert_node != NULL && found == 0){//and if that isn't allocated, spin till you find one that is allocated.
                if(( insert_node->alloc_record.size >= size ) && ( insert_node->allocated == 0 )){
                    found = 1;
                }else{
                    insert_node = insert_node->next;
                }
            }
        }
    }
    else if (pool->policy == BEST_FIT)
    {
        insert_node = NULL;
        size_t gap_i = 0;
        while (gap_i < pool->num_gaps && insert_node == NULL){
            if(pool_mgr->gap_ix[gap_i].size >= size){
                insert_node = pool_mgr->gap_ix[gap_i].node;
            }
            printf("CURIOUS %i, %i \n",insert_node->allocated, insert_node->alloc_record.size);
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
        new_gap = pool_mgr->node_heap->_nodes;
        size_t i = 0;
        //find an unused node. Do this by searching through all previously used nodes (this is what used nodes will be)
        while( i < pool_mgr->used_nodes && new_gap[i].used != 0){
            ++i;
        }

        //if no unused node is found, make sure you have enough nodes in the heap...
        if(i ==  pool_mgr->used_nodes){
            if(_mem_resize_node_heap(pool_mgr) != ALLOC_OK){
                return NULL;
            }
            //then get a new gap
            new_gap = node_from_offset(pool_mgr->node_heap,pool_mgr->used_nodes);
            //did not find an unused node. Therefore create a new node to refer to it in the node_heap
            //update metadata (used_nodes)
            //   update linked list (new node right after the node for allocation)
            pool_mgr->used_nodes+=1;
        }else{//but if one is found point to it.
            new_gap = &new_gap[i];
        }
        node_list_insert(new_gap, pool_mgr->node_heap, insert_node);
        new_gap->used = 1;
        new_gap->allocated = 0;
        new_gap->alloc_record.size = rem_gap;
        //the starting index of the gap is the next available memory slice.
        new_gap->alloc_record.mem =(char *) (insert_node->alloc_record.mem + size-1);
        //   initialize it to a gap node
        assert( _mem_add_to_gap_ix(pool_mgr, rem_gap, new_gap) == ALLOC_OK );
        //   add to gap index
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
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt pool_mgr= (pool_mgr_pt)pool;
    // get node from alloc by casting the pointer to (node_pt)
    //node_pt node = (node_pt) alloc;
    node_pt init = (node_pt)alloc;// set up the allocation pointer from alloc
    node_pt iter =  node_begin(pool_mgr);//Point the iterator at the beginning of the node heap.
    //find the node in the node heap.
    while (iter != NULL && iter != init){
        iter=iter->next;
    }
    // this is node-to-delete -> init
    if(iter == NULL){
        printf ("COULDN'T FIND\n");
        return ALLOC_NOT_FREED;
    }
    assert(&init->alloc_record == alloc);
    // make sure it's found
    assert(&iter->alloc_record == alloc);
    //check and make sure that its actually allocated in the first place
    if( iter->allocated != 1){
        printf("IMPROPER CYCLE\n");
    //if it is not:
    /*
        if it is the only node in the list, it should not be deleted.
        This is testede for by checking to see if their is one node.
        In this instance, you should return alloc_ok and do nothing else.
        Otherwise, place its allocation size into the allocation record at the
        end of the list.
        Then remove it from the gaps.
        And remove it from the list.
        Return alloc_ok
    */
        if(pool_mgr->node_heap->length == 1){
            printf("XXXX\n");
            return ALLOC_OK;
        }
        printf("YYYY\n");
        pool_mgr->node_heap->end->alloc_record.size+=iter->alloc_record.size;
        remove_node( iter, pool_mgr->node_heap);
        _mem_remove_from_gap_ix(pool_mgr, iter->alloc_record.size, iter);
        printf("ZZZZ\n");
        return ALLOC_OK;
    }
    // convert to gap node
    iter->allocated = 0;
    // update metadata (num_allocs, alloc_size)
    pool->num_allocs -= 1;
    pool->alloc_size -= alloc->size;
    // if the next node in the list is also a gap, merge into node-to-delete
    if(pool_mgr->node_heap->length > 1){// if the node has only one node, don't do anything
        node_pt del_me = next_node(iter, pool_mgr->node_heap);
        if (del_me->allocated == 0){//node is guaranteed to exist if there is more than one node, all you need to test if if the node is allocated.
        //   remove the next node from gap index
            alloc_status is_success = _mem_remove_from_gap_ix(pool_mgr, del_me->alloc_record.size, del_me);
        //   check success
            if (is_success == ALLOC_OK){
        //   add the size to the node-to-delete
                iter->alloc_record.size+=iter->next->alloc_record.size;
            }
            else{
                printf("%p, %p, %p, \n", iter, del_me , iter->next);
                printf("%i : %i\n", del_me->allocated, del_me->alloc_record.size);
                printf("BAAZ\n");
                return ALLOC_NOT_FREED;
            }
        //   update node as unused
            iter->next->used = 0;
            iter->next->allocated = 0;
        //   update metadata (used nodes)//done when remove from gap ix is called.->treating used as a total count of all nodes used ever, do not do this.
        //   update linked list:
            del_me = remove_node(del_me, pool_mgr->node_heap);//decreases count by one and updates the links.
            //should I check or assert for some kind of error?
        }
        del_me = node_has_prev( iter , pool_mgr->node_heap );//make sure to reset del_me to null;
        // this merged node-to-delete might need to be added to the gap index-> not yet, do the next check first.
        // but one more thing to check...
        // if the previous node in the list is also a gap, merge into previous!
        if ( del_me != NULL && del_me->allocated == 0) {
            node_pt del_me = iter;//say that delete me is iter.
            iter = prev_node(iter, pool_mgr->node_heap);//say that the previous node is the iterator.
            iter->alloc_record.size+=del_me->alloc_record.size;//unlike the other node, this one was never a part of the gap record to begin with.
            //so just add the allocation size to the next node.
            //   update node as unused
            del_me->used = 0;
            del_me->allocated = 0;
        //   update linked list:
            del_me = remove_node(del_me, pool_mgr->node_heap);
                _print_gap_ix(pool_mgr, 'b' );
            //updating this is tricky: you need to remove the iterator from the pool, then re-add it.
            _mem_remove_from_gap_ix(pool_mgr, iter->alloc_record.size, iter);
            //should I check for some kind of error.
            _print_gap_ix(pool_mgr, 'c');
            del_me = NULL;
        }
    }
    // add the resulting node to the gap index
    alloc_status s = _mem_add_to_gap_ix(pool_mgr, iter->alloc_record.size, iter);
    printf("Status %i\n", s);
    _print_gap_ix(pool_mgr, 'a');
    if(s== ALLOC_OK){
        printf("OKAY!\n");
    }

    return s;
    // check success

    //return ALLOC_FAIL;

    /*                if (next->next) {
                        next->next->prev = src;
                        src->next = next->next;
                    } else {
                        src->next = NULL;
                    }
                    next->next = NULL;
                    next->prev = NULL;
     */


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

}

//Using pointers as in-out variables.
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

    node_pt iter = node_begin(pool_mgr);
    size_t i = 0;
    i=0;
    while(iter != NULL){
        if(iter->used == 1){
            arr[i].allocated = iter->allocated;
            arr[i].size = iter->alloc_record.size;
        }
        i+=1;
        iter = iter->next;
    }
    // loop through the node heap and the segments array
    //    for each node, write the size and allocated in the segment
    // "return" the values:
    *segments = arr;
    *num_segments = pool_mgr->node_heap->length;
    pool_mgr = NULL;
    arr = NULL;
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


//If necessary to resize, resizes pool.
//If unnecessary, return alloc_ok but does nothing else.
//If pool_mgr malformed, returns Alloc_fail.
static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr) {
        // check if necessary
    if (pool_mgr == NULL || pool_mgr->node_heap == NULL){
        return ALLOC_FAIL;// if the pool mgr and the node heap don't exist, return Failure.
    }
    //When I realized that the individual arrays and pools had sizes managed from external structures,
    // it made me kind of sad.
    if ( ((float) pool_mgr->used_nodes / (float) pool_mgr->total_nodes)
        < (float)MEM_NODE_HEAP_FILL_FACTOR)
    {
        return ALLOC_OK;//and if it is not necessary to resize return alloc_ok
    }
    unsigned new_capacity = ( unsigned ) ( pool_mgr->total_nodes * MEM_POOL_STORE_FILL_FACTOR );
    //Check the new capacity is greater than the pool store capacity, could be less because of overflow
    if (new_capacity > pool_mgr->total_nodes){
        //realloc returns a void pointer just to say if pool_store has been allocated.
        node_pt verify_store = resize_node_head(pool_mgr->node_heap, MEM_POOL_STORE_EXPAND_FACTOR);//done
        //This pointer can be null if it fails. Check that.
        if (verify_store != NULL)
        {
            // don't forget to update capacity variables
            pool_mgr->total_nodes = pool_mgr->node_heap->max_size;
            pool_mgr->node_heap->_nodes = verify_store;
            verify_store = NULL;
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
    // loop from there sto the end of the array:
    while ( i < pool_mgr->pool.num_gaps && is_del==0){
        printf("%i : %i : %i : %i\n ", i, size, pool_mgr->gap_ix[i].size, pool_mgr->gap_ix[i].node->alloc_record.size);
        if(node == pool_mgr->gap_ix[i].node){
            is_del = 1;
        }else{
            i++;
        }
    }


    if(is_del){
    //    pull the entries (i.e. copy over) one position up
        while( i < pool_mgr->pool.num_gaps - 1){
            pool_mgr->gap_ix[i].size = pool_mgr->gap_ix[i+1].size;
            pool_mgr->gap_ix[i].node = pool_mgr->gap_ix[i+1].node;
            i++;
        }
        pool_mgr->gap_ix[pool_mgr->pool.num_gaps - 1].size = 0;
        pool_mgr->gap_ix[pool_mgr->pool.num_gaps - 1].node = NULL;
    // update metadata (num_gaps)
        pool_mgr->pool.num_gaps -=1;
        return ALLOC_OK;
    }
    //    this effectively deletes the chosen node
    // zero out the element at array end!
    printf("HEREX\n");
    return ALLOC_FAIL;
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
    //_print_gap_ix(pool_mgr, 'a');
    i = 0;
    return ALLOC_OK;
    // loop from num_gaps - 1 until but not including 0:
    //    if the size of the current entry is less than the previous (u - 1)
    //       swap them (by copying) (remember to use a temporary variable)

}
