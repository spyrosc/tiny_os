
#include "tinyos.h"
#include "util.h"
#include "kernel_cc.h"
#include "kernel_streams.h"



static int socket_read(void* socketcb, char *buf, unsigned int size)
{
	return -1;
}

static int socket_write(void* socketcb, const char* buf, unsigned int size)
{ 
	return -1;
}


static int socket_close(void* socketcb) 
{
	return -1;
}



file_ops socket_ops = {
	.Open=NULL,
	.Read = socket_read,
	.Write =socket_write,
	.Close =socket_close
};

//PORT MAP[]

Socket_CB* Port_map[MAX_PORT+1]={NULL};



Fid_t sys_Socket(port_t port)
{

if (port < 0 || port > MAX_PORT)
	return NOFILE;

    Fid_t fid;
    FCB* fcb;

    if(FCB_reserve(1, &fid, &fcb)==0)
	{
		printf("Failed to allocate socket's Fid\n");
		return NOFILE;
	}

    Socket_CB* scb = (Socket_CB*) xmalloc(sizeof(Socket_CB));
    scb->fid = fid;
    scb->fcb = fcb;
    scb->type = UNBOUND;
    scb->port = port;
    scb->refcount = 0;
    fcb->streamobj = scb;
    fcb->streamfunc = &socket_ops;
    return fid;
}



int sys_Listen(Fid_t sock)
{

   if (sock < 0 || sock > MAX_FILEID)
   	return -1;
    FCB *fcb = get_fcb(sock);
    if (fcb == NULL)
    	return -1;


    Socket_CB* scb=(Socket_CB*)fcb->streamobj;
  
  if (scb == NULL || scb->type != UNBOUND || scb->port <= 0 || Port_map[scb->port] != NULL) 
   {
        return -1;
    }
    scb->type = LISTENER;
    scb->peerORlistener.listener_socket = (listener_s*) xmalloc(sizeof(listener_s));
    scb->peerORlistener.listener_socket->cv_accept = COND_INIT;
    rlnode_init(&scb->peerORlistener.listener_socket->requests,NULL);
    Port_map[scb->port] = scb;
    return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{

 /*
	if (lsock < 0 || lsock > MAX_FILEID)
   	return NOFILE;
    FCB *fcb = get_fcb(lsock);
    if (fcb == NULL)
    	return NOFILE;


    Socket_CB* listenerSCB=(Socket_CB*)fcb->streamobj;

    if (listenerSCB == NULL || listenerSCB->type != LISTENER) {
        return NOFILE;
    }
    while(is_rlist_empty(&listenerSCB->peerORlistener.listener_socket->requests)) {

      kernel_wait_wchan(&listenerSCB->peerORlistener.listener_socket->cv_accept,SCHED_USER,NULL,NO_TIMEOUT);
    }
    rlnode* requestNode = rlist_pop_front(&listenerSCB->peerORlistener.listener_socket->requests);
    //pros8etos kwdikas
    */
    return NOFILE;

}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
 /*
  if (sock < 0 || sock > MAX_FILEID)
   	return -1;
    FCB *fcb = get_fcb(sock);
    if (fcb == NULL)
    	return -1;


    Socket_CB* scb=(Socket_CB*)fcb->streamobj;
 
    if (port < 0 || port >= MAX_PORT || Port_map[port] == NULL || Port_map[port]->type != LISTENER || scb->type != UNBOUND)
     {
        return -1;
     }
    listener_req* request = (listener_req*)xmalloc(sizeof(listener_req));
    request->cv_req = COND_INIT;
    request->isServed = 0;
    request->fid = sock;
    request->fcb = get_fcb(sock);
    request->sockcb = scb;
    rlnode* indermidiate_node=rlnode_init(&request->node_req,request);
    rlist_push_back(&Port_map[port]->peerORlistener.listener_socket->requests,indermidiate_node);
    Cond_Signal(&Port_map[port]->peerORlistener.listener_socket->cv_accept);
    kernel_wait_wchan(&request->cv_req,SCHED_USER,NULL,timeout);
    rlist_remove(indermidiate_node);
    return 0;
       */
	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

