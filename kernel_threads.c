
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"





/*
  This function is provided as an argument to spawn,
  to execute the non-main thread of a process.
*/
void start_other_thread()
{
  int exitval;
  PTCB* otherPTCB = CURTHREAD->owner_PTCB;
  assert(otherPTCB != NULL);
  Task call =  otherPTCB->thread_task;
  int argl = otherPTCB->argL;
  void* args = otherPTCB->argS;

  exitval = call(argl,args);  

  ThreadExit(exitval); 
}



/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
   PTCB* create_PTCB=(PTCB*)malloc(sizeof(PTCB));

    create_PTCB->argL=argl;
    create_PTCB->thread_task=task;
    create_PTCB->exited=0;
    create_PTCB->detouched=0;
    create_PTCB->C_v_join=COND_INIT;
    create_PTCB->ref_counter=1;
    create_PTCB->argS=args;
    create_PTCB->father_process=CURPROC;
    CURPROC->num_threads++;

    rlnode* ptcb_node_P=rlnode_init(&create_PTCB->ptcb_node,create_PTCB);//ftiaxnoume to node 
    rlist_push_back(&CURPROC->PTCB_lista,ptcb_node_P);//to bazoume sthn lista

  
   
  if(task != NULL) {

    create_PTCB->tcb_pointer = spawn_thread(CURPROC, start_other_thread,create_PTCB);
    wakeup(create_PTCB->tcb_pointer);
  }


	return ((Tid_t)create_PTCB);
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
  PTCB* selfPTCB = CURTHREAD->owner_PTCB;
	return (Tid_t)selfPTCB;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
    int ret=-1;
    PTCB* thisPTCB=(PTCB*)tid;

    if (thisPTCB!=NULL)
     {
       if((Tid_t)CURTHREAD->owner_PTCB!=tid && thisPTCB->detouched==0 && thisPTCB->father_process==CURPROC )
       {

           thisPTCB->ref_counter++;
           while(thisPTCB->detouched==0 && thisPTCB->exited==0)
           { 
              kernel_wait_wchan(&thisPTCB->C_v_join,SCHED_USER,NULL,NO_TIMEOUT);
           }
           if(thisPTCB->detouched==0)
           {
            ret=0;

              if(exitval!=NULL){
              *exitval=thisPTCB->exit_value;
                }

           }
           
           thisPTCB->ref_counter--;
           if(thisPTCB->ref_counter==0)
           {
            //rlist_remove(&thisPTCB->ptcb_node);
            //free(thisPTCB);
           }

      }

    }
   


	return ret;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  int ret;
  PTCB* thisPTCB=(PTCB*)tid;
    if (thisPTCB!=NULL)
    {
      if(thisPTCB->exited==1)
       ret= -1;
      
      else
       {
        thisPTCB->detouched=1;
        Cond_Broadcast(&thisPTCB->C_v_join);
        ret= 0;
       }
    }	
    else 
      ret=-1;
    return ret;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

    PTCB* PTCB_pointer = CURTHREAD->owner_PTCB;
    assert(PTCB_pointer!=NULL);
   //An eisai to teleutaio thread pou kanei thread exit kleise kai to PCB.
    if(CURPROC->num_threads==1)
   { 

          if(sys_GetPid()==1) 
          {
            while(sys_WaitChild(NOPROC,NULL)!=NOPROC);
          }

        PCB* curproc = CURPROC;  
          
           
          if(curproc->args) {
            free(curproc->args);
            curproc->args = NULL;
          }

          
          for(int i=0;i<MAX_FILEID;i++) {
            if(curproc->FIDT[i] != NULL) {
              FCB_decref(curproc->FIDT[i]);
              curproc->FIDT[i] = NULL;
            }
          }

         
          PCB* initpcb = get_pcb(1);
          while(!is_rlist_empty(& curproc->children_list)) {
            rlnode* child = rlist_pop_front(& curproc->children_list);
            child->pcb->parent = initpcb;
            rlist_push_front(& initpcb->children_list, child);
          }

          
          if(!is_rlist_empty(& curproc->exited_list)) {
            rlist_append(& initpcb->exited_list, &curproc->exited_list);
            kernel_broadcast(& initpcb->child_exit);
          }

          
          if(curproc->parent != NULL) {  
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



          
          curproc->main_thread = NULL;

          
          curproc->pstate = ZOMBIE;
          curproc->exitval = exitval;

          
          kernel_sleep(EXITED, SCHED_USER);
    
  }
  
else{


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

