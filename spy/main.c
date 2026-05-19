#include <stdio.h>
#include <stdlib.h>

#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>

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

void display_memory_status(MemoriaCompartida *mem)
{
    printf("\n------ Estado de la memoria ------\n");

    printf("%-11s %-12s %-10s %-10s\n",
            "Dirección",
            "Estado",
            "PID",
            "Info");

    for (int address = 0; address < mem->total_espacios; address++) {
        Espacio slot = mem->memoria[address];

        char pid[16] = "-";
        char info[32] = "-";

        if (slot.estado == OCUPADO) {
            sprintf(pid, "%d", slot.pid);

            if (mem->esquema == SEGMENTACION) {
                sprintf(info, "SEG %d", slot.segmento);
            }
        }

        printf("%02d         %-12s %-10s %-10s\n",
                address,
                slot.estado == LIBRE ? "LIBRE" : "OCUPADO",
                pid,
                info);
    }
}

void display_looking_for_memory(MemoriaCompartida *mem)
{
    printf("\n------ Proceso buscando memoria ------\n");
    printf("PID\n");
    if (mem->buscando != 0) {
        printf("%d\n", mem->buscando);
    }
}

void display_locked_processes(MemoriaCompartida *mem)
{
    printf("\n------ Procesos esperando region crititca ------\n");
    printf("PID\n");
    sem_wait(&mem->headers_sem);
    if (mem->n_locked != 0) {
        for (int i = 0; i < mem->n_locked; i++) {
            printf("%d\n", mem->locked[i]);
        }
    }
    sem_post(&mem->headers_sem);
}

void display_processes_in_memory(MemoriaCompartida *mem)
{
    printf("\n------ Procesos en memoria ------\n");
    printf("PID\n");

    pid_t vistos[MAX_PROCS];
    int n_vistos = 0;

    for (int i = 0; i < mem->total_espacios; i++) {
        if (mem->memoria[i].estado == OCUPADO) {
            pid_t pid = mem->memoria[i].pid;

            int repetido = 0;

            for (int j = 0; j < n_vistos; j++) {
                if (vistos[j] == pid) {
                    repetido = 1;
                    break;
                }
            }

            if (!repetido) {
                vistos[n_vistos++] = pid;
                printf("%d\n", pid);
            }
        }
    }
}

void display_dead_processes(MemoriaCompartida *mem)
{
    printf("\n------ Procesos muertos ------\n");
    printf("PID\n");
    for (int i = 0; i < mem->n_muertos; i++) {
        printf("%d\n", mem->muertos[i]);
    }
}

void display_terminated_processes(MemoriaCompartida *mem)
{
    printf("\n------ Procesos terminados ------\n");
    printf("PID\n");
    for (int i = 0; i < mem->n_terminados; i++) {
        printf("%d\n", mem->terminados[i]);

    }
}

int main()
{
    int shmid = shmget(SHM_KEY,
                       sizeof(MemoriaCompartida),
                       0666);

    if (shmid == -1) {
        perror("shmget");
        return 1;
    }

    MemoriaCompartida *mem =
        (MemoriaCompartida *)shmat(shmid, NULL, 0);

    if (mem == (void *)-1) {
        perror("shmat");
        return 1;
    }

    int semid = semget(SEM_KEY, 1, 0666);

    if (semid == -1) {
        perror("semget");
        return 1;
    }

    display_memory_status(mem);
    display_processes_in_memory(mem);
    display_looking_for_memory(mem);
    display_locked_processes(mem);
    display_dead_processes(mem);
    display_terminated_processes(mem);

    shmdt(mem);

    return 0;
}