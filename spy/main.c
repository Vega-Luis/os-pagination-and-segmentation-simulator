#include <stdio.h>
#include <stdlib.h>

#include <sys/shm.h>
#include <sys/sem.h>

#include "../shared.h"

void sem_wait_mem(int semid)
{
    struct sembuf op = {0, -1, 0};
    semop(semid, &op, 1);
}

void sem_signal_mem(int semid)
{
    struct sembuf op = {0, 1, 0};
    semop(semid, &op, 1);
}

int main()
{
    int shmid = shmget(SHM_KEY,
                       sizeof(MemoriaCompartida),
                       0666);

    if (shmid == -1)
    {
        perror("shmget");
        return 1;
    }

    MemoriaCompartida *mem =
        (MemoriaCompartida *)shmat(shmid, NULL, 0);

    if (mem == (void *)-1)
    {
        perror("shmat");
        return 1;
    }

    int semid = semget(SEM_KEY, 1, 0666);

    if (semid == -1)
    {
        perror("semget");
        return 1;
    }

    sem_wait_mem(semid);

    printf("\n=== ESTADO DE MEMORIA ===\n\n");

    for (int i = 0; i < mem->total_espacios; i++)
    {
        Espacio e = mem->memoria[i];

        printf("Espacio[%02d] ", i);

        if (e.estado == LIBRE)
        {
            printf("LIBRE\n");
        }
        else
        {
            printf("OCUPADO | PID=%d", e.pid);

            if (mem->esquema == SEGMENTACION)
            {
                printf(" | SEG=%d", e.segmento);
            }

            printf("\n");
        }
    }

    printf("\n=== PROCESOS EN MEMORIA ===\n");

    pid_t vistos[MAX_PROCS];
    int n_vistos = 0;

    for (int i = 0; i < mem->total_espacios; i++)
    {
        if (mem->memoria[i].estado == OCUPADO)
        {
            pid_t pid = mem->memoria[i].pid;

            int repetido = 0;

            for (int j = 0; j < n_vistos; j++)
            {
                if (vistos[j] == pid)
                {
                    repetido = 1;
                    break;
                }
            }

            if (!repetido)
            {
                vistos[n_vistos++] = pid;
                printf("PID=%d\n", pid);
            }
        }
    }

    printf("\n=== BUSCANDO ESPACIO ===\n");

    if (mem->buscando != 0)
    {
        printf("PID=%d\n", mem->buscando);
    }
    else
    {
        printf("Ninguno\n");
    }

    printf("\n=== MUERTOS ===\n");

    if (mem->n_muertos == 0)
    {
        printf("Ninguno\n");
    }
    else
    {
        for (int i = 0; i < mem->n_muertos; i++)
        {
            printf("PID=%d\n",
                   mem->muertos[i]);
        }
    }

    printf("\n=== TERMINADOS ===\n");

    if (mem->n_terminados == 0)
    {
        printf("Ninguno\n");
    }
    else
    {
        for (int i = 0; i < mem->n_terminados; i++)
        {
            printf("PID=%d\n",
                   mem->terminados[i]);
        }
    }

    sem_signal_mem(semid);

    shmdt(mem);

    return 0;
}