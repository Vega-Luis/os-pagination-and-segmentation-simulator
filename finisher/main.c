#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/shm.h>
#include <sys/sem.h>

#include "../shared.h"

void sem_wait_mem(int semid)
{
    struct sembuf op = {0, -1, 0};

    if (semop(semid, &op, 1) == -1) {
        perror("semop wait");
        exit(1);
    }
}

void sem_signal_mem(int semid)
{
    struct sembuf op = {0, 1, 0};

    if (semop(semid, &op, 1) == -1) {
        perror("semop signal");
        exit(1);
    }
}

int main() {
    int shmid = shmget(SHM_KEY, sizeof(MemoriaCompartida), 0666);

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

    printf("[Finisher] Activando shutdown...\n");

    sem_wait(&mem->headers_sem);
    mem->shutdown = 1;
    sem_post(&mem->headers_sem);

    sem_destroy(&mem->headers_sem);

    printf("[Finisher] Esperando procesos...\n");
    sleep(3);
    shmdt(mem);

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
    }
    else {
        printf("[Finisher] Shared memory eliminada.\n");
    }

    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl");
    }
    else {
        printf("[Finisher] Semaforo eliminado.\n");
    }

    printf("[Finisher] Limpieza completa.\n");

    return 0;
}