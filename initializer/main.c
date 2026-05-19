#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../shared.h"

int main()
{
    int total;

    printf("=== Inicializador ===\n");
    printf("Cuantos espacios de memoria desea? (max %d): ", MAX_MEM);
    scanf("%d", &total);

    if (total < 1 || total > MAX_MEM)
    {
        fprintf(stderr, "Cantidad invalida. Debe ser entre 1 y %d.\n", MAX_MEM);
        exit(1);
    }

    // --- Crear memoria compartida ---
    int shmid = shmget(SHM_KEY, sizeof(MemoriaCompartida), IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("shmget");
        exit(1);
    }

    MemoriaCompartida *mem = (MemoriaCompartida *)shmat(shmid, NULL, 0);
    if (mem == (void *)-1)
    {
        perror("shmat");
        exit(1);
    }

    // --- Inicializar memoria ---
    memset(mem, 0, sizeof(MemoriaCompartida));
    mem->total_espacios = total;
    mem->esquema = 0; // lo define el productor
    mem->buscando = 0;
    mem->n_muertos = 0;
    mem->n_terminados = 0;
    mem->shutdown = 0;
    mem->n_locked = 0;

    for (int i = 0; i < total; i++)
    {
        mem->memoria[i].estado = LIBRE;
        mem->memoria[i].pid = 0;
        mem->memoria[i].segmento = -1;
    }

    printf("Memoria compartida creada con %d espacios. (shmid=%d)\n", total, shmid);

    // --- Crear semaforo de memoria ---
    int semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid == -1)
    {
        perror("semget");
        exit(1);
    }

    union semun arg;
    arg.val = 1; // mutex: solo 1 proceso a la vez
    if (semctl(semid, 0, SETVAL, arg) == -1)
    {
        perror("semctl");
        exit(1);
    }

    printf("Semaforo creado. (semid=%d)\n", semid);


    sem_init(&mem->headers_sem, 1, 1);
    printf("Semaforo para headers inicializado.\n");

    // --- Crear archivo de bitacora ---
    FILE *log = fopen(LOG_FILE, "w");
    if (!log)
    {
        perror("fopen bitacora");
        exit(1);
    }
    fprintf(log, "=== Bitacora de eventos ===\n");
    fclose(log);

    printf("Bitacora creada: %s\n", LOG_FILE);
    printf("Inicializacion completa. El inicializador termina.\n");

    // Desconectarse (no destruir, los otros procesos la siguen usando)
    shmdt(mem);

    return 0;
}