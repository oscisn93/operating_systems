//  mem_mgr.cpp
//
#include <assert.h>
#include <cassert>
#include <list>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma warning(disable : 4996)

#define ARGC_ERROR 1
#define FILE_ERROR 2
#define test 11

#define FRAME_SIZE 256
#define FIFO 1
#define LRU 0
#define REPLACE_POLICY FIFO

// SET TO 128 to use replacement policy: FIFO or LRU,
#define NFRAMES 128
#define PTABLE_SIZE 256
#define TLB_SIZE 16

// an object that represents a page node
// with overloaded equality / inequality operators
struct page_node {
  // index in pg_table
  size_t npage;
  size_t frame_num;
  bool is_present;
  bool is_used;
  // strict equality of all page node atttributes
  bool operator==(const page_node &other) {
    return npage == other.npage && frame_num == other.frame_num &&
           is_present == other.is_present && is_used == other.is_used;
  }
  bool operator!=(const page_node &other) { return !(*this == other); }
};

char *ram = (char *)malloc(NFRAMES * FRAME_SIZE);
page_node pg_table[PTABLE_SIZE];
std::list<page_node> tlb;

const char *passed_or_failed(bool condition) {
  return condition ? " + " : "fail";
}
size_t failed_asserts = 0;

size_t get_page(size_t x) { return 0xff & (x >> 8); }
size_t get_offset(size_t x) { return 0xff & x; }

void get_page_offset(size_t x, size_t &page, size_t &offset) {
  page = get_page(x);
  offset = get_offset(x);
  printf("x is: %zu, page: %zu, offset: %zu, address: %zu, paddress: %zu\n", x,
         page, offset, (page << 8) | get_offset(x), page * 256 + offset);
}

void update_frame_ptable(size_t npage, size_t frame_num) {
  pg_table[npage].frame_num = frame_num;
  pg_table[npage].is_present = true;
  pg_table[npage].is_used = true;
}

int find_frame_ptable(size_t frame) { // FIFO
  for (int i = 0; i < PTABLE_SIZE; i++) {
    if (pg_table[i].frame_num == frame && pg_table[i].is_present == true) {
      return i;
    }
  }
  return -1;
}

size_t get_used_ptable() { // LRU
  size_t unused = -1;
  for (size_t i = 0; i < PTABLE_SIZE; i++) {
    if (pg_table[i].is_used == false && pg_table[i].is_present == true) {
      return (size_t)i;
    }
  }
  // set all page entry used flags to false
  for (size_t i = 0; i < PTABLE_SIZE; i++) {
    pg_table[i].is_used = false;
  }
  for (size_t i = 0; i < PTABLE_SIZE; i++) {
    page_node &r = pg_table[i];
    if (!r.is_used && r.is_present) {
      return i;
    }
  }
  return (size_t)-1;
}

int check_tlb(size_t page) {
  for (page_node i : tlb) {
    if (i.npage == page) {
      return i.frame_num;
    }
  }
  return -1;
}

void open_files(FILE *&fadd, FILE *&fcorr, FILE *&fback) {
  fadd = fopen("addresses.txt", "r");
  if (fadd == NULL) {
    fprintf(stderr, "Could not open file: 'addresses.txt'\n");
    exit(FILE_ERROR);
  }

  fcorr = fopen("correct.txt", "r");
  if (fcorr == NULL) {
    fprintf(stderr, "Could not open file: 'correct.txt'\n");
    exit(FILE_ERROR);
  }

  fback = fopen("BACKING_STORE.bin", "rb");
  if (fback == NULL) {
    fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");
    exit(FILE_ERROR);
  }
}
void close_files(FILE *fadd, FILE *fcorr, FILE *fback) {
  fclose(fadd);
  fclose(fcorr);
  fclose(fback);
}

void initialize_pg_table_tlb() {
  for (int i = 0; i < PTABLE_SIZE; ++i) {
    pg_table[i].npage = (size_t)i;
    pg_table[i].is_present = false;
    pg_table[i].is_used = false;
  }
  for (page_node i : tlb) {
    i.npage = (size_t)-1;
    i.is_present = false;
    i.is_used = false;
  }
}

void summarize(size_t pg_faults, size_t tlb_hits) {
  printf("\nPage Fault Percentage: %1.3f%%", (double)pg_faults / 1000);
  printf("\nTLB Hit Percentage: %1.3f%%\n\n", (double)tlb_hits / 1000);
  printf("ALL logical ---> physical assertions PASSED!\n");
  printf("\n\t\t...done.\n");
}

// adding new entries to tlb
void tlb_add(page_node entry) {
  if (tlb.size() >= TLB_SIZE && tlb.front() != entry) {
    tlb.pop_front();
  }
  tlb.remove(entry);
  tlb.push_back(entry);
}

// replace page
void tlb_remove(size_t &frame) {
  for (page_node i : tlb) {
    if (i.frame_num == frame) {
      tlb.remove(i);
    }
  }
}

void tlb_hit(size_t &frame, size_t &page, size_t &tlb_hits, int result) {
  frame = result;
  tlb_hits++;
}

void tlb_miss(size_t &frame, size_t &page, size_t &tlb_track) {
  frame = pg_table[page].frame_num;
  tlb_add(pg_table[page]);
  tlb_track = tlb.size();
}

void fifo_replace_page(size_t &frame) {
  size_t pg_num = find_frame_ptable(frame);
  pg_table[pg_num].is_present = false;
  pg_table[pg_num].is_used = false;
}

void lru_replace_page(size_t &frame) {
  size_t pg_num = get_used_ptable();
  frame = pg_table[pg_num].frame_num;
  pg_table[pg_num].is_present = false;
  pg_table[pg_num].is_used = false;
}

void page_fault(size_t &frame, size_t &page, size_t &frames_used,
                size_t &pg_faults, size_t &tlb_track, FILE *fbacking) {
  printf("pg fault\t");
  unsigned char buf[BUFSIZ];
  memset(buf, 0, sizeof(buf));
  bool is_memfull = false;

  ++pg_faults;
  if (frames_used >= NFRAMES) {
    is_memfull = true;
  }
  // FIFO only
  frame = frames_used % NFRAMES;
  // TODO
  if (is_memfull) {
    if (REPLACE_POLICY == FIFO) {
      fifo_replace_page(frame);
    } else {
      lru_replace_page(frame);
    }
  }
  // load page into RAM, update pg_table, TLB
  fseek(fbacking, page * FRAME_SIZE, SEEK_SET);
  fread(buf, FRAME_SIZE, 1, fbacking);

  for (int i = 0; i < FRAME_SIZE; i++) {
    *(ram + (frame * FRAME_SIZE) + i) = buf[i];
  }
  update_frame_ptable(page, frame);
  tlb_add(pg_table[page]);
  tlb_track++;
  if (tlb_track > 15) {
    tlb_track = 0;
  }
  ++frames_used;
}

void check_address_value(size_t logic_add, size_t page, size_t offset,
                         size_t physical_add, size_t &prev_frame, size_t frame,
                         int val, int value, size_t o) {

  printf("\nlog: %5lu 0x%04x (pg:%3lu, off:%3lu)-->phy: %5lu (frm: %3lu) (prv: "
         "%3lu)--> val: %4d == value: %4d -- %s",
         logic_add, (unsigned int)logic_add, page, offset, physical_add, frame,
         prev_frame, val, value, passed_or_failed(val == value));

  if (frame < prev_frame) {
    printf("   HIT!\t");
  } else {
    prev_frame = frame;
    printf("----> pg_fault\t");
  }
  if (val != value) {
    ++failed_asserts;
  }
  if (failed_asserts > 5) {
    exit(-1);
  }
  assert(val == value);
}

void run_simulation() {
  // addresses, pages, frames, values, hits and faults
  size_t logic_add, virt_add, phys_add, physical_add;
  size_t page, frame, offset, value, prev_frame = 0, tlb_track = 0;
  size_t frames_used = 0, pg_faults = 0, tlb_hits = 0;
  int val = 0;
  char buf[BUFSIZ];
  // physical memory to store the frames
  bool is_memfull = false;

  initialize_pg_table_tlb();

  // addresses to test, correct values, and pages to load
  FILE *faddress, *fcorrect, *fbacking;
  open_files(faddress, fcorrect, fbacking);

  for (int o = 0; o < 1000; o++) {
    // read from file correct.txt
    fscanf(fcorrect, "%s %s %lu %s %s %lu %s %ld", buf, buf, &virt_add, buf,
           buf, &phys_add, buf, &value);
    fscanf(faddress, "%ld", &logic_add);
    get_page_offset(logic_add, page, offset);

    int result = check_tlb(page);
    printf("------------ Iteration Number %d ----------\tpage:%zu\n", (o + 1),
           page);
    if (result >= 0) {
      tlb_hit(frame, page, tlb_hits, result);
    } else if (pg_table[page].is_present) {
      tlb_miss(frame, page, tlb_track);
    } else {
      // page fault
      page_fault(frame, page, frames_used, pg_faults, tlb_track, fbacking);
    }
    physical_add = (frame * FRAME_SIZE) + offset;
    val = (int)*(ram + physical_add);

    check_address_value(logic_add, page, offset, physical_add, prev_frame,
                        frame, val, value, o);
    printf("\nframes used: %zu\n", frames_used);
  }
  close_files(faddress, fcorrect, fbacking);
  // and time to wrap things up
  free(ram);
  summarize(pg_faults, tlb_hits);
}

int main(int argc, const char *argv[]) {
  run_simulation();
  // allows asserts to fail silently and be counted
  printf("\nFailed asserts: %lu\n\n", failed_asserts);
  return 0;
}
