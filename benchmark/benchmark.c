/**
Benchmarking suite for 2D data structures
*/

#include "benchmark.h"

#ifndef min
#define min(x, y) (y ^ ((x ^ y) & -(x < y)))
#endif

#ifdef READY_TO_RUN

#define COUNT_ALL

/**
 * OperationPacket
 *
 * An OperationPacket channels information between the parent thread and each child
 * thread. It allows the thread to indicate how many insertions, queries, and deletes
 * it processed, as well as providing information to the thread about whether to continue
 * execution, what its virtual ID number is [0, NTHREADS), what the root/first node is,
 * and the bounds of the points to be generated
 *
 * root - the first node to start at
 * p_min - the point with the smallest coordinate values
 * p_max - the point with the largest coordinate values
 * inserts - buffer for number of inserts processed
 * queries - buffer for number of queries processed
 * deletes - buffer for number of deletes processed
 * vid - the virtual ID for the thread
 * actives - buffer for already-active points
 * active_size - size of active points buffer
 * ready - the bit for the thread to say it's ready
 */
typedef volatile struct {
    TYPE *root;
    Point p_min, p_max;
    uint64_t inserts, queries, deletes;
    uint64_t vid;
    Point *actives;
    uint64_t active_size;
    bool ready;
} OperationPacket;

static volatile bool STARTED = false, ACTIVE = true;
void* execute(void *op) {
    OperationPacket *packet = (OperationPacket*)op;

    // read initialization information from OperationPacket
    TYPE *root = packet->root;
    Point p_min = packet->p_min, p_max = packet->p_max;
	srand(rand() + packet->vid * packet->vid);
	srand(rand() + packet->vid);
    packet->inserts = 0;
    packet->queries = 0;
    packet->deletes = 0;

    // prepare point buffer
    const uint64_t npoints = min(2 * packet->active_size, 1000);
    Point *pbuffer = (Point*)malloc(sizeof(*pbuffer) * npoints);  // ``active" points
    uint64_t head = 0, tail = 0;
    for (head = 0; head < npoints; head++)
        pbuffer[head] = packet->actives[head];

    // set up RLU
    rlu_self = (rlu_thread_data_t*)malloc(sizeof(*rlu_self));
    RLU_THREAD_INIT(rlu_self);

    packet->ready = true;

    // wait to begin
    while (!STARTED);

    /*
    ** BENCHMARKING BEGINS
    */

    while (ACTIVE) {
        // writes vs reads
        if (head == tail || random() < WRATIO) {
            // deletes vs inserts
            if (head != tail && random() < DRATIO) {
                Point p = pbuffer[tail];
                tail = (tail + 1) % npoints;

#ifdef COUNT_ALL
                DELETE(root, p);
                packet->deletes++;
#else
                packet->deletes += DELETE(root, p);
#endif
            }
            else {
                Point p;
                register uint64_t i;
                for (i = 0; i < D; i++)
                    p.data[i] = p_min.data[i] + random() * (p_max.data[i] - p_min.data[i]);

                // within buffer
                if ((head + 1) % npoints != tail) {
                    pbuffer[head] = p;
                    head = (head + 1) % npoints;
                }

#ifdef COUNT_ALL
                INSERT(root, p);
                packet->inserts++;
#else
                packet->inserts += INSERT(root, p);
#endif
            }
        }
        else {
            uint64_t size = (head + npoints - tail) % npoints;
            uint64_t index = (uint64_t)(size * random());

#ifdef COUNT_ALL
            QUERY(root, pbuffer[(tail + index) % npoints]);
            packet->queries++;
#else
            packet->queries += QUERY(root, pbuffer[(tail + index) % npoints]);
#endif
        }
    }

    /*
    ** BENCHMARKING ENDS
    */

    // clear out the point buffer
    free(pbuffer);

    // end RLU on thread
    RLU_THREAD_FINISH(rlu_self);
    free(rlu_self);

    // ensure thread exits
    pthread_exit(0);
    return NULL;
}

void test_random(const uint64_t seconds) {
    // seed the RNG based on time to run
    srand(seconds % ((1LL << 32) - 1));
    pthread_mutex_attr_init();

    register uint64_t i;

    // initialize the root of the tree
    float64_t length = 1LL << 32;
    Point root_point;
    for (i = 0; i < D; i++)
        root_point.data[i] = 0;
    TYPE *root = CONSTRUCTOR(length, root_point);

    test_rand_off();

#ifdef PARALLEL

#ifndef NTHREADS
#define NTHREADS 1
#endif

#ifdef VERBOSE
    printf("Parallel %llu threads\n", (unsigned long long)NTHREADS);
#endif

    const uint64_t nthreads = NTHREADS;
#else
#ifdef VERBOSE
        printf("Serial\n");
#endif

    const uint64_t nthreads = 1;
#endif

#ifdef VERBOSE
    printf("Dimensions: %llu\n", (unsigned long long)D);
#endif

    // populate the tree with some initial nodes

#ifdef INITIAL
    const uint64_t initial_population = INITIAL;
#else
    const uint64_t initial_population = 1000000;
#endif

#ifdef VERBOSE
    printf("Populating tree with %llu nodes...\n", (unsigned long long)initial_population);
#endif

    // initialize RLU

    // type: FINE/COARSE, max_write_sets: 1 if COARSE
    RLU_INIT(RLU_TYPE_FINE_GRAINED, nthreads + 1);

    rlu_self = (rlu_thread_data_t*)malloc(sizeof(*rlu_self));

    RLU_THREAD_INIT(rlu_self);
    Point *initial_actives = (Point*)malloc(sizeof(*initial_actives) * initial_population);
    for (i = 0; i < initial_population; i++) {
        register uint64_t j;
        for (j = 0; j < D; j++)
            initial_actives[i].data[j] = (random() - 0.5) * length;
        INSERT(root, initial_actives[i]);
    }
    RLU_THREAD_FINISH(rlu_self);

    free(rlu_self);

#ifdef VERBOSE
    printf("Running for %llu seconds\n", (unsigned long long)seconds);
#endif

#ifdef VERBOSE
    printf("\n[Estimated] {Inserts: %5.2lf%%    Queries: %5.2lf%%    Deletes: %5.2lf%%}\n",
        100.0 * WRATIO * (1 - DRATIO), 100.0 * (1 - WRATIO), 100.0 * WRATIO * DRATIO);
#endif

    // prepare initialization for each thread
    OperationPacket packets[nthreads];
    Point p_min, p_max;
    for (i = 0; i < D; i++) {
        p_min.data[i] = root_point.data[i] - 0.5 * length;
        p_max.data[i] = root_point.data[i] + 0.5 * length;
    }
    const uint64_t actives_per_thread = min(100000, initial_population / nthreads);
    for (i = 0; i < nthreads; i++) {
        packets[i] = (OperationPacket) {
            .root = root,
            .p_min = p_min,
            .p_max = p_max,
            .inserts = 0,
            .queries = 0,
            .deletes = 0,
            .vid = i,
            .actives = initial_actives + i * actives_per_thread,
            .active_size = actives_per_thread,
            .ready = false
        };
    }

    pthread_t threads[nthreads];

    /*
    ** PARALLEL SECTION BEGINS
    */

    // start threads
    for (i = 0; i < nthreads; i++)
        pthread_create(threads + i, NULL, execute, (void*)(packets + i));

    for (i = 0; i < nthreads; i++)
        while (!packets[i].ready);

    STARTED = false;
    ACTIVE = true;

    struct timeval start, end;
    gettimeofday(&start, NULL);

    /*
    ** BENCHMARKING BEGINS
    */

    STARTED = true;  // threads can start now

    sleep(seconds);

    ACTIVE = false;  // threads should stop now

    /*
    ** BENCHMARKING ENDS
    */

    for (i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    /*
    ** PARALLEL SECTION ENDS
    */

    gettimeofday(&end, NULL);
    int64_t time_seconds = end.tv_sec - start.tv_sec;
    int64_t time_microseconds = end.tv_usec - start.tv_usec;
    float64_t total_seconds = time_seconds + time_microseconds * 1e-6;

    // aggregate data
    uint64_t inserts = 0, queries = 0, deletes = 0;
    for (i = 0; i < nthreads; i++) {
        inserts += packets[i].inserts;
        queries += packets[i].queries;
        deletes += packets[i].deletes;
    }
    uint64_t total = inserts + queries + deletes;

#ifdef VERBOSE
    printf("[Real]      {Inserts: %5.2lf%%    Queries: %5.2lf%%    Deletes: %5.2lf%%}\n\n",
        100.0 * inserts / total, 100.0 * queries / total, 100.0 * deletes / total);
    printf("Total operations:   %10llu\n", (unsigned long long)total);
    printf("Number of inserts:  %10llu\n", (unsigned long long)inserts);
    printf("Number of queries:  %10llu\n", (unsigned long long)queries);
    printf("Number of deletes:  %10llu\n", (unsigned long long)deletes);
    printf("Total real time:    %17.6lf s\n", total_seconds);
    printf("Total throughput:   %17.6lf ops/s\n", total / total_seconds);
#else
    printf("%llu, %llu, %llu, %lf, %llu, %llu, %llu, %llu", (unsigned long long)nthreads, (unsigned long long)D,
        (unsigned long long)total, total_seconds, (unsigned long long)initial_population,
        (unsigned long long)inserts, (unsigned long long)queries, (unsigned long long)deletes);
    printf("\n");
#endif

    DESTRUCTOR(root);

#ifdef CLEANUP
    CLEANUP();
#endif

    free(initial_actives);
    pthread_mutex_attr_destroy();
    pthread_exit(0);
}
#endif

void test() {
#ifdef READY_TO_RUN
    test_random(TIME);
#endif
}

int main(int argc, char* argv[]) {
    setbuf(stdout, 0);
#ifdef READY_TO_RUN
#ifdef MTRACE
    mtrace();
#endif

    srand(0);

#ifdef VERBOSE
    printf("[Beginning tests]\n");
    char testname[128];
    sprintf(testname, "Randomized test (%llu seconds)", (unsigned long long)TIME);
    start_test(test, testname);
    printf("\n[Ending tests]\n");
#else
    test();
#endif

    free(rlu_self);

    return 0;
#else
    printf("Need to define at compile time:\n");
    printf("-DTIME (integer time in seconds, must be an LL)\n");
    printf("-DWRATIO (0.0-1.0 write ratio among read/write ops)\n");
    printf("-DDRATIO (0.0-1.0 delete ratio among writes)\n");
    printf("-DHEADER (the header file, in quotes, e.g. \"./DataType.h\")\n");
    printf("-DTYPE (the datatype name)\n");
    printf("-DCONSTRUCTOR (the constructor function)\n");
    printf("-DINSERT (the insertion function)\n");
    printf("-DQUERY (the querying function)\n");
    printf("-DDELETE (the deletion function)\n");
    printf("-DDESTRUCTOR (the datatype destructor)\n");
    printf("\nOptional:\n");
    printf("-DCLEANUP (the cleanup function, takes no argument)\n");
    printf("-DINITIAL (initial population, defaults to 1,000,000 nodes)\n");
    printf("-DMTRACE (define to enable mtrace)\n");
    printf("-DPARALLEL (use pthreads to run in parallel; serial otherwise)\n");
    printf("-DNTHREADS (number of threads to use, defaults to 1)\n");
    printf("-DDIMENSIONS (number of dimensions to use, defaults to 2)\n");
    return 11;
#endif
}
