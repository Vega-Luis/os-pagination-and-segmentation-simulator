#ifndef SHARED_H
#define SHARED_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

// Claves IPC
#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define LOG_FILE "bitacora.log"

// Límites
#define MAX_MEM 50
#define MAX_PROCS 100

// Estados de memoria
#define LIBRE 0
#define OCUPADO 1

// Esquemas
#define PAGINACION 1
#define SEGMENTACION 2

// Un espacio de memoria (página o espacio de segmento)
typedef struct
{
    int estado;   // LIBRE u OCUPADO
    pid_t pid;    // proceso que lo ocupa
    int segmento; // número de segmento (solo segmentación)
} Espacio;

// Memoria compartida completa
typedef struct
{
    Espacio memoria[MAX_MEM];
    int total_espacios;
    int esquema; // PAGINACION o SEGMENTACION

    // Para el Espía
    pid_t buscando; // PID del proceso buscando espacio ahora
    pid_t muertos[MAX_PROCS];
    int n_muertos;
    pid_t terminados[MAX_PROCS];
    int n_terminados;
} MemoriaCompartida;

// Union requerida por semctl
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#endif