//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


//
// TODO:Student Information
//
const char *studentName = "Yilan Chen";
const char *studentID   = "A59002673";
const char *email       = "yic031@ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;     // Number of sets in the I$
uint32_t icacheAssoc;    // Associativity of the I$
uint32_t icacheHitTime;  // Hit Time of the I$

uint32_t dcacheSets;     // Number of sets in the D$
uint32_t dcacheAssoc;    // Associativity of the D$
uint32_t dcacheHitTime;  // Hit Time of the D$

uint32_t l2cacheSets;    // Number of sets in the L2$
uint32_t l2cacheAssoc;   // Associativity of the L2$
uint32_t l2cacheHitTime; // Hit Time of the L2$
uint32_t inclusive;      // Indicates if the L2 is inclusive

uint32_t blocksize;      // Block/Line size
uint32_t memspeed;       // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;       // I$ references
uint64_t icacheMisses;     // I$ misses
uint64_t icachePenalties;  // I$ penalties

uint64_t dcacheRefs;       // D$ references
uint64_t dcacheMisses;     // D$ misses
uint64_t dcachePenalties;  // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//

//
//TODO: Add your Cache data structures here
//




typedef struct Block
{
  uint32_t value;
  struct Block *pre, *next;
}Block;

typedef struct Set
{
  uint32_t size;
  Block *start, *end;
}Set;

Block* createBlock(uint32_t value)
{
  Block *block = (Block*)malloc(sizeof(Block));
  block->value = value;
  block->pre = NULL;
  block->next = NULL;

  return block;
}



// Pop the first block in a set
void setPop(Set* s){
  // If empty, do nothing
  if(!s->size)
    return;

  Block *b = s->start;
  s->start = b->next;

  if(s->start)
    s->start->pre = NULL;

  (s->size)--;
  free(b);
}

// add a block into a set
void setPush(Set* s,  Block *b)
{
  // If set not empty, append to the end
  if(s->size)
  {
    s->end->next = b;
    b->pre = s->end;
    s->end = b;
  }
  // If empty, initialize set
  else
  {
    s->start = b;
    s->end = b;
  }
  (s->size)++;
}

// pop a block in a set with index
Block* setPopIndex(Set* s, int index){
  // Invalid Pop index
  if(index > s->size || index<0)
    return NULL;

  Block *b = s->start;

  if(s->size == 1){
    s->start = NULL;
    s->end = NULL;
  }
  else if (index == 0)
  {
    s->start = b->next;
    s->start->pre = NULL;
  }
  else if (index == s->size - 1)
  {
    b = s->end;
    s->end = s->end->pre;
    s->end->next = NULL;
  }
  else{
    for(int i=0; i<index; i++)
      b = b->next;
    b->pre->next = b->next;
    b->next->pre = b->pre;
  }

  b->next = NULL;
  b->pre = NULL;

  (s->size)--;
  //Block ownership transfer to caller
  return b;
}

Set *icache;
Set *dcache;
Set *l2cache;

uint32_t offset_bits;
uint32_t offset_mask;

uint32_t icache_index_bits;
uint32_t dcache_index_bits;
uint32_t l2cache_index_bits;
uint32_t icache_index_mask;
uint32_t dcache_index_mask;
uint32_t l2cache_index_mask;













//------------------------------------//
//          Cache Functions           //
//------------------------------------//

// Initialize the Cache Hierarchy
//
void
init_cache()
{
  // Initialize cache stats
  icacheRefs        = 0;
  icacheMisses      = 0;
  icachePenalties   = 0;
  dcacheRefs        = 0;
  dcacheMisses      = 0;
  dcachePenalties   = 0;
  l2cacheRefs       = 0;
  l2cacheMisses     = 0;
  l2cachePenalties  = 0;
  
  //
  //TODO: Initialize Cache Simulator Data Structures
  //

  icache = (Set*)malloc(sizeof(Set) * icacheSets);
  dcache = (Set*)malloc(sizeof(Set) * dcacheSets);
  l2cache = (Set*)malloc(sizeof(Set) * l2cacheSets);

  for(int i=0; i<icacheSets; i++)
  {
    icache[i].size = 0;
    icache[i].start = NULL;
    icache[i].end = NULL;
  }

  for(int i=0; i<dcacheSets; i++)
  {
    dcache[i].size = 0;
    dcache[i].start = NULL;
    dcache[i].end = NULL;
  }

  for(int i=0; i<l2cacheSets; i++)
  {
    l2cache[i].size = 0;
    l2cache[i].start = NULL;
    l2cache[i].end = NULL;
  }
    offset_bits = (uint32_t)log2(blocksize);
    offset_mask = (1 << offset_bits) - 1;


    icache_index_bits = (uint32_t)log2(icacheSets);
    dcache_index_bits = (uint32_t)log2(dcacheSets);
    l2cache_index_bits = (uint32_t)log2(l2cacheSets);

    icache_index_mask = ((1 << icache_index_bits) - 1) << offset_bits;
    dcache_index_mask = ((1 << dcache_index_bits) - 1) << offset_bits;
    l2cache_index_mask = ((1 << l2cache_index_bits) - 1) << offset_bits;


}


uint32_t
get_addr(uint32_t addr)
{
  // Memory address should be ||addr|| == ||tag||index||offset||
  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & icache_index_mask) >> offset_bits;
  uint32_t tag = addr >> (icache_index_bits + offset_bits);

  return tag, index, offset;
}



// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
icache_access(uint32_t addr)
{
  //
  //TODO: Implement I$
  //
  // return memspeed;


  // If not intialized, bypass the L1
  if(icacheSets==0){
    return l2cache_access(addr);
  }
  // If intialized, go below
  icacheRefs += 1;

  // Memory address should be ||addr|| == ||tag||index||offset||
  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & icache_index_mask) >> offset_bits;
  uint32_t tag = addr >> (icache_index_bits + offset_bits);


  Block *p = icache[index].start;

  for(int i=0; i<icache[index].size; i++){
    if(p->value == tag){ // Hit
      Block *b = setPopIndex(&icache[index], i); // Get the hit block
      setPush(&icache[index],  b); // move to end of set queue
      return icacheHitTime;
    }
    p = p->next;
  }

  icacheMisses += 1;

  uint32_t penalty = l2cache_access(addr);
  icachePenalties += penalty;

  // Miss replacement - LRU
  Block *b = createBlock(tag);

  if(icache[index].size == icacheAssoc) // set filled, replace LRU (start of set queue)
    setPop(&icache[index]);
  setPush(&icache[index],  b);

  return icacheHitTime + penalty;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  //
  //TODO: Implement D$
  //
  // return memspeed;


  // If not intialized, bypass the L1
  if(dcacheSets==0){
    return l2cache_access(addr);
  }
  // If intialized, go below
  dcacheRefs += 1;

  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & dcache_index_mask) >> offset_bits;
  uint32_t tag = addr >> (dcache_index_bits + offset_bits);

  Block *p = dcache[index].start;

  for(int i=0; i<dcache[index].size; i++){
    if(p->value == tag){ // Hit
      Block *b = setPopIndex(&dcache[index], i); // Get the hit block
      setPush(&dcache[index],  b); // move to end of set queue
      return dcacheHitTime;
    }
    p = p->next;
  }

  dcacheMisses += 1;


  uint32_t penalty = l2cache_access(addr);
  dcachePenalties += penalty;

  // Miss replacement - LRU
  Block *b = createBlock(tag);

  if(dcache[index].size == dcacheAssoc) // set filled, replace LRU (start of set queue)
    setPop(&dcache[index]);
  setPush(&dcache[index],  b);

  return dcacheHitTime + penalty;

}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  //
  //TODO: Implement L2$
  //
  // return memspeed;


  // If not intialized, bypass the L2
  if(l2cacheSets==0){
    return memspeed;
  }
  // If intialized, go below
  l2cacheRefs += 1;

  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & l2cache_index_mask) >> offset_bits;
  uint32_t tag = addr >> (l2cache_index_bits + offset_bits);

  Block *p = l2cache[index].start;

  for(int i=0; i<l2cache[index].size; i++){
    if(p->value == tag){ // Hit
      Block *b = setPopIndex(&l2cache[index], i); // Get the hit block
      setPush(&l2cache[index],  b); // move to end of set queue
      return l2cacheHitTime;
    }
    p = p->next;
  }

  l2cacheMisses += 1;

  // Miss replacement - LRU
  Block *b = createBlock(tag);
  
  if(l2cache[index].size == l2cacheAssoc){ // set filled, replace LRU (start of set queue)
    setPop(&l2cache[index]);
  }
  setPush(&l2cache[index],  b);

  l2cachePenalties += memspeed;
  return l2cacheHitTime + memspeed;

}
