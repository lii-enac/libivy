#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#endif
#include <stdio.h> // DEBUG, pour printf
#include "ivyfifo.h"
#include "param.h"



#define 	MIN(a, b)   ((a) > (b) ? (b) : (a))


static void IvyFifoRealloc(IvyFifoBuffer *f, unsigned int neededSize);

static unsigned int IvyFifoGenericRead(IvyFifoBuffer *f, const unsigned int buf_size, 
				       void (*func)(void*, void*, int), void* dest);

static void IvyFifoDrain(IvyFifoBuffer *f, int size);

/* static unsigned char IvyFifoPeek(IvyFifoBuffer *f, int offs); */



int IvyFifoInit(IvyFifoBuffer *f)
{
  f->wptr = f->rptr =
    f->buffer = (char *) malloc(IVY_FIFO_ALLOC_SIZE);
  f->end = f->buffer + IVY_FIFO_ALLOC_SIZE;
  f->full = 0;

  if (!f->buffer)
    return -1;
  return 0;
}

void IvyFifoFree (IvyFifoBuffer *f)
{
  free(f->buffer);
  f->wptr = f->rptr = f->buffer = NULL;
}


IvyFifoBuffer* IvyFifoNew (void)
{
  IvyFifoBuffer* ifb = (IvyFifoBuffer*) malloc (sizeof (IvyFifoBuffer));
  IvyFifoInit (ifb);
  return (ifb);
}

void IvyFifoDelete (IvyFifoBuffer *f)
{
  IvyFifoFree (f);
  free (f);
}


unsigned int IvyFifoSize (const IvyFifoBuffer  *f)
{
  return (f->end - f->buffer);
}

unsigned int IvyFifoLength (const IvyFifoBuffer  *f)
{
  long size = f->wptr - f->rptr;
  if (size < 0)
    size += f->end - f->buffer;
  return size;
}

unsigned int IvyFifoAvail(const IvyFifoBuffer  *f)
{
  return (IvyFifoSize (f)- IvyFifoLength (f));
}

unsigned int IvyFifoRead (IvyFifoBuffer *f, char *buf, unsigned int buf_size)
{
  return IvyFifoGenericRead(f, buf_size, NULL, buf);
}

void IvyFifoRealloc (IvyFifoBuffer *f, unsigned int new_size) 
{
  unsigned int old_size= IvyFifoSize (f);
  unsigned int alignedNewSize = ((new_size/IVY_FIFO_ALLOC_SIZE) +1) * IVY_FIFO_ALLOC_SIZE;
  if (alignedNewSize > IVY_FIFO_MAX_ALLOC_SIZE) {
    f->full = 1;
  } else if (old_size < alignedNewSize) {
    int len= IvyFifoLength(f);
    IvyFifoBuffer f2;
    
    f2.wptr = f2.rptr =
      f2.buffer = (char *) malloc(alignedNewSize);
    f2.end = f2.buffer + alignedNewSize;
    f2.full = 0;
    
    IvyFifoRead(f, f2.buffer, len);
    f2.wptr += len;
    free(f->buffer);
    *f= f2;
  }
}

void IvyFifoWrite (IvyFifoBuffer *f, const char *buf, unsigned int size)
{
  if (size >= IvyFifoAvail (f)) {
    IvyFifoRealloc(f, size + IvyFifoLength (f));
  }
  if (f->full) {
    return ;
  }
  do {
    unsigned int len = MIN((unsigned int)(f->end - f->wptr), size);
    memcpy(f->wptr, buf, len);
    f->wptr += len;
    if (f->wptr >= f->end)
      f->wptr = f->buffer;
    buf += len;
    size -= len;
  } while (size > 0);
}


unsigned int IvyFifoGenericRead (IvyFifoBuffer *f, const unsigned int buf_size, void (*func)(void*, void*, int), void* dest)
{
  unsigned int bytesToRead, retV;
  retV = bytesToRead = MIN(buf_size, IvyFifoLength(f));
  
  do {
    unsigned int len = MIN((unsigned int)(f->end - f->rptr), bytesToRead);
    if (func) {
      func (dest, f->rptr, len);
    } else {
      memcpy(dest, f->rptr, len);
      dest = (unsigned char*)dest + len;
    }
    IvyFifoDrain(f, len);
    bytesToRead -= len;
  } while (bytesToRead > 0);
  return (retV);
}


unsigned int IvyFifoSendSocket (IvyFifoBuffer *f, const int fd)
{
  unsigned int  maxLen, realLen;
  
  do {
    maxLen = MIN ((unsigned int)(f->end - f->rptr), IvyFifoLength(f));
#ifdef WIN32
    realLen = send (fd, f->rptr, maxLen, 0);
#else
	realLen = send (fd, f->rptr, maxLen, MSG_DONTWAIT);
#endif
    IvyFifoDrain(f, realLen);
    //    printf ("DBG> maxLen=%d realLen=%d IvyFifoLength=%d\n",
    //    maxLen, realLen, IvyFifoLength(f));
  } while (IvyFifoLength(f) && (maxLen == realLen));

  return (IvyFifoLength(f));
}



void IvyFifoDrain (IvyFifoBuffer *f, int size)
{
  f->rptr += size;
  if (f->rptr >= f->end)
    f->rptr -= f->end - f->buffer;
  if (size > 0) {
    f->full = 0;
  }
}

int IvyFifoIsFull (const IvyFifoBuffer  *f) 
{
  return (f->full);
}

/* unsigned char IvyFifoPeek(IvyFifoBuffer *f, int offs) */
/* { */
/*     unsigned char *ptr = f->rptr + offs; */
/*     if (ptr >= f->end) */
/*         ptr -= f->end - f->buffer; */
/*     return *ptr; */
/* } */


//#define TEST_UNITAIRE 1
#ifdef TEST_UNITAIRE
int main (int argc, char**argv)
{
  if (1 == 2)  {
    IvyFifoBuffer ifb;
    unsigned char toRead [4096];
    unsigned char toWrite [8192];
    int (i);
    
    for (i=0; i< sizeof (toRead); i++) {
      toRead[i] = (char) (i%10)+48;
    }
    
    
    IvyFifoInit (&ifb);
    IvyFifoWrite(&ifb, toRead, 4096);
    printf ("DBG> fifo size=%u, length=%u\n", IvyFifoSize(&ifb), IvyFifoLength(&ifb));
    IvyFifoWrite(&ifb, toRead, 1024);
    printf ("DBG> fifo size=%u, length=%u\n", IvyFifoSize(&ifb), IvyFifoLength(&ifb));
    unsigned int nbRead=IvyFifoRead(&ifb, toWrite, sizeof (toWrite));
    //  int nbRead=IvyFifoRead(&ifb, toWrite, 1024);
    printf ("DBG> ndRead=%u, fifo size=%u, length=%u\n", nbRead, IvyFifoSize(&ifb), IvyFifoLength(&ifb));
  }


  {
    IvyFifoBuffer ifb;
    unsigned char toRead [8];
    unsigned char toWrite [16];
    unsigned int nbRead;
    int (i);
    
    for (i=0; i< sizeof (toRead); i++) {
      toRead[i] = (char) (i%10)+48;
    }
    
    
    IvyFifoInit (&ifb);


    IvyFifoWrite(&ifb, toRead, 8);
    printf ("DBG> fifo size=%u, length=%u\n", IvyFifoSize(&ifb), IvyFifoLength(&ifb));

    nbRead=IvyFifoRead(&ifb, toWrite, 4);
    toWrite[nbRead] = 0;
    printf ("DBG> ndRead=%u, fifo size=%u, length=%u, buffer=%s\n", nbRead, IvyFifoSize(&ifb), 
	    IvyFifoLength(&ifb), toWrite);

    IvyFifoWrite(&ifb, toRead, 8);
    printf ("DBG> fifo size=%u, length=%u\n", IvyFifoSize(&ifb), IvyFifoLength(&ifb));

    nbRead=IvyFifoRead(&ifb, toWrite, 8);
    toWrite[nbRead] = 0;
    printf ("DBG> ndRead=%u, fifo size=%u, length=%u, buffer=%s\n", nbRead, IvyFifoSize(&ifb), 
	    IvyFifoLength(&ifb), toWrite);

   nbRead=IvyFifoRead(&ifb, toWrite, 8);
    toWrite[nbRead] = 0;
    printf ("DBG> ndRead=%u, fifo size=%u, length=%u, buffer=%s\n", nbRead, IvyFifoSize(&ifb), 
	    IvyFifoLength(&ifb), toWrite);

   nbRead=IvyFifoRead(&ifb, toWrite, 8);
    toWrite[nbRead] = 0;
    printf ("DBG> ndRead=%u, fifo size=%u, length=%u, buffer=%s\n", nbRead, IvyFifoSize(&ifb), 
	    IvyFifoLength(&ifb), toWrite);

   nbRead=IvyFifoRead(&ifb, toWrite, 4);
    toWrite[nbRead] = 0;
    printf ("DBG> ndRead=%u, fifo size=%u, length=%u, buffer=%s\n", nbRead, IvyFifoSize(&ifb), 
	    IvyFifoLength(&ifb), toWrite);

    IvyFifoWrite(&ifb, toRead, 8);
    printf ("DBG> fifo size=%u, length=%u\n", IvyFifoSize(&ifb), IvyFifoLength(&ifb));

    nbRead=IvyFifoRead(&ifb, toWrite, 8);
    toWrite[nbRead] = 0;
    printf ("DBG> ndRead=%u, fifo size=%u, length=%u, buffer=%s\n", nbRead, IvyFifoSize(&ifb), 
	    IvyFifoLength(&ifb), toWrite);

   nbRead=IvyFifoRead(&ifb, toWrite, 8);
    toWrite[nbRead] = 0;
    printf ("DBG> ndRead=%u, fifo size=%u, length=%u, buffer=%s\n", nbRead, IvyFifoSize(&ifb), 
	    IvyFifoLength(&ifb), toWrite);

   nbRead=IvyFifoRead(&ifb, toWrite, 8);
    toWrite[nbRead] = 0;
    printf ("DBG> ndRead=%u, fifo size=%u, length=%u, buffer=%s\n", nbRead, IvyFifoSize(&ifb), 
	    IvyFifoLength(&ifb), toWrite);

  }


  return (0);
}
#endif // TEST_UNITAIRE
