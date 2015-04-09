/* The default window size */
#define MV_WINW         640
#define MV_WINH         480

/* Zoom factor with mouse wheel */
#define MV_ZOOMFACTOR   1.25
/* The default zoom */
#define MV_ZOOMDEFAULT  64

/* Maximum objects allowed simultaneously */
#define MV_MAXOBJECTS   32      /* TODO make it dynamic, later... */

typedef enum _MVprimitive {
    /* vertices: an unidimensional float array, each two consecutive represents a (x, y) coordinate
     *           float[] vertices = { x0, y0,  x1, y1,  x2, y2, ... }
     * countv  : number of coordinates, or number of elements in vertices array divided by two
     * indices : an unidimensional unsigned int array, each three consecutive forms a triangle
     *           usigned int indices = { i0, i1, i2, ... }
     * counti  : number os elements in indices array
     * colour  : a three element float array containing rgb values between 0 and 1
     * width   : not used
     */
    MV_2D_TRIANGLES   = 1, /* Draw filled triangles */

    /* vertices: an unidimensional float array, each two consecutive represents a (x, y) coordinate
     *           float[] vertices = { x0, y0,  x1, y1,  x2, y2, ... }
     * countv  : number of coordinates, or number of elements in vertices array divided by two
     * indices : an unidimensional unsigned int array, each twoo consecutive forms a line
     *           usigned int indices = { i0, i1, i2, ... }
     * counti  : number os elements in indices array
     * colour  : a three element float array containing rgb values between 0 and 1
     * width   : float width of the line in pixels if greater than 0, otherwise is set to 1
     */
    MV_2D_LINES       = 2, /* Draw lines */

    /* vertices: an unidimensional float array, each one represents a (x, y) coordinate
     *           float[] vertices = { x0, y0,  x1, y1,  x2, y2, ... }
     * countv  : number of coordinates, or number of elements in vertices array divided by two
     * indices : an unidimensional unsigned int array, each twoo consecutive forms a line
     *           usigned int indices = { i0, i1, i2, ... }
     * counti  : number os elements in indices array
     * colour  : a three element float array containing rgb values between 0 and 1
     * width   : float width of the line in pixels if greater than 0, otherwise is set to 1
     */
    MV_2D_POINTS      = 3,


    MV_2D_TRIANGLES_AS_LINES    = 4,


    MV_2D_TRIANGLES_AS_POINTS   = 5,
} MVprimitive;

void    mv_start();
void    mv_stop();

void    mv_init();
void    mv_draw();

void    mv_show(int id);
void    mv_hide(int id);

int     mv_add(MVprimitive primitive, GLfloat * vertices, unsigned int countv, GLuint * indices, unsigned int counti, float * colour, float width, int * id);

extern float mv_white[];
extern float mv_gray[];
extern float mv_dgray[];
extern float mv_red[];
extern float mv_green[];
extern float mv_blue[];
extern float mv_pink[];
extern float mv_yellow[];
extern float mv_cyan[];

