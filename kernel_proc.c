
#include <assert.h>
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "util.h"

/* 
 The process table and related system calls:
 - Exec
 - Exit
 - WaitPid
 - GetPid
 - GetPPid

 */

/* The process table */
PCB PT[MAX_PROC];
unsigned int process_count;

PCB* get_pcb(Pid_t pid)
{
  return PT[pid].pstate==FREE ? NULL : &PT[pid];
}

Pid_t get_pid(PCB* pcb)
{
  return pcb==NULL ? NOPROC : pcb-PT;
}

/* Initialize a PCB */
static inline void initialize_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->argl = 0;
  pcb->args = NULL;

  for(int i=0;i<MAX_FILEID;i++)
    pcb->FIDT[i] = NULL;

  rlnode_init(& pcb->children_list, NULL);
  rlnode_init(& pcb->exited_list, NULL);
  rlnode_init(& pcb->children_node, pcb);
  rlnode_init(& pcb->exited_node, pcb);
  pcb->child_exit = COND_INIT;
}


static PCB* pcb_freelist;

void initialize_processes()
{
  /* initialize the PCBs */
  for(Pid_t p=0; p<MAX_PROC; p++) {
    initialize_PCB(&PT[p]);
  }

  /* use the parent field to build a free list */
  PCB* pcbiter;
  pcb_freelist = NULL;
  for(pcbiter = PT+MAX_PROC; pcbiter!=PT; ) {
    --pcbiter;
    pcbiter->parent = pcb_freelist;
    pcb_freelist = pcbiter;
  }

  process_count = 0;

  /* Execute a null "idle" process */
  if(Exec(NULL,0,NULL)!=0)
    FATAL("The scheduler process does not have pid==0");
}


/*
  Must be called with kernel_mutex held
*/
PCB* acquire_PCB()
{
  PCB* pcb = NULL;

  if(pcb_freelist != NULL) {
    pcb = pcb_freelist;
    pcb->pstate = ALIVE;
    pcb_freelist = pcb_freelist->parent;
    process_count++;
  }

  return pcb;
}

/*
  Must be called with kernel_mutex held
*/
void release_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->parent = pcb_freelist;
  pcb_freelist = pcb;
  process_count--;
}


/*
 *
 * Process creation
 *
 */

/*
	This function is provided as an argument to spawn,
	to execute the main thread of a process.
*/


// !!!!!!!!!!!
void start_main_thread()
{
  int exitval;
  PTCB* mainPTCB = CURTHREAD->owner_PTCB;
  assert(mainPTCB != NULL);
  Task call =  mainPTCB->thread_task;
  int argl = mainPTCB->argL;
  void* args = mainPTCB->argS;

  exitval = call(argl,args);  

  Exit(exitval); 
}


/*
	System call to create a new process.
 */
Pid_t sys_Exec(Task call, int argl, void* args)
{
  PCB *curproc, *newproc;

  
  /* The new process PCB */
  newproc = acquire_PCB();

  
  if(newproc == NULL) goto finish;  /* We have run out of PIDs! */

  if(get_pid(newproc)<=1) {
    /* Processes with pid<=1 (the scheduler and the init process) 
       are parentless and are treated specially. */
    newproc->parent = NULL;
  }
  else
  {
    /* Inherit parent */
    curproc = CURPROC;

    /* Add new process to the parent's child list */
    newproc->parent = curproc;
    rlist_push_front(& curproc->children_list, & newproc->children_node);

    /* Inherit file streams from parent */
    for(int i=0; i<MAX_FILEID; i++) {
       newproc->FIDT[i] = curproc->FIDT[i];
       if(newproc->FIDT[i])
          FCB_incref(newproc->FIDT[i]);
    }
  }


  /* Set the main thread's function */
  newproc->main_task = call;

  /* Copy the arguments to new storage, owned by the new process */
  newproc->argl = argl;
  if(args!=NULL) {
    newproc->args = malloc(argl);
    memcpy(newproc->args, args, argl);
  }
  else
    newproc->args=NULL;

/////////
  newproc->num_threads=1; //ari8mos twn thread
  newproc->exit_called=0; //gia otan kales8ei h Exit

  /* 
    Create and wake up the thread for the main function. This must be the last thing
    we do, because once we wakeup the new thread it may run! so we need to have finished
    the initialization of the PCB.
   */

 PTCB* main_ptcb=(PTCB*)xmalloc(sizeof(PTCB));
    main_ptcb->argL=argl;
    main_ptcb->thread_task=call;
    main_ptcb->exited=0;
    main_ptcb->detouched=0;
    main_ptcb->C_v_join=COND_INIT;
    main_ptcb->ref_counter=1;
    main_ptcb->father_process=newproc;

    if(args!=NULL) {
    main_ptcb->argS = malloc(argl);
    memcpy(main_ptcb->argS, args, argl);
   }
   else
    {main_ptcb->argS=NULL;}


   
    rlnode_init(&newproc->PTCB_lista,NULL); //arxikopoioume thn lista 
    rlnode* ptcb_rlnode_P=rlnode_init(&main_ptcb->ptcb_node,main_ptcb); //ftiaxnoume to PTCB node
    rlist_push_back(&newproc->PTCB_lista,ptcb_rlnode_P); // to pros8etoume sthn lista 
   

  if(call != NULL) {

    main_ptcb->tcb_pointer = spawn_thread(newproc, start_main_thread,main_ptcb);
    curproc->main_thread=main_ptcb->tcb_pointer;
    wakeup(main_ptcb->tcb_pointer);
  }


finish:

  return get_pid(newproc);
}


/* System call */
Pid_t sys_GetPid()
{
  return get_pid(CURPROC);
}


Pid_t sys_GetPPid()
{
  return get_pid(CURPROC->parent);
}


static void cleanup_zombie(PCB* pcb, int* status)
{
  if(status != NULL)
    *status = pcb->exitval;

  rlist_remove(& pcb->children_node);
  rlist_remove(& pcb->exited_node);

  release_PCB(pcb);
}


static Pid_t wait_for_specific_child(Pid_t cpid, int* status)
{

  /* Legality checks */
  if((cpid<0) || (cpid>=MAX_PROC)) {
    cpid = NOPROC;
    goto finish;
  }

  PCB* parent = CURPROC;
  PCB* child = get_pcb(cpid);
  if( child == NULL || child->parent != parent)
  {
    cpid = NOPROC;
    goto finish;
  }

  /* Ok, child is a legal child of mine. Wait for it to exit. */
  while(child->pstate == ALIVE)
    kernel_wait(& parent->child_exit, SCHED_USER);
  
  cleanup_zombie(child, status);
  
finish:
  return cpid;
}


static Pid_t wait_for_any_child(int* status)
{
  Pid_t cpid;

  PCB* parent = CURPROC;

  /* Make sure I have children! */
  if(is_rlist_empty(& parent->children_list)) {
    cpid = NOPROC;
    goto finish;
  }

  while(is_rlist_empty(& parent->exited_list)) {
    kernel_wait(& parent->child_exit, SCHED_USER);
  }

  PCB* child = parent->exited_list.next->pcb;
  assert(child->pstate == ZOMBIE);
  cpid = get_pid(child);
  cleanup_zombie(child, status);

finish:
  return cpid;
}


Pid_t sys_WaitChild(Pid_t cpid, int* status) 
{
  /* Wait for specific child. */
  if(cpid != NOPROC) {
    return wait_for_specific_child(cpid, status);
  }
  /* Wait for any child */
  else {
    return wait_for_any_child(status);
  }

}


void sys_Exit(int exitval)
{
  

  /* Right here, we must check that we are not the boot task. If we are, 
     we must wait until all processes exit. */
  if(sys_GetPid()==1) {
    while(sys_WaitChild(NOPROC,NULL)!=NOPROC);
  }

  PCB* curproc = CURPROC;  /* cache for efficiency */
   PTCB* PTCB_pointer = CURTHREAD->owner_PTCB;
    assert(PTCB_pointer!=NULL);
  if(curproc->exit_called==0)
    {
          curproc->exit_called=1;
          PTCB_pointer->detouched=1;
          Cond_Broadcast(&PTCB_pointer->C_v_join);
        //Perimenoume ola ta thread na kanoun threadExit
           while(CURPROC->num_threads>1)
           {
            kernel_wait_wchan(&curproc->Wait_others,SCHED_USER,NULL,NO_TIMEOUT);
           };
           
         

          /* Do all the other cleanup we want here, close files etc. */
          if(curproc->args) {
            free(curproc->args);
            curproc->args = NULL;
          }

          /* Clean up FIDT */
          for(int i=0;i<MAX_FILEID;i++) {
            if(curproc->FIDT[i] != NULL) {
              FCB_decref(curproc->FIDT[i]);
              curproc->FIDT[i] = NULL;
            }
          }

          /* Reparent any children of the exiting process to the 
             initial task */
          PCB* initpcb = get_pcb(1);
          while(!is_rlist_empty(& curproc->children_list)) {
            rlnode* child = rlist_pop_front(& curproc->children_list);
            child->pcb->parent = initpcb;
            rlist_push_front(& initpcb->children_list, child);
          }

          /* Add exited children to the initial task's exited list 
             and signal the initial task */
          if(!is_rlist_empty(& curproc->exited_list)) {
            rlist_append(& initpcb->exited_list, &curproc->exited_list);
            kernel_broadcast(& initpcb->child_exit);
          }

          /* Put me into my parent's exited list */
          if(curproc->parent != NULL) {   /* Maybe this is init */
            rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
            kernel_broadcast(& curproc->parent->child_exit);
          }

          

        /*Free the Current PCB's PTCB list*/
          while (!is_rlist_empty(&curproc->PTCB_lista)) {
            rlnode *tmp = rlist_pop_front(&curproc->PTCB_lista);
            free(tmp->ptcb);
          }
          //rlist_remove(&PTCB_pointer->ptcb_node);
          //free(PTCB_pointer);


          /* Disconnect my main_thread */
          curproc->main_thread = NULL;

          /* Now, mark the process as exited. */
          curproc->pstate = ZOMBIE;
          curproc->exitval = exitval;

          /* Bye-bye cruel world */
          kernel_sleep(EXITED, SCHED_USER);
    }
else
    {
      
        CURPROC->num_threads--;
       
        PTCB_pointer->exited=1;
        PTCB_pointer->ref_counter--;
        PTCB_pointer->exit_value=exitval;

        if(PTCB_pointer->ref_counter==0)
        {
          //rlist_remove(&PTCB_pointer->ptcb_node);
          //free(PTCB_pointer);
        }
        else
        Cond_Broadcast(&PTCB_pointer->C_v_join);


        Cond_Broadcast(&CURPROC->Wait_others);
        kernel_sleep(EXITED, SCHED_USER);
       
    } 
} 

//info
static int info_read(void* info_cb, char *buf, unsigned int size)
{
  infocb* info_cb_p=(infocb*)info_cb;
 
  int j;
   for(j=0;j<size;j++){
    if (info_cb_p->Cursor_read==info_cb_p->Cursor_write)
         return 0;
    

    else{
        buf[j]=info_cb_p->buffer[info_cb_p->Cursor_read];
        info_cb_p->Cursor_read++;
     }
   }
    
   return j;

}


static int close_info(void* info_cb) 
{
  infocb* info_cb_p=(infocb*)info_cb;
  free(info_cb_p);
  return 0;
}


file_ops info_ops = {
  .Open=NULL,
  .Read = info_read,
  .Write =NULL,
  .Close =close_info
};


Fid_t sys_OpenInfo()
{

  Fid_t fid;
  FCB* fcb;
  if(FCB_reserve(1, &fid, &fcb)==0)
  {
    printf("Fids are exhausted.\n");
    return NOFILE;
  }

  infocb* info_cb_p=(infocb*)xmalloc(sizeof(infocb));
  info_cb_p->Cursor_write=0;
  info_cb_p->Cursor_read=0;

  fcb->streamobj=info_cb_p;
  fcb->streamfunc=&info_ops;

 int i;
  procinfo* small_buf=(procinfo*)xmalloc(sizeof(procinfo));
  for(i=0;i<MAX_PROC;i++){
      PCB* pcb = &PT[i];
      if (pcb->pstate == ALIVE || pcb->pstate == ZOMBIE) {
        small_buf->pid = get_pid(&PT[i]);
        if(small_buf->pid==1)
          small_buf->ppid=NOPROC;
        else
        small_buf->ppid = get_pid(pcb->parent);
        

        small_buf->thread_count=pcb->num_threads;
        small_buf->main_task = pcb->main_task;
        small_buf->argl = pcb->argl;

        if(pcb->pstate == ZOMBIE){
          small_buf->alive=0;
        }
        else{
          small_buf->alive=1;
       }
          if(pcb->args!=NULL)
          memcpy(small_buf->args,pcb->args, pcb->argl);
        
        
        memcpy(&info_cb_p->buffer[info_cb_p->Cursor_write],small_buf, sizeof(procinfo));
        info_cb_p->Cursor_write=info_cb_p->Cursor_write+sizeof(procinfo);
     }
  }

  free(small_buf);
	return fid;
}

