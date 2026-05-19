#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/syscall.h>
#include "../shared.h"

// Variables globales
int shmid, semid;
MemoriaCompartida *mem;

// ─── Semáforo: P (wait) y V (signal) ───
void sem_wait_mem()
{
    struct sembuf op = {0, -1, 0};
    semop(semid, &op, 1);
}

void sem_signal_mem()
{
    struct sembuf op = {0, 1, 0};
    semop(semid, &op, 1);
}

// ─── Escribir en bitácora ───
void escribir_bitacora(pid_t pid, const char *accion, int espacio, int segmento)
{
    FILE *log = fopen(LOG_FILE, "a");
    if (!log)
        return;

    time_t now = time(NULL);
    char *t = ctime(&now);
    t[strlen(t) - 1] = '\0'; // quitar el \n

    if (segmento >= 0)
        fprintf(log, "[%s] PID=%d | %s | espacio=%d | segmento=%d\n", t, pid, accion, espacio, segmento);
    else
        fprintf(log, "[%s] PID=%d | %s | espacio=%d\n", t, pid, accion, espacio);

    fclose(log);
}

// ─── Parámetros por hilo ───
typedef struct
{
    int esquema;
    int cantidad; // páginas o segmentos
    int tam_seg;  // espacios por segmento (solo segmentación)
    int tiempo;   // segundos de sleep
} ParamsHilo;

void add_locked_process(MemoriaCompartida *mem, pid_t pid)
{
    sem_wait(&mem->headers_sem);

    if (mem->n_locked < MAX_PROCS)
    {
        mem->locked[mem->n_locked++] = pid;
    }

    sem_post(&mem->headers_sem);
}

void remove_locked_process(MemoriaCompartida *mem, pid_t pid)
{
    sleep(10);
    sem_wait(&mem->headers_sem);
    printf("Removiendo PID=%d de locked...\n", pid);
    for (int i = 0; i < mem->n_locked; i++) {
        printf("Locked[%d] = %d\n", i, mem->locked[i]);
    }

    for (int i = 0; i < mem->n_locked; i++)
    {
        if (mem->locked[i] == pid)
        {
            mem->locked[i] =
                mem->locked[mem->n_locked - 1];

            mem->n_locked--;

            break;
        }
    }

    sem_post(&mem->headers_sem);
}

// ─── Lógica de cada proceso ───
void *proceso(void *arg)
{
    ParamsHilo *p = (ParamsHilo *)arg;
    pid_t pid = syscall(SYS_gettid);
    pthread_t tid = pthread_self();

    printf("[TID=%lu] Proceso listo. Esquema=%s cantidad=%d tiempo=%ds\n",
           tid,
           p->esquema == PAGINACION ? "Paginacion" : "Segmentacion",
           p->cantidad, p->tiempo);

    // ── 1. Pedir semáforo ──
    sem_wait(&mem->headers_sem);
    mem->buscando = pid;
    sem_post(&mem->headers_sem);

    add_locked_process(mem, pid);
    sem_wait_mem();
    remove_locked_process(mem, pid);

    // ── 2. Buscar espacio ──
    sleep(5); // Simular tiempo de búsqueda
    int asignados[MAX_MEM];
    int n_asignados = 0;
    int espacios_necesarios = (p->esquema == PAGINACION)
                                  ? p->cantidad
                                  : p->cantidad * p->tam_seg;

    for (int i = 0; i < mem->total_espacios && n_asignados < espacios_necesarios; i++)
    {
        if (mem->memoria[i].estado == LIBRE)
        {
            asignados[n_asignados++] = i;
        }
    }

    // ── 3. ¿Hay espacio? ──
    if (n_asignados < espacios_necesarios)
    {
        printf("[TID=%lu] PID=%d No hay espacio suficiente. Proceso muere.\n", tid, pid);

        // Registrar como muerto
        if (mem->n_muertos < MAX_PROCS)
            mem->muertos[mem->n_muertos++] = pid;

        escribir_bitacora(pid, "MUERTO-SIN-ESPACIO", -1, -1);
        mem->buscando = 0;
        sem_signal_mem();
        free(p);
        return NULL;
    }

    // ── Asignar espacios ──
    for (int i = 0; i < n_asignados; i++)
    {
        int idx = asignados[i];
        mem->memoria[idx].estado = OCUPADO;
        mem->memoria[idx].pid = pid;
        mem->memoria[idx].segmento = (p->esquema == SEGMENTACION) ? (i / p->tam_seg) : -1;

        escribir_bitacora(pid, "ASIGNACION", idx,
                          (p->esquema == SEGMENTACION) ? (i / p->tam_seg) : -1);

        printf("[TID=%lu] PID=%d Asignado espacio[%d]\n", tid, pid, idx);
    }

    mem->buscando = 0;

    // ── 4. Devolver semáforo ──
    sem_signal_mem();

    if (mem->shutdown) {
        printf("[TID=%lu] PID=%d Apagando proceso por shutdown.\n", tid, pid);
        free(p);
        return NULL;
    }

    // ── 5. Sleep ──
    printf("[TID=%lu] PID=%d Usando memoria por %ds...\n", tid, pid, p->tiempo);
    sleep(p->tiempo);

    // ── 6. Pedir semáforo para liberar ──
    add_locked_process(mem, pid);
    sem_wait_mem();
    remove_locked_process(mem, pid);


    // ── 7. Liberar memoria ──
    for (int i = 0; i < n_asignados; i++)
    {
        int idx = asignados[i];
        escribir_bitacora(pid, "DESASIGNACION", idx,
                          (p->esquema == SEGMENTACION) ? (i / p->tam_seg) : -1);

        mem->memoria[idx].estado = LIBRE;
        mem->memoria[idx].pid = 0;
        mem->memoria[idx].segmento = -1;

        printf("[TID=%lu] PID=%d Liberado espacio[%d]\n", tid, pid, idx);
    }

    // Registrar como terminado
    if (mem->n_terminados < MAX_PROCS)
        mem->terminados[mem->n_terminados++] = pid;

    // ── 8. Devolver semáforo ──
    sem_signal_mem();

    printf("[TID=%lu] PID=%d Proceso terminado.\n", tid, pid);
    free(p);
    return NULL;
}

// ─── Main ───
int main()
{
    srand(time(NULL));

    // Conectarse a memoria compartida
    shmid = shmget(SHM_KEY, sizeof(MemoriaCompartida), 0666);
    if (shmid == -1)
    {
        perror("shmget");
        exit(1);
    }

    mem = (MemoriaCompartida *)shmat(shmid, NULL, 0);
    if (mem == (void *)-1)
    {
        perror("shmat");
        exit(1);
    }

    // Conectarse al semáforo
    semid = semget(SEM_KEY, 1, 0666);
    if (semid == -1)
    {
        perror("semget");
        exit(1);
    }

    // Preguntar esquema
    printf("=== Productor de Procesos ===\n");
    printf("Seleccione esquema:\n");
    printf("  1. Paginacion\n");
    printf("  2. Segmentacion\n");
    printf("Opcion: ");
    int esquema;
    scanf("%d", &esquema);
    if (esquema != PAGINACION && esquema != SEGMENTACION)
    {
        fprintf(stderr, "Opcion invalida.\n");
        exit(1);
    }
    mem->esquema = esquema;

    printf("Generando procesos cada 30-60 segundos. Ctrl+C para detener.\n\n");

    // Loop principal
    while (!mem->shutdown)
    {
        ParamsHilo *p = malloc(sizeof(ParamsHilo));
        p->esquema = esquema;

        if (esquema == PAGINACION)
        {
            p->cantidad = (rand() % 10) + 1; // 1-10 páginas
            p->tam_seg = 1;
            p->tiempo = (rand() % 41) + 20; // 20-60s
        }
        else
        {
            p->cantidad = (rand() % 5) + 1; // 1-5 segmentos
            p->tam_seg = (rand() % 3) + 1;  // 1-3 espacios por segmento
            p->tiempo = (rand() % 41) + 20; // 20-60s
        }

        pthread_t hilo;
        pthread_create(&hilo, NULL, proceso, p);
        pthread_detach(hilo);

        // Esperar 30-60s para el siguiente proceso
        int espera = (rand() % 11) + 10;
        printf("[Productor] Siguiente proceso en %ds...\n", espera);
        sleep(espera);
    }

    shmdt(mem);
    return 0;
}