
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"





/*read*/
static int pipe_read(void* pipecb, char *buf, unsigned int size)
{
    Pipe_cb* pipe_p=(Pipe_cb*)pipecb;
    if(pipe_p->readClosed)
	 	return -1;

    //Mutex_Lock(&pipe_p->ready);
    int i;


       for(i=0;i<size;i++){
     	while((pipe_p->R%buf_size)==pipe_p->W%buf_size && pipe_p->readClosed==0){
     	if(i==0 && pipe_p->writeClosed==0){
            Cond_Broadcast(&pipe_p->writers_cv);
            kernel_wait_wchan(&pipe_p->readers_cv,SCHED_PIPE,NULL,NO_TIMEOUT);
          }

        else
          	return i;
         }
         if(pipe_p->readClosed){
         	 //Mutex_Unlock(&pipe_p->ready);
         	return -1;
         }
     	buf[i]=pipe_p->buffer[pipe_p->R%buf_size];
     	pipe_p->R++;

    }

	//Mutex_Unlock(&pipe_p->ready);
	return i;
}

/*write*/
static int pipe_write(void* pipecb, const char* buf, unsigned int size)
{ 

     Pipe_cb* pipe_p=(Pipe_cb*)pipecb;
	 if(pipe_p->readClosed || pipe_p->writeClosed)
	 	return -1;

	//Mutex_Lock(&pipe_p->ready);
    int i;

     for(i=0;i<size;i++){
     	while((pipe_p->W+1)%buf_size==pipe_p->R%buf_size && pipe_p->readClosed==0 && pipe_p->writeClosed==0){
         Cond_Broadcast(&pipe_p->readers_cv);
         kernel_wait_wchan(&pipe_p->writers_cv,SCHED_PIPE,NULL,NO_TIMEOUT);
         }
         if(pipe_p->readClosed || pipe_p->writeClosed){
         	 //Mutex_Unlock(&pipe_p->ready);
         	return -1;
         }
     	pipe_p->buffer[pipe_p->W%buf_size]=buf[i];
     	pipe_p->W++;

    }
    
     
    //Mutex_Unlock(&pipe_p->ready);
	return i;
}



/*close read*/
static int pipe_close_read(void* pipecb) 
{
   Pipe_cb* pipe_p=(Pipe_cb*)pipecb;
   pipe_p->readClosed=1;
   Cond_Broadcast(&pipe_p->writers_cv);
   Cond_Broadcast(&pipe_p->readers_cv);
   if (pipe_p->writeClosed==1)
   	   free(pipe_p);
	//FCB_unreserve(size_t num, Fid_t *fid, FCB** fcb);   //??
   return 0; 
}

/*close write*/
static int pipe_close_write(void* pipecb) 
{
   Pipe_cb* pipe_p=(Pipe_cb*)pipecb;
   pipe_p->writeClosed=1;
   Cond_Broadcast(&pipe_p->writers_cv);
   Cond_Broadcast(&pipe_p->readers_cv);
   if (pipe_p->readClosed==1)
   	   free(pipe_p);
   return 0; 
}



static int do_nothing_read(void *Pipe_cb, char *buf, unsigned int size) {
    return -1;
}

static int do_nothing_write(void *Pipe_cb, const char *buf, unsigned int size) {
    return -1;
}


file_ops pipe_read_ops = {
	.Open=NULL,
	.Read = pipe_read,
	.Write =do_nothing_write,
	.Close =pipe_close_read
};

file_ops pipe_write_ops = {
	.Open=NULL,
	.Read =do_nothing_read,
	.Write =pipe_write,
	.Close =pipe_close_write
};

int sys_Pipe(pipe_t* pipe)
{
	Pipe_cb* pipe_p=(Pipe_cb*)xmalloc(sizeof(Pipe_cb));
	Fid_t fid[2];
	FCB* fcb[2];

	if(FCB_reserve(2, fid, fcb)==0)
	{
		printf("Failed to allocate pipe's Fids\n");
		return -1;
	}

   pipe->read=fid[0];
   pipe->write=fid[1];

   pipe_p->R=0;
   pipe_p->W=0;
   pipe_p->readClosed=0;
   pipe_p->writeClosed=0;
   pipe_p->reader=fcb[0];
   pipe_p->writer=fcb[1];
   pipe_p->writers_cv=COND_INIT;
   pipe_p->readers_cv=COND_INIT;
   pipe_p->ready=MUTEX_INIT;

   fcb[0]->streamobj = pipe_p;
   fcb[1]->streamobj = pipe_p;

   fcb[0]->streamfunc = &pipe_read_ops;
   fcb[1]->streamfunc = &pipe_write_ops;

	return 0;
}