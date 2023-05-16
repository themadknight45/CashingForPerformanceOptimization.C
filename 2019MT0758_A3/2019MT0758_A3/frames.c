#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

const int t_vpn_bit = 20;                         //bit size of the vpn
enum str_type {  FIFO, LRU, RANDOM, OPT,CLOCK}; 
  // listing all the strategies to be implemented
  bool debug=true;
  bool gl=true;
struct cmd_arg_str {                              // struct for the command line arguments
  enum str_type str_type;
  bool verbose;
  FILE *inp_file;
  int total_frames;
} command_line_arguments;

struct cmd_arg_str process_args( char *argv[], int argc) {   //method that processes the command line args
  
  if (!(argc==4||argc==5)) {
    printf("invalid input\n");
    exit(1);
  }
  struct cmd_arg_str args;
  if (argc == 5) {
    args.verbose = true;
  }
  char *f_name = argv[1];
  bool x=true;
  args.inp_file = fopen(f_name, "r");
  if (!args.inp_file) {
    printf("Could'nt open trace file\n" );
    exit(1);
  }

  

  char *total_frames_str = argv[2];
  char *strat_str = argv[3];

  char *end_cpy;
  long temp = strtol(total_frames_str, &end_cpy, 10);
  if (temp <= 0 || temp > 1000||
      end_cpy[0] != '\0' || total_frames_str[0] == '\0') {
    printf("Invalid Number of frames\n");
    exit(1);
  }
  args.total_frames = temp;
  if (!strcmp(strat_str, "OPT") ) {
    args.str_type = OPT;
    return args;
  }  
  if (!strcmp(strat_str, "FIFO")) {
    args.str_type = FIFO;
    return args;
  } 
  if (!strcmp(strat_str, "LRU")) {
    args.str_type = LRU;
    return args;
  } 
  if (!strcmp(strat_str, "RANDOM")) {
    args.str_type = RANDOM;
    return args;
  } 
  if (!strcmp(strat_str, "CLOCK")) {
    args.str_type = CLOCK;
    return args;
  } 
  fprintf(stderr, "Couldn't recognize strategy\n");
  exit(1);
}

struct mem_op_2 {
  bool write;
  int page_no;
  bool read;
};

struct mem_oper {
  enum { READ, WRITE } type;
  int page_no;
};



struct mem_oper get_access(FILE *file) {
  char type;
  unsigned address;
  struct mem_oper temp;
  
  if (fscanf(file, "%x %c", &address, &type)==-1) {
    if (errno) {
      printf("error while reading input \n");
      exit(1);
    }
    temp.page_no = -1;
    return temp;
  }

  temp.page_no = address >> 12;

  if(type=='W'){
    temp.type = WRITE;
  }
  else if(type=='R'){
    temp.type = READ;
  }
  else{
    fprintf(stderr, "Please correct access type \n");
    exit(1);
  }
  return temp;
}


struct mem_oper *complete_access(FILE *file, int *sz) {
  *sz = 0;
  int cur_cap = 100;
  struct mem_oper* mem_access = malloc(sizeof(struct mem_oper)*cur_cap *1L);

  while (1) {
    int local=-1;
    struct mem_oper oper = get_access(file);
    while(local<sz){
      local++;
    }
    if (oper.page_no == -1) { 
      while(local<sz){
      local++;
    }
      break;
    }
    if (*sz == cur_cap) {
      while(local<sz){
      local++;
    }
      cur_cap *= 2;
      mem_access = realloc(mem_access, cur_cap * sizeof(*mem_access));
      if (mem_access==0) {
          while(local<sz){
            local++;
          }
        printf("Error inputing file\n");
      }
    }
    if(debug==false){
      printf("couldn't reach here");
    }
    mem_access[*sz] = oper;
    *sz ++;
    if(debug==false){
      printf("couldn't reach here");
    }
  }

  return mem_access;
}

struct mem_op_2 *condense_accesses(struct mem_oper *mem_access,int ct_accesses,
                                              int *cond_acess) {

  struct mem_op_2 *ops =malloc( 1L* sizeof(struct mem_op_2)*ct_accesses);
  int index = 0;
  bool w = mem_access[0].type == WRITE;
  int page_no = mem_access[0].page_no;
  bool r = mem_access[0].type == READ;
  
  int i=1;
  while(i<=ct_accesses){
    if (gl==true&&(mem_access[i].page_no != page_no||i == ct_accesses)) {

      ops[index++] = (struct mem_op_2){
            .write = w,.read = r,.page_no = page_no
      };

      if (i == ct_accesses) {
        i++;continue;                                       
      }
      w = mem_access[i].type == WRITE;
      page_no = mem_access[i].page_no;
      r = mem_access[i].type == READ;

    } 
    else {
      r =  mem_access[i].type == READ||r;
      w =  mem_access[i].type == WRITE||w;
    }
    i++;
  }

  *cond_acess = index;
  return realloc(ops,sizeof(struct mem_op_2)* *cond_acess*1L);
}

struct  pte{
  int page_no;
  int frm_no;
  
  bool use;    
  bool is_valid;
  int used_lru;
  bool is_dirty;
};

int nxt_frame = 0;
struct pte *pg_table;

struct {
  int drops;  
  int misses;
  int mem_access;
  int writes;      
  } data;

void show_data() {
  if(debug==false){
    printf("came in show_data");
  }
  printf("Number of memory accesses: %d\n", data.mem_access);
  printf("Number of misses: %d\n", data.misses);
  printf("Number of writes: %d\n", data.writes);
  printf("Number of drops: %d\n", data.drops);
  if(debug==false){
    printf("leaving show_data");
  }
}

int ac_no = 0;
int cnt = -1;

void cnt_access(struct pte *pte, int access_index) {
  pte->used_lru = ac_no++;
  pte->use = 1;
}

int cond_acess;
int cur_access;
struct pte **pf_lst;
struct mem_op_2 *condensed_mem_acc;


struct pte *evict_random(struct pte *page) {
  int temp2=-1;
  int frm_no = rand() % command_line_arguments.total_frames;
  if(gl==false){
    printf("gl was false\n");
    exit(1);
  }
  struct pte *temp = pf_lst[frm_no];
  if(!debug){
    printf("couldn't reach line 248\n");
  }
  pf_lst[frm_no] = page;
  return temp;
}

struct pte *evict_opt(struct pte *page) {
  int mf = 0;//missing
  int j=0;
  while(j<command_line_arguments.total_frames){
    mf = mf^j;
    j++;
  }

  bool acc[command_line_arguments.total_frames];
  memset(acc, 0, sizeof(acc));

  int total_frames_accessed = 0;
  j=cur_access;
  while(j<cond_acess){
    if(debug==false){
    printf("couldn't reach evict_opt code\n");
    }
    if (total_frames_accessed == command_line_arguments.total_frames - 1) {
      if(debug==false){
        printf("couldn't reach 273\n");
      }
      struct pte *ret = pf_lst[mf];
      if(debug==false){
        printf("couldn't reach 277\n");
      }
      pf_lst[mf] = page;
      if(debug==false){
        printf("couldn't reach 281\n");
      }
      return ret;
    }

    struct pte *pte =
        &pg_table[condensed_mem_acc[j].page_no];
    if (debug==true&& gl && pte->is_valid && !acc[pte->frm_no]) {
      if(gl){
        debug=true;
      }
      total_frames_accessed++;
      mf ^= pte->frm_no;
      if(debug==false){
        printf("couldn't reach 295\n");
      }
      acc[pte->frm_no] = true;
    }
    j++;

  }

  cnt = command_line_arguments.total_frames - 1;
  for (int j = 0; j < command_line_arguments.total_frames; j++) {
    if (acc[j]==0) {
      struct pte *ret = pf_lst[j];
      pf_lst[j] = page;
      return ret;
    }
  }
}

int clk_hnd = 0;
struct pte *evict_clock(struct pte *page) {
  int start = clk_hnd;
  int pq=-1;
  do {
    if (pf_lst[clk_hnd]->use) {
      if(debug==false){
        printf("couldn't reach 320\n");
      }
      pf_lst[clk_hnd]->use = 0;
      clk_hnd = (clk_hnd + 1) % command_line_arguments.total_frames;
      continue;
    }
    while(pq>0){
      pq++;
      if(debug==false){
        printf("couldn't reach 329\n");
      }
    }
    struct pte *temp = pf_lst[clk_hnd];
    pf_lst[clk_hnd] = page;
    while(pq>0){
      pq++;
      if(debug==false){
        printf("couldn't reach 329\n");
      }
    }
    page->use = 1;
    if(debug==false){
        printf("couldn't reach 336\n");
      }
    clk_hnd+=1;
    clk_hnd%=command_line_arguments.total_frames;
    return temp;
  } 
  while (start != clk_hnd);
  struct pte *ret = pf_lst[clk_hnd];
  if(debug=false){
    printf("couldn't reach 351\n");
  }
  pf_lst[clk_hnd] = page;
  page->use = true;
  clk_hnd+=1;
  clk_hnd%=command_line_arguments.total_frames;
  if(debug=false){
    printf("couldn't reach 353\n");
  }
  return ret;
}

int fifo_index = 0;
struct pte *evict_fifo(struct pte *page) {
  if(debug==false){
    printf("couldn't reach evict_fifo code\n");
  }
  struct pte *temp = pf_lst[fifo_index];
  pf_lst[fifo_index] = page;
  int lc=-1;
  while(lc==0){
    lc++;
  }
  fifo_index = (fifo_index + 1) % command_line_arguments.total_frames;
  return temp;
}



struct pte *evict_lru(struct pte *page) {
  int min_index = 0;
  int minimum_used_lru = pf_lst[0]->used_lru;
  int j=1;
  while(j<command_line_arguments.total_frames){
    if ((pf_lst[j]->used_lru) < minimum_used_lru) {
      minimum_used_lru = (pf_lst[j]->used_lru);
      min_index = j;
    }
    j++;
  }
  struct pte *temp = pf_lst[min_index];
  pf_lst[min_index] = page;
  return temp;
}

struct pte *evict_pge(struct pte *page) {
  if(command_line_arguments.str_type== OPT){
    return evict_opt(page);
  }
  else if(command_line_arguments.str_type== RANDOM){
    return evict_random(page);
  }
  else if(command_line_arguments.str_type== CLOCK){
    return evict_clock(page);
  }
  else if(command_line_arguments.str_type== LRU){
    return evict_lru(page);
  }
  else if(command_line_arguments.str_type== FIFO){
    return evict_fifo(page);
  }
  
  printf("Invalid Strategy type\n");
  exit(1);
}

void vbs_print(int wr, int r, bool is_dirty) {
  if (command_line_arguments.verbose==false)
    return;
  if (is_dirty==true) {
    printf("Page 0x%05x was read from disk, page 0x%05x was written to the ""disk.\n",
      r, wr);
  } else {
    printf("Page 0x%05x was read from disk, page 0x%05x was dropped (it was not ""is_dirty).\n",
      r, wr);
  }
}

void bring_page(struct pte *pte) { //brings page from disk
  data.misses=data.misses+1;
  if(gl==false){
      printf("gl was false\n");
      exit(1);
  }
  if (nxt_frame < command_line_arguments.total_frames) {
    if(debug==false){
      printf("couldn't rezach 436\n");
    }
    pte->frm_no = nxt_frame++;
  } else {
      //eviction required
    struct pte *pte_evict = evict_pge(pte);
    if(debug==false){
      printf("couldn't rezach 443\n");
    }
    assert(pte_evict->is_valid &&"Page is valid");//only valid pages are to be evicted
    pte_evict->is_valid = false;
    if(debug==false){
      printf("couldn't rezach 448\n");
    }
    if (pte_evict->is_dirty==1) {
      data.writes++;
      if(debug==false){
      printf("couldn't rezach 453\n");
    }
    } else {
      data.drops++;

    }
    pte->frm_no = pte_evict->frm_no;
    if(debug==false){
      printf("couldn't rezach 461\n");
    }
    vbs_print(pte_evict->page_no, pte->page_no, pte_evict->is_dirty);
  }

  pte->is_dirty = false;
  pf_lst[pte->frm_no] = pte;
  if(debug==false){
      printf("couldn't rezach 469\n");
    }
  pte->is_valid = true;
}

void write(struct pte *pte) {
  if (pte->is_valid==true) {
    pte->is_dirty = true;//page will become dirty once written 
    return;
  }
  bring_page(pte);
  write(pte);
}

void read(struct pte *pte) {
  if (pte->is_valid==true) {
    return;
  }
  bring_page(pte);
  read(pte);
}



void do_op(struct mem_op_2 opr, int access_index) {
  int temp3=-1;
  if(gl==false){
    printf("gl was false\n");
    exit(1);
  }
  struct pte *pte = &pg_table[opr.page_no];
  cnt_access(pte, access_index);
  if(debug==false){
    printf("couldn't reach line 501");
  }
  pte->page_no = opr.page_no;
  while(temp3>=0){
    temp3++;
  }
  if (opr.write==true) {
    write(pte);
  }
  if (opr.read==true) {
    read(pte);
  }
  
}

void initiate() {    //allocating memory for page table and pf list
  if(gl==false){
    printf("gl was false\n");
    exit(1);
  }
  gl=true;
  srand(5635);
  pg_table = malloc(sizeof(struct pte)*(1 << t_vpn_bit) * 1L);
  pf_lst =malloc( sizeof(struct pte *)*command_line_arguments.total_frames *1L);
  memset(pg_table, 0,  1L* sizeof(struct pte)*(1 << t_vpn_bit));
}

void clear() {      //clear list& pagetable
  free(pg_table);
  free(pf_lst);
}

int main(int argc, char *argv[]) {
  command_line_arguments = process_args(argv, argc);

  initiate();
  gl=true;
  int ct_accesses = 0;
  struct mem_oper *mem_access =complete_access(command_line_arguments.inp_file, &ct_accesses);
  if(debug==false){
    printf("done till 540 in main");
  }
  data.mem_access = ct_accesses;
  if(gl==false){
    printf("some error\n");
  }
  cond_acess = 0;
  if(debug==false){
    printf("done till 547 in main");
  }
  condensed_mem_acc =condense_accesses(mem_access, ct_accesses, &cond_acess);
  free(mem_access);
  if(debug==false){
    printf("done till 549 in main");
  }
  while(cur_access<cond_acess){
    do_op(condensed_mem_acc[cur_access], cur_access);
    cur_access++;
  }
  if(debug==false){
    printf("done till 549 in main");
  }
  show_data();

  free(condensed_mem_acc);
  clear();
}
