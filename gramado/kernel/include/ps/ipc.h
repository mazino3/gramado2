/*
 * File: ipc.h 
 *
 * Description:
 *     IPC - ( Inter-Process Comunication ). 
 *     ring 0
 *     
 * History:
 *     2015 - Created by Fred Nora.
 */


#ifndef  ____IPC_H
#define ____IPC_H


// #todo
// Use those structures.


/*
struct ipc_message_d
{
    void *window;   // Fake in ring0.
    int msg;
    unsigned long long1;
    unsigned long long2;
    
    void *buffer;
    size_t buffer_size;
    
    struct ipc_message_d *next;
};

struct ipc_port_d
{
    int id;
    int used;
    int magic;
     
    char *name;
    
    struct process_d *process;
    
    struct ipc_message_d *msg;

    struct ipc_port_d *next;
};

unsigned long ipcportList[32];
*/


// enviando 4 longs par ao window server.
// todo: a estrutura de thread precisa comportar 
// a lista de mensagens com 4 longs.
// O mouse pode usar isso para enviar um pacote com 3 longs.
int
ipc_send_longmessage_to_ws ( 
    struct window_d *window, 
    int msg, 
    unsigned long long1, 
    unsigned long long2,
    unsigned long long3,
    unsigned long long4 );






// window server
int
ipc_send_to_ws ( 
    struct window_d *window, 
    int msg, 
    unsigned long long1, 
    unsigned long long2 );

// network server
int
ipc_send_to_ns ( 
    struct window_d *window, 
    int msg, 
    unsigned long long1, 
    unsigned long long2 );
    
    

void ipc_send_message_to_process ( unsigned long msg_buffer, int pid );
void ipc_send_message_to_thread ( unsigned long msg_buffer, int tid );
int ipc_init (void);


#endif    //____IPC_H


//
// End.
//

