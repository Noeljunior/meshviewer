/* STDLIBS */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <float.h>
#include <unistd.h>

#include "meshviewer.h"

/* Read n lines each containing two floats */
float *        readn_FF(FILE * f, unsigned int *count);

/* Read n lines each containing two floats and one ignored int */
float *        readn_FFi(FILE * f, unsigned int *count);

/* Read n lines each containing two unsigned ints */
unsigned int * readn_UU(FILE * f, unsigned int *count);

/* Read n lines each containing three unsigned ints */
unsigned int * readn_UUU(FILE * f, unsigned int *count);

/* finds the x and y mins and maxs */
void find_minmax_xyf(float * values, int size, float * minx, float * miny, float * maxx, float * maxy);


void read_mesh(const char * path);      /* Reads gustavo's mesh */
void read_mesh_pt(const char * path);   /* Reads the bernardo's portugal mesh */
void read_mesh_juan(const char * path); /* Reads juan's mesh */

int main (int argc, char ** argv) {
    int a;


    mv_start();

    /*read_mesh("mesh/mesh_1338.dat");/**/
    /*read_mesh("mesh/mesh_21573.dat");/**/

    read_mesh_pt("mesh/quadtree_portugal.dat");/**/

    /*read_mesh_juan("mesh/naca0012_2261VERT.mesh");/**/
    /*read_mesh_juan("mesh/naca0012_4630VERT.mesh");/**/
    /*read_mesh_juan("mesh/naca0012_9328VERT.mesh");/**/
    /*read_mesh_juan("mesh/naca_4607VERT.mesh");/**/


    scanf("%d", &a); /* avoid going out */
    mv_stop();
    return 0;
}


/* Read n lines each containing two floats */
float * readn_FF(FILE * f, unsigned int *count) {
    int i, p;
    fscanf(f, "%d", count);
    float * values = (float *) malloc(sizeof(float) * *count * 2);
    for (i = 0, p = 0; i < *count; i++)
        fscanf(f, "%f %f", values + p++, values + p++);
    return values;
}
/* Read n lines each containing two floats and one ignored int */
float * readn_FFi(FILE * f, unsigned int *count) {
    int i, p, trash;
    fscanf(f, "%d", count);
    float * values = (float *) malloc(sizeof(float) * *count * 2);
    for (i = 0, p = 0; i < *count; i++)
        fscanf(f, "%f %f %d", values + p++, values + p++, &trash);
    return values;
}

/* Read n lines each containing two unsigned ints */
unsigned int * readn_UU(FILE * f, unsigned int *count) {
    int i, p;
    fscanf(f, "%d", count);*count *= 2;
    unsigned int * values = (unsigned int *) malloc(sizeof(unsigned int) * *count);
    for (i = 0, p = 0; i < *count/2; i++)
        fscanf(f, "%d %d", values + p++, values + p++);
    return values;
}
/* Read n lines each containing three unsigned ints */
unsigned int * readn_UUU(FILE * f, unsigned int *count) {
    int i, p;
    fscanf(f, "%d", count);*count *= 3;
    unsigned int * values = (unsigned int *) malloc(sizeof(unsigned int) * *count);
    for (i = 0, p = 0; i < *count/3; i++)
        fscanf(f, "%d %d %d", values + p++, values + p++, values + p++);
    return values;
}

void find_minmax_xyf(float * values, int size, float * minx, float * miny, float * maxx, float * maxy) {
    int x, y;
    *maxx = FLT_MIN; *maxy = FLT_MIN,
    *minx = FLT_MAX; *miny = FLT_MAX;
    for (x = 0, y = 1; x < size; x += 2, y += 2) {
        if      (values[x] > *maxx) *maxx = values[x];
        if      (values[x] < *minx) *minx = values[x];

        if      (values[y] > *maxy) *maxy = values[y];
        if      (values[y] < *miny) *miny = values[y];

    }
}


/* Reads gustavo's mesh */
void read_mesh(const char * path) {
    FILE * f;
    float * vertices, *cvertices;
    unsigned int * indices, *cindices;
    unsigned int countv, counti;
    float maxx, maxy, minx, miny;
    int id;

    f = fopen(path, "r");
    if (f == NULL) return;

    vertices = readn_FFi(f, &countv);
    indices  = readn_UUU(f, &counti);
    find_minmax_xyf(vertices, countv, &minx, &miny, &maxx, &maxy);

    float scale      = 1.0 / ((maxx - minx) > (maxy - miny) ? (maxx - minx) : (maxy - miny)),
          translatex = -minx - (maxx-minx)/2.0,
          translatey = -miny - (maxy-miny)/2.0;



    /**/cvertices = (float *) malloc(sizeof(float) * countv * 2);
    memcpy(cvertices, vertices, sizeof(float) * countv * 2);
    cindices = (unsigned int *) malloc(sizeof(unsigned int) * counti);
    memcpy(cindices, indices, sizeof(unsigned int) * counti);

    mv_add(MV_2D_TRIANGLES, cvertices, countv, cindices, counti, mv_gray, 1, &id);
    sleep(1); // TODO this is a workaround by now
    mv_setscale    (id, scale);
    mv_settranslate(id, translatex, translatey);/**/



    /**/cvertices = (float *) malloc(sizeof(float) * countv * 2);
    memcpy(cvertices, vertices, sizeof(float) * countv * 2);
    cindices = (unsigned int *) malloc(sizeof(unsigned int) * counti);
    memcpy(cindices, indices, sizeof(unsigned int) * counti);

    mv_add(MV_2D_TRIANGLES_AS_LINES, cvertices, countv, cindices, counti, mv_white, 1, &id);
    sleep(1); // TODO this is a workaround by now
    mv_setscale    (id, scale);
    mv_settranslate(id, translatex, translatey);/**/



    /**/cvertices = (float *) malloc(sizeof(float) * countv * 2);
    memcpy(cvertices, vertices, sizeof(float) * countv * 2);
    cindices = (unsigned int *) malloc(sizeof(unsigned int) * counti);
    memcpy(cindices, indices, sizeof(unsigned int) * counti);

    mv_add(MV_2D_TRIANGLES_AS_POINTS, cvertices, countv, cindices, counti, mv_red, 3, &id);
    sleep(1); // TODO this is a workaround by now
    mv_setscale    (id, scale);
    mv_settranslate(id, translatex, translatey);/**/


    free(vertices);
    free(indices);
}

/* Reads the bernardo's portugal mesh */
void read_mesh_pt(const char * path) {
    FILE * f;
    float * vertices;
    unsigned int * indices;
    unsigned int countv, counti;
    float maxx, maxy, minx, miny;
    int ids[5];

    f = fopen(path, "r");
    if (f == NULL) return;

    vertices = readn_FF(f, &countv);
    indices  = readn_UU(f, &counti);
    find_minmax_xyf(vertices, countv, &minx, &miny, &maxx, &maxy);
    mv_add(MV_2D_LINES, vertices, countv, indices, counti, mv_gray, 2, ids + 0);

    vertices = readn_FF(f, &countv);
    indices  = readn_UU(f, &counti);
    mv_add(MV_2D_LINES, vertices, countv, indices, counti, mv_yellow, 1, ids + 1);

    vertices = readn_FF(f, &countv);
    indices  = readn_UU(f, &counti);
    mv_add(MV_2D_LINES, vertices, countv, indices, counti, mv_pink, 1, ids + 2);

    vertices = readn_FF(f, &countv);
    indices  = readn_UU(f, &counti);
    mv_add(MV_2D_LINES, vertices, countv, indices, counti, mv_green, 1, ids + 3);

    vertices = readn_FF(f, &countv);
    indices  = readn_UU(f, &counti);
    mv_add(MV_2D_LINES, vertices, countv, indices, counti, mv_red, 2, ids + 4);


    sleep(1); /* TODO this is a workaround by now */
    float scale      = 1.0 / ((maxx - minx) > (maxy - miny) ? (maxx - minx) : (maxy - miny)),
          translatex = -minx - (maxx-minx)/2.0,
          translatey = -miny - (maxy-miny)/2.0;

    mv_setscale    (ids[0], scale);
    mv_settranslate(ids[0], translatex, translatey);

    mv_setscale    (ids[1], scale);
    mv_settranslate(ids[1], translatex, translatey);

    mv_setscale    (ids[2], scale);
    mv_settranslate(ids[2], translatex, translatey);

    mv_setscale    (ids[3], scale);
    mv_settranslate(ids[3], translatex, translatey);

    mv_setscale    (ids[4], scale);
    mv_settranslate(ids[4], translatex, translatey);
}

/* Reads juan's mesh */
void read_mesh_juan(const char * path) {
    FILE * f;
    float * vertices;
    unsigned int * indices;
    unsigned int countv, counti;
    float maxx, maxy, minx, miny;
    int id;

    f = fopen(path, "r");
    if (f == NULL) return;

    char trash[1024];
    int  trashi, trashii, i;

    /* discard the trash */
    fscanf(f, "%s %d %s %d %s ", trash, &trashi, trash, &trashi, trash);

    /* number of vertices */
    fscanf(f, "%d", &countv);
    vertices = (float *) malloc(sizeof(float) * countv * 2);

    for (i = 0; i < countv; i++)            /* vertices */
        fscanf(f, "%f %f %d %d", vertices + i*2, vertices + i*2 + 1, &trashi, &trashi);
    
    /* edges / ignored by now */
    fscanf(f, "%s %d", trash, &trashi);
    for (i = 0; i < trashi; i++) fscanf(f, "%d %d %d", &trashii, &trashii, &trashii);
    
    fscanf(f, "%s %d", trash, &counti);   /* number of triangles */
    counti *= 3; 
    indices = (unsigned int *) malloc(sizeof(unsigned int) * counti);
    for (i = 0; i < counti/3; i++) {         /* triangles */
        fscanf(f, "%d %d %d %d", indices + i*3, indices + i*3 + 1, indices + i*3 + 2, &trashi);
        indices[i*3]--; indices[i*3+1]--; indices[i*3+2]--;
    }
    fscanf(f, "%s", trash);                       /* EOF */

    find_minmax_xyf(vertices, countv, &minx, &miny, &maxx, &maxy);
    mv_add(MV_2D_TRIANGLES_AS_LINES, vertices, countv, indices, counti, mv_yellow, 1, &id);

    sleep(1); /* TODO this is a workaround by now */
    float scale      = 1.0 / ((maxx - minx) > (maxy - miny) ? (maxx - minx) : (maxy - miny)),
          translatex = -minx - (maxx-minx)/2.0,
          translatey = -miny - (maxy-miny)/2.0;

    mv_setscale    (id, scale);
    mv_settranslate(id, translatex, translatey);
}



