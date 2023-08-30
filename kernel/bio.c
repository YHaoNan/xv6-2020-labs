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

#define BUKSIZE 13
#define BUFSIZE (BUKSIZE * 10)
#define hash(dev, blockno) (((dev) % BUKSIZE + (blockno) % BUKSIZE) % BUKSIZE)

struct {
  struct buf buf[BUFSIZE];
  struct spinlock lock;
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

struct buk {
  // head is a pesudo buf node, 
  // head.next is the real first node in bucket
  struct buf head; 
  struct spinlock lock;
} hashtable[BUKSIZE];

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(b = bcache.buf; b < bcache.buf+BUFSIZE; b++){
    initsleeplock(&b->lock, "buffer");
  }

  // 平均分配buf到每个坑位
  b = bcache.buf;
  for(int i=0; i<BUKSIZE; i++) {
    initlock(&hashtable[i].lock, "bcache-buk");
    // buf数量是buk数量的倍数，所以不会有遗漏的
    for (int j=0; j<BUFSIZE/BUKSIZE; j++) {
      b->dev = 0; // 不会有dev=0的
      b->blockno = i;
      b->next = hashtable[i].head.next;
      b->prev = &hashtable[i].head;
      if (hashtable[i].head.next) hashtable[i].head.next->prev = b;
      hashtable[i].head.next = b;
      b++;
    }
  }
}

void init_buf(uint dev, uint blockno, struct buf* b) {
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
}

// 在一个buk中寻找最近最少使用的unusedbuf
// 调用时必须持有buk的锁
// 如果没有，返回null pointer
struct buf* find_lru_unused_buf(struct buk *buk) {
  uint minusedtime = ticks;
  struct buf *popbuf = 0;
  
  for (struct buf *cur=buk->head.next; cur; cur=cur->next) {
    if (cur->refcnt == 0  && cur->lastusetime <= minusedtime) {
      popbuf = cur;
      minusedtime = cur->lastusetime;
    }
  }

  if (popbuf) {
    popbuf->prev->next = popbuf->next;
    if (popbuf->next) popbuf->next->prev = popbuf->prev;
    popbuf->next = 0;
    popbuf->prev = 0;
  }
  return popbuf;
}
void insert_buf_to_buk(struct buf *buf, struct buk *buk) {
  buf->prev = &buk->head;
  buf->next = buk->head.next;
  if (buk->head.next) buk->head.next->prev = buf;
  buk->head.next = buf;
}
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int curbukidx = hash(dev, blockno);
  struct buk *buk = &hashtable[curbukidx];

  acquire(&buk->lock);

  // Is the block already cached?
  struct buf *cur;
  for (cur=buk->head.next; cur; cur=cur->next) {
    if (cur->dev == dev && cur->blockno == blockno) {
      cur->refcnt++;
      release(&buk->lock);
      acquiresleep(&cur->lock);
      return cur;
    }
  }

  // Not cached.
  // 首先尝试从自己的桶剔除最近最少使用的unusedbuf
  struct buf *popbuf = find_lru_unused_buf(buk);
  if (popbuf) {
    init_buf(dev, blockno, popbuf);
    insert_buf_to_buk(popbuf, buk);
    release(&buk->lock);
    acquiresleep(&popbuf->lock);
    return popbuf;
  }
  release(&buk->lock);

  // 尝试从其它的桶剔除最近最少使用的unusedbuf
  struct buk *stealbuk;
  struct spinlock *fst, *sec;
  for (int i=0; i<BUKSIZE; i++) {
    if (i==curbukidx) continue;
    stealbuk = &hashtable[i];
    // 按指定顺序加锁
    fst = i < curbukidx ? &stealbuk-> lock : &buk->lock;
    sec = i < curbukidx ? &buk->lock : &stealbuk->lock;
    acquire(fst); acquire(sec);
    popbuf = find_lru_unused_buf(stealbuk);
    if (popbuf) {
      init_buf(dev, blockno, popbuf);
      insert_buf_to_buk(popbuf, buk);
    }
    release(sec); release(fst);
    if (popbuf) {
      acquiresleep(&popbuf->lock);
      return popbuf;
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

  struct buk *buk = &hashtable[hash(b->dev, b->blockno)];
  
  releasesleep(&b->lock);

  acquire(&buk->lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lastusetime = ticks;
  }
  
  release(&buk->lock);
}

void
bpin(struct buf *b) {
  struct buk *buk = &hashtable[hash(b->dev, b->blockno)];
  acquire(&buk->lock);
  b->refcnt++;
  release(&buk->lock);
}

void
bunpin(struct buf *b) {
  struct buk *buk = &hashtable[hash(b->dev, b->blockno)];
  acquire(&buk->lock);
  b->refcnt--;
  release(&buk->lock);
}