
#ifndef LSD_H
#define LSD_H
/*
    LSD: Logging System, Duh!
    Simples Logging-System mit Wichtigkeits-Stufen
    Und simplem File-Handling
    Und simplem Multi-Threading
    Und Vektoren
    Und Array-Funktionen
    Und Path-Funktionen
    Und Delta-Timer-Funktionen
    Und Socket basiertem Webserver
    Und Memory-Debugging
    Und dynamischen Arrays
*/

#define LSD_THREADS_MAX 100
#define LSD_DELTAS_MAX 100
#define LSD_WEBSERVER_MAX_CONNECTIONS 1
#define LSD_WEBSERVER_PORT 1234
#define LSD_WEBSERVER_MAX_READBUFFER_SIZE 1024

#define LSD_EXIT_ERROR 2
#define LSD_EXIT_SUCCESS 0

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <netdb.h>   

#ifdef LSD_Debugmem
    #define malloc(X) LSD_Debugmem_malloc(X,__FILE__,__LINE__)
    #define realloc(X,Y) LSD_Debugmem_realloc(X,Y,__FILE__,__LINE__)
    #define free(X) LSD_Debugmem_free(X,__FILE__,__LINE__)
#endif

typedef int bool;

#define true 1
#define false 0

#define LSD_Delta_none          0.0f
#define LSD_Thread_function(X)  void* X(void* i)
#define LSD_Thread_init()       unsigned char LSD_Thread_index = *((int*) i)
#define LSD_Thread_finish()     LSD_Threads[LSD_Thread_index].done = true; return NULL

#define LSD_Vec_add(Z,X,Y) Z.x = X.x + Y.x; Z.y = X.y + Y.y;
#define LSD_Vec_sub(Z,X,Y) Z.x = X.x - Y.x; Z.y = X.y - Y.y;
#define LSD_Vec_mul(Z,X,Y) Z.x = X.x * Y.x; Z.y = X.y * Y.y;
#define LSD_Vec_div(Z,X,Y) Z.x = X.x / Y.x; Z.y = X.y / Y.y;

#define LSD_Table_at(X,Y,Z)             (*((Z*)X.data+X.list[Y]*X.size))
#define LSD_Table_actually_at(X,Y,Z)    (*((Z*)X.data+Y*X.size))
#define LSD_Table_remove(X,Y)           {int temp = Y; LSD_Math_remove_object_from_array(X.list,&X.used,&temp);} LSD_Sys_NOP()
#define LSD_Table_create(X,Y,Z)         LSD_Table X; X.used = 0; X.max = Y; X.size = sizeof(Z); X.data = malloc(X.max * X.size); X.list = malloc(X.max * sizeof(int)); memset(X.list,0,X.max * sizeof(int)); memset(X.data,0,X.max * sizeof(Z))
#define LSD_Table_init(X,Y,Z)           X.used = 0; X.max = Y; X.size = sizeof(Z); X.data = malloc(X.max * X.size); X.list = malloc(X.max * sizeof(int)); memset(X.list,0,X.max * sizeof(int)); memset(X.data,0,X.max * sizeof(Z))
#define LSD_Table_destroy(X)            free(X.data); free(X.list)
#define LSD_Table_push(X,Y,Z)           LSD_Assert(X.used < X.max,"Kein Platz mehr im Table!"); X.list[X.used] = LSD_Math_get_id_from_array(X.list, &X.used, X.max); LSD_Table_at(X,X.used,Z) = Y
#define LSD_Table_pop(X)                LSD_Table_remove(X,X.used)
#define LSD_Table_resize(X,Y)           X.max = Y; X.data = realloc(X.data,X.size * X.max); X.list = realloc(X.list,X.max * sizeof(int))

#define LSD_Table_nocast_at(X,Y)          (X.data+X.list[Y]*X.size)
#define LSD_Table_nocast_actually_at(X,Y) (X.data+Y*X.size)
#define LSD_Table_nocast_push(X,Y,Z)      LSD_Assert(X.used < X.max,"Kein Platz mehr im Table!"); X.list[X.used] = LSD_Math_get_id_from_array(X.list, &X.used, X.max); memcpy((X.data+X.used*X.size),&Y,X.size) 

#define LSD_Table_dynamic_create(X,Z)         LSD_Table_create(X,1,Z)
#define LSD_Table_dynamic_init(X,Z)           LSD_Table_init(X,1,Z)
#define LSD_Table_dynamic_push(X,Y,Z)         LSD_Table_dynamic_resize(X); LSD_Table_push(X,Y,Z)
#define LSD_Table_dynamic_resize(X)           if (X.used == X.max) {LSD_Table_resize(X,X.max + 1 + X.max / 2);}

#define LSD_Sys_varname_get(var) #var
#define LSD_Sys_strcmp(X,Y) (0 == strcmp(X,Y))

#define LSD_Assert(X,Y) LSD_Assert_i(X,__LINE__,__FILE__,Y)

#define LSD_Math_AABB(pos1,pos2,size1,size2) (pos1.x < pos2.x+size2.x && pos2.x < pos1.x+size1.x && pos1.y < pos2.y+size2.y && pos2.y < pos1.y+size1.y)

struct LSD_Vec2i
{
    int x,y;
};

struct LSD_Vec2f
{
    float x,y;
};

struct LSD_Vec2d
{
    double x,y;
};

enum LSD_Log_type
{
    LSD_ltMESSAGE,LSD_ltWARNING,LSD_ltERROR, LSD_ltCUSTOM, LSD_ltINTERNAL
};

enum LSD_Log_level
{
    LSD_llALL, LSD_llBAD, LSD_llERROR, LSD_llNONE
};

struct LSD_File
{
    char* filename;
    char* data;
    int lines;
};

struct LSD_Thread
{
    char* name;
    void* (*function)(void*);
    bool done;
    pthread_t tid;
    unsigned char lid;
};

struct LSD_Delta
{
    struct timeval begin, end;
    char* name;
    double delta;
    double last_delta;
};

struct LSD_WebServer
{
    int port;
    int server_fd, client_fd;
    int addrlen;
    struct sockaddr_in address;

    void (*POST_handle)(struct LSD_WebServer* server);
    void (*GET_handle)(struct LSD_WebServer* server);

    char read_buffer[LSD_WEBSERVER_MAX_READBUFFER_SIZE];

    const char* directory_prefix;
    const char* response_template;
    char* response_message;
};

struct LSD_Table
{
    int max;
    int used;
    int size;
    int* list;
    void* data;
};

typedef struct LSD_Vec2i LSD_Vec2i;
typedef struct LSD_Vec2f LSD_Vec2f;
typedef struct LSD_Vec2d LSD_Vec2d;
typedef enum LSD_Log_type LSD_Log_type;
typedef enum LSD_Log_level LSD_Log_level;
typedef struct LSD_File LSD_File;
typedef struct LSD_Thread LSD_Thread;
typedef struct LSD_Delta LSD_Delta;
typedef struct LSD_WebServer LSD_WebServer;
typedef struct LSD_Table LSD_Table;


extern LSD_Thread LSD_Threads[LSD_THREADS_MAX];
extern int LSD_Thread_list[LSD_THREADS_MAX];
extern int LSD_Thread_used;

/* Hiermit kann das Level gesetzt werden, bei dem Logs tatsächlich in stdout angezeigt werden */
void LSD_Log_level_set(LSD_Log_level level);

/* Logging Funktion mit Vargargs, wie bei printf() */
void LSD_Log(LSD_Log_type type, char* format, ...);

/* Öffne eine Datei. Danach kann das Feld "data" für den Inhalt und "lines" für die Anzahl der Zeilen ausgelesen werden */
LSD_File* LSD_File_open(char* filename);

/* Freit den Speicher der für den Inhalt einer Datei benötigt wurde */
void LSD_File_close(LSD_File* f);

/* Schreibt einen char-pointer in eine Datei */
LSD_File* LSD_File_write(LSD_File* file, char* data);

/* Fügt einen Thread mit einem Namen und einer Funktion hinzu */
void LSD_Thread_add(char* name, void*(*func)(void*));

/* Check, ob ein Thread fertiwg ist und entfernt ihn gegebenenfalls aus der Liste */
void LSD_Thread_system(void);

/* Callback um zu sehen, ob ein Thread fertig ist. Wird ohne LSD_Thread_system() nicht benötigt */
bool LSD_Thread_done(char* name);

/* Füge einen neuen Timer gelinkt mit einem Namen hinzu */
void LSD_Delta_add(char* name);

/* Das LSD-Delta-Struct nach Namen finden */
LSD_Delta* LSD_Delta_get(char* name);

/* LSD-Delta-Struct updaten (Sollte am anfang von einem Loop gecallt werden!) */
void LSD_Delta_tick(char* name);

/* Entfernt ein LSD_Delta */
void LSD_Delta_remove(char* name);

/* Erstellt einen neuen Vektor mit entsprechenden typen */
LSD_Vec2i LSD_Vec_new_int(int x, int y);
LSD_Vec2f LSD_Vec_new_float(float x, float y);
LSD_Vec2d LSD_Vec_new_double(double x, double y);

/* Entfernt ein Objekt aus einem Array */
void LSD_Math_remove_object_from_array(int arr[], int* max_arr, int* index_to_be_removed);

/* Returnt eine Zahl, die vorher noch nicht verwendet wurde aus einem Array */
int LSD_Math_get_id_from_array(int black_list[], int* black_fill, int max_id);

/* Streckt / Quetscht eine Zahl mit angegebenem Zahlenbereich auf einen neuen Zahlenbereich */
float LSD_Math_map(float num, float min1, float max1, float min2, float max2);

/* Zufallsgenerator, der Zahlen zwischen 0 und RAND_MAX ausgibt (RAND_MAX ist eine Konstante, die in der C-Stdlib definiert ist) */
int LSD_Math_random_get(void);

/* Setzt die Working-Directory um setback Chars zurück */
void LSD_Sys_path_change(char* argv[], int setback, char* path_return);

/* Bereitet einen Webserver vor, um später mit LSD_WebServer_serve_while() zu serven */
LSD_WebServer* LSD_WebServer_open(const char* dp, int port,void (*GET)(struct LSD_WebServer* server),void (*POST)(struct LSD_WebServer* server));

/* Beendet jegliche Kommunikation mit Klienten und free'd den Speicherplatz des Servers */
void LSD_WebServer_close(LSD_WebServer* server);

/* Standart POST-, bzw GET-Handler für einen Server der keine speziellen Versionen davon braucht */
void LSD_WebServer_STD_GET(LSD_WebServer* server);
void LSD_WebServer_STD_POST(LSD_WebServer* server);

/* Lässt den Webserver so lange serven, bis der Boolean nicht mehr war ist */
void LSD_WebServer_serve_while(LSD_WebServer* server, bool* running);

/* Lässt den Webserver eine POST-, bzw. GET-Request bearbeiten */
void LSD_WebServer_serve_once(LSD_WebServer* server);

/* Malloc-, realloc- und free- Ersatz für einfaches Memory-Debugging */
void* LSD_Debugmem_malloc(size_t size, char* FILE, int LINE);
void* LSD_Debugmem_realloc(void* ptr, size_t size, char* FILE, int LINE);
void  LSD_Debugmem_free(void* ptr, char* FILE, int LINE);

/* Init- und Deinit- Funktionen für LSD_Debugmem- Funktionen */
void  LSD_Debugmem_init(int backlog);
void  LSD_Debugmem_deinit(void);

/* Wirklich einfach eine NOP-Funktion, die nichts macht */
void LSD_Sys_NOP(void);

/* Interne Assertion-Funktion */
void LSD_Assert_i(bool assertion,int l, char* f, char* msg);


#endif
