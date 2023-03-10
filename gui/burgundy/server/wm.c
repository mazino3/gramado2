/*
 * File: wm.c 
 *     The Window Manager.
 * History:
 *     2020 - Create by Fred Nora.
 */

#include "gws.h"

extern int gUseMouse;

// Global main structure.
// Not a pointer.
// See: window.h
struct gws_windowmanager_d  WindowManager;

// -------------------------------------

// Windows - (struct)

struct gws_window_d *__root_window; 
struct gws_window_d *active_window;  // active
// Input events:
struct gws_window_d *keyboard_owner;
struct gws_window_d *mouse_owner;  // captured
struct gws_window_d *mouse_hover;  // hover
// If the window server has a taskbar.
// maybe we don't need that.
struct gws_window_d  *taskbar_window; 
struct gws_window_d  *taskbar_startmenu_button_window; 
//char startmenu_string[32];

// ...
// z-order ?
// But we can use multiple layers.
// ex-wayland: background, bottom, top, overlay.
struct gws_window_d *first_window;
struct gws_window_d *last_window;
struct gws_window_d *top_window;     // z-order
// -------------------------------------

static const char *app1_string = "terminal.bin";
static const char *app2_string = "editor.bin";
static const char *app3_string = "browser.bin";
static const char *app4_string = "fileman.bin";
//static const char *app4_string = "cmdline.bin";

static unsigned long last_input_jiffie=0;

// global.
// Permission:
// TRUE = The kernel can use the handler.
int g_handler_flag;

// #todo
// Input event
// Comparar os tick, em dois momentos:
// aqui e no driver.
// Calcular o delta.
//unsigned long gMouseInputEventTime=0;
//unsigned long gKeyboardInputEventTime=0;

//#define DEFAULT_ALIVIO  16
//static unsigned long alivio=DEFAULT_ALIVIO;

// Taskbar
#define TB_HEIGHT  40
#define TB_BUTTON_PADDING  4
#define TB_BUTTON_HEIGHT  (TB_HEIGHT - (TB_BUTTON_PADDING*2))
#define TB_BUTTON_WIDTH  TB_BUTTON_HEIGHT
#define TB_BUTTONS_MAX  8

// Affects the active window
#define OPTION_NOTHING  0
#define OPTION_MINIMIZE  1
#define OPTION_MAXIMIZE  2
#define OPTION_CLOSE     3
static int current_option=OPTION_NOTHING;


//
// tb buttons
//


// Quantos botões ja temos.
static int tb_buttons_count=0;  
static int tb_buttons[TB_BUTTONS_MAX];
static int tb_buttons_status[TB_BUTTONS_MAX];

//
// tb windows
//

// Ponteiros de estrutura de janelas.
static unsigned long tb_windows[TB_BUTTONS_MAX];

//
// tb pids
//

static int tb_pids[TB_BUTTONS_MAX];

//
// Window list.
//

// Global.
unsigned long windowList[WINDOW_COUNT_MAX];

// ---------

#define WM_DEFAULT_BACKGROUND_COLOR   COLOR_GRAY


static long old_x=0;
static long old_y=0;

//#todo
//GetWindowRect
//GetClientRect

// refresh rate of the whole screen.
static unsigned long fps=0;

// refresh rate for all dirty objects. In one round.
static unsigned long frames_count=0;
//static unsigned long frames_count_in_this_round;

static unsigned long ____old_time=0;
static unsigned long ____new_time=0;


//
// private functions: prototypes ==========================
//

static unsigned long 
wmProcedure(
    struct gws_window_d *window,
    int msg,
    unsigned long long1,
    unsigned long long2 );


static void animate_window( struct gws_window_d *window );
static void __Tile(void);
static void run_selected_option(void);

static unsigned long 
on_keyboard_event(
    int msg,
    unsigned long long1,
    unsigned long long2 );
static void on_mouse_event(int event_type, long x, long y);
static void on_update_window(int event_type);

int control_action(int msg, unsigned long long1);

static void __draw_button_mark_by_wid( int wid, int button_number );




// ===================
// pinta um retangulo no botao
// indicando que o app esta rodando.
static void __draw_button_mark_by_wid( int wid, int button_number )
{
    struct gws_window_d *w;
    
//#todo: max limit
    if( wid<0 )
        return;
    if( button_number<0 || button_number > 3)
        return;

    w = (struct  gws_window_d *) windowList[wid];
    if( (void*) w == NULL )
        return;
    if(w->magic!=1234)
        return;

// Draw a small rectangle.
// It means that this app was lauched.
    
    if (tb_buttons_status[button_number] == TRUE)
    {
        rectBackbufferDrawRectangle ( 
            (w->left +3), 
            (w->top +4), 
            (w->width -8), 
            2, 
            COLOR_RED, 
            FALSE, //refresh
            0 );   //rop_flags
    }
}


static void run_selected_option(void)
{
    struct gws_window_d *w;

    yellow_status("RUN");

// Nothing to do.
    if( WindowManager.initialized != TRUE )
        return;

// get active window
    //w = (struct gws_window_d *) windowList[active_window];

    w = (void*) active_window;
    
    if( (void*) w == NULL ){
        current_option=OPTION_NOTHING;
        return;
    }
    if(w->magic!=1234){
        current_option=OPTION_NOTHING;
        return;
    }
    if(w->type!=WT_OVERLAPPED){
        current_option=OPTION_NOTHING;
        return;
    }


// =============

    // Clicked with option: 'nothing'.
    if(current_option==OPTION_NOTHING)
    {
        gwssrv_change_window_position(w,20,20);
        gws_resize_window(w,100,100);
        redraw_window(w,TRUE);
        current_option=OPTION_NOTHING;
        return;
    }


    if(current_option==OPTION_MINIMIZE){
        gws_resize_window(w,100,100);
        redraw_window(w,TRUE);
        current_option=OPTION_NOTHING;
        return;
    }
    
    //#bugbug: e se a janela ativa for a root?
    if(current_option==OPTION_MAXIMIZE)
    {
        //redraw_window_by_id(active_window,TRUE);
        //redraw_window(w,TRUE);
        wm_update_window_by_id(w->id);
    }

    if(current_option==OPTION_CLOSE){
    }


    current_option=OPTION_NOTHING;
}


// #todo: explain it
static unsigned long 
on_keyboard_event(
    int msg,
    unsigned long long1,
    unsigned long long2 )
{
    struct gws_window_d *w;

// Window with focus.
    //w = (struct gws_window_d *) get_focus();
    
    //#maybe
    //if(!w){ w=get_active(); }
    
    w = (void*) keyboard_owner;
    if ( (void*) w == NULL ){
        printf("on_keyboard_event: w\n");
        return 0;
    }
    if (w->magic!=1234){
        printf("on_keyboard_event: w magic\n");
        return 0;
    }

    //keyboard_owner = (void*) w;
    //WindowManager

    return (unsigned long) wmProcedure(
                               (struct gws_window_d *) w,
                               (int) msg,
                               (unsigned long) long1,
                               (unsigned long) long2 ); 
}


// Quando temos um evento de mouse,
// vamos enviar esse evento para a
// janela com o foco de entrada.
// Os aplicativos estao recebendo
// os eventos enviados para as janelas com o foco de entrada.

static void on_mouse_event(int event_type, long x, long y)
{
// Window with focus.
    struct gws_window_d *w;
    unsigned long in_x=0;
    unsigned long in_y=0;

// Error. Nothing to do.
    if(event_type<0)
        return;

    w = (struct gws_window_d *) get_focus();
    if( (void*) w==NULL ){
        return;
    }

    //#todo
    //if(w->magic != 1234)
        //return;

// Is it inside the window with focus?
    if( x >= w->left &&
        x <= w->right &&
        y >= w->top &&
        y <= w->bottom )
    {
        // Values inside the window.
        in_x = (unsigned long) (x - w->left);
        in_y = (unsigned long) (y - w->top);

        // Change the input pointer
        // inside the window with focus.
        if (w->type == WT_EDITBOX_MULTIPLE_LINES)
        {
            if(in_x>0)
                w->ip_x = in_x/8;
            if(in_y>0)
                w->ip_y = in_y/8;
        }

// data
        w->single_event.wid   = (int) w->id;
        w->single_event.msg   = (int) event_type;
        w->single_event.long1 = (unsigned long) in_x;  //x - w->left;
        w->single_event.long2 = (unsigned long) in_y;  //y - w->top;
        w->single_event.has_event = TRUE;

        // ---------------
        int Tail = (int) w->ev_tail;
        w->ev_wid[Tail]   = (unsigned long) (w->id & 0xFFFFFFFF);
        w->ev_msg[Tail]   = (unsigned long) (event_type & 0xFFFFFFFF);
        w->ev_long1[Tail] = (unsigned long) in_x;  //x - w->left; 
        w->ev_long2[Tail] = (unsigned long) in_y;  //y - w->top;
        w->ev_tail++;
        if(w->ev_tail>=32)
            w->ev_tail=0;
            return;
        }
//fail
    w->single_event.has_event = FALSE;
}


// Post a message into the window with focus message queue.
// #todo: We need a worker for posting messages into the queues.
// Do we already have one?
static void on_update_window(int event_type)
{
// Window with focus.
    struct gws_window_d *w;

// Error. Nothing to do.
    if(event_type<0){
        return;
    }

    w = (struct gws_window_d *) get_focus();
    if ( (void*) w==NULL ){ return; }
    if (w->magic != 1234) { return; }

// Update window structure.
// #?? We are using a single event.
// But maybe we can use a queue, just like the 
// messages in the thread structure.

    w->single_event.wid   = w->id;
    w->single_event.msg   = event_type;
    w->single_event.long1 = 0;
    w->single_event.long2 = 0;
        
    w->single_event.has_event = TRUE;

// ---------------
    int Tail = (int) w->ev_tail;
    w->ev_wid[Tail]   = (unsigned long) (w->id & 0xFFFFFFFF);
    w->ev_msg[Tail]   = (unsigned long) (event_type & 0xFFFFFFFF);
    w->ev_long1[Tail] = (unsigned long) 0; 
    w->ev_long2[Tail] = (unsigned long) 0;
    w->ev_tail++;
    if (w->ev_tail >= 32){
        w->ev_tail=0;
    }
}


int control_action(int msg, unsigned long long1)
{
    struct gws_window_d *aw;
    struct gws_window_d *w;

    int minimize_wid =-1;
    int maximize_wid =-1;
    int close_wid=-1;

    if(msg<0)
        return -1;

// -----------------------
// overlapped window
    aw = (void*) get_active_window();
    if ( (void*) aw == NULL )
        return -1;
    if(aw->magic!=1234)
        return -1;

// -----------------------
// titlebar
    w = (void*) aw->titlebar;
    if ( (void*) w == NULL )
        return -1;
    if(w->magic!=1234)
        return -1;

    if (w->Controls.initialized != TRUE)
        return -1;

// Get IDs.
    minimize_wid = (int) w->Controls.minimize_wid;
    maximize_wid = (int) w->Controls.maximize_wid;
    close_wid    = (int) w->Controls.close_wid;
    
    switch (msg){
    case GWS_SysKeyDown:
        if(long1==VK_F9)
        {
            //printf("F9\n");
            set_status_by_id(minimize_wid,BS_PRESSED);
            redraw_window_by_id(minimize_wid,TRUE);
            return 0;
        }
        if(long1==VK_F10)
        {
            //printf("F10\n");
            set_status_by_id(maximize_wid,BS_PRESSED);
            redraw_window_by_id(maximize_wid,TRUE);
            return 0;
        }
        if(long1==VK_F11)
        {
            //printf("F11\n");
            set_status_by_id(close_wid,BS_PRESSED);
            redraw_window_by_id(close_wid,TRUE);
            return 0;
        }
        return 0;
        break;
    case GWS_SysKeyUp:
        if(long1==VK_F9)
        {
            //printf("F9\n");
            set_status_by_id(minimize_wid,BS_RELEASED);
            redraw_window_by_id(minimize_wid,TRUE);
            return 0;
        }
        if(long1==VK_F10)
        {
            //printf("F10\n");
            set_status_by_id(maximize_wid,BS_RELEASED);
            redraw_window_by_id(maximize_wid,TRUE);
            return 0;
        }
        if(long1==VK_F11)
        {
            //printf("F11\n");
            set_status_by_id(close_wid,BS_RELEASED);
            redraw_window_by_id(close_wid,TRUE);
            return 0;
        }
        break;
    default:
        break;
    };

    return -1;
}


void show_client( struct gws_client_d *c, int tag )
{
    if( (void*) c == NULL )
        return;
    if(c->magic!=1234)
        return;

    if(tag<0)
        return;
    if(tag >= 4)
        return;

// This client doesn't belong to this tag.
    if( c->tags[tag] != TRUE )
        return;


    if(c->window < 0)
        return;
    if(c->window >= WINDOW_COUNT_MAX)
        return;

// Show
    if(c->window == WT_OVERLAPPED)
        redraw_window_by_id(c->window,TRUE);

// Show next.
    //show_client(c->next,tag);
}


 //#todo: notworking
void show_client_list(int tag)
{
    struct gws_client_d *c;
    
    c = (struct gws_client_d *) first_client;
    
    while(1)
    {
        if( (void*) c == NULL )
            break;
            
        if( (void*) c != NULL )
            show_client(c,tag);
        
        c = (struct gws_client_d *) c->next;
    };
}


 //#todo: not teste yet
struct gws_client_d *wintoclient(int window)
{
    struct gws_client_d *c;
    int i=0;

    if(window<0)
        return NULL;
    if(window>=WINDOW_COUNT_MAX)
        return NULL;
    
    c = (struct gws_client_d *) first_client;
    while( (void*) c != NULL )
    {
        if(c->magic == 1234 )
        {
            if( c->window == window ){
                return (struct gws_client_d *) c;
            }
        }
        c = (struct gws_client_d *) c->next;
    };
    return NULL;
}

void __set_default_background_color(unsigned int color)
{
    WindowManager.default_background_color = (unsigned int) color;
}

unsigned int __get_default_background_color(void)
{
    return (unsigned int) WindowManager.default_background_color;
}

void __set_custom_background_color(unsigned int color)
{
    WindowManager.custom_background_color = (unsigned int) color;
    WindowManager.has_custom_background_color = TRUE;
}

unsigned int __get_custom_background_color(void)
{
    return (unsigned int) WindowManager.custom_background_color;
}

int __has_custom_background_color(void)
{
    if (WindowManager.has_custom_background_color == TRUE){
        return TRUE;
    }
    return FALSE;
}

int __has_wallpaper(void)
{
    if (WindowManager.has_wallpaper == TRUE){
        return TRUE;
    }
    return FALSE;
}

// Called by main.c
void wmInitializeStructure(void)
{
// Clear the structure.
    WindowManager.mode = WM_MODE_TILED;  //tiling
// orientation
    WindowManager.vertical = FALSE;   // horizontal. default
    //WindowManager.vertical = TRUE;

// How many frames until now.
    WindowManager.frame_counter = 0;
    WindowManager.fps = 0;

// root window
    WindowManager.root = NULL;
    if ( (void*) __root_window != NULL ){
        if ( __root_window->magic == 1234 ){
            WindowManager.root = (struct gws_window_d *) __root_window;
        }
    }
    WindowManager.taskbar = NULL;
    if ( (void*) taskbar_window != NULL ){
        if ( taskbar_window->magic == 1234 ){
            WindowManager.taskbar = (struct gws_window_d *) taskbar_window;
        }
    }

// fullscreen support.
    WindowManager.is_fullscreen = FALSE;
    //WindowManager.is_fullscreen = TRUE;
    WindowManager.fullscreen_window = NULL;

    //WindowManager.box1 = NULL;
    //WindowManager.box2 = NULL;
    //WindowManager.tray1 = NULL;
    //WindowManager.tray2 = NULL;

// #todo
// Desktop rectangles

// Working area.
// Área de trabalho.
    WindowManager.wa_left   = 0;
    WindowManager.wa_top    = 0;
    WindowManager.wa_width  = 0;
    WindowManager.wa_height = 0;

// Default background color.
    __set_default_background_color(WM_DEFAULT_BACKGROUND_COLOR);
// Default background color.
    __set_custom_background_color(COLOR_GREEN);
    WindowManager.has_custom_background_color = FALSE;
// Wallpaper
    WindowManager.has_wallpaper = FALSE;
// Has loadable theme.
    WindowManager.has_theme = FALSE;
// Not initialized yet.
// We need to setup the windows elements.
    WindowManager.initialized = FALSE;

    Initialization.wm_struct_checkpoint = TRUE;
}


// Internal
// Called by wm_process_windows().

void __update_fps(void)
{
    unsigned long dt=0;
    char rate_string[32];

    //debug_print ("__update_fps:\n");

// counter
    frames_count++;

//
// == time =========================================
//

    // #bugbug
    // We have a HUGE problem here.
    // We can't properly get the data inside the structures. 
    // The value is not the same when we enter inside the kernel via
    // keyboard interrupt or via system interrupt.


// get current time.

// #bugbug
// A variável do indice 120 não esta sendo usada.
// Vamos tentar a variável do indice 118, que é a jiffies.

    //____new_time = rtl_get_progress_time();
    //____new_time = (unsigned long) rtl_get_system_metrics (120);
    ____new_time = (unsigned long) rtl_get_system_metrics (118);

// delta
    dt = (unsigned long) (____new_time - ____old_time);

    ____old_time = ____new_time;


    fps = (1000/dt);

// mostra 
    //if ( show_fps_window == TRUE )
    //{
        //itoa(____new_time,rate_string);
        //itoa(dt,rate_string);
        itoa(fps,rate_string);
        yellow_status(rate_string);
    //}

    return;
    

    //if(dt<8)
        //return;

//=============================================================
// ++  End

    //t_end = rtl_get_progress_time();
    //__refresh_rate =  t_end - t_start;
    //__refresh_rate = __refresh_rate/1000;
    //printf ("@ %d %d %d \n",__refresh_rate, t_now, t_old);


    //====================================
    // fps++
    // conta quantos frames. 

    // se passou um segundo.
    //if ( dt > 1000 )
    if ( dt > 8 )
    {
        // Save old time.
        ____old_time = ____new_time;
        
        //fps = frames_count; // quantos frames em 1000 ms aproximadamente?
        //itoa(fps,rate_string); 

        itoa(dt,rate_string); // mostra o delta.

        //if ( show_fps_window == TRUE ){
            yellow_status(rate_string);
        //}

        // Clean for next round.
        frames_count=0;
        fps=0;
        dt=0;
    }
    //fps--
    //=======================

    //debug_print ("__update_fps: done\n");
}


// WORKER
// Paint button borders.
// Called by doCreateWindow
// >>> No checks
// IN: window, color, color, color, color.
// color1: left,top
// color2: right, bottom
// #check
// This routine is calling the kernel to paint the rectangle.
void 
__draw_button_borders(
    struct gws_window_d *w,
    unsigned int color1,
    unsigned int color2,
    unsigned int color2_light,
    unsigned int outer_color )
{

    // #test
    // size in pizels de apenas 1/3 de todo o size.
    unsigned long BorderSize = 1;
    // Isso deve ser o total.
    //window->border_size = ?

    //debug_print("__draw_button_borders:\n");

// This is the window for relative positions.
    if ( (void*) w == NULL ){
        return;
    }
    if (w->magic!=1234){
        return;
    }

//  ____
// |
//
// board1, borda de cima e esquerda.

// Cima
// top, top+1, top+2
    rectBackbufferDrawRectangle ( 
        w->left+1, 
        w->top, 
        w->width-2,
        BorderSize, 
        outer_color, TRUE,0 );
    rectBackbufferDrawRectangle ( 
        w->left+1, 
        w->top+1, 
        w->width-2, 
        BorderSize, 
        color1, TRUE,0 );
    rectBackbufferDrawRectangle ( 
        w->left+1+1, 
        w->top+1+1,
        w->width-4, 
        BorderSize, 
        color1, TRUE,0 );

// Esq
// left, left+1, left+2
    rectBackbufferDrawRectangle ( 
        w->left, 
        w->top+1, 
        BorderSize, 
        w->height-2,
        outer_color, TRUE,0 );
    rectBackbufferDrawRectangle ( 
        w->left+1, 
        w->top+1, 
        BorderSize, 
        w->height-2,
        color1, TRUE,0 );
    rectBackbufferDrawRectangle ( 
        w->left+1+1, 
        w->top+1+1, 
        BorderSize, 
        w->height-4,
        color1, TRUE,0 );

//  
//  ____|
//
// board2, borda direita e baixo.

// Dir
// right-3, right-2, right-1
    rectBackbufferDrawRectangle ( 
        ((w->left) + (w->width) -1), 
        w->top+1, 
        BorderSize, 
        w->height-2, 
        outer_color, TRUE, 0 );
    rectBackbufferDrawRectangle ( 
        ((w->left) + (w->width) -1 -1), 
        w->top+1, 
        BorderSize, 
        w->height-2, 
        color2, TRUE, 0 );
    rectBackbufferDrawRectangle ( 
        ((w->left) + (w->width) -1 -1 -1), 
        w->top+1+1, 
        BorderSize, 
        w->height-4, 
        color2_light, TRUE, 0 );


// Baixo
// bottom-1, bottom-2, bottom-3
    rectBackbufferDrawRectangle ( 
        w->left+1, 
        ( (w->top) + (w->height) -1 ),  
        w->width-2, 
        BorderSize, 
        outer_color, TRUE, 0 );
    rectBackbufferDrawRectangle ( 
        w->left+1, 
        ( (w->top) + (w->height) -1 -1),  
        w->width-2, 
        BorderSize, 
        color2, TRUE, 0 );
    rectBackbufferDrawRectangle ( 
        w->left+1+1, 
        ( (w->top) + (w->height) -1 -1 -1),  
        w->width-4, 
        BorderSize, 
        color2_light, TRUE, 0 );
}


// Criando controles para uma tilebar ou 
// outro tipo de janela talvez.
// The title bar has controls.
//tbWindow->Controls.minimize = NULL;
//tbWindow->Controls.maximize = NULL;
//tbWindow->Controls.close = NULL;

void do_create_controls(struct gws_window_d *window)
{
    struct gws_window_d *minimize;
    struct gws_window_d *maximize;
    struct gws_window_d *close;
    int id=-1;

    if ( (void*)window == NULL ){
        return;
    }
    if (window->magic != 1234){
        return;
    }
    //if(window->isTitleBar!=TRUE)
    //    return;


    window->Controls.minimize_wid = -1;
    window->Controls.maximize_wid = -1;
    window->Controls.close_wid    = -1;
    window->Controls.initialized = FALSE;

    unsigned long LastLeft = 0; 
    unsigned long Top=0;
    unsigned long ButtonWidth = 28;
    unsigned long ButtonHeight = (METRICS_TITLEBAR_DEFAULT_HEIGHT -2);

    unsigned long PaddingWidth = 1;

// #test
// #bugbug
    //Top=1;
    //ButtonWidth  = (unsigned long) (window->width -4);
    //ButtonHeight = (unsigned long) (window->height -4);

// ================================================
    LastLeft = 
        (unsigned long) ( window->width - (3*PaddingWidth) - (ButtonWidth*3) );

    minimize = 
        (struct gws_window_d *) CreateWindow ( 
            WT_BUTTON, 0, 1, 1, 
            "_",  //string  
            LastLeft,  //l 
            Top,       //t 
            ButtonWidth, 
            ButtonHeight,   
            window, 0, COLOR_GRAY, COLOR_GRAY );

    if ( (void *) minimize == NULL ){
        //gwssrv_debug_print ("xx: minimize fail \n");
        return;
    }
    if(minimize->magic!=1234)
        return;
    
    minimize->type = WT_BUTTON;
    minimize->isMinimizeControl = TRUE;
    
    id = RegisterWindow(minimize);
    if (id<0){
        gwssrv_debug_print ("xxx: Couldn't register minimize\n");
        return;
    }
    window->Controls.minimize_wid = (int) id;


// ================================================
    LastLeft = window->width - (2*PaddingWidth) - (ButtonWidth*2);
    
    maximize = 
        (struct gws_window_d *) CreateWindow ( 
            WT_BUTTON, 0, 1, 1, 
            "M",  //string  
            LastLeft,  //l 
            Top,       //t 
            ButtonWidth, 
            ButtonHeight,   
            window, 0, COLOR_GRAY, COLOR_GRAY );

    if ( (void *) maximize == NULL ){
        //gwssrv_debug_print ("xx: maximize fail \n");
        return;
    }
    if(maximize->magic!=1234)
        return;

    maximize->type = WT_BUTTON;
    maximize->isMaximizeControl = TRUE;
    
    id = RegisterWindow(maximize);
    if (id<0){
        gwssrv_debug_print ("xxx: Couldn't register maximize\n");
        return;
    }
    window->Controls.maximize_wid = (int) id;

// ================================================
    LastLeft = window->width - (1*PaddingWidth) - (ButtonWidth*1);
    
    close = 
        (struct gws_window_d *) CreateWindow ( 
            WT_BUTTON, 0, 1, 1, 
            "X",  //string  
            LastLeft,  //l 
            Top,       //t 
            ButtonWidth, 
            ButtonHeight,   
            window, 0, COLOR_GRAY, COLOR_GRAY );

    if ( (void *) close == NULL ){
        //gwssrv_debug_print ("xx: close fail \n");
        return;
    }
    if(close->magic!=1234)
        return;

    close->type = WT_BUTTON;
    close->isCloseControl = TRUE;

    id = RegisterWindow(close);
    if (id<0){
        gwssrv_debug_print ("xxx: Couldn't register close\n");
        return;
    }
    window->Controls.close_wid = (int) id;

    window->Controls.initialized = TRUE;
}

// worker:
// Draw the border of edit box and overlapped windows.
// >> no checks
// #check
// This routine is calling the kernel to paint the rectangle.
void 
__draw_window_border( 
    struct gws_window_d *parent, 
    struct gws_window_d *window )
{
    //debug_print("__draw_window_border:\n");

    if ( (void*) parent == NULL )
        return;

    if ( (void*) window == NULL )
        return;

// Editbox
    if ( window->type == WT_EDITBOX || 
         window->type == WT_EDITBOX_MULTIPLE_LINES )
    {

        // border size:
        // #todo: it can't be hardcoded,
        // We're gonna have themes.
        if (window->focus == TRUE){
            window->border_size=2;
        }
        if (window->focus == FALSE){
            window->border_size=1;
        }

        // top
        rectBackbufferDrawRectangle( 
            window->left, 
            window->top, 
            window->width, 
            window->border_size, 
            window->border_color1, 
            TRUE, 
            0 );
        // left
        rectBackbufferDrawRectangle( 
            window->left, 
            window->top, 
            window->border_size, 
            window->height, 
            window->border_color1, 
            TRUE, 
            0 );
        // right
        rectBackbufferDrawRectangle( 
            (window->left + window->width - window->border_size), 
            window->top,  
            window->border_size, 
            window->height, 
            window->border_color2, 
            TRUE, 
            0 );
        // bottom
        rectBackbufferDrawRectangle ( 
            window->left, 
            (window->top + window->height - window->border_size), 
            window->width, 
            window->border_size, 
            window->border_color2, 
            TRUE, 
            0 );
        
        // #test
        // Subtract border size.
        //window->left   += window->border_size;
        //window->top    += window->border_size;
        //window->right  -= window->border_size;
        //window->bottom -= window->border_size;
    }

// Overlapped
    if (window->type == WT_OVERLAPPED)
    {

        if (window->active == TRUE)
            window->border_size=2;
        if (window->active == FALSE)
            window->border_size=1;

        // top
        rectBackbufferDrawRectangle( 
            parent->left + window->left, 
            parent->top  + window->top, 
            window->width, 
            window->border_size, 
            window->border_color1, 
            TRUE, 
            0 );
        // left
        rectBackbufferDrawRectangle( 
            parent->left + window->left, 
            parent->top + window->top, 
            window->border_size, window->height, 
            window->border_color1, 
            TRUE,
            0 );
        // right
        rectBackbufferDrawRectangle( 
            (parent->left + window->left + window->width - window->border_size), 
            (parent->top + window->top), 
            window->border_size, 
            window->height, 
            window->border_color2, 
            TRUE,
            0 );
        // bottom
        rectBackbufferDrawRectangle ( 
            (parent->left + window->left), 
            (parent->top + window->top + window->height - window->border_size), 
            window->width, 
            window->border_size, 
            window->border_color2, 
            TRUE,
            0 );
    
        // #test
        // Subtract border size.
        //window->left   += window->border_size;
        //window->top    += window->border_size;
        //window->right  -= window->border_size;
        //window->bottom -= window->border_size;

    }
}


/*
 * wmCreateWindowFrame:
 * 
 */

// Called by CreateWindow in createw.c

// #importante:
// Essa rotina será chamada depois que criarmos uma janela básica,
// mas só para alguns tipos de janelas, pois nem todos os tipos 
// precisam de um frame. Ou ainda, cada tipo de janela tem um 
// frame diferente. Por exemplo: Podemos considerar que um checkbox 
// tem um tipo de frame.
// Toda janela criada pode ter um frame.
// Durante a rotina de criação do frame para uma janela que ja existe
// podemos chamar a rotina de criação da caption bar, que vai criar os
// botões de controle ... mas nem toda janela que tem frame precisa
// de uma caption bar (Title bar).
// Estilo do frame:
// Dependendo do estilo do frame, podemos ou nao criar a caption bar.
// Por exemplo: Uma editbox tem um frame mas não tem uma caption bar.

// IN:
// parent = parent window ??
// window = The window where to build the frame.
// x
// y
// width
// height
// style = Estilo do frame.

// OUT:
// 0   = ok, no erros;
// < 0 = not ok. something is wrong.

int 
wmCreateWindowFrame ( 
    struct gws_window_d *parent,
    struct gws_window_d *window,
    unsigned long border_size,
    unsigned int border_color1,
    unsigned int border_color2,
    unsigned int border_color3,
    unsigned int ornament_color1,
    unsigned int ornament_color2,
    int style ) 
{
    int useFrame       = FALSE;
    int useTitleBar    = FALSE;
    int useTitleString = FALSE;
    int useBorder      = FALSE;
    int useIcon        = FALSE;
    // ...

// #bugbug
// os parâmetros 
// parent, 
// x,y,width,height 
// não estão sendo usados.


// Overlapped.
// Janela de aplicativos.

// Title bar and status bar.
    struct gws_window_d  *tbWindow;
    struct gws_window_d  *sbWindow;

    int id=-1;  //usado pra registrar janelas filhas.

    int Type=0;


// Border size
    unsigned long BorderSize = (border_size & 0xFFFF);

// Border color
    unsigned int BorderColor1 = border_color1;  // top/left
    unsigned int BorderColor2 = border_color2;  // right/bottom
    unsigned int BorderColor3 = border_color3;

// Ornament color
    unsigned int OrnamentColor1 = ornament_color1;
    unsigned int OrnamentColor2 = ornament_color2;

// Title bar height
    unsigned long TitleBarHeight = METRICS_TITLEBAR_DEFAULT_HEIGHT;

// Title bar color.
    unsigned int TitleBarColor = COLOR_BLUE1;   // Light blue (Um pouco fosco) 
    //unsigned long TitleBarColor = 0x001473E6;  // Claro.  
    //unsigned long TitleBarColor = 0x00000E80;  // Dark blue
    // ...

    //unsigned long X = (x & 0xFFFF);
    //unsigned long Y = (y & 0xFFFF);
    //unsigned long Width = (width & 0xFFFF);
    //unsigned long Height = (height & 0xFFFF);

// #debug
    //gwssrv_debug_print ("wmCreateWindowFrame:\n");

// #todo
// Se estamos minimizados ou a janela mãe está minimizada,
// então não temos o que pintar.
// #todo
// O estilo de frame é diferente se estamos em full screen ou maximizados.
// não teremos bordas laterais
// #todo
// Cada elemento da frame que incluimos, incrementa
// o w.top do retângulo da área de cliente.

// check parent

    if ( (void*) parent == NULL ){
        gwssrv_debug_print ("wmCreateWindowFrame: [FAIL] parent\n");
        return -1;
    }

    if (parent->used != TRUE || parent->magic != 1234)
        return -1;

// check window

    if ( (void*) window == NULL ){
        gwssrv_debug_print ("wmCreateWindowFrame: [FAIL] window\n");
        return -1;
    }

    if (window->used != TRUE || window->magic != 1234)
        return -1;

// #bugbug
// Estamos mascarando pois os valores anda corrompendo.

    window->left   = (window->left   & 0xFFFF);
    window->top    = (window->top    & 0xFFFF);
    window->width  = (window->width  & 0xFFFF);
    window->height = (window->height & 0xFFFF);

// #todo
// Desenhar o frame e depois desenhar a barra de títulos
// caso esse estilo de frame precise de uma barra.
// Editbox
// EDITBOX NÃO PRECISA DE BARRA DE TÍTULOS.
// MAS PRECISA DE FRAME ... QUE SERÃO AS BORDAS.
    
//
// Type
//

// Qual é o tipo da janela em qual precisamos
// criar o frame. Isso indica o tipo de frame.

    Type = window->type;

    switch (Type){
    
    case WT_EDITBOX:     
    case WT_EDITBOX_MULTIPLE_LINES:     
        useFrame=TRUE; 
        useIcon=FALSE;
        useBorder=TRUE;
        break;

    case WT_OVERLAPPED:  
        useFrame=TRUE; 
        useIcon=TRUE;
        useTitleBar=TRUE;  // Normalmente uma janela tem a barra de t[itulos.
        if ( window->style & 0x0001 ){ useTitleBar=FALSE; }    //maximized
        //if ( window->style & 0x0004 ){ useTitleBar=FALSE; }  //fullscreen
        useTitleString=TRUE;
        useBorder=TRUE;
        break;
    
    case WT_BUTTON:      
        useFrame=TRUE;
        useIcon=FALSE; 
        break;

    //default: break;
    };

    if ( useFrame == FALSE ){
        gwssrv_debug_print ("wmCreateWindowFrame: [ERROR] This type does not use a frame.\n");
        return -1;
    }

// ===============================================
// editbox

    if ( Type == WT_EDITBOX || 
         Type == WT_EDITBOX_MULTIPLE_LINES )
    {

        // #todo
        // The window structure has a element for border size
        // and a flag to indicate that border is used.
        // It also has a border style.

        // #todo: Essa rotina de cores deve ir para
        // dentro da função __draw_window_border().
        // ou passar tudo via argumento.
        // ou criar uma rotina para mudar as caracteristicas da borda.
         
        // Se tiver o foco.
        if ( window->focus == TRUE ){
            BorderColor1 = COLOR_BLUE;
            BorderColor2 = COLOR_BLUE;
            BorderSize = 2;
        }else{
            BorderColor1 = COLOR_INACTIVEBORDER;
            BorderColor2 = COLOR_INACTIVEBORDER;
            BorderSize = 1;
        };
        
        window->border_size = 0;
        window->borderUsed = FALSE;
        if (useBorder==TRUE){
            window->border_color1 = (unsigned int) BorderColor1;
            window->border_color2 = (unsigned int) BorderColor2;
            window->border_size = BorderSize;
            window->borderUsed = TRUE;
        }

        // Draw the border of an edit box.
        __draw_window_border(parent,window);

        return 0;
    }


// ===============================================
// overlapped?
// Draw border, titlebar and status bar.

    // String support.
    // String at center?
    unsigned long StringLeftPad = 0;
    unsigned long StringTopPad = 8;  // char size.
    size_t StringSize = (size_t) strlen( (const char *) window->name );
    if (StringSize > 64){
        StringSize = 64;
    }

// pad | icon | pad | pad
    StringLeftPad = 
        (unsigned long) ((2*METRICS_ICON_LEFTPAD) +16 +METRICS_ICON_LEFTPAD);

// #todo:
// String right não pode ser maior que 'last left' button.

    if (Type == WT_OVERLAPPED)
    {
        // #todo
        // Maybe we nned border size and padding size.
        
        // Consistente para overlapped.
        BorderSize = METRICS_BORDER_SIZE;
        // ...
        
        // #todo
        // The window structure has a element for border size
        // and a flag to indicate that border is used.
        // It also has a border style.

        // Se tiver o foco.
        if ( window->focus == TRUE ){
            BorderColor1 = COLOR_BLUE1;
            BorderColor2 = COLOR_BLUE1;
        }else{
            BorderColor1 = COLOR_INACTIVEBORDER;
            BorderColor2 = COLOR_INACTIVEBORDER;
        };

        window->border_size = 0;
        window->borderUsed = FALSE;
        if (useBorder==TRUE){
            window->border_color1 = (unsigned int) BorderColor1;
            window->border_color2 = (unsigned int) BorderColor2;
            window->border_size   = BorderSize;
            window->borderUsed    = TRUE;
        }

        // Quatro bordas de uma janela overlapped.
        __draw_window_border(parent,window);

        // #important:
        // The border in an overlapped window will affect
        // the top position of the client area rectangle.
        window->rcClient.top += window->border_size;

        //
        // Title bar
        //

        // #todo
        // The window structure has a flag to indicate that
        // we are using titlebar.
        // It also has a title bar style.
        // Based on this style, we can setup some
        // ornaments for this title bar.
        // #todo
        // Simple title bar.
        // We're gonna have a wm inside the window server.
        // The title bar will be very simple.
        // We're gonna have a client area.
        // #bugbug
        // Isso vai depender da resolução da tela.
        // Um tamanho fixo pode fica muito fino em uma resolução alta
        // e muito largo em uma resolução muito baixa.
        
        // Title bar
        // Se a janela overlapped tem uma title bar.
        // #todo: Essa janela foi registrada?
        if (useTitleBar == TRUE)
        {
            window->titlebar_height = TitleBarHeight;
            window->titlebar_color      = (unsigned int) TitleBarColor;
            window->titlebar_text_color = (unsigned int) COLOR_RED;
            
            // #important: WT_SIMPLE with text.
            // lembre-se estamos relativos à area de cliente
            // da janela mão, seja ela de qual tipo for.
            tbWindow = 
                (void *) doCreateWindow ( 
                             WT_SIMPLE, 0, 1, 1, "TitleBar", 
                             0,  //l 
                             0,  //t
                             (window->width - BorderSize - BorderSize), 
                             window->titlebar_height, 
                             (struct gws_window_d *) window, 
                             0, 
                             window->titlebar_color,  //frame 
                             window->titlebar_color,  //client
                             (unsigned long) window->rop );   // rop_flags from the parent 

            if ( (void *) tbWindow == NULL ){
                gwssrv_debug_print ("wmCreateWindowFrame: tbWindow fail \n");
                return -1;
            }
            tbWindow->type = WT_SIMPLE;
            tbWindow->isTitleBar = TRUE;
            window->titlebar = (struct gws_window_d *) tbWindow;  // Window pointer!
            // Register window
            id = RegisterWindow(tbWindow);
            if (id<0){
                gwssrv_debug_print ("wmCreateWindowFrame: Couldn't register window\n");
                return -1;
            }
            
            //local
            do_create_controls(tbWindow);

            // #important:
            // The Titlebar in an overlapped window will affect
            // the top position of the client area rectangle.
            // Depois de pintarmos a titlebar,
            // temos que atualizar o top da área de cliente.
            window->rcClient.top += window->titlebar_height;

            // Ornamento:
            // Ornamento na parte de baixo da title bar.
            // #important:
            // O ornamento é pintado dentro da barra, então isso
            // não afetará o positionamento da área de cliente.
            // border on bottom.
            // Usado para explicitar se a janela é ativa ou não
            // e como separador entre a barra de títulos e a segunda
            // área da janela de aplicativo.
            // Usado somente por overlapped window.
        
            window->frame.ornament_color1   = OrnamentColor1;  //COLOR_BLACK;
            window->titlebar_ornament_color = OrnamentColor1;  //COLOR_BLACK;
        
            rectBackbufferDrawRectangle ( 
                tbWindow->left, 
                ( (tbWindow->top) + (tbWindow->height) - METRICS_TITLEBAR_ORNAMENT_SIZE ),  
                tbWindow->width, 
                METRICS_TITLEBAR_ORNAMENT_SIZE, 
                OrnamentColor1, 
                TRUE,
                0 );  // rop_flags no rop in this case?

            //
            // Icon (Titlebar)
            //

            // O posicionamento em relação
            // à janela é consistente por questão de estilo.
            // See: bmp.c
            // IN: index, left, top.

            window->titlebarHasIcon = FALSE;
            
            // Icon ID:
            // Devemos receber um argumento do aplicativo,
            // na hora da criação da janela.
            window->frame.titlebar_icon_id = 1;

            // Decode the bmp that is in a buffer
            // and display it directly into the framebuffer. 
            // IN: index, left, top
            // see: bmp.c

            if (useIcon == TRUE)
            {
                window->titlebarHasIcon = TRUE;
                gwssrv_display_system_icon( 
                    (int) window->frame.titlebar_icon_id, 
                    (tbWindow->left + METRICS_ICON_LEFTPAD), 
                    (tbWindow->top  + METRICS_ICON_TOPPAD) );
            }

            //
            // String (title bar)
            //
        
            window->titlebar_text_color = COLOR_WHITE;
        
            // #todo
            // Temos que gerenciar o posicionamento da string.
        
            // #bugbug: Use 'const char *'
            tbWindow->name = 
                (char *) strdup( (const char *) window->name );
        
            //#todo: validation
            //if ( (void*) tbWindow->name == NULL ){}
        
            if (useTitleString == TRUE){
                grDrawString ( 
                    ((tbWindow->left) + StringLeftPad), 
                    ((tbWindow->top)  + StringTopPad), 
                    COLOR_WHITE, 
                    tbWindow->name );
            }

        }  //--use title bar.
        // ooooooooooooooooooooooooooooooooooooooooooooooo

        // #todo:
        // nessa hora podemos pintar a barra de menu, se o style
        // da janela assim quiser. Depois disso precisaremos
        // atualizar o top da área de cliente.
        //window->rcClient.top += window->titlebar_height;

        //
        // Status bar (bottom)
        // 
        
        // A janela overlapped tem statusbar.
        // Se for maximized ou fullscreen
        // #todo: Essa janela foi registrada?
        if ( window->style & 0x0008 )
        {
            window->statusbar_height = 32;
            window->statusbar_color = (unsigned int) 0x00AC81;

            unsigned long sbTop = 0;
            unsigned long sbWidth = 8;
            if ( (void*) window->parent != NULL )
            {
                sbTop = 
                (unsigned long) (window->rcClient.height - window->statusbar_height);
                sbWidth = 
                (unsigned long) (window->rcClient.width);
            }

            // estamos relativos à nossa área de cliente
            // Seja ela do tipo que for.
            // #todo: apos criarmos a janela de status no fim da
            // area de cliente, então precisamos redimensionar a
            // nossa área de cliente.
            sbWindow = 
                (void *) doCreateWindow ( 
                             WT_SIMPLE, 0, 1, 1, "Statusbar", 
                             0,       //l                                                //left
                             sbTop,   //t
                             sbWidth, //width 
                             window->statusbar_height,  //height                                  //height
                             (struct gws_window_d *) window, 
                             0, 
                             window->statusbar_color,  //frame
                             window->statusbar_color,  //client
                             (unsigned long) window->rop );   // rop_flags  
            
            // Depois de pintarmos a status bar, caso o estilo exija,
            // então devemos atualizar a altura da área de cliente.
            window->rcClient.height -= window->statusbar_height;

            
            if ( (void *) sbWindow == NULL ){
                gwssrv_debug_print ("wmCreateWindowFrame: sbWindow fail \n");
                return -1;
            }
            sbWindow->type = WT_SIMPLE;
            sbWindow->isStatusBar = TRUE;
            window->statusbar = (struct gws_window_d *) sbWindow;  // Window pointer.
            // Register window
            id = RegisterWindow(tbWindow);
            if (id<0){
                gwssrv_debug_print ("wmCreateWindowFrame: Couldn't register window\n");
                return -1;
            }
        }

        // ok
        return 0;
    }

// ===============================================
// button

    //button
    if (Type == WT_BUTTON)
    {
        gwssrv_debug_print ("wmCreateWindowFrame: [TODO] frame for button\n");
        
        //todo frame or not
        //just like the edit box.   
        
        // ok     
        return 0;
    }

// ===============================================

    return 0;
}


void wm_flush_rectangle(struct gws_rect_d *rect)
{
    if( (void*) rect == NULL )
        return;
    if(rect->magic!=1234)
        return;

    gwssrv_refresh_this_rect(rect);
}


// Change the root window color and reboot.
void wm_reboot(void)
{
    if ( (void*) __root_window != NULL )
    {
        if (__root_window->magic==1234){
            __root_window->bg_color = COLOR_BLUE;
            redraw_window(__root_window,TRUE);
        }
    }
    rtl_reboot();
}

void wm_flush_window(struct gws_window_d *window)
{
    if( (void*) window == NULL ){
        return;
    }
    if(window->used != TRUE) { return; }
    if(window->magic != 1234){ return; }
    gws_show_window_rect(window);
}

void wm_flush_screen(void)
{
    gwssrv_show_backbuffer();
}


// Refresh screen via kernel.
// Copy the backbuffer in the frontbuffer(lfb).
// #??
// It uses the embedded window server in the kernel.
//#define	SYSTEMCALL_REFRESHSCREEN        11
// #todo
// trocar o nome dessa systemcall.
// refresh screen será associado à refresh all windows.

void gwssrv_show_backbuffer(void)
{
    // #todo: Just invalidate root window.
    gramado_system_call(11,0,0,0);
}

// Called by the main routine for now.
// Its gonne be called by the timer.
// See: comp.c
void 
wmCompose(
    unsigned long jiffies, 
    unsigned long clocks_per_second )
{
    compose();
}


/*
// Marca como 'dirty' todas as janelas filhas,
// dai o compositor faz o trabalho de exibir na tela.
void refresh_subwidnows( struct gws_window_d *w );
void refresh_subwidnows( struct gws_window_d *w )
{
}
*/


/*
 * wmRefreshDirtyRectangles: 
 */

// Called by compose().
// O compositor deve ser chamado para compor um frame 
// logo após uma intervenção do painter, que reaje às
// ações do usuário.
// Ele não deve ser chamado X vezes por segundo.
// Quem deve ser chamado X vezes por segundo é a rotina 
// de refresh, que vai efetuar refresh dos retângulos sujos e
// dependendo da ação do compositor, o refresh pode ser da tela toda.

void wmRefreshDirtyRectangles(void)
{
    register int i=0;
    struct gws_window_d *tmp;

// #debug
    //gwssrv_debug_print("wmRefreshDirtyRectangles:\n");

//==========================================================
// ++  Start

    //t_start = rtl_get_progress_time();

//
// == Update screen ====================
//

// Redrawing all the windows.
// redraw using zorder.
// refresh using zorder.
// Invalidating all the windows ... 
// and it will be flushed into the framebuffer for the ring0 routines.

//
// Update
//

/*
// #debug 
// Bar for debug.
// update message/hour/date ...
    if ( (void*) __root_window != NULL ) 
    {
        if ( __root_window->magic == 1234 )
        {
                //redraw_window(tmp,FALSE);  //bugbug
                grDrawString(
                    __root_window->left, 
                    __root_window->top,
                    COLOR_WHITE,
                    "Gramado OS");
               invalidate_window(__root_window);
               //__root_window->redraw = FALSE;
        }
    }
*/

// ======================================================
// Flush
// #todo #bugbug
// Flush all the dirty windows into the framebuffer.
// It will lookup the main window list.
// This is a very slow way of doing this.
// But it is just a test.


// =======================
// #test

    //int UpdateScreenFlag=FALSE;
    int UpdateScreenFlag=TRUE;

    if (UpdateScreenFlag != TRUE){
        return;
    }

// Refresh
// Lookup the main window list.
// #todo: This is very slow. We need a linked list.
// Get next
// It is a valid window and
// it is a dirty window.
// Flush the window's rectangle.

    for (i=0; i<WINDOW_COUNT_MAX; ++i)
    {
        tmp = (struct gws_window_d *) windowList[i];

        if ( (void*) tmp != NULL )
        {
            if ( tmp->used == TRUE && tmp->magic == 1234 )
            {
                if ( tmp->dirty == TRUE )
                {
                    //Wrappers
                    //wm_flush_window(tmp);       //checking parameters
                    //gws_show_window_rect(tmp);  //checking parameters and invalidate.

                    // Direct, no checks.
                    gws_refresh_rectangle ( 
                        tmp->left, tmp->top, tmp->width, tmp->height ); 

                    validate_window(tmp);
                }
            }
        }
    };

// =======================
}


// #bugbug
// This name is wrong.
// A frame can be only a window inside a client application.
void flush_frame(void)
{
    wm_flush_screen();
}


static void animate_window( struct gws_window_d *window )
{
    int i=0;
    
    if( (void*) window == __root_window)
        return;
    if(window->magic!=1234)
        return;
    
    for (i=0; i<800; i++)
    {
         if( (window->left - 1) == 0)
             return;
         if( (window->top - 1) == 0)
             return;

         gwssrv_change_window_position(
              window, 
              window->left -1, 
              window->top  -1);
              redraw_window(window,FALSE);
              invalidate_window(window);
    };
}


// Starting with the first_window of the list,
// create a stack of windows
// This is the zorder.
// #todo:
// only application windows? overlapped.

static void __Tile(void)
{
    struct gws_window_d *w;
    int cnt=0;
    int c=0;
    register int i=0;

// Nothing to do.

    if (CONFIG_USE_TILE != 1){
        return;
    }
    if (current_mode == GRAMADO_JAIL){
        return;
    }
    if (WindowManager.initialized != TRUE){
        return;
    }

// Start with the first_window of the list.
// zorder: The last window is on top of the zorder.

// =============================
// Get the size of the list.
    cnt=0;
    w = (struct gws_window_d *) first_window;
    if((void*)w==NULL){
        debug_print("__Tile: w==NULL\n");
        return; 
    }
    while ((void*)w != NULL){
        w = (struct gws_window_d *) w->next;
        cnt++;
    };

// =============================
// Starting with the first window of the list,
// create a stack of windows in the top/left corner of the screen.
    w = (struct gws_window_d *) first_window;
    if ((void*)w==NULL){ 
        debug_print("__Tile: w==NULL\n");
        return; 
    }
    //if(w->magic!=1234)
    //    return;

// #bugbug: 
// limite provisorio

    //if( cnt>4 ){
    //    cnt=4;
    //}

// Initializing
    unsigned long Left   = WindowManager.wa_left;
    unsigned long Top    = WindowManager.wa_top;
    unsigned long Width  = WindowManager.wa_width;
    unsigned long Height = WindowManager.wa_height;
    unsigned long l2=0;
    unsigned long t2=0;
    unsigned long w2=0;
    unsigned long h2=0;

    i=0;

    if (cnt<=0)
        return;

    while ((void*)w != NULL)
    {
        if (i >= cnt){
            break;
        }

        // Window manager in tiled mode.
        if (WindowManager.mode == WM_MODE_TILED)
        {
            // Horizontal
            if(WindowManager.vertical==FALSE)
            {
                // for titlebar color support.
                // not the active window.
                w->active = FALSE;
                w->focus = TRUE;
                w->border_size = 1;
                
                // resize.
                // width: Metade da largura da área de trabalho.
                // height: Altura da área de trabalho dividido pela
                // quantidade de janelas que temos.
                Width  = (unsigned long) (WindowManager.wa_width / 2) -4;
                Height = (unsigned long) WindowManager.wa_height;
                if (cnt > 1)
                    Height = (unsigned long) (WindowManager.wa_height / (cnt-1));
                
                w2 = Width;
                h2 = Height -4;
                gws_resize_window(w, w2, h2);

                // positions.
                // left: comaça na metade da área de tranalho.
                // top: depende do indice da janela na lista.
                Left = (unsigned long) (WindowManager.wa_width / 2) +2;
                Top  = (unsigned long) (Height * i);
                l2 = Left;
                t2 = Top +2; 
                gwssrv_change_window_position(w, l2, t2);

                // master?
                // Se estivermos na última janela da lista,
                // então ela será a master.
                // ocupara toda a metade esquerda da área de trabalho.
                if (i == cnt-1)
                {
                    // for titlebar color support.
                    // the active window.
                    w->active = TRUE;
                    w->focus = TRUE;
                    w->border_size = 2;
                    keyboard_owner = (void*) w;
                    last_window    = (void*) w;
                    top_window     = (void*) w;  //z-order: top window.
                    
                    // resize.
                    // width: metade da área de trabalho.
                    // height: altura da área de trabalho.
                    Width  = (unsigned long) (WindowManager.wa_width / 2);
                    Height = (unsigned long) WindowManager.wa_height;
                    w2 = Width  -4;
                    h2 = Height -4;
                    gws_resize_window(w, w2, h2);

                    // positions.
                    // Canto superior esquerdo. Com padding.
                    Left = (unsigned long) WindowManager.wa_left;
                    Top  = (unsigned long) WindowManager.wa_top; 
                    l2 = Left +2;
                    t2 = Top  +2;
                    gwssrv_change_window_position(w, l2, t2);
                }
            }

            // Vertical.
            if(WindowManager.vertical==TRUE)
            {
                //#todo:
                //w->height = WindowManager.wa_height; 
                //w->width  = (WindowManager.wa_width/cnt);
                //w->left   = (w->width * i);
                //w->top    = 0;
            }
        }

        w = (struct gws_window_d *) w->next;
        i++;
    };
}


// #todo
// Explain it better.
void wm_update_window_by_id(int wid)
{
    struct gws_window_d *w;
    unsigned long fullWidth = 0;
    unsigned long fullHeight = 0;

// Redraw and show the root window.
    //redraw_window(__root_window,TRUE);

// wid
    if(wid<0)
        return;
    if(wid>=WINDOW_COUNT_MAX)
        return;  
// Window structure
    w = (struct gws_window_d *) windowList[wid];
    if ( (void*)w==NULL )  { return; }
    if ( w->used != TRUE ) { return; }
    if ( w->magic != 1234 ){ return; }

    if (w->type != WT_OVERLAPPED){
        return;
    }

    // #test
    // Empilhando verticalmente.
    if( WindowManager.initialized != TRUE )
        return;

// tiled mode.
// Esses metodos irao atualizar tambem os valores da barra de titulos.
    if(WindowManager.mode == WM_MODE_TILED){
        gwssrv_change_window_position(w,0,0);
        gws_resize_window(
            w,
            WindowManager.wa_width,
            WindowManager.wa_height);
    }

    if (WindowManager.is_fullscreen == TRUE)
    {
        // #test
        // for titlebar color support.
        // the active window.
        w->active = TRUE;
        w->focus = TRUE;
        w->border_size = 2;
        keyboard_owner = (void*) w;
        last_window    = (void*) w;
        top_window     = (void*) w;  //z-order: top window.

        fullWidth  = gws_get_device_width();
        fullHeight = gws_get_device_height();
        gwssrv_change_window_position(w,0,0);
        gws_resize_window(
            w,
            fullWidth,
            fullHeight);
    }

    //keyboard_owner = (void *) w;
    //last_window    = (void *) w;
    //top_window     = (void *) w;

    set_active_window(w);
    set_focus(w);

    redraw_window(w,FALSE);
    invalidate_window(w);

// Paint the childs of the window with focus.
    on_update_window(GWS_Paint);

//#todo: string
    //wm_Update_TaskBar("Win",TRUE);
}

void wm_update_active_window(void)
{
    if ( (void*) active_window == NULL )
        return;
    if (active_window->magic != 1234)
        return;

    wm_update_window_by_id( (int) active_window->id );
}

// vamos gerenciar a janela de cliente
// recentemente criada.
// Precisamos associar essa estrutura de janela
// com uma estrutura de cliente. 
// Somente janelas overlapped serao consideradas clientes
// por essa rotina.
// Isso sera chamado de dentro do serviço que cria janelas.

int wmManageWindow(struct gws_window_d *w)
{
    struct gws_client_d *c;
    struct gws_client_d *tmp;
    register int i=0;

    //yellow_status("wmMan:");
    
    if( (void*) w == NULL )
        goto fail;
    if(w->magic != 1234)
        goto fail;
    if(w->type != WT_OVERLAPPED)
        goto fail;

    if( (void*) first_client == NULL )
        goto fail;

    c = (void *) malloc( sizeof( struct gws_client_d ) );
    if( (void*) c == NULL )
        goto fail;

    c->l = w->left;
    c->t = w->top;
    c->w = w->width;
    c->h = w->height;

    for (i=0; i<4; i++){
        c->tags[i] = TRUE;
    };

    c->pid = w->client_pid;
    c->tid = w->client_tid;

    c->used = TRUE;
    c->magic = 1234;

// coloca na lista
    tmp = (struct gws_client_d *) first_client;
    if( (void*) tmp == NULL )
        goto fail;
    if( tmp->magic != 1234 )
        goto fail;
    
    while(1){
        // found
        if( (void*) tmp->next == NULL )
            break;
        // next
        tmp = (struct gws_client_d *) tmp->next; 
    };

    if( tmp->magic != 1234 )
        goto fail;

    tmp->next = (struct gws_client_d *) c;

    //yellow_status("done");
    return 0;

fail:
    yellow_status("wmManageWindow");
    printf("wmManageWindow: fail\n");
    return -1;
    //while(1){}
}


// Repinta todas as janelas seguindo a ordem da lista
// que está em last_window.
// No teste isso é chamado pelo kernel através do handler.
// Mas também será usado por rotinas internas.
void wm_update_desktop(int tile)
{
    struct gws_window_d *w;  // tmp
    struct gws_window_d *l;  // last of the stack

// #test
// Starting with the first window of the list,
// create a stack o windows in the top/left corner of the screen.
    if (tile){
        //#todo: Maybe we use an argument here. A set of flags.
        if (WindowManager.mode == WM_MODE_TILED)
            __Tile();
    }

// Redraw root window, but do not shot it yet.
    redraw_window(__root_window,FALSE);

// ======================================
// Redraw the whole stack of windows,
// but do not show them yet.
// Only for app windows. (>>> OVERLAPPED <<<).
// Set the last window in the stack as the active window.
// Set focus on the last window of the stack. 

    w = (struct gws_window_d *) first_window;
    if ((void*)w == NULL){ 
        wm_Update_TaskBar("DESKTOP",FALSE);
        invalidate_window(__root_window);
        return; 
    }
    if (w->magic!=1234)
        return;

// This is the last valid for now.
    l = (struct gws_window_d *) w;

    while(1){

        if((void*)w==NULL){ 
            break; 
        }

        if ( (void*) w != NULL )
        {
            // Only overlapped windows.
            if (w->type == WT_OVERLAPPED)
            {
                // This is the last valid for now.
                l = (struct gws_window_d *) w;
                
                // Redraw, but do no show it.
                redraw_window(w,FALSE);

                // paint the childs of the window with focus.
                on_update_window(GWS_Paint);
                
                //invalidate_window(w);
            }
        }

        w = (struct gws_window_d *) w->next; 
    }; 

// Set focus on last valid. Starting at first one.
// Activate
    set_active_window(l);
    set_focus(l);  //no ... focus on client window.

    current_option = OPTION_NOTHING;

// Update the taskbar at the bottom of the screen,
// but do not show it yet.
    wm_Update_TaskBar("DESKTOP",FALSE);
// Invalidate the root window.
// Shows the whole screen
    invalidate_window(__root_window);
}

// Set focus on a window.
// This is the window that owns the keyboard input.
// Change the foreground thread.
// The new foreground thread will be 
// the thread associated with this given window.
void set_focus(struct gws_window_d *window)
{
// A janela que ja tinha o foco de entrada.
    struct gws_window_d *wOld;
    int ClientTID = -1;

    if( (void*) window == NULL ){
        return;
    }
    if ( window->used != TRUE ) { return; }
    if ( window->magic != 1234 ){ return; }

// #todo
// Pegar a antiga janela com o foco de entra.
// ela deve perder o foco de entrada.
    
    //wOld = (struct gws_window_d *) keyboard_owner;
    //if (window != wOld)
    //    kill_focus(wOld);

// #todo
// Vamos setar alguma coisa na estrutura de janela,
// para que o redraw redesenhe o elemento
// contendo alguma indicação de foco.

    //window->focus = TRUE;

// Global variable

// The window that owns the keyboard.
    //keyboard_owner = (struct gws_window_d *) window;
// The window with focus.
    keyboard_owner = (void*) window;

// Pede para o kernel mudar a foreground thread.
// A foreground thread será a thread associada com a janela
// que possui o foco de entrada.
    ClientTID = (int) (window->client_tid & 0xFFFFFFFF);
    if (ClientTID<0){
        return;
    }
    sc82 ( 10011, ClientTID, ClientTID, ClientTID );
}


/*
//#todo
struct gws_window_d *get_active(void);
struct gws_window_d *get_active(void)
{
}
*/

// Pega o ponteiro da janela com foco de entrada.
struct gws_window_d *get_focus(void)
{
    struct gws_window_d *w;

    w = (struct gws_window_d *) keyboard_owner;
    if( (void*)w==NULL ){
        return NULL;
    }
    if(w->used!=TRUE) { return NULL; }
    if(w->magic!=1234){ return NULL; }

    return (struct gws_window_d *) w; 
}

void set_status_by_id( int wid, int status )
{
    struct gws_window_d *w;

// wid
    if(wid<0)
        return;
    if(wid>=WINDOW_COUNT_MAX)
        return;  
// Window structure
    w = (struct gws_window_d *) windowList[wid];
    if ((void*)w==NULL){
        return;
    }
    if (w->used != TRUE) { return; }
    if (w->magic != 1234){ return; }

// Set status
    w->status = status;
}

void set_focus_by_id(int wid)
{
    struct gws_window_d *w;

// wid
    if(wid<0)
        return;
    if(wid>=WINDOW_COUNT_MAX)
        return;  
// Window structure
    w = (struct gws_window_d *) windowList[wid];
    if ((void*)w==NULL){
        return;
    }
    if (w->used != TRUE) { return; }
    if (w->magic != 1234){ return; }

    set_focus(w);
}

void set_first_window( struct gws_window_d *window)
{
    first_window = (struct gws_window_d *) window;
}

struct gws_window_d *get_first_window(void)
{
    return (struct gws_window_d *) first_window;
}

void set_last_window( struct gws_window_d *window )
{
    if( (void*) window == NULL ){
         return;
    }
    if (window->magic!=1234){ return; }

    wm_add_window_into_the_list(window);
}

struct gws_window_d *get_last_window(void)
{
    return (struct gws_window_d *) last_window;
}

void activate_first_window(void)
{
// Structure validation
    if( (void*) first_window == NULL )
        return;
    if (first_window->used != TRUE)
        return;
    if (first_window->magic != 1234)
        return;
// Type validation
    if (first_window->type != WT_OVERLAPPED)
        return;
// Set.
    set_active_window(first_window);
}

void activate_last_window(void)
{
// Structure validation
    if( (void*) last_window == NULL )
        return;
    if (last_window->used != TRUE) { return; }
    if (last_window->magic != 1234){ return; }
// Type validation
    if (last_window->type != WT_OVERLAPPED)
        return;
// Set
    set_active_window(last_window);
}

// The list starts with first_window.
void wm_add_window_into_the_list( struct gws_window_d *window )
{
    struct gws_window_d  *Next;

// ========================
    //if( window == __root_window )
        //return;
// ========================

// Structure validation
    if( (void*) window == NULL )
        return;
    if (window->used != TRUE)
        return;
    if (window->magic != 1234)
        return;
// Type validation
    if (window->type != WT_OVERLAPPED){
        return;
    }

// =====================================
// Se não existe uma 'primeira da fila'.
// Então somos a primeira e a última.
    if( (void*) first_window == NULL )
    {
        first_window = window;
        last_window  = window;
        goto done;
    }

// Invalid first window.
    if ( first_window->magic != 1234 ){
        first_window = window;
        last_window  = window;
        goto done;
    }

// ===================================
// Se exite uma 'primeira da fila'.
    Next = first_window;
    while ( (void*) Next->next != NULL ){
        Next = Next->next;
    };

// Agora somos a última da fila.
    Next->next  = (struct gws_window_d *) window;

done:
    last_window  = (struct gws_window_d *) window;
    window->next = NULL;
    set_active_window(window);
}


// not tested yet
void wm_remove_window_from_list_and_kill( struct gws_window_d *window)
{
    struct gws_window_d *w;
    struct gws_window_d *pick_this_one;

    if( (void*) window == NULL )
        return;

    w = (struct gws_window_d *) first_window;

    if( (void*) w == NULL )
        return;

    while(1)
    {
        if( (void*) w == NULL )
            break;

        if(w==window)
        {
            // remove
            pick_this_one = (struct gws_window_d *) w;
            
            // Glue the list.
            w = w->next;
            
            // Kill
            pick_this_one->used = FALSE;
            pick_this_one->magic = 0;
            pick_this_one = NULL;
            break;
        }
        w = w->next;
    };
}


// ====================

/*
DEC	HEX	CHARACTER
0	0	NULL
1	1	START OF HEADING (SOH)
2	2	START OF TEXT (STX)
3	3	END OF TEXT (ETX)
4	4	END OF TRANSMISSION (EOT)
5	5	END OF QUERY (ENQ)
6	6	ACKNOWLEDGE (ACK)
7	7	BEEP (BEL)
8	8	BACKSPACE (BS)
9	9	HORIZONTAL TAB (HT)
10	A	LINE FEED (LF)
11	B	VERTICAL TAB (VT)
12	C	FF (FORM FEED)
13	D	CR (CARRIAGE RETURN)
14	E	SO (SHIFT OUT)
15	F	SI (SHIFT IN)
16	10	DATA LINK ESCAPE (DLE)
17	11	DEVICE CONTROL 1 (DC1)
18	12	DEVICE CONTROL 2 (DC2)
19	13	DEVICE CONTROL 3 (DC3)
20	14	DEVICE CONTROL 4 (DC4)
21	15	NEGATIVE ACKNOWLEDGEMENT (NAK)
22	16	SYNCHRONIZE (SYN)
23	17	END OF TRANSMISSION BLOCK (ETB)
24	18	CANCEL (CAN)
25	19	END OF MEDIUM (EM)
26	1A	SUBSTITUTE (SUB)
27	1B	ESCAPE (ESC)
28	1C	FILE SEPARATOR (FS) RIGHT ARROW
29	1D	GROUP SEPARATOR (GS) LEFT ARROW
30	1E	RECORD SEPARATOR (RS) UP ARROW
31	1F	UNIT SEPARATOR (US) DOWN ARROW
*/


void 
wm_draw_char_into_the_window(
    struct gws_window_d *window, 
    int ch,
    unsigned int color )
{
// draw char support.
    unsigned char _string[4];
// Vamos checar se e' um controle ou outro tipo de char.
    unsigned char ascii = (unsigned char) ch;
    int is_control=FALSE;

// Invalid window
    if ((void*)window == NULL)
        return;
    if (window->magic != 1234)
        return;
// Invalid char
    if (ch<0){
        return;
    }

    //#debug
    //if(ascii == 'M'){
    //    printf("M: %d\n",ascii);
    //}

/*
// #bugbug
// Com essa rotina ficamos impedidos de imprimirmos
// algumas letras maiúsculas, pois elas possuem o mesmo
// scancode que esses arrows.
// UP, LEFT, RIGHT, DOWN
// #todo
// Update input pointer for this window.
    if( ch==0x48 || 
        ch==0x4B || 
        ch==0x4D || 
        ch==0x50 )
    {
        // #todo: 
        // Update input pointers for this window.
        // right
        if(ch==0x4D){ window->ip_x++; }
        // down
        if(ch==0x50){ window->ip_y++; }
        return;
    }
*/

// Backspace
// (control=0x0E)
// #todo: 
// Isso tem que voltar apagando.
    if (ch==VK_BACK)
    {
        window->ip_x--;
        if (window->ip_x < 0){
            window->ip_x = 0;
        }
        return;
    }

// TAB
// (control=0x0F)
// O ALT esta pressionado?
    if (ch == VK_TAB)
    {
        window->ip_x += 8;
        //#todo limits
        if (window->ip_x >= window->width_in_chars)
        {
            window->ip_x = 0;
            if (window->type == WT_EDITBOX_MULTIPLE_LINES)
            {
                window->ip_y++;
                //#todo
                //if(window->ip_y >= window->height_in_bytes)
            }
        }
        return;
    }

// [Enter]
    if (ch == VK_RETURN)
    {
        window->ip_x = 0;
        window->ip_y++;
        return;
    }


// Not printable.
// 32~127
// A=41h | a=61H
// Control character or non-printing character (NPC).
// see:
// https://en.wikipedia.org/wiki/Control_character
// https://en.wikipedia.org/wiki/ASCII#Printable_characters

    // Not printable
    if (ascii < 0x20 || ascii >= 0x7F )
    {
        // Control char
        if(ascii < 0x20 || ascii == 0x7F ){
            is_control = TRUE;
        }
        return;
    }

// string
   _string[0] = (unsigned char) ch;
   _string[1] = 0;

// types
    if( window->type == WT_OVERLAPPED ){ return; }
    if( window->type == WT_SCROLLBAR ) { return; }
    if( window->type == WT_STATUSBAR ) { return; }
    if( window->type == WT_CHECKBOX )  { return; }
    if( window->type == WT_BUTTON )    { return; }
    // ...

// #todo
// Isso pode receber char se tiver em modo de edição.
// Para editarmos a label.
// #todo: edit label if in edition mode.
// #todo: open application if its a desktop icon.
    if (window->type == WT_ICON){
        return;
    }

// Editbox
// Printable chars.
// Print the char into an window 
// of type Editbox.
// Ascci printable chars: (0x20~0x7F)
// Terry's font has more printable chars.

    if ( window->type == WT_EDITBOX ||
         window->type == WT_EDITBOX_MULTIPLE_LINES )
    {
        // #todo
        // Devemos enfileirar os chars dentro de um buffer
        // indicado na estrutura de janela.
        // Depois podemos manipular o texto, inclusive,
        // entregarmos ele para o aplicativo. 
        
        // Draw char
        // #bugbug: Maybe we need to use draw_char??();
        // see: dtext.c
        //if(ascii=='M'){printf("M: calling dtextDrawText\n");}
        dtextDrawText ( 
            (struct gws_window_d *) window,
            (window->ip_x*8), 
            (window->ip_y*8), 
            (unsigned int) color, 
            (unsigned char *) _string );  //&_string[0] );

        // Refresh rectangle
        // x,y,w,h
        gws_refresh_rectangle ( 
            (window->left + (window->ip_x*8)), 
            (window->top  + (window->ip_y*8)), 
            8, 
            8 );

        // Increment pointer.
        // Se for maior que a quantidade
        // de bytes (chars?) na janela.
        window->ip_x++;
        if(window->ip_x >= window->width_in_chars)
        {
            window->ip_x=0;
            if (window->type == WT_EDITBOX_MULTIPLE_LINES)
            {    
                window->ip_y++;
                
                //if( window->ip_y > window->height_in_chars)
                //     fail!
            }
        }
    }
}

/*
//local worker
void __switch_window(void)
{
}
*/

// #todo: explain it better.
void __switch_focus(void)
{
    return;

/*
    struct gws_window_d *w;

// something is wrong
    if (window_with_focus < 0 ||
        window_with_focus >= WINDOW_COUNT_MAX)
    {
        if( (void*)first_window == NULL )
            return;
        if(first_window->magic != 1234)
            return;
        window_with_focus = (int) first_window->id;
        return;
    }

    w = (struct gws_window_d *) windowList[window_with_focus];
    if ( (void*) w == NULL )
        return;
    if (w->magic != 1234)
        return;

     w = (void*) w->next;
    if ( (void*) w == NULL )
        return;
    if (w->magic != 1234)
        return;

    window_with_focus = (int) w->id;

    //w->border_color1 = COLOR_INACTIVEBORDER;
    //w->border_color2 = COLOR_INACTIVEBORDER;
    w->border_size = 2;

//activate window
    if( w->type == WT_OVERLAPPED )
    {
        active_window = (void*) w;
    }

    w->focus = TRUE;
    redraw_window(w,FALSE);

// Pede para o kernel mudar a foreground thread.
// janela que recebera input.

    int tid = w->client_tid;
    if (tid < 0)
        return;
    sc82 (
        10011,
        tid, tid, tid);

    invalidate_window(w);
*/
}


// Post message:
// Colocaremos uma mensagem na fila de mensagens
// da thread associada com a janela indicada via argumento.
// Coloca em tail.

int
wmPostMessage(
    struct gws_window_d *window,
    int msg,
    unsigned long long1,
    unsigned long long2 )
{
    unsigned long message_buffer[8];

// Structure validation
    if ( (void*) window == NULL ){
        return -1;
    }
    if (window->used != TRUE) { return -1; }
    if (window->magic != 1234){ return -1; }

    if (msg<0){
        return -1;
    }

// standard
// wid, msg code, data1, data2
    message_buffer[0] = (unsigned long) (window->id & 0xFFFF);
    message_buffer[1] = (unsigned long) (msg        & 0xFFFF);
    message_buffer[2] = (unsigned long) long1;
    message_buffer[3] = (unsigned long) long2;
// extra
    //message_buffer[4] = 0;
    //message_buffer[5] = 0;

// Invalid client tid.
    if (window->client_tid < 0){
        return -1;
    }
// receiver
    message_buffer[4] = 
        (unsigned long) (window->client_tid & 0xFFFF);
// sender
    message_buffer[5] = 0; //?

    int ClientTID = 
        (int) window->client_tid;
    
    unsigned long MessageBuffer = 
        (unsigned long) &message_buffer[0];

//
// Post
//

// Add the message into the queue. In tail.
// IN: tid, message buffer address
    rtl_post_system_message( 
        (int) ClientTID, (unsigned long) MessageBuffer );

// Pede para o kernel mudar a foreground thread.
// Seleciona o próximo 'input reponder'.
// Assim o kernel colocará as próximas mensagens
// na fila dessa thread.
    sc82 ( 10011, ClientTID, ClientTID, ClientTID );

    return 0;
}

// local
// Se o mouse esta passando sobre os botoes
// da barra de tarefas.
void __probe_tb_button_hover(long long1, long long2)
{
// Probe taskbar buttons.
// Well. Not valid in fullscreen.

    int Status=0;
    register int i=0;
    int max=4; // We have 4 buttons in the taskbar.
    struct gws_window_d *w;  // The window for a button.

    if(WindowManager.initialized!=TRUE){
        return;
    }
    if(WindowManager.is_fullscreen==TRUE){
        return;
    }

// No more hover window.
    mouse_hover=NULL;

    for (i=0; i<max; i++){

    // Get a pointer for a window.
    w = (struct gws_window_d *) tb_windows[i];
    // If this is a valid pointer.
    if ( (void*) w != NULL )
    {
        // Paranoia
        if (w->magic == 1234)
        {
            // Is the pointer inside this window?
            Status = is_within(
                         (struct gws_window_d *) w, long1, long2 );
            // Yes it is!
            if (Status==TRUE)
            {
                // Register the hover window.
                mouse_hover = (void*) w;
                // #debug
                // yellow_status("oops");
                //rtl_reboot();
                // ok, done.
                return;
            }
        }
    }
    };
}


/*
// Se o mouse esta passando sobre a janela ativa.
// que e' overlapped.
void __probe_activewindow_hover(long long1, long long2)
{
    struct gws_window_d *aw;

    int aw_wid = get_active_window();
    if( aw_wid < 0 || aw_wid >= WINDOW_COUNT_MAX )
        return;
    aw = (struct gws_window_d *) windowList[aw_wid];
    if( (void*) aw == NULL )
        return;
    if( aw->magic != 1234 )
        return;

    
    int Status = -1;

    // global
    mouse_hover=NULL; 

    Status = is_within(
                 (struct gws_window_d *) aw,
                 long1, 
                 long2 );

    if(Status==TRUE)
    {
        //yellow_status("oops");
        //rtl_reboot();

        // Register the hover window.
        mouse_hover = aw; 
    }
}
*/


// Talvez precisaremos de mais parametros.
// The keyboard input messages will affect
// the window with focus.
// For the mouse input, the window has a NULL pointer.

static unsigned long 
wmProcedure(
    struct gws_window_d *window,
    int msg,
    unsigned long long1,
    unsigned long long2 )
{
    int Status=FALSE;
    unsigned long r=0;
    char name_buffer[64];
// Active window id.
    int aw_wid = -1;

    // #debug
    //printf("wmProcedure: w=? m=%d l1=%d l2=%d\n", 
        //msg, long1, long2 );

// -------------
// Redirect if the menu is active.
    int menu_res=0;
    if (is_menu_active == TRUE)
    {
        // see: menu.c
        menu_res = 
            (int) menuProcedure(
                      NULL,
                      (int) msg,
                      (unsigned long) long1,
                      (unsigned long) long2 );

        return (int) menu_res;
    }
// -------------


// See:
// globals.h

    switch(msg){

    case GWS_Create:
        printf("wmProcedure: [1] GWS_Create\n");
        break;
    case GWS_Destroy:
        printf("wmProcedure: [2] GWS_Destroy\n");
        break;
    case GWS_Move:
        printf("wmProcedure: [3] GWS_Move\n");
        break;
    case GWS_Size: //get size?
        printf("wmProcedure: [4] GWS_Size\n");
        break;
    case GWS_Resize: //set size ?
        printf("wmProcedure: [5] GWS_Resize\n");
        break;
    // ...
    case GWS_Close:
        printf("wmProcedure: [7] GWS_Close\n");
        if (long1==0){
            printf("Closing root window.\n");
            //exit(0);
        }
        break;
    case GWS_Paint:
        printf("wmProcedure: [8] GWS_Paint\n");
        break;
    case GWS_SetFocus: // set focus
        printf("wmProcedure: [9] GWS_SetFocus\n");
        break;
    case GWS_KillFocus: //kill focus
        printf("wmProcedure: [10] GWS_KillFocus\n");
        break;
    case GWS_Activate:
        printf("wmProcedure: [11] GWS_Activate\n");
        break;
    case GWS_ShowWindow:
        printf("wmProcedure: [12] GWS_ShowWindow\n");
        break;
    case GWS_SetCursor:
        printf("wmProcedure: [13] GWS_SetCursor\n");
        break;
    case GWS_Hide:
        printf("wmProcedure: [14] GWS_Hide\n");
        break;
    case GWS_Maximize:
        printf("wmProcedure: [15] GWS_Maximize\n");
        break;
    case GWS_Restore:
        printf("wmProcedure: [16] GWS_Restore\n");
        break;
    case GWS_ShowDefault:
        printf("wmProcedure: [17] GWS_ShowDefault\n");
        break;
    case GWS_SetFocus2:
        printf("wmProcedure: [18] GWS_SetFocus2\n");
        break;
    case GWS_GetFocus2:
        printf("wmProcedure: [19] GWS_GetFocus2\n");
        break;

// #todo
// Esse eh o momento de exibirmos o cursor do mouse.
// Precisamos fazer refresh para apagar o cursor antigo
// depois pintarmos o cursor novo direto no lfb.
// Mas nao temos aqui a rotina de pintarmos direto no
// lfb.
// #todo: Trazer as rotinas de exibiçao de cursor
// para ca, depois deixar de usar
// as rotinas de pintura de cursor que estao no kernel.

    case GWS_MouseMove:

        //printf("MOVE\n");

        // The compositor is doing this job
        // and of it's routine. See: comp.c
        comp_set_mouse_position(long1,long2);
        
        //------
        //#dangerdanger
        //#todo: show the backbuffer
        //gws_refresh_rectangle(old_x,old_y,8,8);
        //old_x = long1;
        //old_y = long2;
        //#todo: print directly into the lfb.
        //frontbuffer_draw_rectangle( 
        //    long1, long2, 8, 8, COLOR_YELLOW, 0 );
        //------        

        // Esta passando sobre a janela ativa.
        //__probe_activewindow_hover(long1,long2);

        // Em qual botão o mouse esta passando por cima.
        // lembrando: o botao esta dentro de outra janela.
        // Probe taskbar buttons.
        __probe_tb_button_hover(long1,long2);


        //========
        //o ponteiro do mouse esta dentro do main menu?
        if ( (void*)__root_window != NULL )
        {
            if( (void*)__root_window->contextmenu != NULL )
            {
                if( __root_window->contextmenu->in_use == TRUE )
                {
                    // Call the main menu dialog.
                    // see: menu.c
                    menuProcedure(
                        NULL,
                        (int) msg,
                        (unsigned long) long1,
                        (unsigned long) long2 );

                    return 0;
                }
            }
        }
        //========
        
        return 0;
        break;

    // # Esse é o momento em que estabelecemos qual é a 
    // janela que possui o mouse.
    // De acordo com a posição do ponteiro na tela.
    // A partir dai os eventos devem ir para essa janela.
    case GWS_MousePressed:
    
        //#debug
        //printf("PRESSED\n");

        // safety first.
        // mouse_hover validation
        if ( (void*) mouse_hover == NULL )
            return -1;
        if (mouse_hover->magic!=1234)
            return -1;
    
        // button number
        //if(long1==1){ yellow_status("P1"); }
        //if(long1==2){ yellow_status("P2"); }
        //if(long1==3){ yellow_status("P3"); }

        // Em qual botão da taskbar?
        // Se for igual à um dos botões da tb.
        if( mouse_hover->id == tb_buttons[0] ||
            mouse_hover->id == tb_buttons[1] ||
            mouse_hover->id == tb_buttons[2] ||
            mouse_hover->id == tb_buttons[3] )
        {
            if (mouse_hover->id == tb_buttons[0])
            {
                if(tb_buttons_status[0] == TRUE)
                return 0;
            }
            if (mouse_hover->id == tb_buttons[1])
            {
                if(tb_buttons_status[1] == TRUE)
                return 0;
            }
            if (mouse_hover->id == tb_buttons[2])
            {
                if(tb_buttons_status[2] == TRUE)
                return 0;
            }
            if (mouse_hover->id == tb_buttons[3])
            {
                if(tb_buttons_status[3] == TRUE)
                return 0;
            }

            set_status_by_id(mouse_hover->id,BS_PRESSED);
            redraw_window_by_id(mouse_hover->id,TRUE);
            return 0;
        }

        // #todo #maybe
        // Clicamos e o ponteiro esta sobre a janela ativa.
        //aw_wid = (int) get_active_window();
        //if( mousehover_window == aw_wid )
        //{
            //wm_update_window_by_id(aw_wid);
            //redraw_window_by_id(mousehover_window,TRUE);
            //return 0;
        //}
        
        return 0;
        break;

    // 36
    case GWS_MouseReleased:

        //#debug
        //printf("RELEASED\n");

        //wm_Update_TaskBar("RELEASED",TRUE);
        
        // button number
        //if(long1==1){ yellow_status("R1"); }
        //if(long1==2){ yellow_status("R2"); }
        //if(long1==3){ yellow_status("R3"); }

        // Post it to the client. (app).
        // When the mouse is on any position in the screen.
        on_mouse_event( 
            GWS_MouseReleased,              // event type
            comp_get_mouse_x_position(),    // current cursor x
            comp_get_mouse_y_position() );  // current cursor y

        // When the mouse is hover a tb button.
        
        // safety first.
        // mouse_hover validation
        if ( (void*) mouse_hover == NULL )
            return -1;
        if (mouse_hover->magic!=1234)
            return -1;

        //if(long1==1){ yellow_status("R1"); }
        //if(long1==2){ yellow_status("R2"); wm_update_desktop(TRUE); return 0; }
        //if(long1==1){ 
            //yellow_status("R1"); 
            //create_main_menu(8,8);
            //return 0; 
        //}
        //if(long1==3){ yellow_status("R3"); return 0; }
        //if(long1==2){ create_main_menu(mousex,mousey); return 0; }
        //if(long1==2){ create_main_menu(mousex,mousey); return 0; }

        //tb_button[0]
        if(mouse_hover->id == tb_buttons[0])
        {
            // ja esta rodando.
            if(tb_buttons_status[0] == TRUE)
                return 0;
            set_status_by_id(mouse_hover->id,BS_RELEASED);
            redraw_window_by_id(mouse_hover->id,TRUE);
            //create_main_menu(8,8);
            //wm_update_active_window();
            //current_option = OPTION_MINIMIZE;
            //yellow_status("0: Min");
            //tb_pids[0] = (int) rtl_clone_and_execute("terminal.bin");
            tb_buttons_status[0] = TRUE;
            return 0;
        }

        //tb_button[1]
        if(mouse_hover->id == tb_buttons[1])
        {
            // ja esta rodando.
            if(tb_buttons_status[1] == TRUE)
                return 0;
            set_status_by_id(mouse_hover->id,BS_RELEASED);
            redraw_window_by_id(mouse_hover->id,TRUE);
            //current_option = OPTION_MAXIMIZE;
            //yellow_status("1: Max");
            //tb_pids[1] = (int) rtl_clone_and_execute("editor.bin");
            tb_buttons_status[1] = TRUE;
            return 0;
        }

        //tb_button[2]
        if(mouse_hover->id == tb_buttons[2])
        {
            // ja esta rodando.
            if(tb_buttons_status[2] == TRUE)
                return 0;
            set_status_by_id(mouse_hover->id,BS_RELEASED);
            redraw_window_by_id(mouse_hover->id,TRUE);
            //current_option = OPTION_CLOSE;
            //yellow_status("2: Close");
            //tb_pids[2] = (int) rtl_clone_and_execute("fileman.bin");
            tb_buttons_status[2] = TRUE;
            return 0;
        }

        //tb_button[3]
        if(mouse_hover->id == tb_buttons[3])
        {
            // ja esta rodando.
            if(tb_buttons_status[3] == TRUE)
                return 0;
            set_status_by_id(mouse_hover->id,BS_RELEASED);
            redraw_window_by_id(mouse_hover->id,TRUE);
            //yellow_status("3: OK");
            //run_selected_option();
            //tb_pids[3] = (int) rtl_clone_and_execute("browser.bin");
            tb_buttons_status[3] = TRUE;
            // mostra o cliente se ele faz parte da tag 3.
            //show_client(first_client->next,3);
            //show_client_list(3);  //#todo: notworking
            return 0;
        }

        return 0;
        break;


    // #bugbug
    // Quando imprimir na tela e quando enviar para o cliente?
    // Uma flag deve indicar se o sistema deve ou nao imprimir 
    // previamente o char.
    // Caixas de edição podem deixar todo o trabalho
    // de teclas de digitação para o sistema, liberando o aplicativo
    // desse tipo de tarefa. Mas editores de texto querem 
    // processar cada tecla digitada.

    case GWS_KeyDown:
        // Imprime o char na janela indicada.
        // Essa é a janela com foco de entrada.
        //if( pre_print === TRUE)
        //wm_draw_char_into_the_window(window,(int)long1,COLOR_RED);
        wm_draw_char_into_the_window(
            window,
            (int) long1,
            xCOLOR_GRAY1 );
        // Enfileirar a mensagem na fila de mensagens
        // da janela com foco de entrada.
        // O cliente vai querer ler isso.
        // Na verdade estamos colocando na janela indicada
        // via argumento ... penso que o caller selecionou
        // a janela com foco.
        wmPostMessage(
            (struct gws_window_d *) window,
            (int) msg,
            (unsigned long) long1,
            (unsigned long) long2);

        return 0;
        break;
    
    //case GWS_KeyUp:
        //break;

    case GWS_SysKeyDown:
        if(long1 == VK_F1){
            // ja esta rodando.
            if(tb_buttons_status[0] == TRUE)
                return 0;
            set_status_by_id(tb_buttons[0],BS_PRESSED);
            redraw_window_by_id(tb_buttons[0],TRUE);
            return 0;
        }
        if(long1 == VK_F2){
            // ja esta rodando.
            if(tb_buttons_status[1] == TRUE)
                return 0;
            set_status_by_id(tb_buttons[1],BS_PRESSED);
            redraw_window_by_id(tb_buttons[1],TRUE);
            return 0;
        }
        if(long1 == VK_F3){
            // ja esta rodando.
            if(tb_buttons_status[2] == TRUE)
                return 0;
            set_status_by_id(tb_buttons[2],BS_PRESSED);
            redraw_window_by_id(tb_buttons[2],TRUE);
            return 0;
        }
        if(long1 == VK_F4){
            // ja esta rodando.
            if(tb_buttons_status[3] == TRUE)
                return 0;
            set_status_by_id(tb_buttons[3],BS_PRESSED);
            redraw_window_by_id(tb_buttons[3],TRUE);
            return 0;
        }
        
        if (long1 == VK_F9 || 
            long1 == VK_F10 || 
            long1 == VK_F11)
        {
            control_action(msg,long1);
            return 0;
        }

        //printf("wmProcedure: [?] GWS_SysKeyDown\n");
        // Enfileirar a mensagem na fila de mensagens
        // da janela com foco de entrada.
        // O cliente vai querer ler isso.
        wmPostMessage(
            (struct gws_window_d *) window,
            (int)msg,
            (unsigned long)long1,
            (unsigned long)long2);
        //wm_update_desktop(TRUE); // 
        return 0;
        break;

    case GWS_SysKeyUp:
        if(long1 == VK_F1){
            // ja esta rodando.
            if(tb_buttons_status[0] == TRUE)
                return 0;
            set_status_by_id( tb_buttons[0], BS_RELEASED );
            redraw_window_by_id(tb_buttons[0],TRUE);
            memset(name_buffer,0,64-1);
            strcpy(name_buffer,app1_string);
            tb_pids[0] = (int) rtl_clone_and_execute(name_buffer);
            tb_buttons_status[0] = TRUE;
            return 0;
        }
        if(long1 == VK_F2){
            // ja esta rodando.
            if(tb_buttons_status[1] == TRUE)
                return 0;
            set_status_by_id( tb_buttons[1], BS_RELEASED );
            redraw_window_by_id(tb_buttons[1],TRUE);
            //tb_pids[1] = (int) rtl_clone_and_execute("fileman.bin");
            memset(name_buffer,0,64-1);
            strcpy(name_buffer,app2_string);
            tb_pids[1] = (int) rtl_clone_and_execute(name_buffer);
            tb_buttons_status[1] = TRUE;
            return 0;
        }
        if(long1 == VK_F3){
            // ja esta rodando.
            if(tb_buttons_status[2] == TRUE)
                return 0;
            set_status_by_id( tb_buttons[2], BS_RELEASED );
            redraw_window_by_id(tb_buttons[2],TRUE);
            memset(name_buffer,0,64-1);
            strcpy(name_buffer,app3_string);
            tb_pids[2] = (int) rtl_clone_and_execute(name_buffer);
            tb_buttons_status[2] = TRUE;
            return 0;
        }
        if(long1 == VK_F4){
            // ja esta rodando.
            if(tb_buttons_status[3] == TRUE)
                return 0;
            set_status_by_id( tb_buttons[3], BS_RELEASED );
            redraw_window_by_id(tb_buttons[3],TRUE);
            memset(name_buffer,0,64-1);
            strcpy(name_buffer,app4_string);
            tb_pids[3] = (int) rtl_clone_and_execute(name_buffer);
            tb_buttons_status[3] = TRUE;
            // #test: ps2 full initialization.
            // sc80(350,1,1,1);
            // gUseMouse = TRUE;
            return 0;
        }

        // update desktop
        if (long1 == VK_F5)
        {
            set_status_by_id(tb_buttons[0],BS_RELEASED);
            set_status_by_id(tb_buttons[1],BS_RELEASED);
            set_status_by_id(tb_buttons[2],BS_RELEASED);
            set_status_by_id(tb_buttons[3],BS_RELEASED);
            WindowManager.is_fullscreen = FALSE;
            wm_update_desktop(TRUE);
            return 0;
        }

        //=====
        if(long1==VK_F6){
            // Enter fullscreen mode.
            if (WindowManager.is_fullscreen != TRUE){
                wm_enter_fullscreen_mode();
                return 0;
            }
            // Exit fullscreen mode.
            if (WindowManager.is_fullscreen == TRUE){
                wm_exit_fullscreen_mode(TRUE);
                return 0;
            }
            return 0;
        }
        
        if (long1 == VK_F9 || 
            long1 == VK_F10 || 
            long1 == VK_F11)
        {
            control_action(msg,long1);
            return 0;
        }
        
        return 0;
        break;

    // 9090 - ( Shift + F12 )
    case GWS_SwitchFocus:
        //printf("Switch "); fflush(stdout);
        __switch_focus();
        //printf("wmProcedure: [?] GWS_SwitchFocus\n");
        //next = window->next;
        //window->focus = TRUE;
        //redraw_window(window,1);
        return 0;
        break;

    //case MSG_REPAINT:
        //if (window == __root_window){
        //    wm_update_desktop(TRUE);
        //}
        //break;

    default:
        return 0;
        break;
    };

    return 0;
}


// Entry point
// Called by the kernel.
// Order: rdi, rsi, rdx, rcx, r8, r9.
// IN: wid, msg_code, long1, long2
// #todo: 
// Para algumas chamadas o kernel pode passar 
// o ponteiro para um bloco de informações.
unsigned long 
wmHandler(
    unsigned long arg1_rdi,
    unsigned long arg2_rsi,
    unsigned long arg3_rdx,
    unsigned long arg4_rcx )
{
    unsigned long r=0;

// Final message
    struct gws_window_d *w;
    int msg=0;
    unsigned long long1=0;
    unsigned long long2=0;

// #debug
//    debug_print ("wmHandler:-----------------------------\n");

   if( g_handler_flag != TRUE ){
       return 0;
   }

// #debug
    //printf("wmHandler: %x %x %x %x\n", 
        //arg1_rdi, arg2_rsi, arg3_rdx, arg4_rcx );

// wid
// Ignoring this parameter
// We ware called by the kernel, and the kernel has no information
// about the windows. So, the messages sent by the kernel are able
// to act on the active window, on the window with focus and
// on the server itself.

    //int wid=-1;
    //wid = (int) (arg1_rdi & 0xFFFF);

// Message
    msg = (int) (arg2_rsi & 0xFFFF);


// #special
// Refresh rectangles and exit.
// GWS_RefreshDirtyRectangles

// #todo:
// This way the kernel is able to call the
// compositor at a given timer.
// Called by the kernel in pit.c

    if ( msg == 9091 )
    {
        // debug_print ("wmHandler: 9091\n");
        // IN: jiffies, clocks per second
        wmCompose( 
            (unsigned long) arg3_rdx,
            (unsigned long) arg4_rcx );
        
        return 0;
    }

// #test
// Redraw all the windows. Back to front.
// GWS_UpdateDesktop

    if ( msg == 9092 )
    {
        //debug_print ("wmHandler: 9092\n");
        wm_update_desktop(TRUE);
        return 0;  //important: We need to return.
    }


// ==============================================
// #test
// Testing some random functions.

    if (msg == 9093)
    {
        debug_print ("wmHandler: 9093\n");

        // #test
        // drawing a rect using kernel and ws routines.
        // TRUE = use kgws ; FALSE =  do not use kgws.

        rectBackbufferDrawRectangle0(
            10, 10, 40, 40,
            COLOR_RED,
            TRUE,      // fill? 
            0,         // rop falgs
            FALSE );   // TRUE = use kgws. (kernel service)
        //refresh_rectangle_via_kgws(10, 10, 40, 40);

        return 0;
    }
// ==============================================

// Data

    long1 = (unsigned long) arg3_rdx;
    long2 = (unsigned long) arg4_rcx;

//
// Calling wmProcedure()
//

    switch (msg){

    case GWS_MouseMove:
    case GWS_MousePressed:
    case GWS_MouseReleased:
        //#debug
        //if( msg == GWS_MousePressed ){ printf("%d\n",long1); }
        r = (unsigned long) wmProcedure(
                NULL,  //(struct gws_window_d *) 0,
                (int) msg,
                (unsigned long) long1,
                (unsigned long) long2 ); 
        return (unsigned long) r;
        break;

// #important:
// Mandaremos input de teclado somente para 
// a janela com foco de entrada,
// seja ela de qualquer tipo.
// get_focus() will give us a valid pointer or null.
// mensagens desse tipo
// devem ir para a janela com o foco de entrada.
// #todo #test
// Se não temos uma janela com foco de entrada,
// então devemos mandar a mensagem para a janela ativa?

    case GWS_KeyDown:
    case GWS_SysKeyDown:
    case GWS_SysKeyUp:
    case GWS_SwitchFocus:
        return (unsigned long) on_keyboard_event(
            (int) msg, 
            (unsigned long) long1, 
            (unsigned long) long2 );
        break;

    //case 9091:
        //wmCompositor();
        //return 0;  //important
        //break;

    // Mensagens que atuam sobre o window server.
    default:
        //printf("wmHandler: default message\n");
        return 0;
        break;
    };

done:
    // #debug
    // debug_print ("wmHandler: done\n");
    return (unsigned long) r;
}


unsigned long wmGetLastInputJiffie(int update)
{
    if(update==TRUE){
        last_input_jiffie = (unsigned long) rtl_jiffies();
    }
    return (unsigned long) last_input_jiffie;
}


// read n bytes from stdin
int wmSTDINInputReader(void)
{
    size_t nreads=0;
    char buffer[512];
    register int i=0;

    nreads = (size_t) read(0,buffer,512);
    if (nreads<=0){
        return -1;
    }

    //printf("%d bytes\n",nreads);

    i=0;
    for (i=0; i<nreads; i++)
    {
            g_handler_flag = TRUE;
            wmHandler( 
                0,             // window pointer
                GWS_KeyDown,   // msg code
                buffer[i],    // long1
                buffer[i] );  // long2
            g_handler_flag = FALSE;
    };

    return (int) nreads;
}


// ------------------------------------------------
// This is basically the low level input support 
// for the Gramado OS when running the Gramado Window System.
// This routine do not pump the messages from a file, just
// like the traditional way. It just get messages in a queue
// in the control thread of the window server process.
// The kernel post all the input messages into this queue for us.
// See: dev/tty in the kernel source code.
// ------------------------------------------------
// Read input from thread's queue.
// Esse nao eh o melhor metodo.
// #todo: precisamos ler de um arquivo que contenha
// um array de estruturas de eventos.
// #todo: Essas rotinas de input de dispositivo
// precisam ficar em bibliotecas. Mas de uma
// biblioteca pode existir no servidor, uma
// pra cada tipo de sistema.
// vamos tentar 32 vezes,
// pois nossa lista tem 32 ou 64 slots.
// Se encontrarmos um evento, entao pegamos ele.
// #bugbug: Isso eh um problema,
// pois quando nao tiver mensagens na fila,
// teremos que executar esse loop.
// #todo
// A mensagem de tecla pressionada
// deve vir com a informação de quanto
// tempo ela permaneceu pressionada.
// processamos ate 32 input válidos.
// isso deve ajudar quando movimentarmos o mouse.
// #importante:
// Se o servidor for portado para outro sistema
// então precisamos converter o formato do eventro
// entregue pelo sistema operacional, em um formato
// que o servidor consegue entender.
// Por enquanto o servidor entende o formato de eventos
// entregues pelo gramado.
// Called by main.c

int wmInputReader(void)
{
// Process all the messages in the queue, 
// starting at the first message.
// Disrespecting the circular input.
    int __Status = -1;
    register int i=0;
// see: event.h
    struct gws_event_d e;

    for (i=0; i<=31; i++)
    {
        // Não volte ao inicio da fila
        if(i<31) { __Status = rtl_get_event2(i,FALSE); }
        // Volte ao inicio da fila.
        if(i==31){ __Status = rtl_get_event2(i,TRUE);  }

        // #todo
        // Se a mensagem for um input de teclado,
        // então enviamos a mensagem 
        // para a janela com o foco de entrada.
        // Mensagens de outro tipo 
        // podem ir para outras janelas.
        
        if(__Status==TRUE)
        {
            // #test
            e.msg   = (int)           RTLEventBuffer[1];
            e.long1 = (unsigned long) RTLEventBuffer[2];
            e.long2 = (unsigned long) RTLEventBuffer[3];

            // MOUSE events
            // Calling procedure.
            if ( e.msg == GWS_MouseMove || 
                 e.msg == GWS_MousePressed ||
                 e.msg == GWS_MouseReleased )
            {
                // If the mouse is enabled.
                // #todo: return if gUseMouse=FALSE.
                if (gUseMouse == TRUE)
                {
                    //return (unsigned long) wmProcedure(
                    wmProcedure(
                        NULL,
                        (int) e.msg,
                        (unsigned long) e.long1,
                        (unsigned long) e.long2 ); 
                }
                //return 0;
            }

            // keyboard
            // mensagens desse tipo
            // devem ir para a janela com o foco de entrada.
            if ( e.msg == GWS_KeyDown ||
                 e.msg == GWS_SysKeyDown ||
                 e.msg == GWS_SysKeyUp )
            {
                // Print char into the keyboard window.
                on_keyboard_event( 
                    (int) e.msg, 
                    (unsigned long) e.long1, 
                    (unsigned long) e.long2 );
                //return 0;
            }

            // Arrows

            // #todo: 
            // We also can use 'keydown' and check the vk.
            // Control arrow right
            if(e.msg == GWS_ControlArrowRight)
            {
                //printf("ws: Control right\n"); 
                dock_active_window(2);
                //return 0; 
            }
            // Control arrow up
            if(e.msg == GWS_ControlArrowUp)
            {
                //printf("ws: Control up\n"); 
                dock_active_window(1);
                //return 0; 
            }
            // Control arrow down
            if(e.msg == GWS_ControlArrowDown)
            {
                //printf("ws: Control down\n");
                dock_active_window(3);
                //return 0;
            }
            //  Control arrow left
            if(e.msg == GWS_ControlArrowLeft)
            {
                //printf("ws: Control left\n");
                dock_active_window(4); 
                //return 0; 
            }


            // Gui events?

            // cut (control+x)?
            if(e.msg == GWS_Cut)
            { printf("ws: cut :)\n"); return 0; }
            // copy (ok)
            // Copy or kill?
            if(e.msg == GWS_Copy)
            { printf("ws: copy\n"); return 0; }
            // paste (ok)
            if(e.msg == GWS_Paste)
            { printf("ws: paste\n"); return 0; }
            // undo
            if(e.msg == GWS_Undo)
            { printf("ws: undo\n"); return 0; }
            // select all: control+a
            if(e.msg == GWS_SelectAll)
            { printf("ws: select all\n"); return 0; }
            // find: control+f
            if(e.msg == GWS_Find)
            { printf("ws: find\n"); return 0; }
            // Save: [control + s]
            // criar, ativar ou desativar o menu.
            // #todo: We need a new name.
            // see: menu.c
            if(e.msg == GWS_Save)
            {
                //printf("ws: save\n"); 
                on_menu();
                return 0; 
            }

            // Via alt + f4
            // qemu intercepts this combination.
            //if( RTLEventBuffer[1] == 77104 )
            //{
            //    return 0;
            //}
            
            // #tests
            // Via shift + f12
            if(e.msg == 88112)
            {
                gUseMouse = TRUE;
                wm_change_bg_color(COLOR_RED,TRUE,TRUE); //ok
                //printf ("server: [88112]\n");
                //__switch_focus();
                //wm_update_desktop(TRUE); //ok.
                return 0;
            }
        }
    };

    return 0;
}


void wm_change_bg_color(unsigned int color, int tile, int fullscreen)
{
// Change the custon background color.
    __set_custom_background_color(color);

    if ( (void*) __root_window == NULL )
        return;
    if (__root_window->magic!=1234)
        return;

// Change
    __root_window->bg_color = (unsigned int) color;

// Validate.
    if (fullscreen){
        wm_exit_fullscreen_mode(tile);
        return;
    }

// tile
    if (tile){
        wm_update_desktop(TRUE);
    }else{
        wm_update_desktop(FALSE);
    };
}

void wm_enter_fullscreen_mode(void)
{
    struct gws_window_d *w;

    if (WindowManager.initialized!=TRUE)
        return;

// Last window.
    w = (struct gws_window_d *) last_window;
    if ( (void*) w != NULL ){
        WindowManager.is_fullscreen = TRUE;
        WindowManager.fullscreen_window = 
            (struct gws_window_d *) w;
        wm_update_window_by_id(w->id);
        return;
    }

// First window.
    w = (struct gws_window_d *) first_window;
    if ( (void*) w != NULL ){
        WindowManager.is_fullscreen = TRUE;
        WindowManager.fullscreen_window = 
            (struct gws_window_d *) w;
        wm_update_window_by_id(w->id);
        return;
    }
}

void wm_exit_fullscreen_mode(int tile)
{
    if (WindowManager.initialized!=TRUE)
        return;

    WindowManager.is_fullscreen = FALSE;
    wm_update_desktop(tile);
}


// yellow bar. (rectangle not window)
// developer status.
void yellowstatus0(char *string,int refresh)
{
    //methods. get with the w.s., not with the system.
    unsigned long w = gws_get_device_width();
    unsigned long h = gws_get_device_height();

    unsigned long offset_string1 = 8;  //( 8*1 );
    unsigned long offset_string2 = ( 8*5 );
    unsigned long bar_size = w;

    struct gws_window_d *aw;

// Validation
    //aw = (struct gws_window_d *) windowList[active_window];
    aw = (void*) active_window;
    if( (void*) aw == NULL ){
        return;
    }
    if(aw->magic!=1234){
        return;
    }

    //if(aw->type!=WT_OVERLAPPED){
    //    return;
    //}

    //debug_print ("yellow_status:\n");
    
    //#todo
    //if ( (void*) string == NULL ){ return; }
    //if ( *string == 0 ){ return; }


// Desenha a barra no backbuffer

    if ( current_mode == GRAMADO_JAIL ){
        //bar_size = w;
        bar_size = (w>>1);
        rectBackbufferDrawRectangle ( 
            aw->left +2, 
            aw->top  +2, 
            bar_size, 
            24, 
            COLOR_YELLOW, 
            1,
            0 );
    }else{

        //bar_size = (offset_string2 + (4*8) );
        //bar_size = (offset_string2 + (100) );
        bar_size = (w>>1);
        rectBackbufferDrawRectangle ( 
            aw->left +2, 
            aw->top +2, 
            bar_size, 
            24, 
            COLOR_YELLOW, 
            1,
            0 );
    };

// Escreve as strings
    
    grDrawString ( 
        aw->left +2 + offset_string1, 
        aw->top  +2 + 8, COLOR_BLACK, 
        string );
    
    //grDrawString ( offset_string2, 8, COLOR_BLACK, "FPS" );
    
    // Mostra o retângulo.
     
    if (bar_size == 0){
        bar_size = 32;
    }

    if(refresh){
        gws_refresh_rectangle(
            (aw->left +2), (aw->top +2), bar_size, 24 );
    }
}


void yellow_status(char *string)
{
    if( (void*)string==NULL ){
        return;
    }
    yellowstatus0(string,TRUE);
}


int 
is_within2 ( 
    struct gws_window_d *window, 
    unsigned long x, 
    unsigned long y )
{
    struct gws_window_d *pw;
    struct gws_window_d *w;

// #bugbug
// E se a janela tem janela mae?

// window validation
    if ( (void*) window == NULL )
        return FALSE;
    if ( window->used != TRUE && window->magic != 1234 ){
        return FALSE;
    }

// ====

// pw
// The parent window.
    pw = window->parent;
    if ( (void*) pw == NULL )
        return FALSE;
    if ( pw->used != TRUE && pw->magic != 1234 ){
        return FALSE;
    }

// w
// The window itself
    w = window;
    if ( (void*) w == NULL )
        return FALSE;
    if ( w->used != TRUE && w->magic != 1234 ){
        return FALSE;
    }

//relative to the parent.
    int x1= pw->left + w->left; 
    int x2= x1 + w->width;
    int y1= pw->top  + w->top;
    int y2= y1 + w->height;

    if( x > x1 && 
        x < x2 &&
        y > y1 && 
        y < y2 )
    {
        return TRUE;
    }

    return FALSE;
}


//#todo: Explain it.
int 
is_within ( 
    struct gws_window_d *window, 
    unsigned long x, 
    unsigned long y )
{

// #bugbug
// E se a janela tem janela mae?

    if ( (void*) window != NULL )
    {
        if ( window->used == TRUE && window->magic == 1234 )
        {
            // yes!
            if ( x >= window->left   && 
                 x <= window->right  &&
                 y >= window->top    &&
                 y <= window->bottom )
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

// validate
void validate_window (struct gws_window_d *window)
{
    if ( (void*) window != NULL )
    {
        if ( window->used == TRUE && window->magic == 1234 )
        {
            window->dirty = FALSE;
        }
    }
}


void invalidate_window_by_id( int wid )
{
    struct gws_window_d *w;

// #todo: 
// Chamar o metodo de validação de janela.

// wid
    if (wid<0)
        return;
    if (wid>=WINDOW_COUNT_MAX)
        return;
// Window structure
    w = (struct gws_window_d *)windowList[wid];
    if((void*)w==NULL){
        return;
    }
    if (w->used!=TRUE) { return; }
    if (w->magic!=1234){ return; }

    invalidate_window(w);    
}

// Invalidate
void invalidate_window (struct gws_window_d *window)
{
    if ( (void*) window != NULL )
    {
        if ( window->used == TRUE && window->magic == 1234 )
        {
            window->dirty = TRUE;
        }
    }
}

void invalidate_root_window(void)
{
    invalidate_window ( (struct gws_window_d *) __root_window );
}

void invalidate_taskbar_window(void)
{
    invalidate_window ( (struct gws_window_d *) taskbar_window );
}

void __begin_paint(struct gws_window_d *window)
{
    if( (void*) window == NULL )
        return;

    validate_window(window);
}

void __end_paint(struct gws_window_d *window)
{
    if( (void*) window == NULL )
        return;

    invalidate_window(window);
}

/*
void destroy_window (struct gws_window_d *window);
void destroy_window (struct gws_window_d *window)
{
    // #todo
    // if( window == __root_window)
        // return;
    if ( (void*) window != NULL )
    {
        if ( window->used == TRUE && window->magic == 1234 )
        {
            // ...
        }
    }
}
*/

//
//===================================================================
//

// redraw_window:
// Let's redraw the window.
// Called by serviceRedrawWindow().
// #todo
// devemos repintar as janelas filhas, caso existam.
// IN: 
// window pointer, show or not.

int 
redraw_window ( 
    struct gws_window_d *window, 
    unsigned long flags )
{
    unsigned int __tmp_color = COLOR_WINDOW;

    //#debug
    //gwssrv_debug_print ("redraw_window:\n");

// Structure validation.
    if ( (void *) window == NULL ){
        goto fail;
        //return -1;
    }
    if (window->used!=TRUE || window->magic!=1234){
        goto fail;
        //return -1;
    }

// =======================
// shadowUsed
// A sombra pertence à janela e ao frame.
// A sombra é maior que a própria janela.
// ?? Se estivermos em full screen não tem sombra ??
//CurrentColorScheme->elements[??]
//@todo: 
// ?? Se tiver barra de rolagem a largura da 
// sombra deve ser maior. ?? Não ...
//if()
// @todo: Adicionar a largura das bordas verticais 
// e barra de rolagem se tiver.
// @todo: Adicionar as larguras das 
// bordas horizontais e da barra de títulos.
// Cinza escuro.  CurrentColorScheme->elements[??] 
// @TODO: criar elemento sombra no esquema. 
// ??
// E os outros tipos, não tem sombra ??
// Os outros tipos devem ter escolha para sombra ou não ??
// Flat design pode usar sombra para definir se o botão 
// foi pressionado ou não.

// shadow: Not used for now.
    if (window->shadowUsed == TRUE)
    {
        if ( (unsigned long) window->type == WT_OVERLAPPED )
        {
            if (window->focus == 1){ __tmp_color = xCOLOR_GRAY1; }
            if (window->focus == 0){ __tmp_color = xCOLOR_GRAY2; }

            // Shadow rectangle.
            rectBackbufferDrawRectangle ( 
                (window->left +1), 
                (window->top +1), 
                (window->width +1 +1), 
                (window->height +1 +1), 
                __tmp_color, 
                TRUE,     // fill?
                (unsigned long) window->rop );  // #todo: rop operations for this window.
        }
    }

// =======================
// backgroundUsed
// ## Background ##
// Background para todo o espaço ocupado pela janela e pelo seu frame.
// O posicionamento do background depende do tipo de janela.
// Um controlador ou um editbox deve ter um posicionamento relativo
// à sua janela mãe. Já uma overlapped pode ser relativo a janela 
// gui->main ou relativo à janela mãe.

// background rectangle.
    if (window->backgroundUsed == TRUE)
    {
        // redraw the background rectandle.
        rectBackbufferDrawRectangle ( 
                window->left, 
                window->top, 
                window->width, 
                window->height, 
                window->bg_color, 
                TRUE, 
                (unsigned long) window->rop );   // ROP for this window.

        // All done for WT_SIMPLE type.
        if( window->type == WT_SIMPLE ){
            goto done;
        }
    }

//
// botao ==========================================
//

// =======================
// WT_BUTTON

    //Termina de desenhar o botão, mas não é frame
    //é só o botão...
    //caso o botão tenha algum frame, será alguma borda extra.
    int Focus=0;    //(precisa de borda)
    int Selected=0;
    unsigned int border1=0;
    unsigned int border2=0;

    if ( (unsigned long) window->type == WT_BUTTON )
    {
        //if ( (void*) window->parent == NULL )
            //printf("redraw_window: [FAIL] window->parent\n");

        /*
        if ( (void*) window->parent != NULL )
        {
           wmDrawFrame ( 
            (struct gws_window_d *) window->parent,  //parent.
            (struct gws_window_d *) window,      //bg do botão em relação à sua parent. 
            METRICS_BORDER_SIZE,       //border size
            (unsigned int)COLOR_BLACK, //border color 1
            (unsigned int)COLOR_BLACK, //border color 2
            (unsigned int)COLOR_BLACK, //border color 3
            (unsigned int)COLOR_BLACK, //ornament color 1
            (unsigned int)COLOR_BLACK, //ornament color 2
            1 );  //style
        }
        */
        
        //border color
        //o conceito de status e state
        //está meio misturado. ja que estamos usando
        //a função de criar janela para criar botão.
        //#bugbug
        
        switch (window->status)
        {
            case BS_FOCUS:
                border1 = COLOR_BLUE;
                border2 = COLOR_BLUE;
                break;

            //case BS_PRESS:
            case BS_PRESSED:
                Selected = 1;
                border1 = xCOLOR_GRAY1;  //GWS_COLOR_BUTTONSHADOW3;
                border2 = COLOR_WHITE;   //GWS_COLOR_BUTTONHIGHLIGHT3;
                break;

            case BS_HOVER:
                break;
                    
            case BS_DISABLED:
                border1 = COLOR_GRAY;
                border2 = COLOR_GRAY;
                break;

            case BS_PROGRESS:
                break;

            case BS_DEFAULT:
            default: 
                Selected = 0;
                border1 = COLOR_WHITE;    //GWS_COLOR_BUTTONHIGHLIGHT3;
                border2 = xCOLOR_GRAY1;   //GWS_COLOR_BUTTONSHADOW3;
                break;
        };

        // name support.
        size_t tmp_size = (size_t) strlen ( (const char *) window->name );
        if (tmp_size>64){
            tmp_size=64;
        }
        unsigned long offset = 
        ( ( (unsigned long) window->width - ( (unsigned long) tmp_size * (unsigned long) gcharWidth) ) / 2 );

        // redraw the button border.
        // #todo:
        // as cores vao depender do etado do botao.
        // #todo: veja como foi feito na hora da criaçao do botao.
        __draw_button_borders(
            (struct gws_window_d *) window,
            (unsigned int) border1,        //buttonBorderColor1,
            (unsigned int) border2,        //buttonBorderColor2,
            (unsigned int) xCOLOR_GRAY5,   //buttonBorderColor2_light,
            (unsigned int) COLOR_BLACK );  //buttonBorder_outercolor );
             
        // Button label
        //gwssrv_debug_print ("redraw_window: [FIXME] Button label\n"); 

        if (Selected == TRUE){
            grDrawString ( 
                (window->left) +offset, 
                (window->top)  +8, 
                COLOR_WHITE, window->name );
        }

        if (Selected == FALSE){
            grDrawString ( 
                (window->left) +offset, 
                (window->top)  +8, 
                COLOR_BLACK, window->name );
        }

        // ok, repintamos o botao que eh um caso especial
        // nao precisamos das rotinas abaixo,
        // elas serao par aos outros tipos de janela.
        
        goto done;
        //return 0;  
    }

// =======================================
// redraw_frame:
// only the boards
// redraw the frame para alguns tipos menos para botao.
// O bg ja fi feito logo acima.
// Remember:
// We can't recreate the windows, just redraw'em.
// #todo
// Precisamos de uma rotina que redesenhe o frame,
// sem alocar criar objetos novos.

// Only the boards.
    if ( window->type == WT_OVERLAPPED || 
         window->type == WT_EDITBOX || 
         window->type == WT_EDITBOX_MULTIPLE_LINES )
    {
        if ( (void*) window != NULL )
        {
            if ( (void*) window->parent != NULL )
            {
                if (window->parent->magic == 1234)
                {
                    // Redraw titlebar for overlapped windows.
                    if (window->type == WT_OVERLAPPED)
                    {
                        if ( (void*) window->titlebar != NULL )
                        {
                            if (window->titlebar->magic == 1234 )
                            {
                                if (window->active == TRUE)
                                {
                                    window->titlebar->bg_color = COLOR_BLUE1;
                                    window->titlebar_color = COLOR_BLUE1;
                                    window->titlebar_ornament_color = xCOLOR_BLACK;
                                }

                                if (window->active == FALSE)
                                {
                                    window->titlebar->bg_color = COLOR_GRAY;
                                    window->titlebar_color = COLOR_GRAY;
                                    window->titlebar_ornament_color = xCOLOR_GRAY2;
                                }

                                //bg
                                rectBackbufferDrawRectangle ( 
                                    window->titlebar->left, 
                                    window->titlebar->top, 
                                    window->titlebar->width, 
                                    window->titlebar->height, 
                                    window->titlebar->bg_color, 
                                    TRUE,   // fill
                                    (unsigned long) window->rop );  // rop for this window
                            
                                // ornament
                                rectBackbufferDrawRectangle ( 
                                    window->titlebar->left, 
                                    ( (window->titlebar->top) + (window->titlebar->height) - METRICS_TITLEBAR_ORNAMENT_SIZE ),  
                                    window->titlebar->width, 
                                    METRICS_TITLEBAR_ORNAMENT_SIZE, 
                                    window->titlebar_ornament_color, 
                                    TRUE,  // fill
                                    (unsigned long) window->rop );  // rop_flags
                            
                                // redraw controls.
                                // recursive
                                // #bugbug
                                // Recursive is dangerous and
                                // it is painting in the wrong place.
                                //redraw_window(window->titlebar->Controls.minimize,FALSE);
                                //redraw_window(window->titlebar->Controls.maximize,FALSE);
                                //redraw_window(window->titlebar->Controls.close,FALSE);
                            }
                        }
                    }//tb

                    // let's repaint the borders for some types
                    if( window->type == WT_OVERLAPPED ||
                        window->type == WT_EDITBOX ||
                        window->type == WT_EDITBOX_MULTIPLE_LINES )
                    {
                        if(window->active == TRUE )
                            window->border_size = 2;
                        if(window->active != TRUE )
                            window->border_size = 1;
                        __draw_window_border(window->parent, window);
                    } // borders
                }
            }
        }
    }

done:
    if (flags == TRUE){
        gws_show_window_rect(window);
    }
    return 0;
fail:
    return -1;
}


int redraw_window_by_id(int wid, unsigned long flags)
{
    struct gws_window_d *w;

// wid
    if (wid<0 || wid>=WINDOW_COUNT_MAX)
        return -1;
// structure validation
    w = (void*) windowList[wid];
    if( (void*) w == NULL )
        return -1;
    if(w->magic!=1234)
        return -1;
    
    redraw_window(w,flags);
    return 0;
}

// Here we're gonna redraw the given window
// and invalidate it.
int 
update_window ( 
    struct gws_window_d *window, 
    unsigned long flags )
{
    if ( (void*) window == NULL )
        return -1;

    return (int) redraw_window(window,flags);
}

/*
 * gwssrv_initialize_color_schemes:
 *     Isso configura os esquemas de cores utilizados 
 * pelo sistema.
 *     Essa rotina é chamada apenas uma vez na inicialização
 * do kernel.
 *     O esquema de cores a ser utilizado deve estar 
 * salvo no perfil do usuário que fez o logon.
 * Os dois esquemas padrão chamam-se: 'humility' e 'pride'.
 * + O esquema 'humility' são cores com tema cinza, 
 * lembrando interfaces antigas.
 * + O esquema 'pride' são cores modernas 
 *   ( Aquele verde e preto e cinza, das primeiras versões, 
 * com imagens publicadas. )
 * #todo: é preciso criar rotinas que selecionem entre os 
 * modo criados e habilitados.
 * É preciso criar rotinas que permitam que aplicativos 
 * em user mode criem esquemas de cores e habilite eles.
 */

void 
gwssrv_initialize_color_schemes (int selected_type)
{
    struct gws_color_scheme_d *humility;
    struct gws_color_scheme_d *pride;

// HUMILITY
// Criando o esquema de cores humility. (cinza)

    humility = (void *) malloc ( sizeof(struct gws_color_scheme_d) );

    if ( (void *) humility == NULL ){
        gwssrv_debug_print ("gwssrv_initialize_color_schemes: humility\n");
        printf             ("gwssrv_initialize_color_schemes: humility\n"); 
        GWSHumilityColorScheme = NULL;
        // #bugbug
        // ? return ????
    }else{

		//Object.
		//humility->objectType = ObjectTypeColorScheme;
		//humility->objectClass = ObjectClassGuiObjects;

        humility->used  = TRUE;
        humility->magic = 1234;
        humility->name  = "Humility";
		
		//Colors
		//Definidas em ws.h
        humility->elements[csiNull] = 0;                             //0
        humility->elements[csiDesktop] = HUMILITY_COLOR_BACKGROUND;  //1
        humility->elements[csiWindow] = HUMILITY_COLOR_WINDOW;       //2
        humility->elements[csiWindowBackground] = HUMILITY_COLOR_WINDOW_BACKGROUND;	 //3
        humility->elements[csiActiveWindowBorder] = HUMILITY_COLOR_ACTIVE_WINDOW_BORDER;  //4
        humility->elements[csiInactiveWindowBorder] = HUMILITY_COLOR_INACTIVE_WINDOW_BORDER;  //5
        humility->elements[csiActiveWindowTitleBar] = HUMILITY_COLOR_ACTIVE_WINDOW_TITLEBAR;  //6
        humility->elements[csiInactiveWindowTitleBar] = HUMILITY_COLOR_INACTIVE_WINDOW_TITLEBAR;  //7
        humility->elements[csiMenuBar] = HUMILITY_COLOR_MENUBAR;                //8
        humility->elements[csiScrollBar] = HUMILITY_COLOR_SCROLLBAR;            //9  
        humility->elements[csiStatusBar] = HUMILITY_COLOR_STATUSBAR;            //10
        humility->elements[csiMessageBox] = HUMILITY_COLOR_MESSAGEBOX;          //11
        humility->elements[csiSystemFontColor] = HUMILITY_COLOR_SYSTEMFONT;     //12
        humility->elements[csiTerminalFontColor] = HUMILITY_COLOR_TERMINALFONT; //13
        // ...

		//Sanvando na estrutura padrão para o esquema humility.
        GWSHumilityColorScheme = (void*) humility;
    };

// PRIDE 
// Criando o esquema de cores PRIDE. (colorido)

    pride = (void *) malloc ( sizeof(struct gws_color_scheme_d) );

    if ( (void *) pride == NULL ){
        gwssrv_debug_print ("gwssrv_initialize_color_schemes: pride\n");
        printf ("gwssrv_initialize_color_schemes: pride\n");
        GWSPrideColorScheme = NULL;
        // #bugbug
        // ? return ????
    }else{

		//Object.
		//pride->objectType  = ObjectTypeColorScheme;
		//pride->objectClass = ObjectClassGuiObjects;

        pride->used  = TRUE;
        pride->magic = 1234;
        pride->name  = "Pride";

		//Colors
		//Definidas em ws.h
        pride->elements[csiNull] = 0;
        pride->elements[csiDesktop] = PRIDE_COLOR_BACKGROUND;  
        pride->elements[csiWindow] = PRIDE_COLOR_WINDOW;
        pride->elements[csiWindowBackground] = PRIDE_COLOR_WINDOW_BACKGROUND;
        pride->elements[csiActiveWindowBorder] = PRIDE_COLOR_ACTIVE_WINDOW_BORDER;  
        pride->elements[csiInactiveWindowBorder] = PRIDE_COLOR_INACTIVE_WINDOW_BORDER;  
        pride->elements[csiActiveWindowTitleBar] = PRIDE_COLOR_ACTIVE_WINDOW_TITLEBAR;    
        pride->elements[csiInactiveWindowTitleBar] = PRIDE_COLOR_INACTIVE_WINDOW_TITLEBAR;
        pride->elements[csiMenuBar] = PRIDE_COLOR_MENUBAR;
        pride->elements[csiScrollBar] = PRIDE_COLOR_SCROLLBAR;
        pride->elements[csiStatusBar] = PRIDE_COLOR_STATUSBAR;
        pride->elements[csiMessageBox] = PRIDE_COLOR_MESSAGEBOX;
        pride->elements[csiSystemFontColor] = PRIDE_COLOR_SYSTEMFONT;    //12
        pride->elements[csiTerminalFontColor] = PRIDE_COLOR_TERMINALFONT;  //13
        // ...

		// Sanvando na estrutura padrão para o esquema pride.
        GWSPrideColorScheme = (void *) pride;
    };

// Select
// Configurando qual será o esquema padrão.
// #todo: 
// Criar uma função que selecione qual dois esquemas serão usados
// apenas selecionando o ponteiro da estrutura.  

    switch (selected_type){

    case ColorSchemeNull:
        GWSCurrentColorScheme = (void *) GWSHumilityColorScheme;
        break;

    case ColorSchemeHumility:
        GWSCurrentColorScheme = (void *) GWSHumilityColorScheme;
        break;

    case ColorSchemePride:
        GWSCurrentColorScheme = (void *) GWSPrideColorScheme; 
        break;

    default:
        GWSCurrentColorScheme = (void *) GWSHumilityColorScheme;
        break;
    };

// Check current
    if ( (void*) GWSCurrentColorScheme == NULL ){
        gwssrv_debug_print ("gwssrv_initialize_color_schemes: GWSCurrentColorScheme\n");
        printf             ("gwssrv_initialize_color_schemes: GWSCurrentColorScheme\n"); 
        exit(1);
    }
}

// seleciona o tipo ...isso é um serviço.
int gwssrv_select_color_scheme (int type)
{

    //#debug
    //printf("gwssrv_select_color_scheme: type={%d} \n", type);

    switch (type){
        case ColorSchemeHumility:  goto do_humility;  break;
        case ColorSchemePride:     goto do_pride;     break;
        default:
            gwssrv_debug_print("gwssrv_select_color_scheme: [FAIL] Type not defined\n");
            goto fail;
            break;
    };

do_humility:

    if ( (void *) GWSHumilityColorScheme == NULL ){
        gwssrv_debug_print("HumilityColorScheme fail\n");
        goto fail;
    }else{

        if ( GWSHumilityColorScheme->used  != TRUE || 
             GWSHumilityColorScheme->magic != 1234 )
        {
            gwssrv_debug_print("HumilityColorScheme sig fail\n");
            goto fail;
        }

        gwssrv_debug_print("Humility selected\n");
        GWSCurrentColorScheme = GWSHumilityColorScheme;
        goto done;
    };

do_pride:

    if ( (void *) GWSPrideColorScheme == NULL ){
        gwssrv_debug_print("GWSPrideColorScheme fail\n");
        goto fail;
    }else{
        if( GWSPrideColorScheme->used  != TRUE || 
            GWSPrideColorScheme->magic != 1234 )
        {
            gwssrv_debug_print("PrideColorScheme sig fail\n");
            goto fail;
        }

        gwssrv_debug_print ("Pride selected\n"); 
        GWSCurrentColorScheme = GWSPrideColorScheme;
        goto done;
    };

done:
    return 0;
fail:
    gwssrv_debug_print ("gwssrv_select_color_scheme: fail\n");
    return 1;
}

/*
 * gws_show_window_rect:
 *     Mostra o retângulo de uma janela que está no backbuffer.
 *     Tem uma janela no backbuffer e desejamos enviar ela 
 * para o frontbuffer.
 *     A rotina de refresh rectangle tem que ter o vsync
 *     #todo: criar um define chamado refresh_window.
 */
// ??
// Devemos validar essa janela, para que ela 
// não seja redesenhada sem antes ter sido suja?
// E se validarmos alguma janela que não está pronta?
// #test: validando

int gws_show_window_rect (struct gws_window_d *window)
{
    //struct gws_window_d  *p;

    //#debug
    //debug_print("gws_show_window_rect:\n");

// Structure validation
    if ( (void *) window == NULL ){
        return -1;
    }
    if (window->used != TRUE) { return -1; }
    if (window->magic != 1234){ return -1; }

//#shadow 
// ?? E se a janela tiver uma sombra, 
// então precisamos mostrar a sombra também. 
//#bugbug
//Extranhamente essa checagem atraza a pintura da janela.
//Ou talvez o novo tamanho favoreça o refresh rectangle,
//ja que tem rotinas diferentes para larguras diferentes

    //if ( window->shadowUsed == 1 )
    //{
        //window->width = window->width +4;
        //window->height = window->height +4;

        //refresh_rectangle ( window->left, window->top, 
        //    window->width +2, window->height +2 ); 
        //return (int) 0;
    //}

    //p = window->parent;


// Refresh rectangle
// See: rect.c   

    gws_refresh_rectangle ( 
        window->left, 
        window->top, 
        window->width, 
        window->height ); 

    validate_window(window);

    return 0;

fail:
    debug_print("gws_show_window_rect: fail\n");
    return (int) -1;
}


int flush_window (struct gws_window_d *window)
{
    return (int) gws_show_window_rect(window);
}


int flush_window_by_id(int wid)
{
    struct gws_window_d *w;

// wid
    if (wid<0 || wid>=WINDOW_COUNT_MAX)
        return -1;
// Structure validation
    w = (void*) windowList[wid];
    if ( (void*) w == NULL )
        return -1;

// Flush
    flush_window(w);
    return 0;
}

struct gws_window_d *get_window_from_wid(int wid)
{
    struct gws_window_d *w;

// wid
    if (wid<0 || wid>=WINDOW_COUNT_MAX)
        return NULL;
// Struture validation
    w = (void*) windowList[wid];
    if ( (void*) w == NULL )
        return NULL;
    if (w->magic!=1234)
        return NULL;

// Return the pointer
    return (struct gws_window_d *) w;
}

/*
// #todo
// Retorna o ponteiro de estrutura de janela
// dado o id da janela.
struct gws_window_d *gws_window_from_id (int id);
struct gws_window_d *gws_window_from_id (int id)
{
    struct gws_window_d *w;
    // ...
    return (struct gws_window_d *) w;
}
*/

// Taskbar
// Window server's widget.
// Cria a barra na parte de baixo da tela.
// com 4 tags.
// os aplicativos podem ser agrupados por tag.
// quando uma tag eh acionada, o wm exibe 
// todos os aplicativos que usam essa tag.
void create_taskbar (unsigned long tb_height)
{
    int WindowId = -1;  // bar
    int menu_wid;       // button
    unsigned int frame_color  = 0x00C3C3C3; //COLOR_GRAY;
    unsigned int client_color = 0x00C3C3C3; //COLOR_GRAY;
    unsigned long w = gws_get_device_width();
    unsigned long h = gws_get_device_height();

    if (w==0 || h==0){
        gwssrv_debug_print ("create_taskbar: w h\n");
        printf             ("create_taskbar: w h\n");
        exit(1);
    }

// Taskbar.
// Create  window.

    if (tb_height<40){
        tb_height = 40;
    }
    if(tb_height >= h){
        tb_height = h-40;
    }

    unsigned long wLeft   = (unsigned long) 0;
    unsigned long wTop    = (unsigned long) (h-tb_height);  //(h-40);
    unsigned long wWidth  = (unsigned long) w;
    unsigned long wHeight = (unsigned long) tb_height;  //40;

    taskbar_window = 
        (struct gws_window_d *) CreateWindow ( 
                                    WT_SIMPLE, 
                                    0, //style
                                    1, //status 
                                    1, //view
                                    "TaskBar",  
                                    wLeft, wTop, wWidth, wHeight,   
                                    gui->screen_window, 0, 
                                    frame_color, 
                                    client_color );



// Struture validation
    if ( (void *) taskbar_window == NULL ){
        gwssrv_debug_print ("create_taskbar: taskbar_window\n"); 
        printf             ("create_taskbar: taskbar_window\n");
        exit(1);
    }
    if ( taskbar_window->used != TRUE || 
         taskbar_window->magic != 1234 )
    {
        gwssrv_debug_print ("create_background: taskbar_window validation\n"); 
        printf             ("create_background: taskbar_window validation\n");
        exit(1);
    }

// Register the window.
    WindowId = (int) RegisterWindow(taskbar_window);
    if (WindowId<0){
        gwssrv_debug_print ("create_taskbar: Couldn't register window\n");
        printf             ("create_taskbar: Couldn't register window\n");
        exit(1);
    }

// wid
    taskbar_window->id = WindowId;
// Setup Window manager.
    WindowManager.taskbar = (struct gws_window_d *) taskbar_window;
// Show
    //flush_window(taskbar_window);

// #debug

    /*
    printf ("bar: %d %d %d %d\n",
        taskbar_window->left,
        taskbar_window->top,
        taskbar_window->width,
        taskbar_window->height );

    //refresh_screen();
    //while(1){}
    */

// ===================================
// button box:

// ===================================
// Clean the button list and the pid list.
    register int b=0;
    for (b=0; b<8; b++)
    {
        tb_buttons[b]=0;
        tb_buttons_status[b]=FALSE;
        tb_pids[b]=0;
    };
    tb_buttons_count=0;


// ========================================
// start menu button

    //unsigned long Space = TB_BUTTON_PADDING;   //4;
    //unsigned long b_width  = TB_BUTTON_WIDTH;    //(8*10);
    //unsigned long b_height = TB_BUTTON_HEIGHT;   //40-(Space*2);
    //unsigned long b_left   = TB_BUTTON_PADDING;  //Space;
    //unsigned long b_top    = TB_BUTTON_PADDING;  //Space;

    unsigned long b_left   = 0; 
    unsigned long b_top    = TB_BUTTON_PADDING;

    //unsigned long b_width  = TB_BUTTON_WIDTH;
    //unsigned long b_height = TB_BUTTON_HEIGHT;
    unsigned long b_width  = (unsigned long)(tb_height -8);
    unsigned long b_height = (unsigned long)(tb_height -8);

    register int i=0;         //iterator
    int nbuttons=4;  //quantidade de botões na lista    
    struct gws_window_d *tmp_button;
    int tmp_wid=-1;
    char button_label[32];
    
// Creating n buttons in the taskbar

    for (i=0; i<nbuttons; i++){

    b_left = 
        TB_BUTTON_PADDING + ( (TB_BUTTON_PADDING*i) + (b_width*i) );

    itoa(i,button_label);
    button_label[2] = 0;
    
    tmp_button = 
        (struct gws_window_d *) CreateWindow ( 
            WT_BUTTON, 0, 1, 1, 
            button_label,  //string  
            b_left, 
            b_top, 
            b_width, 
            b_height,   
            taskbar_window, 0, 
            frame_color, 
            client_color );    

    if ( (void *) tmp_button == NULL ){
        gwssrv_debug_print ("create_taskbar: tmp_button\n"); 
        printf             ("create_taskbar: tmp_button\n");
        exit(1);
    }
    if ( tmp_button->used != TRUE || tmp_button->magic != 1234 ){
        gwssrv_debug_print ("create_background: tmp_button validation\n"); 
        printf             ("create_background: tmp_button validation\n");
        exit(1);
    }

// Register the button.
    tmp_wid = RegisterWindow(tmp_button);
    if (tmp_wid<0){
        gwssrv_debug_print ("create_taskbar: Couldn't register button\n");
        printf             ("create_taskbar: Couldn't register button\n");
        exit(1);
    }

    if(i==0){
        taskbar_startmenu_button_window = tmp_button;
    }

//save
    // id de janelas.
    tb_buttons[i] = (int) tmp_wid;
    // ponteiros de estruturas de janelas do tipo botão.
    tb_windows[i] = (unsigned long) tmp_button;

    // Número de botões criados.
    tb_buttons_count++;
    };

// Show
    flush_window_by_id(taskbar_window->id);
    //flush_window_by_id(__taskbar_startmenu_button_window->id);
    //flush_window(taskbar_window);
    //flush_window(__taskbar_startmenu_button_window);

// The first keyboard owner.
    //keyboard_owner = (void*) taskbar_window;

// #debug

/*
    printf ("button: %d %d %d %d\n",
        __taskbar_startmenu_button_window->left,
        __taskbar_startmenu_button_window->top,
        __taskbar_startmenu_button_window->width,
        __taskbar_startmenu_button_window->height );

    refresh_screen();
    while(1){}
*/
}

// Create root window
// Called by gwsInit in gws.c.
// #todo: Talvez essa função possa receber argumentos.
struct gws_window_d *wmCreateRootWindow(void)
{
    struct gws_window_d *w;
// It's because we need a window for drawind a frame.
// WT_OVERLAPPED needs a window and WT_SIMPLE don't.
    unsigned long rootwindow_valid_type = WT_SIMPLE;
    unsigned long left = 0;
    unsigned long top = 0;
// #bugbug: Estamos confiando nesses valores.
// #bugbug: Estamos usado device info sem checar.
    unsigned long width  = (unsigned long) (__device_width  & 0xFFFF );
    unsigned long height = (unsigned long) (__device_height & 0xFFFF );

    if (__device_width == 0 || __device_height == 0 ){
        debug_print("wmCreateRootWindow: w h\n");
        printf     ("wmCreateRootWindow: w h\n");
        exit(1);
    }

// Background color
    unsigned int rootwindow_color = WM_DEFAULT_BACKGROUND_COLOR;
    __set_default_background_color( WM_DEFAULT_BACKGROUND_COLOR );
    __set_custom_background_color(WM_DEFAULT_BACKGROUND_COLOR);

    // #debug
    // debug_print("wmCreateRootWindow:\n");

// (root window)
// #bugbug: Estamos usado device info sem checar.

    w = 
        (struct gws_window_d *) CreateWindow ( 
                                    rootwindow_valid_type,  
                                    0, //style
                                    1, //status
                                    1, //view
                                    "RootWindow",  
                                    left, top, width, height,
                                    NULL, 0, rootwindow_color, rootwindow_color );

// Struture validation
    if ( (void*) w == NULL){
        debug_print("wmCreateRootWindow: w\n");
        printf     ("wmCreateRootWindow: w\n");
        exit(1);
    }
    w->used = TRUE;
    w->magic = 1234;
// Buffers
    w->dedicated_buf = NULL;
    w->back_buf = NULL;
    w->front_buf = NULL;
    w->depth_buf = NULL;
// Device contexts
    w->window_dc = NULL;
    w->client_dc = NULL;

// Default dc.
    if ( (void*) gr_dc != NULL )
    {
        if (gr_dc->initialized == TRUE)
        {
            w->window_dc = (struct dc_d *) gr_dc;
            w->client_dc = (struct dc_d *) gr_dc;
        
            if ( (void*) CurrentProjection != NULL )
            {
                 if (CurrentProjection->magic == 1234)
                 {
                     if ( CurrentProjection->initialized == TRUE ){
                         CurrentProjection->dc = (struct dc_d *) gr_dc;
                     } 
                 }
            }
        }
    }

    w->is_solid = TRUE;
    w->rop = 0;
// Setup the surface in ring0
    setup_surface_rectangle(left,top,width,height);
// invalidate the surface in ring0.
    invalidate_surface_retangle();
    w->dirty = TRUE;  // Invalidate again.
    w->locked = TRUE;
// Root window
    gwsDefineInitialRootWindow(w);

// #
// Do not register now.
// The caller will do that thing.
    return (struct gws_window_d *) w;
}

int gwsDefineInitialRootWindow (struct gws_window_d *window)
{

// Structure validation
    if ( (void *) window == NULL ){
        debug_print("gwsDefineInitialRootWindow: window\n");
        printf     ("gwsDefineInitialRootWindow: window\n");
        exit(1);
        //return -1;
    }
    if (window->magic != 1234){
        debug_print("gwsDefineInitialRootWindow: window magic\n");
        printf     ("gwsDefineInitialRootWindow: window magic\n");
        exit(1);
        //return -1;
    }

    __root_window      = (struct gws_window_d *) window;
    WindowManager.root = (struct gws_window_d *) window;
    return 0;
}


int dock_active_window(int position)
{
    struct gws_window_d *w;
    
    //int wid=-1;
    //wid = (int) get_active_window();
    //if(wid<0)
    //    return -1;
    //if(wid>=WINDOW_COUNT_MAX)
    //    return -1;
    //w = (struct gws_window_d *) windowList[wid];

// Structure validation
    w = (void*) active_window;
    if ( (void*) w == NULL )
        return -1;
    if (w->magic!=1234)
        return -1;

    if (w == __root_window)
        return -1;
    if (w == taskbar_window)
        return -1;
// Dock
    dock_window(w,position);
    return 0;
}

int dock_window( struct gws_window_d *window, int position )
{
    unsigned long w = gws_get_device_width();
    unsigned long h = gws_get_device_height();

    if ( w==0 || h==0 ){
        return -1;
    }

// Strudture validation
    if ( (void*) window == NULL )
        return -1;
    if (window->magic!=1234)
        return -1;

    if (window == __root_window)
        return -1;
    if (window == taskbar_window)
        return -1;

    switch (position){
        case 1:  //top
            gwssrv_change_window_position( window, 0, 0); 
            gws_resize_window( window, w, h>>1 );
            break;
        case 2:  //right
            gwssrv_change_window_position( window, (w>>1), 0); 
            gws_resize_window( window, (w>>1)-4, h-4 );
            break;
        case 3:  //bottom
            gwssrv_change_window_position( window, 0, h>>1); 
            gws_resize_window( window, w, h>>1 );
            break;
        case 4:  //left
            gwssrv_change_window_position( window, 0, 0); 
            gws_resize_window( window, (w>>1)-4, h-4 );
            break;

        default:
            return -1;
            break;
    };

    redraw_window(window,TRUE);
    return 0; 
}

struct gws_window_d *get_active_window (void)
{
    return (struct gws_window_d *) active_window;
}

void set_active_window (struct gws_window_d *window)
{
    if (window == active_window)
        return;

    if ( (void*) window == NULL )
        return;
    if (window->magic!=1234)
        return;

    active_window = (void*) window;
}

struct gws_window_d *get_window_with_focus(void)
{
    return (struct gws_window_d *) keyboard_owner;
}


void set_window_with_focus(struct gws_window_d * window)
{
    if (window == keyboard_owner)
        return;

    if ( (void*) window == NULL )
        return;
    if (window->magic!=1234)
        return;

    keyboard_owner = (void*) window; 
/*  
//#test
    struct gws_window_d *w;
    w = (struct gws_window_d *) windowList[id];
    sc82 (10011,w->client_tid,w->client_tid,w->client_tid);
*/
}


// Pegando a z-order de uma janela.
int get_zorder ( struct gws_window_d *window )
{
    if ( (void *) window != NULL ){
        return (int) window->zIndex;
    }

    return (int) -1;
}


struct gws_window_d *get_top_window (void)
{
    return (struct gws_window_d *) top_window;
}

// Setando a top window.
void set_top_window (struct gws_window_d *window)
{
    if (window == top_window)
        return;

    if ( (void*) window == NULL )
        return;
    if (window->magic!=1234)
        return;

    top_window = (void*) window;
}


int 
gws_resize_window ( 
    struct gws_window_d *window, 
    unsigned long cx, 
    unsigned long cy )
{
    if ( (void *) window == NULL ){
        return -1;
    }

// #todo
    //if(window == __root_window)
        //return -1;


    // Só precisa mudar se for diferente.
    if ( window->width  != cx ||
         window->height != cy )
    {
        window->width  = (unsigned long) cx;
        window->height = (unsigned long) cy;

        // Muda tambem as dimençoes da titlebar.
        // Muda somente a largura, pois a altura deve 
        // continuar a mesma;
        if( window->type == WT_OVERLAPPED )
        {
            // titlebar
            if ( (void*) window->titlebar != NULL )
            {
                window->titlebar->width = 
                    (window->width - window->border_size - window->border_size );
            }

            // client area . (rectangle).

            // width
            // menos bordas laterais
            window->rcClient.width = 
                (unsigned long) (window->width -2 -2 );
            // height
            // menos bordas superior e inferior
            // menos a barra de tarefas.
            //#bugbug: e se a janela for menor que 32?
            window->rcClient.height = 
                (unsigned long) (window->height -2 -32 -2); 
        }
    }

// #bugbug
// Precisa mesmo pintar toda vez que mudar as dimensoes
    //invalidate_window(window);
    return 0;
}


// #test
// Isso so faz sentido num contexto de reinicializaçao 
// do desktop.
void reset_zorder(void)
{
     register int i=0;
     struct gws_window_d *w;

     for ( i=0; i<WINDOW_COUNT_MAX; ++i)
     {
         w = (struct gws_window_d *) windowList[i];
         if ( (void*) w != NULL )
         {
             if ( w->used == TRUE && w->magic == 1234 )
             {
                 // Coloca na zorder as janelas overlapped.
                 if (w->type == WT_OVERLAPPED){
                     zList[i] = windowList[i];
                 }
             }
         }
     };
}

// gwssrv_change_window_position:
// Muda os valores do posicionamento da janela.
// #todo: Podemos mudar o nome para wm_change_window_position().
int 
gwssrv_change_window_position ( 
    struct gws_window_d *window, 
    unsigned long x, 
    unsigned long y )
{

// #??
// Isso deve mudar apenas o deslocamento em relacao
// a margem e nao a margem ?

    if ( (void *) window == NULL ){
        gwssrv_debug_print("gwssrv_change_window_position: window\n");
        return -1;
    }

// #todo
    //if(window == __root_window)
        //return -1;

    /*
    if ( window->left != x ||
         window->top  != y )
    {
        window->left = (unsigned long) x;
        window->top  = (unsigned long) y;
    }
    */

// #bugbug #todo
// Temos que checar a validade da parent.

    window->x = x;
    window->y = y;
    //if ( (void*) window->parent == NULL ){ return; };
    window->left = (window->parent->left + window->x);
    window->top  = (window->parent->top  + window->y);

// Se overlapped:
// Muda também as posições da titlebar.
// Muda também as posições do área de cliente.
    if (window->type == WT_OVERLAPPED)
    {
        // Title bar window.
        if ( (void*) window->titlebar != NULL )
        {
            window->titlebar->left = 
                ( window->left + window->border_size );
            window->titlebar->top = 
                ( window->top  + window->border_size );
        
            //if (window->titlebar->Controls.initialized == TRUE)
            //{
                // get pointer from id.
                // change position of the controls.
            //}
        }

        // client area . (rectangle).

        // left
        window->rcClient.left = (unsigned long) 2;  // + borda da esq
        // top
        window->rcClient.top = (unsigned long) 2+32;
        // width
        // menos bordas laterais
        window->rcClient.width = 
            (unsigned long) (window->width -2 -2 );
        // height
        // menos bordas superior e inferior
        // menos a barra de tarefas.
        window->rcClient.height = 
            (unsigned long) (window->height -2 -32 -2); 
    }

// #bugbug
// Precisa mesmo pinta toda vez que mudar a posiçao?
    //invalidate_window(window);
    return 0;
}


// Lock a window. 
void gwsWindowLock (struct gws_window_d *window)
{
    if ( (void *) window == NULL ){
        return;
    }
    window->locked = (int) WINDOW_LOCKED;  //1.
}

// Unlock a window. 
void gwsWindowUnlock (struct gws_window_d *window)
{
    if ( (void *) window == NULL ){
        return;
    }
    window->locked = (int) WINDOW_UNLOCKED;  //0.
}


int gwssrv_init_windows(void)
{
    register int i=0;

    //window.h
    windows_count     = 0;
    keyboard_owner = NULL;
    active_window = NULL;
    top_window    = NULL;
    //...
    show_fps_window = FALSE;

// Window list
    for (i=0; i<WINDOW_COUNT_MAX; ++i){ windowList[i] = 0; };
// z order list
    for (i=0; i<ZORDER_MAX; ++i){ zList[i] = 0; };
    // ...

    return 0;
}

//
// == ajusting the window to fit in the screen. =======================
//

/*
 credits: hoppy os.
void 
set_window_hor (
    tss_struct *tss,
    int i,
    int j)
{
    int d = j-i;
    
    if ( i >= tss->crt_width) 
    {
        i = tss->crt_width-2;
        j = i+d;
    }
    
    if (j<0) 
    {
        j=0;
        i = j-d;
    }
    
    if (i>j) 
    {
        if (i>0)
           j=i;
        else
            i=j;
    }
    
    tss->window_left=i;
    tss->window_right=j;
}
*/


/*
 credits: hoppy os.
void 
set_window_vert ( 
    tss_struct *tss,
    int i,
    int j )
{
    int d = j-i;

    if (i >= tss->crt_height) 
    {
        // ajusta o i.
        i = tss->crt_height-1;
        j = i+d;
    }
    
    if (j<0) 
    {
        // ajusta o i.
        j = 0;
        i = j-d;
    }

    if (i>j) 
    {
        if (i>0)
            j=i;
        else
            i=j;
    }

    tss->window_top    = i;
    tss->window_bottom = j;
}
*/


/*
// process id
// Retorna o pid associado à essa janela.
// #todo: fazer essa rotina também na biblioteca client side.
pid_t get_window_pid( struct gws_window_d *window);
pid_t get_window_pid( struct gws_window_d *window)
{
}
*/

/*
// thread id
// Retorna o tid associado à essa janela.
// #todo: fazer essa rotina também na biblioteca client side.
int get_window_tid( struct gws_window_d *window);
int get_window_tid( struct gws_window_d *window)
{
}
*/

// teremos mais argumentos
void wm_Update_TaskBar( char *string, int flush )
{
    if ( WindowManager.is_fullscreen == TRUE ){
        return;
    }

    if ( (void*) string == NULL ){
        return;
    }

    if (*string == 0){
        return;
    }

// Window
    if ( (void*) taskbar_window == NULL ){
        return;
    }
    if (taskbar_window->magic!=1234){
        return;
    }

// Redraw the bar.
// Redraw the buttons.

    redraw_window_by_id(taskbar_window->id,TRUE);
    //redraw_window_by_id(
    //    __taskbar_startmenu_button_window->id,TRUE);
    //redraw_window(taskbar_window,TRUE);
    //redraw_window(__taskbar_startmenu_button_window,TRUE);

// Redraw, all the valid buttons in the list.
    register int i=0;
    int wid=0;
    for (i=0; i<TB_BUTTONS_MAX; i++)
    {
        if (tb_buttons[i] != 0){
            wid = (int) tb_buttons[i];
            redraw_window_by_id(wid,TRUE);
            __draw_button_mark_by_wid(wid,i);
        }
    };


//
// String
//

// String info.
    unsigned long string1_left = 
        (unsigned long) (taskbar_window->width - 100);
    unsigned long string2_left = 
        (unsigned long) (string1_left - (8*4));
    unsigned long string_top  = ((taskbar_window->height-8) >> 1);//8 
    unsigned int string_color = COLOR_BLACK;
    size_t string_size = (size_t) strlen(string);

    char frame_counter_string[32];
    itoa(
        WindowManager.frame_counter,
        frame_counter_string );

// Draw the text.
// less than 10 chars.
    if (string_size < 10){
        // String 2
        dtextDrawText(
            taskbar_window, string2_left, string_top, string_color, "|" );
        // String 1
        dtextDrawText(
            taskbar_window,
            string1_left, string_top, string_color, string );
    }

// Show the window.
    if (flush==TRUE){
        flush_window_by_id(taskbar_window->id);
        //flush_window(taskbar_window);
        //invalidate_window(taskbar_window);
    }
}

void wmInitializeGlobals(void)
{
    //debug_print ("wmInitializeGlobals:\n");
    fps=0;
    frames_count=0;
    ____old_time=0;
    ____new_time=0;
    old_x=0;
    old_y=0;
}

//
// End
//


