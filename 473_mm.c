// Starter code for the page replacement project
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <signal.h>
#include "473_mm.h"

typedef struct fifo {
  int protection;
  struct fifo* next;
  int pmLocation;
  int pageNum;
} fifoPage;

typedef struct tc {
  int protection;
  struct tc* next;
  int pmLocation;
  int pageNum;
  int r;
  int m;
} tcPage;

void fifo(int num, siginfo_t* info, void* context);
int findPageFIFO(int pg, int difference);
void tc(int num, siginfo_t* info, void* context);
int findPageTC(int pg, int difference);
int clock();

int pageSizes;
int virtualMemorySize;
int numPhysicalPages;
void* virtualAddress;
unsigned int lastPAddress = 0;
fifoPage* headOfFIFO;
fifoPage* tailOfFIFO;
tcPage* headOfTC;
tcPage* tailOfTC;
tcPage* headOfClock;
int fifoQueueAmt;
int tcQueueAmt;

void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy) {
  struct sigaction handler;
  virtualAddress = vm;
  virtualMemorySize = vm_size;
  numPhysicalPages = n_frames;
  pageSizes = page_size;
  mprotect(virtualAddress, virtualMemorySize, PROT_NONE); 
  if(policy == 1) {
    tailOfFIFO = NULL;
    headOfFIFO = NULL;
    fifoQueueAmt = 0;
    handler.sa_sigaction = fifo;
  }
  if(policy == 2) {
    tailOfTC = NULL;
    headOfTC = NULL;
    headOfClock = NULL;
    tcQueueAmt = 0;
    handler.sa_sigaction = tc;
  }
  handler.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &handler, NULL);
}

void fifo(int num, siginfo_t* info, void* context) {
  int pAddress;
  int pg = (info->si_addr - virtualAddress) / pageSizes;
  int difference = (info->si_addr - virtualAddress) % pageSizes;
  int stopped = findPageFIFO(pg, difference);
  if(stopped == 1) {
    return;
  }
  int wb = 0;
  int removedPage = -1;
  int physicalLoc;
  if(fifoQueueAmt < numPhysicalPages) {
    fifoQueueAmt++;
  }
  else if(fifoQueueAmt >= numPhysicalPages) { 
    if(headOfFIFO->protection == (PROT_READ | PROT_WRITE)) {
      wb = 1;
    }
    physicalLoc = headOfFIFO->pmLocation;
    removedPage = headOfFIFO->pageNum;
    fifoPage* toBeFreed = headOfFIFO;
    mprotect((virtualAddress + pageSizes * headOfFIFO->pageNum), pageSizes, PROT_NONE);
    headOfFIFO = headOfFIFO->next;
    free(toBeFreed);
  }
  if(headOfFIFO != NULL) {
    if(removedPage == -1) {
      fifoPage* new = (fifoPage*)malloc(sizeof(fifoPage));
      new->next = NULL;
      new->pageNum = pg;
      new->protection = PROT_READ;
      new->pmLocation = fifoQueueAmt - 1;
      tailOfFIFO->next = new;
      tailOfFIFO = new;
    }
    else if(removedPage != -1) {
      fifoPage* new = (fifoPage*)malloc(sizeof(fifoPage));
      new->next = NULL;
      new->pageNum = pg;
      new->protection = PROT_READ;
      new->pmLocation = physicalLoc;
      tailOfFIFO->next = new;
      tailOfFIFO = new;
    }
  }
  else if(headOfFIFO == NULL) {
    headOfFIFO = (fifoPage*)malloc(sizeof(fifoPage));
    headOfFIFO->next = NULL;
    headOfFIFO->pageNum = pg;
    headOfFIFO->protection = PROT_READ;
    headOfFIFO->pmLocation = 0;
    tailOfFIFO = headOfFIFO;
  }
  mprotect((virtualAddress + pageSizes * pg), pageSizes, PROT_READ);
  if(removedPage != -1) { 
    pAddress = physicalLoc * pageSizes + difference;
    lastPAddress = pAddress;
    mm_logger(pg, 0, removedPage, wb, pAddress);
  }
  else if(removedPage == -1) {
    pAddress = (fifoQueueAmt - 1) * pageSizes + difference;
    lastPAddress = pAddress;
    mm_logger(pg, 0, -1, 0, pAddress); 
  }
}

int findPageFIFO(int pg, int difference) {
  fifoPage* page = headOfFIFO;
  int pAddress;
  int stop = 0;
  while(page != NULL) {
    if(page->pageNum == pg) {
      mprotect((virtualAddress + pageSizes * pg), pageSizes, PROT_READ | PROT_WRITE);
      pAddress = (page->pmLocation * pageSizes) + difference;
      page->protection = (PROT_READ | PROT_WRITE);
      if(pAddress != lastPAddress) {
        mm_logger(pg, 2, -1, 0, pAddress);
      }
      lastPAddress = pAddress;
      stop = 1;
      break;
    }
    page = page->next;
  }
  return stop;
}

void tc(int num, siginfo_t* info, void* context) {
  int pAddress;
  int pg = (info->si_addr - virtualAddress) / pageSizes;
  int difference = (info->si_addr - virtualAddress) % pageSizes;
  int stopped = findPageTC(pg, difference);
  if(stopped == 0) {
    mprotect((virtualAddress + pageSizes * pg), pageSizes, PROT_READ); 
    if(tcQueueAmt >= numPhysicalPages) {
      int wb = clock();
      tcPage* clockHead = headOfClock;
      int loc = clockHead->pmLocation;
      int removedPage = clockHead->pageNum;
      mprotect((virtualAddress + pageSizes * removedPage), pageSizes, PROT_NONE);
      headOfClock = headOfClock->next;
      tcPage* temp = headOfTC;
      while(temp->pageNum != removedPage) {
        temp = temp->next;
      }
      temp->protection = PROT_READ;
      temp->pageNum = pg;
      temp->r = 1;
      temp->m = 0;
      pAddress = loc * pageSizes + difference;
      lastPAddress = pAddress;
      mm_logger(pg, 0, removedPage, wb, pAddress);
    }
    else if(tcQueueAmt < numPhysicalPages) {
      if(headOfTC != NULL) {
        tcPage* new = (tcPage*)malloc(sizeof(tcPage));
        new->next = NULL;
        new->pageNum = pg;
        new->protection = PROT_READ;
        new->pmLocation = tcQueueAmt;
        new->r = 1;
        new->m = 0;
        tailOfTC->next = new;
        new->next = headOfTC;
        tailOfTC = new; 
      }
      else if(headOfTC == NULL) {
        headOfTC = (tcPage*)malloc(sizeof(tcPage));
        headOfTC->next = NULL;
        headOfTC->pageNum = pg;
        headOfTC->protection = PROT_READ;
        headOfTC->pmLocation = 0;
        headOfTC->r = 1;
        headOfTC->m = 0;
        tailOfTC = headOfTC;
        headOfClock = headOfTC; 
      }
      pAddress = tcQueueAmt * pageSizes + difference; 
      lastPAddress = pAddress;
      mm_logger(pg, 0, -1, 0, pAddress); 
      tcQueueAmt++;
    }
  }
}

int findPageTC(int pg, int difference) {
  int pAddress;
  int stop = 0;
  int s;
  tcPage* page = headOfTC;
  while(page != NULL) {
    if(page->pageNum == pg) {
      s = 0;
      if(page->protection == PROT_NONE) {
        s = 1;
        mprotect((virtualAddress + pageSizes * pg), pageSizes, PROT_READ);
        page->protection = PROT_READ;
        page->r = 1;
      }
      else if(page->protection != PROT_NONE) {
        if(page->m == 0) {
          s = 1;
        }
        mprotect((virtualAddress + pageSizes * pg), pageSizes, PROT_READ | PROT_WRITE);
        page->protection = (PROT_READ | PROT_WRITE);
        page->m = 1;
        page->r = 1;
      }
      if(s == 1) {
        pAddress = page->pmLocation * pageSizes + difference;
        if(pAddress != lastPAddress) {
          mm_logger(pg, 0, -1, 0, pAddress);
        }
        lastPAddress = pAddress;
      }
      stop = 1;
      break;
    }
    if(page == tailOfTC) {
      break;
    }
    page = page->next;
  }
  return stop;
}

int clock() {
  int wb = 0;
  while(true) {
    int r = headOfClock->r;
    int m = headOfClock->m;
    if(r == 0 && m == 0) {
      break;
    }
    else if(r == 1 && m == 0) {
      mprotect((virtualAddress + pageSizes * headOfClock->pageNum), pageSizes, PROT_NONE);
      headOfClock->protection = PROT_NONE;
      headOfClock->r = 0;
    }
    else if(r == 0 && m == 1) {
      headOfClock->r = -1;
    }
    else if(r == 1 && m == 1) {
      mprotect((virtualAddress + pageSizes * headOfClock->pageNum), pageSizes, PROT_NONE);
      headOfClock->protection = PROT_NONE;
      headOfClock->r = 0;
    }
    else if(r == -1 && m == 1) {
      wb = 1;
      break;
    }
    headOfClock = headOfClock->next;
  }
  return wb;
}
