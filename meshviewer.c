/* STDLIBS */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>

/* GLLIBS */
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "meshviewer.h"

#include "infocolour.h"


/*
 *  This file name and color, used in INFO/WARN/ERR msgs
 */
static char *MOD = "MSV";
static char *COL = GREEN;



#define WINTITLE        "Mesh Viewer"
#define ERRPRE          "MVERROR:"
#define printe(...)     fprintf(stderr, __VA_ARGS__)
#define printerr(err)   printe("%s %s\n", ERRPRE, err)
#define INFOPRE         "MVINFO:"
#define printi(...)     printf(__VA_ARGS__)
#define printinfo(info) printi("%s %s\n", INFOPRE, info)
#define KDOWN           0
#define KUP             1

/* TODO list

    * update an un object: colour array
    * set background colour
    * ZOOM/UNZOOM TO THE CURSOR
    * legend
    * INFOMF + verbose + quiet

    * A REFACTOR!!

    - keybind to std zoom
    - double click to zoom at click
    - free everything when stoping
    - make number of objects dynamic
    - zoom min and zoom max
    - pan min/max
    - write help

    - add posibility of passing the borders to set custom colour
    - emit render event after rendering a frame


    --------- DONE ---------
    * destroy a plot!
    * do not return mv_add until var n is defined
    * a flag so it can receive an colour array-per-vertex
    * DO NOT FREE THE INPUT ARRAYS
    * add multiple layer support

*/

/* * * * * * * * * * TYPES * * * * * * * * * */
typedef enum _evtype {
    /* objects events */
    EV_CREATEOBJ     = 1,
    EV_SETPROPERTY   = 2,
    EV_DESTROYOBJ    = 3,
    EV_UPDATEBUFFER  = 4,
    

    /* render events */
    EV_RENDER         = 100,
} evtype;

typedef struct object {
    GLuint      vao,
                vbo,  cbo,  ibo,
                vbos, cbos, ibos;
    GLenum      mode;
    GLfloat     width;
} object;

typedef struct program {
    GLuint      shaderprogram,
                vertexshader,
                fragmentshader;
} program;

typedef struct renderobj {
    program     *prog;
    object      *obj;
    GLfloat     scale,
                translate[2],
                rotate;
    int         enable;
    int         layer;
} renderobj;
typedef enum ev_esp {
    EV_SP_VISIBILITY = 1 << 0,
    EV_SP_SCALE      = 1 << 1,
    EV_SP_TRANSLATE  = 1 << 2,
    EV_SP_ROTATE     = 1 << 3,
    EV_SP_LAYER      = 1 << 4,
} ev_esp;

typedef enum ev_eup {
    EV_UP_VERTEX     = 1 << 0,
    EV_UP_COLOUR     = 1 << 1,
} ev_eup;


/* * * * * * * * * * GL ENV * * * * * * * * * */
SDL_Window *        window;
int                 running;
GLint               curWidth  = MV_WINW,    /* Window size */
                    curHeight = MV_WINH;
int                 fpscount  = 0;          /* FPS counter */
pthread_t           renderthread;
unsigned long long  totalpoints = 0;
int                 MAXLAYERS = 1;
int                 verbosity = 0;

/* * * * * * * * * * EVENTS * * * * * * * * * */
void (*ev_callbacks[256]) (void* data1, void* data2);

/* * * * * * * * * * RENDER * * * * * * * * * */
program *           defprog[16];
renderobj *         renderobjs[MV_MAXOBJECTS];
int                 allocatedRO = 0;
/* SPECIFIC RENDER OBJECTS */
/* the selection box */
renderobj *         ro_selbox;
int                 sb_isshow,
                    sb_ix, sb_iy,
                    sb_fx, sb_fy;
/* Keyboard and mouse */
int                 modshift, modctrl, modalt;
/* Camera position and zoom */
double              cx,  cy,
                    ctx, cty;
double              cz,  ctz;
double              az,  ap;
float               zoom_factor = MV_ZOOMFACTOR;


/* * * * * * * * * * HEADERS * * * * * * * * * */
void *          inthread        (void * m);
void            ev_emit         (int code, void* data1, void* data2);
void            ev_add          (int code, void (*func) (void*, void*));
void            ev_remove       (int code);
void            ev_keyboard     (int type, SDL_Keysym keysym);
void            ev_mouse        (SDL_Event event);
void            ev_renderloop   (void* data1, void* data2);
void            ev_createobj    (void* data1, void* data2);
void            eve_createobj(GLenum mode, GLfloat * vertices, GLfloat * colours, int size, int countv, int countc, GLuint * indices, int counti, GLfloat width, int layer,  int * id);
void            ev_setproperty  (void* data1, void* data2);
void            eve_setproperty (ev_esp change, unsigned int id, char visibility, float scale, float translatex, float translatey, float rotate, int layer);
void            ev_destroyobj(void* data1, void* data2);
void            eve_destroyobj(int n);
void            ev_updatebuffer(void* data1, void* data2);
void            eve_updatebuffer(ev_eup change, int id, GLfloat * vertices, GLfloat * colours, int size);

Uint32          timer_countfps  (Uint32 interval, void* param);

void            renderloop      ();
void            render_ro       (renderobj * ro);
int             add_ro          (renderobj * ro);
program *       new_program     (const char * vertexsource, const char * fragmentsource);
program *       new_program_default();
object *        new_buffer      (GLenum mode, GLfloat * vertices, GLfloat * colours, int size, int countv, int countc, GLuint * indices, int counti);

renderobj *     new_ro_selbox   ();
void            sb_dozoom       ();

void            transform_triangles_in_lines(GLuint ** pindices, unsigned int * pcounti, int countv);
void            transform_triangles_in_points(GLuint ** pindices, unsigned int * pcounti, int countv);

/* * * * * * * * * * * * * * * * * *
 *  THE MESH VIEWER
 * * * * * * * * * * * * * * * * * */
void mv_start(int maxlayers, int verbose) {
    static char *FUN = "mv_start()";
    MAXLAYERS = maxlayers > 1 ? maxlayers : 1;
    verbosity = verbose;

    pthread_mutex_t mutex;
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        if (verbosity > -1) WARNMF("Couldn't create a mutex so there is no meshviewer");
        return;
    }

    if (pthread_mutex_lock(&mutex) == 0) {
        pthread_create(&renderthread, NULL, inthread, &mutex);

        if (pthread_mutex_lock(&mutex) != 0) {
            ERRMF("Couldn't wait for the object to be created. Unspecified behaviour from now on");
            exit(-1);
        }
    } else {
        ERRMF("I can't start the meshviewer without locking a mutex. Try again later");
        exit(-1);
    }
    pthread_mutex_destroy(&mutex);
}

void mv_wait() {
    pthread_join(renderthread, NULL);
}

void mv_stop() {
    running = 0;
}

void mv_init();
void mv_draw();

void * inthread(void * m) {
    static char *FUN = "inthread()";
    if (verbosity > -1) INFOMF("Starting render thread");
    mv_init();

    pthread_mutex_t *mutex = (pthread_mutex_t *) m;
    if (pthread_mutex_unlock(mutex) != 0) {
        ERRMF("Couldn't release a mutex while initializing the meshviewer; it's time to a deadlock!");
        exit(-1);
    }

    mv_draw();
    if (verbosity > -1) INFOMF("Leaving render thread %d", running);
    pthread_exit(NULL);
}

void mv_init() {
    static char *FUN = "init()";
    /* Init the GL env */
    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    /* SDL WINDOW */
    window = SDL_CreateWindow(WINTITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        curWidth, curHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_GL_CreateContext(window);

    /* * * * Init GLGlew * * * */
    GLenum result = glewInit();
    if (result != GLEW_OK) {
        ERRMF("Some Glew error. Do you have it? Anyway, exiting...");
        exit(-1);
    }
    if (verbosity > 0) INFOMF("Using openGL version %s", glGetString(GL_VERSION));
    
    /* * * * Init OpenGL * * * */
    glEnable(GL_DEPTH_TEST | GL_LINE_SMOOTH | GL_POLYGON_SMOOTH | GL_ALPHA_TEST);
    glClearColor(0,0,0,1);
    //glClearColor(1,1,1,1);
    
    glDepthMask(GL_FALSE);
    glEnable   (GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    
    /* SDL register timers */
    SDL_AddTimer(1000, timer_countfps, 0);
    
    /* SDL Register the events */
    ev_add(EV_RENDER,       ev_renderloop);
    ev_add(EV_CREATEOBJ,    ev_createobj);
    ev_add(EV_SETPROPERTY,  ev_setproperty);
    ev_add(EV_DESTROYOBJ,   ev_destroyobj);
    ev_add(EV_UPDATEBUFFER, ev_updatebuffer);
    
    
    /* Prepare the render stuff */
    ctz = cz = MV_ZOOMDEFAULT; /* TODO make the zoom somekind auto - based on mins maxs? */
    defprog[0] = new_program_default();
    ro_selbox = new_ro_selbox();
    running = 1;
}

void mv_draw() {
    SDL_Event event;

    ev_emit(EV_RENDER, NULL, NULL);

    while (running && SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SDL_USEREVENT:
                if (ev_callbacks[event.user.code])        /* is it allocated? */
                    (*ev_callbacks[event.user.code])(event.user.data1, event.user.data2);
                break;


            case SDL_KEYDOWN: if (!event.key.repeat) ev_keyboard(KDOWN, event.key.keysym); break;
            case SDL_KEYUP:                          ev_keyboard(KUP,   event.key.keysym); break;
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEWHEEL:                     ev_mouse(event);                      break;


            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) { /* Resized */
                    curWidth  = event.window.data1,
                    curHeight = event.window.data2;
                }
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
                break;
            case SDL_QUIT:                           running = 0;                          break;
            default: break;
        }
    }
    SDL_Quit();
}



/* * * * * * * * * * * * * * * * * *
 *  THE EVENTS
 * * * * * * * * * * * * * * * * * */
void ev_add(int code, void (*func) (void*, void*)) {
    ev_callbacks[code] = func;
}
void ev_remove(int code) {
    ev_callbacks[code] = NULL;
}
void ev_emit(int code, void* data1, void* data2) {
    SDL_Event event;
    event.type       = SDL_USEREVENT;
    event.user.code  = code;
    event.user.data1 = data1;
    event.user.data2 = data2;
    SDL_PushEvent(&event);
}

int eegg = 0;
void ev_keyboard(int type, SDL_Keysym keysym) {

    if (type == 1 && modalt) printf("Key%s: %d\n", type == KDOWN ? "down" : "up", keysym.sym);

    if (keysym.sym == 1073742049 || keysym.sym == 1073742053) modshift = type; /* Shift */
    if (keysym.sym == 1073742048 || keysym.sym == 1073742052) modctrl  = type; /* Control */
    if (keysym.sym == 1073742050 || keysym.sym == 1073742050) modalt   = type; /* Alt */
    
    if (keysym.sym == 27 && type == KDOWN) { /* TODO elegant way to go out, eventually */
        if (eegg < 5) eegg++;
        else          running = 0;
    }
    
    if (keysym.sym == 97 && type == KDOWN) { /* A */
    }

    if (keysym.sym >= 49 && keysym.sym <= 57 && type == KDOWN) { /* 0 */
        if (renderobjs[keysym.sym - 49] != NULL)
            renderobjs[keysym.sym - 49]->enable ^= 1;
    }
}

void ev_mouse(SDL_Event event) {
    SDL_MouseMotionEvent *evm = (SDL_MouseMotionEvent *) &event;
    SDL_MouseButtonEvent *evb = (SDL_MouseButtonEvent *) &event;

    switch (event.type) {

        case SDL_MOUSEBUTTONDOWN:
                if (evb->button == SDL_BUTTON_RIGHT) {
                    sb_ix = sb_fx = evb->x;
                    sb_iy = sb_fy = evb->y;
                    sb_isshow = 1;
                }
            break;
        case SDL_MOUSEBUTTONUP:
            if (evb->button == SDL_BUTTON_RIGHT) {
                sb_isshow = 0;
                sb_dozoom();
            }
            break;
        case SDL_MOUSEMOTION:
            if (evm->state & 1 << (SDL_BUTTON_LEFT - 1)) {
                ctx += evm->xrel / cz;
                cty -= evm->yrel / cz;
            }
            if (sb_isshow) {
                sb_fx = evm->x;
                sb_fy = evm->y;
            }
            break;
        case SDL_MOUSEWHEEL:
            if (evb->x > 0) ctz *= zoom_factor;
            else            ctz /= zoom_factor;
            break;
    }
}

void ev_renderloop(void* data1, void* data2) {
    renderloop();
}

/* CREATE OBJECT EVENT  */
typedef struct ev_object {
    GLenum              mode;
    GLfloat             * vertices, * colours;
    GLuint              *indices;
    int                 size, countv, countc, counti;
    GLfloat             width;
    int                 layer;
    sem_t               sem;
} ev_object;

void ev_createobj(void* data1, void* data2) {
    static char *FUN = "ev_createobj()";
    if (allocatedRO >= MV_MAXOBJECTS) {
        if (data2 != NULL) *((int *)data2) = -1;
        if (verbosity > -1) WARNMF("Trying to add an object but the object list is full (%d objects)", MV_MAXOBJECTS);
        return;
    }
    ev_object * obj = (ev_object *) data1;
    renderobj * ro = (renderobj *) malloc(sizeof(renderobj));
    int id = add_ro(ro);
    if (data2 != NULL) *((int *)data2) = id;

    ro->obj          = new_buffer(obj->mode, obj->vertices, obj->colours, obj->size, obj->countv, obj->countc, obj->indices, obj->counti);;
    ro->prog         = defprog[0];
    ro->scale        = 1.0;
    ro->translate[0] = 0.0;
    ro->translate[1] = 0.0;
    ro->rotate       = 0.0;
    ro->enable       = 1;
    ro->layer        = obj->layer;
    ro->obj->width   = obj->width;

    totalpoints += obj->counti;
    if (verbosity > 0) INFOMF("New object added. Now drawing %lld points in %d object%s", totalpoints, allocatedRO, allocatedRO == 1 ? "" : "s");

    if (sem_post(&obj->sem) != 0) {
        ERRMF("Couldn't release the semaphore while creating a new object; it's time to a deadlock!");
        exit(-1);
    }
    free(data1);
}

void eve_createobj(GLenum mode, GLfloat * vertices, GLfloat * colours, int size, int countv, int countc, GLuint * indices, int counti, GLfloat width, int layer, int * id) {
    static char *FUN = "eve_createobj()";
    ev_object * obj = (ev_object *) malloc(sizeof(ev_object));

    if (sem_init(&obj->sem, 0, 1) != 0) {
        if (verbosity > -1) WARNMF("Couldn't create a semaphore. I'll forget this new object, sorry");
        return;
    }

    if (sem_trywait(&obj->sem) == 0) {
        obj->mode       = mode;
        obj->vertices   = vertices;
        obj->colours    = colours;
        obj->indices    = indices;
        obj->size       = size;
        obj->countv     = countv;
        obj->countc     = countc;
        obj->counti     = counti;
        obj->width      = width;
        obj->layer      = layer;

        ev_emit(EV_CREATEOBJ, obj, id);

        if (sem_wait(&obj->sem) != 0) { /*timeout TODO*/
            ERRMF("Couldn't wait for the new object to be created. Unspecified behaviour from now on");
            exit(-1);
        }
    } else
        if (verbosity > -1) WARNMF("I can't create a new object without locking a semaphore. I'll forget this update, sorry");

    sem_destroy(&obj->sem);
}

/* SET PROPERTIES EVENT */
typedef struct ev_property {
    unsigned int    id;
    ev_esp          change;
    char            visibility;
    float           scale,
                    translate[2],
                    rotate;
    int             layer;
} ev_property;

void ev_setproperty(void* data1, void* data2) {
    ev_property * sp = (ev_property*) data1;
    if (sp->id >= allocatedRO || renderobjs[sp->id] == NULL)
        return;

    if (sp->change & EV_SP_VISIBILITY) {
        renderobjs[sp->id]->enable = sp->visibility;
    }
    if (sp->change & EV_SP_SCALE) {
        renderobjs[sp->id]->scale = sp->scale;
    }
    if (sp->change & EV_SP_TRANSLATE) {
        renderobjs[sp->id]->translate[0] = sp->translate[0];
        renderobjs[sp->id]->translate[1] = sp->translate[1];
    }
    if (sp->change & EV_SP_ROTATE) {
        renderobjs[sp->id]->rotate = sp->rotate;
    }
    if (sp->change & EV_SP_LAYER) {
        renderobjs[sp->id]->layer = sp->layer;
    }
    free(data1);
}
void eve_setproperty(ev_esp change, unsigned int id, char visibility, float scale, float translatex, float translatey, float rotate, int layer) {
    ev_property * obj = (ev_property *) malloc(sizeof(ev_property));
    obj->id           = id;
    obj->change       = change;
    obj->visibility   = visibility;
    obj->scale        = scale;
    obj->translate[0] = translatex;
    obj->translate[1] = translatey;
    obj->rotate       = rotate;
    obj->layer        = layer;
    ev_emit(EV_SETPROPERTY, obj, NULL);
}

/* SET PROPERTIES EVENT */
void ev_destroyobj(void* data1, void* data2) {
    static char *FUN = "ev_destroyobj()";
    /* destroy the object n */
    int n = *((int *) data1);
    free(data1);

    if (n >= allocatedRO || renderobjs[n] == NULL)
        return;

    allocatedRO--;

    totalpoints -= renderobjs[n]->obj->ibos;

    /* free the buffers */
    glDeleteBuffers(1, &(renderobjs[n]->obj->vbo));
    glDeleteBuffers(1, &(renderobjs[n]->obj->cbo));
    glDeleteBuffers(1, &(renderobjs[n]->obj->ibo));
    glDeleteVertexArrays(1, &(renderobjs[n]->obj->vao));

    free(renderobjs[n]->obj);
    free(renderobjs[n]);
    renderobjs[n] = NULL;
    if (verbosity > 0) INFOMF("I just destroyed an object (id: %d). %d still in use", n, allocatedRO);
}

void eve_destroyobj(int n) {
    int * pn = (int *) malloc(sizeof(int));
    *pn = n;
    ev_emit(EV_DESTROYOBJ, pn, NULL);
}


typedef struct ev_updateobject {
    ev_eup              change;
    GLenum              mode;
    GLfloat             *vertices, *colours;
    GLuint              *indices;
    int                 size, sizei;
    sem_t               sem;
} ev_updateobject;

void ev_updatebuffer(void* data1, void* data2) {
    static char *FUN = "ev_updatebuffer()";

    int n = *((int *) data2);
    if (renderobjs[n] == NULL) {
        if (verbosity > -1) WARNMF("Trying to update a non-existing object id: %d. Ignoring that request", n);
        return;
    }
    ev_updateobject * obj = (ev_updateobject *) data1;

    object * bo = renderobjs[n]->obj;


    glBindVertexArray(bo->vao);
    if (obj->change & EV_UP_VERTEX) { /* update verteices */
        glBindBuffer(GL_ARRAY_BUFFER, bo->vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, obj->size * 2 * sizeof(GLfloat), obj->vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    if (obj->change & EV_UP_COLOUR) { /* update colours */
        glBindBuffer(GL_ARRAY_BUFFER, bo->cbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, obj->size * 3 * sizeof(GLfloat), obj->colours);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glBindVertexArray(0);


    if (verbosity > 0) INFOMF("Object %d successfully updated", n);

    if (sem_post(&obj->sem) != 0) {
        ERRMF("Couldn't release the semaphore while updating an object; it's time to a deadlock!");
        exit(-1);
    }
    free(data1);
}
void eve_updatebuffer(ev_eup change, int id, GLfloat * vertices, GLfloat * colours, int size) {
    static char *FUN = "eve_updatecolour()";
    ev_updateobject * obj = (ev_updateobject *) malloc(sizeof(ev_updateobject));

    if (sem_init(&obj->sem, 0, 1) != 0) {
        if (verbosity > -1) WARNMF("Couldn't create a semaphore. I'll forget this update, sorry");
        return;
    }

    if (sem_trywait(&obj->sem) == 0) {
        obj->change     = change;
        obj->mode       = 0;
        obj->vertices   = vertices;
        obj->colours    = colours;
        obj->indices    = NULL;
        obj->size       = size;
        obj->sizei      = 0;
        ev_emit(EV_UPDATEBUFFER, obj, &id);

        if (sem_wait(&obj->sem) != 0) {
            ERRMF("Couldn't wait for the object to be updated. Unspecified behaviour from now on");
            exit(-1);
        }
    } else
        if (verbosity > -1) WARNMF("I can't update an object without locking a mutex. I'll forget this update, sorry");

    sem_destroy(&obj->sem);
}


/* * * * * * * * * * * * * * * * * *
 *  THE TIMERS
 * * * * * * * * * * * * * * * * * */
Uint32 timer_countfps(Uint32 interval, void* param) {
    char title[50];
    sprintf(title, "%s (%d fps)", WINTITLE, fpscount);
    fpscount = 0;
    SDL_SetWindowTitle(window, title);
    return interval;
}



/* * * * * * * * * * * * * * * * * *
 *  THE RENDER
 * * * * * * * * * * * * * * * * * */
float   a     = 0;
long    last  = 0;
float   delta = 0.0;

void renderloop() {
    long now = SDL_GetTicks();
    if (now > last) {
	    delta = ((float)(now - last)) / 1000;
	    last = now;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport (0, 0, curWidth, curHeight);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* zoom and translate smoothner */
    double animv    = delta * 20.0 * MV_ANIMSPEED;
    float  stopanim = 0.1;
    double dx = (ctx - cx),
           dy = (cty - cy),
           dz = (ctz - cz);

    if (fabs(dz * animv) > fabs(dz) || fabs(dz * animv) < stopanim) cz = ctz;
    else                                                            cz += dz * animv;


    if (fabs(dx * animv) > fabs(dx) || fabs(dx * animv * cz) < stopanim) cx = ctx;
    else                                                                 cx += dx * animv;
    if (fabs(dy * animv) > fabs(dy) || fabs(dy * animv * cz) < stopanim) cy = cty;
    else                                                                 cy += dy * animv;

     glOrtho((-curWidth/2.0)   / cz - cx,
             (curWidth/2.0)    / cz - cx,
             (-curHeight/2.0)  / cz - cy,
             (curHeight/2.0)   / cz - cy,
             -10.0, 10.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glRotatef(a, 0, 0, 1);/**/
    a += eegg;
    
    int i, j;
    for (j = 0; j < MAXLAYERS; j++) {
        for (i = 0; i < MV_MAXOBJECTS; i++) {
            if (!renderobjs[i])
                continue;
            if (MAXLAYERS > 1 && renderobjs[i]->layer != j)
                continue;
            if (!renderobjs[i]->enable)
                continue;
            render_ro(renderobjs[i]);
        }
    }
    
    /* SELECTION BOX */
    if (sb_isshow) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-curWidth/2.0, curWidth/2.0, -curHeight/2.0, curHeight/2.0, -10.0, 10.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    
        glTranslatef(-curWidth/2.0 + sb_ix, curHeight/2.0 - sb_iy, 0.0);
        glScalef((sb_fx - sb_ix), (sb_iy - sb_fy) , 1.0);
        
        render_ro(ro_selbox);
    }
    
    SDL_GL_SwapWindow(window);
    ev_emit(EV_RENDER, NULL, NULL);
    fpscount++;
}

void render_ro(renderobj * ro) {
    glUseProgram(ro->prog->shaderprogram);
    glBindVertexArray(ro->obj->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ro->obj->ibo);

    if (ro->obj->width > 0) {
        glPointSize(ro->obj->width);
        glLineWidth(ro->obj->width);
    }
    else {
        glPointSize(1.0);
        glLineWidth(1.0);
    }

    glPushMatrix();
        glScalef(ro->scale, ro->scale, 1.0);
        glRotatef(ro->rotate, 0.0f, 0.0f, 1.0f);
        glTranslatef(ro->translate[0], ro->translate[1], 0.0);
        glDrawElements(ro->obj->mode, ro->obj->ibos, GL_UNSIGNED_INT, NULL);
    glPopMatrix();

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

int add_ro(renderobj * ro) {
    int i;
    for (i = 0; i < MV_MAXOBJECTS; i++) {
        if (renderobjs[i] == NULL) {
            renderobjs[i] = ro;
            allocatedRO++;
            return  i;
        }
    }
    return -1;
}

program *  new_program(const char * vertexsource, const char * fragmentsource) {
    static char *FUN = "new_program()";
    GLuint vertexshader = glCreateShader(GL_VERTEX_SHADER);

    /* VERTEX SHADER */
    glShaderSource(vertexshader, 1, (const GLchar**)&vertexsource, 0);
    glCompileShader(vertexshader);


    int IsCompiled_VS, maxLength;
    glGetShaderiv(vertexshader, GL_COMPILE_STATUS, &IsCompiled_VS);
    if(!IsCompiled_VS) {
       glGetShaderiv(vertexshader, GL_INFO_LOG_LENGTH, &maxLength);
       char * vertexInfoLog = (char *)malloc(maxLength);
       glGetShaderInfoLog(vertexshader, maxLength, &maxLength, vertexInfoLog);
       ERRMF("Look at this shader/vertex error: %s", vertexInfoLog);
       free(vertexInfoLog);
       return NULL;
    }

    /* FRAGMENT SHADER */
    GLuint fragmentshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentshader, 1, (const GLchar**)&fragmentsource, 0);
    glCompileShader(fragmentshader);

    int IsCompiled_FS;
    glGetShaderiv(fragmentshader, GL_COMPILE_STATUS, &IsCompiled_FS);
    if(!IsCompiled_FS) {
       glGetShaderiv(fragmentshader, GL_INFO_LOG_LENGTH, &maxLength);
       char * fragmentInfoLog = (char *)malloc(maxLength);
       glGetShaderInfoLog(fragmentshader, maxLength, &maxLength, fragmentInfoLog);
       ERRMF("Look at this shader/fragment error: %s", fragmentInfoLog);
       free(fragmentInfoLog);
       return NULL;
    }

    /* PROGRAM */
    GLuint shaderprogram = glCreateProgram();

    glAttachShader(shaderprogram, vertexshader);
    glAttachShader(shaderprogram, fragmentshader);

    glBindAttribLocation(shaderprogram, 0, "in_Position");
    glBindAttribLocation(shaderprogram, 1, "in_Color");

    glLinkProgram(shaderprogram);

    int IsLinked;
    glGetProgramiv(shaderprogram, GL_LINK_STATUS, (int *)&IsLinked);
    if(!IsLinked) {
        glGetProgramiv(shaderprogram, GL_INFO_LOG_LENGTH, &maxLength);
        char * shaderProgramInfoLog = (char *)malloc(maxLength);
        glGetProgramInfoLog(shaderprogram, maxLength, &maxLength, shaderProgramInfoLog);
        ERRMF("Look at this shader/program error: %s", shaderProgramInfoLog);
        free(shaderProgramInfoLog);
        return NULL;
    }

    program * prog       = (program *) malloc(sizeof(program));
    prog->shaderprogram  = shaderprogram;
    prog->vertexshader   = vertexshader;
    prog->fragmentshader = fragmentshader;

    return prog;
}

program *  new_program_default() {
    const char * vs = "\
        #version 120\n\
        attribute vec4 in_Color;\n\
        void main() {\n\
	        gl_FrontColor = in_Color;\n\
	        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n\
        }\n\
        ";
    const char * fs = "\
        #version 120\n\
        void main() {\n\
	        gl_FragColor = gl_Color;\n\
        }\n\
        ";
    return new_program(vs, fs);
}

object * new_buffer(GLenum mode, GLfloat * vertices, GLfloat * colours, int size, int countv, int countc, GLuint * indices, int counti) {
    GLuint vao, vbo[3];

    glGenVertexArrays(1, &vao);
    glGenBuffers(3, vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, size * countv * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, countv, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, size * countc * sizeof(GLfloat), colours, GL_STATIC_DRAW);
    glVertexAttribPointer(1, countc, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, counti * sizeof(GLuint), indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    object * obj = (object*) malloc(sizeof(object));

    obj->mode = mode;
    obj->vao = vao;
    obj->vbo = vbo[0];
    obj->cbo = vbo[1];
    obj->ibo = vbo[2];
    obj->ibos = counti;
    obj->width = -1.0;
    
    return obj;
}

/* SELECTION BOX */
renderobj * new_ro_selbox() {
    renderobj * ro = (renderobj *) malloc(sizeof(renderobj));
    
    GLfloat vertices[] = { 0.0, 0.0, 1.0,   1.0, 0.0, 1.0,   1.0, 1.0, 1.0,   0.0, 1.0, 1.0 };
    GLuint  indices[] = { 0, 1, 2, 3 };
    
    GLfloat * colours = (GLfloat *) malloc(sizeof(GLfloat) * 4 * 4);
    int i;
    for (i = 0; i < 4 * 4; i += 4) {
        colours[i]   = 0.0;
        colours[i+1] = 0.71;
        colours[i+2] = 0.82;
        colours[i+3] = 0.6;
    }
    
    object * tobj = new_buffer(GL_QUADS, vertices, colours, 4, 3, 4, indices, 4);
    ro->obj = tobj;
    ro->prog = defprog[0];
    
    ro->scale        = 1.0;
    ro->translate[0] = 0.0;
    ro->translate[1] = 0.0;
    ro->rotate       = 0.0;
    free(colours);
    ro->enable = 1;

    return ro;
}

void sb_dozoom() {
    int w = (sb_fx - sb_ix),
        h = (sb_fy - sb_iy);
    if (w == 0 || h == 00) return;

    if (curWidth / (float)abs(w) < curHeight / (float)abs(h))
        ctz *= curWidth / (float)abs(w);
    else
        ctz *= curHeight / (float)abs(h);

    w = curWidth/2 - (sb_ix + w / 2);
    h = curHeight/2 -(sb_iy + h / 2);

    ctx += w / cz;
    cty -= h / cz;
}


/* * * * * * * * * * * * * * * * * *
 *  THE ADDNERS AND STD COLOURS, PUBLIC API
 * * * * * * * * * * * * * * * * * */
float mv_white[]    = { 1.0, 1.0, 1.0 };
float mv_gray[]     = { 0.5, 0.5, 0.5 };
float mv_dgray[]    = { 0.2, 0.2, 0.2 };
float mv_red[]      = { 1.0, 0.0, 0.0 };
float mv_green[]    = { 0.0, 1.0, 0.0 };
float mv_blue[]     = { 0.0, 0.0, 1.0 };
float mv_pink[]     = { 1.0, 0.0, 1.0 };
float mv_yellow[]   = { 1.0, 1.0, 0.0 };
float mv_cyan[]     = { 0.0, 1.0, 1.0 };

void mv_show(int id) {
    if (!running) return;
    eve_setproperty(EV_SP_VISIBILITY, id, 1, 0, 0, 0, 0, 0);
}
void mv_hide(int id) {
    if (!running) return;
    eve_setproperty(EV_SP_VISIBILITY, id, 0, 0, 0, 0, 0, 0);
}
void mv_destroy(int id) {
    if (!running) return;
    eve_destroyobj(id);
}
void mv_setscale(int id, float scale) {
    if (!running) return;
    eve_setproperty(EV_SP_SCALE, id, 0, scale, 0, 0, 0, 0);
}
void mv_settranslate(int id, float x, float y) {
    if (!running) return;
    eve_setproperty(EV_SP_TRANSLATE, id, 0, 0, x, y, 0, 0);
}
void mv_setrotate(int id, float angle) {
    if (!running) return;
    eve_setproperty(EV_SP_ROTATE, id, 0, 0, 0, 0, angle, 0);
}
void mv_setlayer(int id, int layer) {
    if (!running) return;
    eve_setproperty(EV_SP_LAYER, id, 0, 0, 0, 0, 0, layer);
}
void mv_updatecolourarray(int id, float * colour, int size) {  //printf("WILL VERTEX %d\n", running);
    if (!running) return;
    int i;

    GLfloat * colours = (GLfloat *) malloc(sizeof(GLfloat) * size * 3);
    for (i = 0; i < size * 3; i += 3) {
        colours[i]   = colour[i];
        colours[i+1] = colour[i+1];
        colours[i+2] = colour[i+2];
    }
    eve_updatebuffer(EV_UP_COLOUR, id, NULL, colours, size);
    free(colours);
}

void mv_updatevertexarray(int id, float * vertices, int size) {  //printf("WILL VERTEX %d\n", running);
    if (!running) return;
    int i;

    GLfloat * v = (GLfloat *) malloc(sizeof(GLfloat) * size * 2);
    for (i = 0; i < size * 2; i += 2) {
        v[i]   = vertices[i];
        v[i+1] = vertices[i+1];
    }
    eve_updatebuffer(EV_UP_VERTEX, id, v, NULL, size);
    free(v);
}








int mv_add(MVprimitive primitive, float * vertices, unsigned int countv, unsigned int * indices, unsigned int counti, float * colour, float width, int layer, int * id) {
    static char *FUN = "mv_add()";
    if (!running) {
        if (verbosity > -1) WARNMF("The meshviwer should be "BOLD"running"RESET" so a new object can be drawn");
        return 0;
    }
    GLenum mode;

    MVprimitive p = primitive & ((1 << 5) -1);

    /* Set the GL primitive */
    switch (p) {
        case MV_2D_TRIANGLES:           mode = GL_TRIANGLES; break;
        case MV_2D_LINES:               mode = GL_LINES;     break;
        case MV_2D_POINTS:              mode = GL_POINTS;    break;

        case MV_2D_TRIANGLES_AS_LINES:  mode = GL_LINES;     break;
        case MV_2D_TRIANGLES_AS_POINTS: mode = GL_POINTS;    break;
        default:
            if (verbosity > -1) WARNMF("Have you read the documentation? I can't draw that primitive...");
            return -1;/* NOT IMPLEMENT / BAD ARGUMENT */
    }

    /* compute indices array */
    int freeid = 0;
    if (indices == NULL) {
        counti = countv;
        indices = (unsigned int *) malloc(sizeof(unsigned int) * counti);
        int i;
        for (i = 0; i < counti; i++)
            indices[i] = i;
        freeid = 1;
    }

    /* Transform the indices for minimize the number of drawn elements */
    if (p == MV_2D_TRIANGLES_AS_LINES)
        transform_triangles_in_lines(&indices, &counti, countv);
    if (p == MV_2D_TRIANGLES_AS_POINTS)
        transform_triangles_in_points(&indices, &counti, countv);


    GLfloat * colours = (GLfloat *) malloc(sizeof(GLfloat) * countv * 3);
    if (primitive & MV_USE_COLOUR_ARRAY) {
        int i;
        for (i = 0; i < countv * 3; i += 3) {
            colours[i]   = colour[i];
            colours[i+1] = colour[i+1];
            colours[i+2] = colour[i+2];
        }
    } else { /* Build a colour array for each vertice */
        int i;
        for (i = 0; i < countv * 3; i += 3) {
            colours[i]   = colour[0];
            colours[i+1] = colour[1];
            colours[i+2] = colour[2];
        }
    }

    eve_createobj(mode, vertices, colours, countv, 2, 3, indices, counti, width, layer, id);
    if (primitive & MV_USE_COLOUR_ARRAY)
        free(colours);
    if (freeid)
        free(indices);
    return 0;
}

int mv_add_plot(MVprimitive primitive, double (*f)(double x), double xmin, double xmax, double step, float * colour, float width, int layer, int * id) {
    static char *FUN = "mv_add_plot()";
    if (!running) return 0;
    switch (primitive) {
        case MV_2D_LINES:   break;
        case MV_2D_POINTS:  break;
        default:
            if (verbosity > -1) WARNMF("How whould I plot like that? That's a invalid primitive, I'll assume MV_2D_LINES");
            primitive = MV_2D_LINES;
    }
    
    int steps = (int) ((xmax - xmin) / step);
    
    if (primitive == MV_2D_POINTS && steps < 1) {
        if (verbosity > -1) WARNMF("Trying to plot %d points. Won't work", steps);
        return -1;
    } else
    if (primitive == MV_2D_LINES && steps < 2) {
        if (verbosity > -1) WARNMF("Trying to plot %d points as lines. Won't work", steps);
        return -1;
    }


    float *vertices = (float *) malloc(sizeof(float) * steps*2);
    unsigned int *indices = NULL;
    int i, counti = 0;
    for (i = 0; i < steps; i++) {
        vertices[i*2]   = xmin + i * step;
        vertices[i*2+1] = (float) f(xmin + i * step);
    }

    if (primitive == MV_2D_POINTS) {
        indices = (unsigned int *) malloc(sizeof(unsigned int) * steps);
        for (i = 0; i < steps; i++)
            indices[i] = i;
        counti = steps;
    }
    else if (primitive == MV_2D_LINES) {
        counti = (steps-1)*2;
        indices = (unsigned int *) malloc(sizeof(unsigned int) * counti);
        for (i = 0; i < steps - 1; i++) {
            indices[i*2]   = i;
            indices[i*2+1] = i+1;
        }
    }

    return mv_add(primitive, vertices, steps, indices, counti, colour, width, layer, id);
}

void transform_triangles_in_lines(GLuint ** pindices, unsigned int * pcounti, int countv) {
    /* transform triangles in edges */
    GLuint * indices = *pindices;
    int      counti  = *pcounti;

    typedef struct item {
        unsigned int id;
        struct item * n;
    } item;
    item ** items = (item **) calloc(countv, sizeof(item *));
    int ttitems = 0, i;
    for (i = 0; i < counti; i+=3) {

        int a,b;
        item ** t;

        a = indices[i]; b = indices[i+1];
        if (a > b) { int c = a; a = b; b = c; }
        for (t = &items[a];  ; t = &(*t)->n) {
            if (*t == NULL) {
                *t = (item *) malloc(sizeof(item));
                (*t)->id = b;
                (*t)->n = NULL;
                ttitems++; break;
            } else if ((*t)->id == b) break;
        }

        a = indices[i+1]; b = indices[i+2];
        if (a > b) { int c = a; a = b; b = c; }
        for (t = &items[a];  ; t = &(*t)->n) {
            if (*t == NULL) {
                *t = (item *) malloc(sizeof(item));
                (*t)->id = b;
                (*t)->n = NULL;
                ttitems++; break;
            } else if ((*t)->id == b) break;
        }

        a = indices[i+2]; b = indices[i];
        if (a > b) { int c = a; a = b; b = c; }
        for (t = &items[a];  ; t = &(*t)->n) {
            if (*t == NULL) {
                *t = (item *) malloc(sizeof(item));
                (*t)->id = b;
                (*t)->n = NULL;
                ttitems++; break;
            } else if ((*t)->id == b) break;
        }
    }

    GLuint * tindices = (GLuint *) malloc(sizeof(GLuint) * ttitems * 2);
    int p = 0;
    for (i = 0; i < countv; i++) {
        item * t;
        for (t = items[i]; t != NULL ;) {
            tindices[p++] = i;
            tindices[p++] = t->id;

            item * tt = t;
            t = t->n;
            free (tt);
        }
    }

    free(items);
    free(indices);

    *pindices = tindices;
    *pcounti  = ttitems * 2;
}

void transform_triangles_in_points(GLuint ** pindices, unsigned int * pcounti, int countv) {
    GLuint * indices = *pindices;
    int      counti  = *pcounti;

    char * vmask = (char *) calloc(countv, sizeof(char));
    int i;

    for (i = 0; i < counti; i++)
        vmask[indices[i]] = 1;

    int countp = 0;
    for (i = 0; i < countv; i++)
        if (vmask[i]) countp++;

    GLuint * nindices = (GLuint *) malloc(sizeof(GLuint) * countp);

    for (i = 0; i < countp; i++)
        nindices[i] = i;

    free(vmask);
    free(indices);

    *pindices = nindices;
    *pcounti  = countp;
}


