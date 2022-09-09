// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

struct {
  struct spinlock lock;
  struct buf head; //using Linked list here
}hash_buckets[NBUCKETS];

uint64 get_hashkey(uint dev, uint blkno)
{
  return (dev * 10000 + blkno) % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // initialize hash_bucket
  char name[20];
  for(int i = 0; i < NBUCKETS; i++){
    // init lock
    memset(name,0,sizeof(name));
    snprintf(name,10,"bcache %d",i);
    initlock(&hash_buckets[i].lock, name);
    // init head
    hash_buckets[i].head.prev = &hash_buckets[i].head;
    hash_buckets[i].head.next = &hash_buckets[i].head;
  }

  // Create linked list of buffers
  // don't organize a linked list here anymore
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    b->next = hash_buckets[0].head.next;
    b->prev = &hash_buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    hash_buckets[0].head.next->prev = b;
    hash_buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint64 key = get_hashkey(dev, blockno);
  // printf("dev:%d blockno:%d key:%d\n",dev,blockno,key);

  acquire(&hash_buckets[key].lock);
  // Is the block already cached?
  for(b = hash_buckets[key].head.next; b != &hash_buckets[key].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->ticks = ticks;
      release(&hash_buckets[key].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // just hold on, don't release bucket lock
  // allocate one
  // just steal from itself or others
  struct buf * ans = 0;
  for(int i = key, cnt = 0; cnt < NBUCKETS ; i = (i+1) % NBUCKETS){
    if(i != key){
      if(!holding(&hash_buckets[i].lock)){
        acquire(&hash_buckets[i].lock);
      } else {
        continue; // to speed up
      }
    }

    cnt++; // that's my logic

    for(b = hash_buckets[i].head.next; b != &hash_buckets[i].head; b = b->next){
      if(b->refcnt == 0 && (ans == 0 || (ans->ticks > b->ticks))){
        ans = b;
      }
    }

    if(ans){
      if(i != key){
        // release it from dual linked list
        ans->next->prev = ans->prev;
        ans->prev->next = ans->next;
        release(&hash_buckets[i].lock);
        // attach it to key's linked list
        ans->next = hash_buckets[key].head.next;
        ans->prev = &hash_buckets[key].head;
        hash_buckets[key].head.next->prev = ans;
        hash_buckets[key].head.next = ans;
      }
      ans->dev = dev;
      ans->blockno = blockno;
      ans->valid = 0;
      ans->refcnt = 1;
      ans->ticks = ticks;
      release(&hash_buckets[key].lock);
      acquiresleep(&ans->lock);
      return ans;
    } else{
      if(i != key) release(&hash_buckets[i].lock);
    }
    
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint64 key = get_hashkey(b->dev, b->blockno);
  acquire(&hash_buckets[key].lock);
  b->ticks = ticks;
  b->refcnt--;
  release(&hash_buckets[key].lock);
}

void
bpin(struct buf *b) {
  uint64 key = get_hashkey(b->dev, b->blockno);
  acquire(&hash_buckets[key].lock);
  b->refcnt++;
  release(&hash_buckets[key].lock);
}

void
bunpin(struct buf *b) {
  uint64 key = get_hashkey(b->dev, b->blockno);
  acquire(&hash_buckets[key].lock);
  b->refcnt--;
  release(&hash_buckets[key].lock);
}


