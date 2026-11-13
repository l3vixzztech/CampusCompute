#include "logger.h"

#include "common.h"

#include <pthread.h>
#include <stdarg.h>

typedef struct {
    int sequence;
    int producer_id;
    char text[192];
} LogEvent;

static LogEvent queue[LOGGER_QUEUE_CAPACITY];
static int q_head, q_tail, q_count;
static int producers_finished, producer_target;
static int events_per_producer_target;
static int produced_count, consumed_count, high_water, full_waits;
static pthread_mutex_t queue_lock;
static pthread_cond_t not_empty;
static pthread_cond_t not_full;
static FILE *log_stream;

static void sleep_ms(long milliseconds)
{
    struct timespec ts;

    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void log_line(const char *fmt, ...)
{
    char stamp[32];
    va_list ap;

    format_timestamp(stamp, sizeof(stamp));
    fprintf(log_stream, "[%s] ", stamp);
    va_start(ap, fmt);
    vfprintf(log_stream, fmt, ap);
    va_end(ap);
    fputc('\n', log_stream);
    fflush(log_stream);
}

static void *producer_main(void *arg)
{
    int id = (int)(long)arg;
    int i;
    long delay = 4L + id * 2L;

    printf("[producer %d] started (thread id %lu)\n", id,
           (unsigned long)pthread_self());
    for (i = 1; i <= events_per_producer_target; i++) {
        LogEvent event;

        event.sequence = 0;
        event.producer_id = id;
        snprintf(event.text, sizeof(event.text),
                 "job J%d step %d processed by producer %d", ((i + id) % 5) + 1,
                 i, id);

        pthread_mutex_lock(&queue_lock);
        while (q_count == LOGGER_QUEUE_CAPACITY) {
            full_waits++;
            printf("[producer %d] queue full (%d/%d), waiting for consumer\n",
                   id, q_count, LOGGER_QUEUE_CAPACITY);
            pthread_cond_wait(&not_full, &queue_lock);
        }

        event.sequence = produced_count + 1;
        queue[q_tail] = event;
        q_tail = (q_tail + 1) % LOGGER_QUEUE_CAPACITY;
        q_count++;
        produced_count++;
        if (q_count > high_water)
            high_water = q_count;
        printf("[producer %d] produced event %d: %s (queue=%d/%d)\n", id,
               event.sequence, event.text, q_count, LOGGER_QUEUE_CAPACITY);
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&queue_lock);

        sleep_ms(delay);
    }

    pthread_mutex_lock(&queue_lock);
    producers_finished++;
    printf("[producer %d] finished; producers done=%d/%d\n", id,
           producers_finished, producer_target);
    pthread_cond_broadcast(&not_empty);
    pthread_cond_broadcast(&not_full);
    pthread_mutex_unlock(&queue_lock);

    return NULL;
}

static void *consumer_main(void *arg)
{
    (void)arg;
    printf("[consumer] started (thread id %lu), waiting for events\n",
           (unsigned long)pthread_self());

    for (;;) {
        LogEvent event;

        pthread_mutex_lock(&queue_lock);
        while (q_count == 0 && producers_finished < producer_target)
            pthread_cond_wait(&not_empty, &queue_lock);

        if (q_count == 0 && producers_finished == producer_target) {
            pthread_mutex_unlock(&queue_lock);
            break;
        }

        event = queue[q_head];
        q_head = (q_head + 1) % LOGGER_QUEUE_CAPACITY;
        q_count--;
        consumed_count++;
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&queue_lock);

        log_line("[consumer] wrote seq=%d producer=%d: %s", event.sequence,
                 event.producer_id, event.text);
        printf("[consumer] consumed event %d from producer %d (queue=%d/%d)\n",
               event.sequence, event.producer_id, q_count,
               LOGGER_QUEUE_CAPACITY);
        sleep_ms(60);
    }

    printf("[consumer] all producers finished and queue is empty; exiting\n");
    return NULL;
}

int command_logger(int events_per_producer)
{
    pthread_t producers[LOGGER_PRODUCERS];
    pthread_t consumer;
    int created_producers = 0;
    int consumer_created = 0;
    int i;
    int rc = 0;

    if (events_per_producer < 1 || events_per_producer > 10) {
        fprintf(stderr, "error: events per producer must be 1..10\n");
        return 2;
    }
    if (ensure_log_dir() != 0)
        return 1;

    q_head = q_tail = q_count = 0;
    producers_finished = 0;
    producer_target = LOGGER_PRODUCERS;
    events_per_producer_target = events_per_producer;
    produced_count = consumed_count = high_water = full_waits = 0;

    pthread_mutex_init(&queue_lock, NULL);
    pthread_cond_init(&not_empty, NULL);
    pthread_cond_init(&not_full, NULL);

    log_stream = fopen(LOG_FILE, "w");
    if (log_stream == NULL) {
        fprintf(stderr, "error: cannot open %s: %s\n", LOG_FILE,
                strerror(errno));
        return 1;
    }

    print_rule("Concurrent activity logger (bounded producer-consumer)");
    printf("Producers: %d | Consumer: 1 | Queue capacity: %d | Events/producer: %d\n",
           LOGGER_PRODUCERS, LOGGER_QUEUE_CAPACITY, events_per_producer);

    /* Start the consumer first so producers can always be drained. */
    {
        int err = pthread_create(&consumer, NULL, consumer_main, (void *)1L);

        if (err != 0) {
            fprintf(stderr, "error: cannot create consumer: %s\n",
                    strerror(err));
            fclose(log_stream);
            log_stream = NULL;
            pthread_mutex_destroy(&queue_lock);
            pthread_cond_destroy(&not_empty);
            pthread_cond_destroy(&not_full);
            return 1;
        }
        consumer_created = 1;
    }

    for (i = 1; i <= LOGGER_PRODUCERS; i++) {
        int err = pthread_create(&producers[i - 1], NULL, producer_main,
                                 (void *)(long)i);

        if (err != 0) {
            fprintf(stderr, "error: cannot create producer %d: %s\n", i,
                    strerror(err));
            rc = 1;
            break;
        }
        created_producers++;
    }

    if (rc != 0) {
        pthread_mutex_lock(&queue_lock);
        producer_target = created_producers;
        pthread_cond_broadcast(&not_empty);
        pthread_cond_broadcast(&not_full);
        pthread_mutex_unlock(&queue_lock);
    }

    for (i = 0; i < created_producers; i++)
        pthread_join(producers[i], NULL);
    if (consumer_created)
        pthread_join(consumer, NULL);

    printf("[logger] all %d threads joined cleanly\n",
           created_producers + consumer_created);

    if (pthread_mutex_destroy(&queue_lock) != 0 ||
        pthread_cond_destroy(&not_empty) != 0 ||
        pthread_cond_destroy(&not_full) != 0) {
        fprintf(stderr, "warning: a synchronization object was still busy\n");
        rc = 1;
    } else {
        printf("[logger] mutex and condition variables destroyed\n");
    }

    printf("[logger] produced=%d consumed=%d high-water=%d full-waits=%d\n",
           produced_count, consumed_count, high_water, full_waits);
    if (consumed_count != produced_count) {
        fprintf(stderr, "error: produced/consumed mismatch\n");
        rc = 1;
    }

    fclose(log_stream);
    log_stream = NULL;
    printf("[logger] log written to %s\n", LOG_FILE);
    return rc;
}
