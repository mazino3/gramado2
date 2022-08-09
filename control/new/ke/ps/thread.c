
// thread.c

#include <kernel.h>

//
// The current thread global variable.
//

// ------
tid_t current_thread=0;
tid_t foreground_thread=0;
tid_t active_thread=0;     // This thread will receive the input.
tid_t next_thread=0;       // next user mode thread.
// ------



// Ponteiro para a idle thread atual
// Sempre que mudar a idle thread devemos usar esse ponteiro
// para mostrar qual será a nova idle thread.
// Cada idle thread pode prestar um serviço diferente, como o
// gerenciamento de energia.
//struct thread_d  *____IDLE;

// The control thread of the window sever kernel module.
// The ring 0 thread. tid0
//struct thread_d  *tid0_thread;

// Essa é a thread de controle do processo init2.bin
// É o primeiro processo em ring3.
// Idle Thread. TID=0    
struct thread_d  *InitThread;
// Ponteiro para a thread usada na hora da clonagem de processos.
struct thread_d  *ClonedThread;

//
// == private functions: prototypes ================
//

// worker for create_thread.
static void __ps_initialize_thread_common_elements( struct thread_d *t );

// worker for create_thread.
static void 
__ps_setup_x64_context ( 
    struct thread_d *t, 
    unsigned int cpl,
    unsigned long init_stack,
    unsigned long init_rip );

//===================

// worker
static void __ps_initialize_thread_common_elements( struct thread_d *t )
{
    register int i=0;

    t->objectType  = ObjectTypeThread;
    t->objectClass = ObjectClassKernelObjects;

//
// == Wait support ==================
//

    // Thread->wait4pid =

    // loop
    // Waiting reasons.
    for ( i=0; i<8; ++i ){ t->wait_reason[i] = (int) 0; };


//
// == Message support ============
//

// #todo
// Devemos adiar inicialização desses elementos relativos 
// a mensagens.




// ======================

// For the msg_d structure.
// see: window.h

// Clear the message queue.
    for ( i=0; i<32; ++i ){ t->MsgQueue[i] = 0; };
    t->MsgQueueHead = 0;
    t->MsgQueueTail = 0;

// Create all the 32 pointers.
    struct msg_d  *tmp;

    for ( i=0; i<32; ++i )
    {
        tmp = (struct msg_d *) kmalloc( sizeof( struct msg_d ) );
        if( (void*) tmp == NULL )
            panic("__ps_initialize_thread_common_elements: tmp");
    
        tmp->window = NULL;
        tmp->msg = 0;
        tmp->long1 = 0;
        tmp->long2 = 0;
        tmp->long3 = 0;
        tmp->long4 = 0;

        tmp->used = TRUE;
        tmp->magic = 1234;
        
        // Coloca o ponteiro que criamos na lista de ponteiros.
        t->MsgQueue[i] = (unsigned long) tmp;
    }
// ===================================

// Signal support

// #remember: t->signal |= 1<<(signal-1);
    t->signal = 0;
    t->umask = 0;

    t->exit_code = 0;

    return;
}

// =======================================
// local
// low level worker
// do not check parameters validation

static void 
__ps_setup_x64_context ( 
    struct thread_d *t, 
    unsigned int cpl,
    unsigned long init_stack,
    unsigned long init_rip )
{


// cpl

    if (cpl != RING0 && cpl != RING3 ){
        panic ("__ps_setup_x64_context: Invalid cpl\n");
    }

    t->cpl = (unsigned int) cpl;

//
// == Context support =============================================
//

    // (Machine dependent)
    // We need a worker routine for that thing.

    // #todo
    // A stack frame vai depender do iopl.
      
    // Stack frame.
    // #todo: Usar uma estrutura de contexto.
    // ss (0x20 | 3)
    // cs (0x18 | 3)
    // rflags for ring3: (0x3200).
    
    

    // ring 0
    if ( t->cpl == RING0 )
    {
        t->rflags_initial_iopl = (unsigned int) 0;
        t->rflags_current_iopl = (unsigned int) 0;

        t->ss     = 0x10;
        t->rsp    = (unsigned long) init_stack; 
        t->rflags = (unsigned long) 0x0202;
        t->cs     = 0x8; 
        t->rip    = (unsigned long) init_rip; 

        // (0x10 | 0)
        t->ds = 0x10; 
        t->es = 0x10; 
        t->fs = 0x10; 
        t->gs = 0x10; 
    }

    // ring 3
    if ( t->cpl == RING3 )
    {
        //#see: weak protection for threads in ring 3.
        t->rflags_initial_iopl = (unsigned int) 3;
        t->rflags_current_iopl = (unsigned int) 3;

        t->ss     = 0x23;    
        t->rsp    = (unsigned long) init_stack; 
        t->rflags = (unsigned long) 0x3202;
        t->cs     = 0x1B; 
        t->rip    = (unsigned long) init_rip; 

        // (0x20 | 3)
        t->ds = 0x23; 
        t->es = 0x23; 
        t->fs = 0x23; 
        t->gs = 0x23; 
    }

//
// Common
//

    // This is used by the control thread.
    t->initial_rip = (unsigned long) init_rip; 

    t->rax = 0;
    t->rbx = 0;
    t->rcx = 0;
    t->rdx = 0;
    t->rsi = 0;
    t->rdi = 0;
    t->rbp = 0;

    // We can save something here for control purpose.
    // It can be used for the spawner.

    t->r8 = 0;
    t->r9 = 0;
    t->r10 = 0;
    t->r11 = 0;
    t->r12 = 0;
    t->r13 = 0;
    t->r14 = 0;
    t->r15 = 0;

    //Thread->tss = current_tss;

// ===============================================

    // The context is not saved.
    t->saved = FALSE;

    return;
}

// helper
// Thread stats
unsigned long GetThreadStats ( int tid, int index )
{
    struct thread_d  *t;

//#todo:
//checar validade dos argumentos.

    if ( tid < 0 || index < 0 )
    {
       return 0;
    }

// Struct

    t = (void *) threadList[tid];

    if ( (void *) t == NULL ){
        // ?? refresh
        printf ("GetThreadStats: struct \n");
        return 0; 

    } else {
        // Checar validade
        // ...
    };

// See: 
// https://en.wikipedia.org/wiki/Processor_affinity
// ...
 
    switch (index){

        case 1:  return (unsigned long) t->tid;       break;
        case 2:  return (unsigned long) t->ownerPID;  break;
        case 3:  return (unsigned long) t->type;      break;
        case 4:  return (unsigned long) t->state;     break;
        case 5:  return (unsigned long) t->plane;     break;
        case 6:  return (unsigned long) t->cpu;       break;
        case 7:  return (unsigned long) t->affinity;  break;
 
        // #bugbug: repetido. 6.
        case 8:  return (unsigned long) t->cpu;  break;   
        
        case 9:  return (unsigned long) t->next_cpu;  break; 

        // #
        case 10:  
            //return (unsigned long) t->DirectoryPA; 
            return (unsigned long) t->pml4_PA;  
            break; 
        
        // The initial privilege level.
        // only 2 bits
        case 11:
            return (unsigned long) (t->rflags_initial_iopl & 3);  
            break; 

        case 12:  return (unsigned long) t->base_priority;  break; 
        case 13:  return (unsigned long) t->priority;  break;          

        case 14:
            return (unsigned long) 0;
            //return (unsigned long) t->pagefaultCount;
            break;          

        case 15:  return (unsigned long) t->is_preemptable;  break;
        case 16:  return (unsigned long) t->saved;  break;

        case 17:  return (unsigned long) t->HeapStart;   break;
        case 18:  return (unsigned long) t->StackStart;  break;

        case 19:  return (unsigned long) t->HeapSize;  break;
        case 20:  return (unsigned long) t->StackSize;  break;
        case 21:  return (unsigned long) t->step;  break;
        case 22:  return (unsigned long) t->initial_time_ms;  break;
        case 23:  return (unsigned long) t->total_time_ms;  break;
        case 24:  return (unsigned long) t->quantum;  break;
        case 25:  return (unsigned long) t->quantum_limit_max;  break;
        case 26:  return (unsigned long) t->standbyCount;  break;
        case 27:  return (unsigned long) t->runningCount;  break;
        case 28:  return (unsigned long) t->runningCount_ms;  break;
        case 29:  return (unsigned long) t->readyCount;  break;
        case 30:  return (unsigned long) t->ready_limit;  break;
        case 31:  return (unsigned long) t->waitingCount;  break;
        case 32:  return (unsigned long) t->waiting_limit; break;
        case 33:  return (unsigned long) t->blockedCount;  break;
        case 34:  return (unsigned long) t->blocked_limit;  break;
        case 35:  return (unsigned long) t->ticks_remaining;  break;
        
        case 36:  
            return (unsigned long) t->profiler_percentage_running;
            break;
            
        case 37:
            return (unsigned long) t->profiler_percentage_running_res;
            break;

        case 38:
            return (unsigned long) t->profiler_percentage_running_mod;
            break;
         
        case 39:
            return (unsigned long) t->profiler_ticks_running;
            break;

        case 40:
            return (unsigned long) t->profiler_last_ticks;
            break;

        // The current privilege level
        // #bugbug
        // It doesn't make sense.
        case 41:
            panic("GetThreadStats: 41");
            //return (unsigned long) t->current_iopl;
            break;
        
        case 42:  return (unsigned long) t->signal;  break;
        case 43:  return (unsigned long) t->umask;  break;
        
        //#todo
        //default:
            //break;
    };

    return 0;
}


// Get thread name.
int getthreadname ( int tid, char *buffer )
{
    struct thread_d  *t;

    char *name_buffer = (char *) buffer;

//#todo
//checar validade dos argumentos.

    if ( tid < 0  )
    {
       return 0;
    }


// struct 
    t = (struct thread_d *) threadList[tid]; 

    if ( (void *) t == NULL ){
        // msg
        return -1;
    }else{
        if ( t->used != TRUE || t->magic != 1234 ){
            //msg
            return -1;
        }
        
        // 64 bytes
        strcpy ( name_buffer, (const char *) t->__threadname );       
        
        return (int) t->threadName_len;
    };

    return -1;
}


/*
 * FindReadyThread:
 *     Pega a primeira thread READY que encontrar.
 *     E se não encontrar nenhuma, retorna NULL.
 */

// OUT:
// Return a pointer to the found thread.

void *FindReadyThread (void)
{
    register int i=0;
    struct thread_d  *Thread;

    for ( i=0; i<THREAD_COUNT_MAX; ++i )
    {
        Thread = (void *) threadList[i];

        if ( (void *) Thread != NULL )
        {
            if ( Thread->used  == TRUE && 
                 Thread->magic == 1234 && 
                 Thread->state == READY )
            {
                return (void *) Thread;
            }
        }
    };

    // Nenhuma foi encontrada.   
    // #todo: Message ??

    return NULL;
}

// Get State 
// (Zero e' tipo NULL?).
int GetThreadState (struct thread_d *thread)
{
    if ( (void *) thread == NULL ){
        // Message
        return 0;   //#bugbug: This is a valid state.
    }

    return (int) thread->state;
}


// Get Type
// (Zero e' tipo NULL?).
int GetThreadType (struct thread_d *thread)
{
    if ( (void *) thread == NULL )
    {
        //Message
        return 0;   // #bugbug: This is a valid type.
        //return -1;
    }
  
    return (int) thread->type;
}


/*
 * init_threads:
 *     Inicializa o thread manager.
 *     Inicializa as estruturas e vari�veis 
 *     que lidam com threads.
 */

// Called by init_microkernel in mk.c

int init_threads (void){

    register int i=0;


    debug_print("init_threads:\n");

	// Globais
	current_thread = 0;  //Atual. 

	//ProcessorBlock.threads_counter = (int) 0;  //N�mero de threads no processador.
	UPProcessorBlock.threads_counter = (int) 0;  //N�mero de threads no processador.
	
    old = 0;                                   //?
    forkid = 0;                                //
    task_count = (unsigned long) 0;            //Zera o contador de tarefas criadas.
	//...
	
	// @todo: Porque essas vari�veis usam o termo 'task'?
	//        task � sinonimo de process.
	
	//Vari�veis usadas na inicializa��o de uma nova tarefa.	
	start_new_task_status  = (unsigned long) 0;    //Se h� uma nova tarefa.
	start_new_task_id = (int) 0;                   //Id d� nova tarefa.
	start_new_task_address = (unsigned long) 0;    //Endere�o da nova tarefa.
	
	//@todo: H� mais vari�veis para serem inicializadas??!!

//
// Clear thread lists.
//

// normal threads
    i=0;
    while ( i < THREAD_COUNT_MAX ){
        threadList[i] = (unsigned long) 0; 
        i++;
    };

// i/o threads
    i=0;
    while ( i < THREAD_COUNT_MAX ){
        io_threadList[i] = (unsigned long) 0; 
        i++;
    };

// interactive threads
    i=0;
    while ( i < THREAD_COUNT_MAX ){
        interactive_threadList[i] = (unsigned long) 0; 
        i++;
    };


// ...

    return 0;
}


// GetCurrentTID
//      Pega o id da thread atual.

int GetCurrentTID (void)
{
    return (int) current_thread;
}



void *GetThreadByTID (int tid)
{
    struct thread_d *t;

    if (tid < 0 || 
        tid >= THREAD_COUNT_MAX )
    {
        return NULL;
    }

    t = (void *) threadList[tid];

    return (void *) t;
}


// GetCurrentThread:
//     Retorna o endereço da estrutura da thread atual.

void *GetCurrentThread(void)
{
    return (void*) GetThreadByTID(current_thread);
}


void *GetForegroundThread(void)
{
    return (void*) GetThreadByTID(foreground_thread);
}


void *GetWSThread(void)
{
    int tid = -1;
    if(WindowServerInfo.initialized != TRUE)
        return NULL;
    tid = (int) WindowServerInfo.tid;
    return (void*) GetThreadByTID(tid);
}


/*
 * SelectForExecution:
 * Um thread entra em standby, sinalizando que está pronto 
 * para entrar em execução.
 * Nesse caso, durante a rotina de taskswitch, checar-se-a se 
 * existe um thread em estado standby, caso haja, a thread é colocada 
 * pra executar pelo método spawn. 
 * Esse m�todo de spawn j� foi testado, segundo a contagem, duas thread 
 * come�aram a rodas atrav�s desse m�todo de spawn. 
 * Provavelmente as threads 'shell' e 'taskman', pois a thread 'idle' � 
 * chamada com um spawn exclusivo para ela, o que � desnecess�rio e 
 * poder� ser revisto. @todo
 * MOVIMENTO 1, (Initialized --> Standby).
 */

void SelectForExecution ( struct thread_d *Thread )
{
    if ( (void *) Thread == NULL){
        debug_print ("SelectForExecution: Thread fail\n");
        return;
    }

// #todo
// Validation ??
// @todo: if initialized ---> Standby.
// @todo: if zombie ---> Standby.
// Talvez aqui seja necess�rio checar o estado da thread.
// Quem pode entrar no estado standby??
// >> Uma thread no estado initialized pode entrar no estado standby 
// >> Uma thread no estado zombie pode entrar no estado standby.
// >> @todo: se uma thread estiver em qualquer um dos outros estados ela 
// não pode entrar em standby.

//setState:


//
// MOVIMENT 1, (Initialized --> Standby).
//
    Thread->state = (int) STANDBY;

//
// #bugbug      OVERFLOW !!!!!
//

// This function is wrong .... 
// Maybe it is putting values outside the vector.

    debug_print ("SelectForExecution: [FIXME] Overflow in queue_insert_data() \n");
    
    //queue_insert_data ( queue, (unsigned long) Thread, QUEUE_STANDBY );
}

void thread_show_profiler_info (void)
{
    struct thread_d *thread;
    int i=0;

    printf ("\n");

    for (i=0; i<THREAD_COUNT_MAX; i++)
    {
        thread = (struct thread_d *) threadList[i];

        if ( (void *) thread != NULL )
        {
            if ( thread->used == TRUE && thread->magic == 1234 )
            {
                printf ("tid=%d totalp=%d last_ticks=%d ( %d percent ) name={%s} \n", 
                    thread->tid,
                    profiler_ticks_limit,
                    thread->profiler_last_ticks,
                    thread->profiler_percentage_running,
                    thread->name_address );
            }
        }
    };

    refresh_screen();
}


unsigned long 
thread_get_profiler_percentage (struct thread_d *thread)
{
    if ( (void *) thread == NULL ){
        panic ("thread_get_profiler_percentage: thread\n");
    }
 
    return ( unsigned long ) thread->profiler_percentage_running;
}

// threads
void show_thread_information (void)
{
    struct thread_d  *Idle;
    struct thread_d  *Current;

    printf ("show_thread_information:\n");

// =================================
// Idle thread
    Idle = (struct thread_d *) UPProcessorBlock.IdleThread;
    if ( (void *) Idle != NULL )
    {
        if (Idle->magic == 1234)
        {
            printf ("Idle->tid = %d\n", Idle->tid );
        }
    }


// =================================
// Current thread
    Current = (void *) GetCurrentThread();
    if ( (void *) Current != NULL )
    {
        if (Current->magic == 1234)
        {
            printf ("Current->tid   = %d\n", Current->tid );
            printf ("current_thread = %d\n", current_thread );
        }
    }

// Show all the slots
// see: threadi.c

    show_slots(); 

    printf("Done\n");
    refresh_screen();
}


// ??
// Chamada pelo timer.c
int thread_profiler(int service)
{
    struct thread_d  *Idle;
    struct thread_d  *__current;
    struct thread_d  *__tmp;
    int i=0;
    unsigned long __total = 0; //todas inclusive idle.
    
    // safety
    if ( service < 0 )
    {
       // msg?
       return -1;
    }

    Idle = (struct thread_d *) UPProcessorBlock.IdleThread;
    if ( (void *) Idle == NULL ){
        panic("thread_profiler: Idle\n");
    }
    if (Idle->magic != 1234){
        panic("thread_profiler: Idle validation\n");
    }
   

//
// Current thread
//

    __current = (struct thread_d *) GetCurrentThread();

    if ( (void *) __current == NULL ){
        panic ("thread_profiler: __current\n");
    }

    if (__current->magic != 1234){
        panic ("thread_profiler: __current validation\n");
    }

    //unsigned long __total_ticks;
    //__total_ticks = (unsigned long) get_systime_totalticks();


// Service

    switch (service)
    {
        // Increment current thread.
        case 1:
            __current->profiler_ticks_running++;
            return 0;
            break;

        // Finalize?
        // Trabalha no profiler de todas as threads.
        case 2:
            for (i=0; i<THREAD_COUNT_MAX; i++)
            {
                __tmp = (struct thread_d *) threadList[i];
                
                if ( (void *) __tmp != NULL )
                {
                    if ( __tmp->used == TRUE && 
                         __tmp->magic == 1234 )
                    {
                        // Salva a contagem dessa thread para consulta futura.
                        __tmp->profiler_last_ticks = __tmp->profiler_ticks_running;
                        
                        // zera a contagem dessa thread,
                        __tmp->profiler_ticks_running = 0;
                        
                        __tmp->profiler_percentage_running_res = 
                            (__tmp->profiler_last_ticks / profiler_ticks_limit );
                        
                        __tmp->profiler_percentage_running_mod = 
                            (__tmp->profiler_last_ticks % profiler_ticks_limit );    
                        
                        __tmp->profiler_percentage_running = 
                            (__tmp->profiler_percentage_running_mod / (profiler_ticks_limit/100) );
                    }
                }  
            };

            // Todas menos a Idle.
            // (system usage)
            profiler_percentage_all_normal_threads = 
                (100 - Idle->profiler_percentage_running );
            
            // Idle state.
            profiler_percentage_idle_thread = 
                Idle->profiler_percentage_running ;
            
            return 0;  //ok done.

            break;

        //...
        
        default:
            break;
    };

// Salva a contagem de vezes que 
// a thread rodou durante o período.

    return -1;
}


/*
 * create_thread:
 *     Cria um thread
 * #todo: 
 * O processo ao qual o thread vai ser atribu�do deve ser passado 
 * via argumento, se o argumento for nulo, ent�o usa-se o 
 * processo atual como dono do thread.
 * Obs: Essa rotina deve fazer a inicializa��o da parte independente
 * da arquitetura e chamar as rotinas referentes � inicializa���es
 * dependentes da arquitetura, que ficar�o em outro diret�rio.
 * IN:
 *     @todo: Esses argumentos presisam ser melhorados.
 * OUT:
 *     Retorno do tipo ponteiro de estrutura.
 *     Retorna o endere�o da estrutura da thread.
 *     Retorna NULL se der errado.
 * 2015 - Created by Fred Nora.
 * 2021 - 64bit version
 */

// #todo
// Incluir o 'ring' como parâmetro.
// Isso vai ajudar a função decidir quais seletores de segmento usar.
// #todo
// This is gonna be the wrapper for this routine.
// This wrapper will call two workers:
// The first routine will create the objet itself and
// setup all the machine independent elements.
// The second one will setup all the machine dependent elements.

struct thread_d *create_thread ( 
    struct room_d     *room,
    struct desktop_d  *desktop,
    struct window_d   *window,
    unsigned long init_rip, 
    unsigned long init_stack, 
    pid_t pid, 
    char *name,
    unsigned int cpl,
    int personality )
{
    struct process_d *Process;
    struct thread_d  *Thread;

// Empty slot
    struct thread_d *Empty;

    pid_t ProcessID = -1;

// Counter
    int i = USER_BASE_TID;

    int Personality = personality;

// #debug
    debug_print ("create_thread: #todo\n");
    //printf ("create_thread: #todo\n");

//======================================
// check parameters.

    if( (void*) room == NULL ){
        debug_print ("create_thread: room parameter\n");
    }
    if( (void*) desktop == NULL ){
        debug_print ("create_thread: desktop parameter\n");
    }
    if( (void*) window == NULL ){
        debug_print ("create_thread: window parameter\n");
    }

// #bugbug
// Nao podemos usar isso aqui porque a rotina declonagem
// chama essa funçao mas reconfigura esse valor logo em seguida.

    //if( init_rip == 0 ){
    //    panic ("create_thread: [ERROR] init_rip\n");
    //}

// #bugbug
// Nao podemos usar isso aqui porque a rotina declonagem
// chama essa funçao mas reconfigura esse valor logo em seguida.

    //if( init_stack == 0 ){
    //    panic ("create_thread: [ERROR] init_stack\n");
    //}

// PID
    if( pid < 0 ){
        panic ("create_thread: [ERROR] pid\n");
    }

// name
    if( (void*) name == NULL ){
        panic ("create_thread: [ERROR] name\n");
    }
    if( *name == 0 ){
        panic ("create_thread: [ERROR] *name\n");
    }

// cpl
    if( cpl != RING0 && cpl != RING3 ){
        panic ("create_thread: [ERROR] Invalid cpl\n");
    }

//======================================
// Limits da thread atual.
// #bugbug: 
// N�o sei pra que isso. 
// Pois a thread atual n�o importa.
// @todo: deletar isso. 


    if ( current_thread < 0 || 
         current_thread >= THREAD_COUNT_MAX )
    {
        debug_print ("create_thread: current_thread fail\n");
        printf      ("create_thread: current_thread fail\n");
        return NULL;
    }

// #todo
// Filtrar o processo ao qual a thread pertence.
// #bugbug:
// Não sabemos a condição do processo atual para 
// permitirmos que ele seja o dono da thread.

    ProcessID = (int) pid;

    if( ProcessID < 0 || 
        ProcessID >= PROCESS_COUNT_MAX )
    {
        //#bugbug: Isso pode ser um problemão.
        panic("create_thread: pid");
        //ProcessID = current_process;
    }

//
// Process
//

// Ja temos um PID para o processo que � dono da thread.

    Process = (void *) processList[ProcessID]; 
    if ( (void *) Process == NULL ){
        panic ("create_thread: Process\n");
    }
    if ( Process->used != TRUE  ){
        panic ("create_thread: Process->used\n");
    }
    if ( Process->magic != 1234 ){
        panic ("create_thread: Process->magic\n");
    }

//
// Thread
//

// Alocando memória para a estrutura da thread.
// Obs: Estamos alocando memória dentro do heap do kernel.

    Thread = (void *) kmalloc ( sizeof(struct thread_d) );
    if ( (void *) Thread == NULL ){
        panic ("create_thread: [FAIL] Thread\n");
    }
    memset( Thread, 0, sizeof(struct thread_d) );

    Thread->exit_in_progress = FALSE;

// Belongs to this process.
    Thread->process  = (void *) Process;
    Thread->ownerPID = (pid_t)  ProcessID;

// Paging

    Thread->pml4_VA  = (unsigned long ) Process->pml4_VA;
    Thread->pml4_PA  = (unsigned long ) Process->pml4_PA;

    Thread->pdpt0_VA = (unsigned long ) Process->pdpt0_VA; 
    Thread->pdpt0_PA = (unsigned long ) Process->pdpt0_PA; 
    Thread->pd0_VA   = (unsigned long ) Process->pd0_VA; 
    Thread->pd0_PA   = (unsigned long ) Process->pd0_PA; 

//
// Security
//

// #todo
// Include these element into the thread structure.

    //Thread->usession = Process->usession;
    //Thread->room     = Process->room;
    //Thread->desktop  = Process->desktop;

// ====

// #todo
// The parameters needs to provide us this information.

    Thread->type = THREAD_TYPE_SYSTEM; 
    Thread->base_priority = (unsigned long) PRIORITY_SYSTEM;  //static
    Thread->priority      = (unsigned long) PRIORITY_SYSTEM;  //dynamic


    Thread->plane = FOREGROUND_THREAD;

// ==============
// Surface rectangle.

    struct rect_d *r;

    r = (struct rect_d *) kmalloc ( sizeof(struct rect_d) );
    if ( (void*) r == NULL ){
        panic("create_thread: r\n");
    }
    r->left = 0;
    r->top  = 0;
    r->width  = 50; 
    r->height = 50;
    r->dirty = FALSE; 
    r->used = TRUE;
    r->magic = 1234;
    Thread->surface_rect = r;
// ==============


// =====================

// local worker
// Initializing the common basic elements.
    __ps_initialize_thread_common_elements( (struct thread_d *) Thread );

//
// Input model
//

// ??
    //Thread->input_model = THREAD_INPUTMODEL_NULL;

    Thread->input_flags = 
        (unsigned long) (INPUT_MODEL_STDIN | INPUT_MODEL_MESSAGEQUEUE );

// ??
// Explain it better.
    Thread->_protected = FALSE;

// Thread name.

    Thread->name_address = (unsigned long) name;
    //#todo: Usar Thread->name. 
    //#todo: Thread->cmd.
    //#test 64 bytes max.
    strcpy ( Thread->__threadname, (const char *) name );

// This thread can be preempted.
    Thread->is_preemptable = PREEMPTABLE;

    // ...

    //Nothing.

// Loop.
// Vamos gerar um TID válido.
// #bugbug: 
// Isso pode virar um loop infinito!

    int Round=0;  //#todo: Change to cycle.

try_next_slot:

    // 3 rounds.
    if (Round > 3){
        panic ("create_thread: [FAIL] No more slots\n");
        //return NULL;
    }

// Recomeça o loop na base para id de usuarios.

    i++;
    if ( i >= THREAD_COUNT_MAX )
    {
       i = USER_BASE_TID;  
       Round++; 
    }

// Get empty thread structure pointer.
// Not empty
// Voltamos.
// #bugbug: 
// Isso pode n�o parar nunca.

    Empty = (void *) threadList[i];

    if ( (void *) Empty != NULL ){
        goto try_next_slot;
    }
    
// ======================================

// Index Ok.
// Now we have an index number.

    Thread->tid = (int) i;

//
// == Time support ============
//

// Temporizadores. 

// step:
// Quantas vezes ela usou o processador no total. 
// Quantas vezes ela já rodou no total.
// How many jiffies. total_jiffies.

    Thread->step = 0;

// Quantum.
    Thread->quantum           = QUANTUM_MIN;
    Thread->quantum_limit_min = QUANTUM_MIN; 
    Thread->quantum_limit_max = QUANTUM_MAX; 

    Thread->standbyCount = 0;

    Thread->runningCount = 0;
    Thread->runningCount_ms = 0;

    Thread->readyCount  = 0; 
    Thread->ready_limit = QUANTUM_MAX;

    Thread->waitingCount  = 0;
    Thread->waiting_limit = QUANTUM_MAX;

    Thread->blockedCount  = 0; 
    Thread->blocked_limit = QUANTUM_MAX;

// Not used now. But it works fine.
    Thread->ticks_remaining = 1000; 

    Thread->initial_time_ms = get_systime_ms();
    Thread->total_time_ms = 0;

// ===================================


// @todo: 
// Essa parte � dependente da arquitetura i386.
// Poder� ir pra outro arquivo.

// init_stack:
// O endere�o de in�cio da pilha � passado via argumento.
// Ent�o quem chama precisa alocar mem�ria para a pilha.
// @todo: Podemos checar a validade dessa pilha ou � problema 
// na certa.

// init_eip:
// O endere�o in�cio da sess�o de c�digo da thread � 
// passado via argumento. Ent�o quem chama essa rotina 
// deve providendiar um endere�o v�lido.
// Obs: init_eip Aceita endere�os inv�lidos pois a thread 
// fecha nesses casos por PG fault. Mas o sistema pode travar 
// se for a �nica thread e um �nico processo. 

    //if( init_stack == 0 ){ ... }
    //if( init_eip == 0 ){ ... }


//
// == Context support ================
//

// Call the second worker to setup all the machine dependent elements.
// Just like the context et al.
// The contexts needs its own structure.

// ring 0
    if (cpl == RING0)
    {
        // iopl 0 for threads in ring 0.
        // cpl = iopl
        Thread->rflags_initial_iopl = (unsigned int) 0;
        Thread->rflags_current_iopl = (unsigned int) 0;

        __ps_setup_x64_context( 
            (struct thread_d *) Thread,
            RING0,
            (unsigned long) init_stack,
            (unsigned long) init_rip );
    }

// ring 3
    if (cpl == RING3)
    {
        // weak protection
        // iopl 3 for threads in ring 3.
        Thread->rflags_initial_iopl = (unsigned int) 3;
        Thread->rflags_current_iopl = (unsigned int) 3;

        __ps_setup_x64_context( 
            (struct thread_d *) Thread,
            RING3,
            (unsigned long) init_stack,
            (unsigned long) init_rip );
    }


//cpu.
    //Thread->cpuID = 0;
    //Thread->confined = 0;
    //Thread->CurrentProcessor = 0;
    //Thread->NextProcessor = 0;

//ServiceTable ..
//Ticks ...
//DeadLine ... 


    //Thread->PreviousMode  //ring???
    //Thread->idealprocessornumber
    //Thread->event

// ORDEM: 
// O que segue � referenciado com pouca frequ�ncia.

//
// Finalization
//

//@todo:
//herdar o quantum do processo.
//herdar a afinidade do processo.(cpu affinity) 

// #todo: 
// Incrementar a contagem de threads no processo.
    //Process->threadCount++;

//Proxima thread da lista.

    Thread->next = NULL;

// Coloca na lista.
// #bugbug: Check overflow again.

    threadList[ Thread->tid ] = (unsigned long) Thread;

    Thread->state = INITIALIZED;  

// Validation
    Thread->used = TRUE;
    Thread->magic = THREAD_MAGIC;
// ===================================================

//
// Thread counter
//

// #importante
// Contador de threads
// Vamos atualizar o contador de threads, 
// pois mais uma thread existe, mesmo que n�o esteja rodando ainda.

    //ProcessorBlock.threads_counter++;
    UPProcessorBlock.threads_counter++;

// limits 

    //if ( ProcessorBlock.threads_counter >= THREAD_COUNT_MAX )
    if ( UPProcessorBlock.threads_counter >= THREAD_COUNT_MAX ){
        panic ("create_thread: [FAIL] UPProcessorBlock.threads_counter \n");
    }

//done:

// #debug
    debug_print ("create_thread: Done\n");
    //printf ("create_thread: Done\n");

// Warning !!! 
// ( NÃO COLOCAR PARA EXECUÇÃO, 
//   OUTRA FUNÇÃO DEVE COLOCAR ISSO PARA EXECUÇÃO )

    //SelectForExecution(t);  //MOVEMENT 1 (Initialized ---> Standby)

// Return the pointer.
    return (void *) Thread;
}


/*
 * exit_thread:
 *     Exit a thread.
 *     Torna o estado ZOMBIE mas não destrói a estrutura.
 *     Outra rotina destruirá as informações de uma estrutura de thread zombie.
 */
 
void exit_thread (int tid)
{
    struct thread_d  *Idle;
    struct thread_d  *Thread;

    if ( tid < 0 || tid >= THREAD_COUNT_MAX )
    {
        debug_print ("exit_thread: tid\n");
        return;
    }

//
// Init thread.
//

// We can't exit the idle thread.

    Idle = (struct thread_d *) UPProcessorBlock.IdleThread;
    if ( (void *) Idle == NULL ){
        panic ("exit_thread: Idle\n");
    }
    if ( Idle->magic != 1234 ){
        panic ("exit_thread: Idle validation\n");
    }
    if ( tid == Idle->tid ){
        panic ("exit_thread: We can't kill the Idle thread!\n");
    }

//
// Thread
//

// Get thread structure.

    Thread = (void *) threadList[tid];
    if ( (void *) Thread == NULL ){
        printf ("exit_thread: Thread doesn't exist\n");
        goto fail;
    }
    if ( Thread->magic != 1234 ){
            printf ("exit_thread: Thread validation \n");
            goto fail;
    }

// Zombie
// Lembrando que se deixarmos no estado ZOMBIE o 
// deadthread collector vai destruir a estrutura.

        // Let the scheduler put this thread in the ZOMBIE state.
        Thread->exit_in_progress = TRUE;
        //Thread->state = ZOMBIE;
    return;

fail:
    //#debug
    refresh_screen();
    return;
}


// exit current thread.
void exit_current_thread(void)
{
    exit_thread(current_thread);
}


/*
 * copy_thread_struct:
 *     Clona uma thread.
 *     Usado no suporte a fork e execução de novos processos.
 */

// OUT:
// Pointer for the clone.

struct thread_d *copy_thread_struct ( struct thread_d *thread )
{
    struct thread_d  *father;
    struct thread_d  *clone;

    // Counters
    int w=0;
    int q=0;


    debug_print("copy_thread_struct:\n");


    // A thread que vai ser copiada.
    father = thread;
    if ( (void *) father == NULL ){
        panic ("copy_thread_struct: father\n");
    }

    if(father->magic!=1234)
    {
        debug_print("copy_thread_struct: father validation\n");
              panic("copy_thread_struct: father validation\n");
    }


    // #todo
    // Aqui, na hora de criar o nome, vamos dar
    // um nome personalizado pra não ficar tudo igual.
    //char new_name[32];

    // IN: 
    // room, desktop, window,
    // initial eip, initial stack, 
    // pid, thread name.
    
    // #bugbug
    // Bad parameters,
    // eip and stack are '0'
    // This way the routine will fail.
    
    // Como isso eh uma rotina de clonagem entao podemos usar
    // os valores atuais da thread original
    // thread->eip
    // thread->esp
    
    // #todo:We need a better name
    // use a buffer for that
    // char nameBuffer[32];



// #bugbug
// Não podemos herdar o rip e o rsp.
// porque as threads terão espaço de endereçamento diferentes
// e uma não poderá acessar a pilha da outra.

    /*
    if (rip == 0){
        panic ("copy_thread_struct: rip");
    }
    */

    /*
    if (rsp == 0){
        panic ("copy_thread_struct: rsp");
    }
    */

    // #bugbug
    // Conferir quem é o pai owner pid dessa thread.

    //father->ownerPID or current_process ??

// #todo
// for now we can copy only threads in ring 3.

    unsigned int father_cpl = (unsigned int) father->cpl;
    int father_personality = (int) father->personality;

    if(father_cpl != RING3)
        panic("copy_thread_struct: father_cpl!=RING3\n");


    if(father_personality != PERSONALITY_GRAMADO &&
       father_personality != PERSONALITY_GWS )
    {
        debug_print("copy_thread_struct: father_personality\n");
              panic("copy_thread_struct: father_personality\n");
    }

    debug_print("copy_thread_struct: create thread\n");
    
    clone = 
        (struct thread_d *) create_thread ( 
                                NULL, NULL, NULL, 
                                0,  // initial rip 
                                0,  // initial rsp
                                father->ownerPID,  //current_process, 
                                "clone-thread",
                                father_cpl,
                                father_personality );

// The copy.
    if ( (void *) clone == NULL )
    {
        debug_print ("copy_thread_struct: clone\n");
        panic       ("copy_thread_struct: clone\n");
    }

    if(clone->magic!=1234)
    {
        debug_print ("copy_thread_struct: clone validation\n");
        panic       ("copy_thread_struct: clone validation\n");
    }

/*
    if ( clone->personality != father->personality)
    {
        debug_print("copy_thread_struct: clone->personality != father->personality\n");
        panic      ("copy_thread_struct: clone->personality != father->personality\n");
    }
*/

    clone->personality = (int) father->personality;

//
// Saving ...
//
    // Saving.
    // See: thread.h

    ClonedThread = clone;


// Type, base priority and priority.

    clone->type  = father->type; 

    clone->base_priority = father->base_priority; 
    clone->priority      = father->priority;


//
// Input
//

// #suspenso. 
// Vamos usar as flags em 't->input_flags'
    //clone->input_model = father->input_model; 
    
    clone->input_flags = father->input_flags;

// #importante
// Esse momento � critico.
// dependendo do estado da thread ele pode n�o rodar.
// ou ela pode rodar e falhar por n�o esta pronta,
// vamos testar op��es.

    // Começando com o clone bloqueada ...
    // Mas isso será mudado pela função que chamou essa.
    // obs: Isso funcionou.
    clone->state = BLOCKED;  

//Apenas Initialized, pois a fun��o SelectForExecution
//seleciona uma thread para a execu��o colocando ela no
//state Standby.	

// #todo: 
// ISSO DEVERIA VIR POR ARGUMENTO
    clone->plane = father->plane;

//
// cpl
//

// #bugbug
// for now we can only copy threads in ring 3.

    unsigned int clone_cpl = (unsigned int) clone->cpl;
    if(clone_cpl != RING3)
        panic("copy_thread_struct: clone_cpl!=RING3\n");



//
// iopl
//

    clone->rflags_initial_iopl = (unsigned int) father->rflags_initial_iopl;
    clone->rflags_current_iopl = (unsigned int) father->rflags_current_iopl;

// #bugbug
// For now we can only accept weak protection for threads in ring 3.

    if ( clone->rflags_initial_iopl != 3 ){
        panic ("copy_thread_struct: clone rflags_initial_iopl\n");
    }

    //if ( clone->rflags_current_iopl != 3 ){
    //    panic ("copy_thread_struct: clone rflags_current_iopl\n");
    //}


// Is the context saved or not?
    clone->saved = (int) father->saved;
// Is it preemptable or not?
    clone->is_preemptable = (int) father->is_preemptable;  

	//Heap and Stack.
	//Thread->Heap;
	//Thread->HeapSize;
	//Thread->Stack;
	//Thread->StackSize;

// Temporizadores. 
// step - Quantas vezes ela usou o processador no total.

//
// == Jiffies ==============
//

    clone->step = 0;

    clone->quantum           = father->quantum; 
    clone->quantum_limit_min = father->quantum_limit_min;
    clone->quantum_limit_max = father->quantum_limit_max;

    if ( clone->quantum < QUANTUM_MIN ){ clone->quantum = QUANTUM_MIN; }
    if ( clone->quantum > QUANTUM_MAX ){ clone->quantum = QUANTUM_MAX; }

    if ( clone->quantum_limit_min < QUANTUM_MIN ){ clone->quantum_limit_min = QUANTUM_MIN; }
    if ( clone->quantum_limit_max > QUANTUM_MAX ){ clone->quantum_limit_max = QUANTUM_MAX; }
    
	// runningCount - Tempo rodando antes de parar.
	// readyCount - Tempo de espera para retomar a execução.
	// blockedCount - Tempo bloqueada.

    clone->standbyCount    = father->standbyCount;

    clone->runningCount    = father->runningCount; 
    clone->runningCount_ms = father->runningCount_ms;

    clone->readyCount      = father->readyCount; 
    clone->ready_limit     = father->ready_limit;

    clone->waitingCount    = father->waitingCount;
    clone->waiting_limit   = father->waiting_limit;

    clone->blockedCount    = father->blockedCount; 
    clone->blocked_limit   = father->blocked_limit;

    // Not used now. But it works fine.

    clone->ticks_remaining = father->ticks_remaining; 

    clone->initial_time_ms = father->initial_time_ms;
    clone->total_time_ms   = father->total_time_ms;

// Signal
// Sinais para threads.

    clone->signal = father->signal;
    clone->umask  = father->umask;

// #todo: 
// Essa parte � dependente da arquitetura i386.
// Poder� ir pra outro arquivo.

// init_stack:
// O endere�o de in�cio da pilha � passado via argumento.
// Ent�o quem chama precisa alocar mem�ria para a pilha.
// @todo: Podemos checar a validade dessa pilha ou � problema 
// na certa.

// init_eip:
// O endere�o in�cio da sess�o de c�digo da thread � 
// passado via argumento. Ent�o quem chama essa rotina 
// deve providendiar um endere�o v�lido.
// Obs: init_eip Aceita endere�os inv�lidos pois a thread 
// fecha nesses casos por PG fault. Mas o sistema pode travar 
// se for a �nica thread e um �nico processo. 

	//if( init_stack == 0 ){ ... }
	//if( init_eip == 0 ){ ... }

// Contexto x86 usado pela thread.

// #todo
// We gotta check all these values bellow.


	// ss (0x20 | 3)
	// cs (0x18 | 3)

//
// Stack frame
//

    clone->ss     = (unsigned short) (father->ss & 0xFFFF);    // RING 3.
    clone->rsp    = (unsigned long) father->rsp;   // wrong
    clone->rflags = (unsigned long) father->rflags;
    clone->cs     = (unsigned short) (father->cs & 0xFFFF);
    clone->rip    = (unsigned long) father->rip;   // wrong 

//O endereço incial, para controle.

    clone->initial_rip = (unsigned long) father->initial_rip; 

// check iopl

     int cpl=-1;
     cpl = (int) (clone->cs & 0xFF);
     cpl = (int) (cpl & 3);
     
     if(cpl != 3){
         panic("copy_thread_struct: cpl\n");
     }

// #bugbug:
// We need the initial stack address
// (0x20 | 3)

    clone->ds = father->ds; 
    clone->es = father->es; 
    clone->fs = father->fs; 
    clone->gs = father->gs; 

    clone->rax = father->rax;
    clone->rbx = father->rbx;
    clone->rcx = father->rcx;
    clone->rdx = father->rdx;
    clone->rsi = father->rsi;
    clone->rdi = father->rdi;
    clone->rbp = father->rbp;
    clone->r8 = father->r8;
    clone->r9 = father->r9;
    clone->r10 = father->r10;
    clone->r11 = father->r11;
    clone->r12 = father->r12;
    clone->r13 = father->r13;
    clone->r14 = father->r14;
    clone->r15 = father->r15;

//
// TSS
//
    // #todo

    // clone->tss = thread->tss;


	//cpu.
	//Thread->cpuID = 0;
	//Thread->confined = 0;
	//Thread->CurrentProcessor = 0;
	//Thread->NextProcessor = 0;

// @todo: 
// O processo dono da thread precisa ter um diret�rio 
// de p�ginas v�lido.

// #bugbug
// Page Directory. (#CR3).
// Estamos usando o page directory do processo.
// Page directory do processo ao qual a thread pertence.

	//clone->DirectoryPA = thread->DirectoryPA; 
    //clone->DirectoryVA = thread->DirectoryVA;

    //ServiceTable ..
    //Ticks ...
    //DeadLine ... 

	//Thread->PreviousMode  //ring???
	//Thread->idealprocessornumber
	//Thread->event

// ORDEM: 
// O que segue � referenciado com pouca frequ�ncia.

    clone->waitingCount = father->waitingCount;  //Tempo esperando algo.
    clone->blockedCount = father->blockedCount;  //Tempo bloqueada.

//qual processo pertence a thread.  
    clone->process = father->process; 

	//Thread->window_station
	//Thread->desktop
	//Thread->control_menu_procedure
	//Thread->wait4pid =

// Razões para esperar.
    for ( w=0; w<8; w++ ){
        clone->wait_reason[w] = father->wait_reason[w];
    };

//...
//@todo:
//herdar o quantum do processo.
//herdar a afinidade do processo.(cpu affinity) 

    clone->exit_code = father->exit_code;

//
// #debug
//

/*
	show_slot (thread->tid);
	show_reg (thread->tid);

	show_slot (clone->tid);
	show_reg (clone->tid);
	
	refresh_screen();
	while(1){}
*/

    debug_print("copy_thread_struct: done\n");
    
// Returning the pointer for the clone.

    return (struct thread_d *) clone;
}


// #bugbug 
// It's a work in progress
int thread_getchar (void)
{
    debug_print ("thread_getchar: [FIXME]\n");
    return -1;
    //return (int) ps2tty_get_byte_from_input_buffer(); 
}





























































