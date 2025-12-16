//
// Created by TCWei on 12/16/25.
//

#ifndef TINYCPERF_C_PROFILER_H
#define TINYCPERF_C_PROFILER_H
#include <math.h>
#include <stdlib.h>
#include <time.h>
typedef struct timespec TimePoint;

typedef struct TimeLog {
  const char *name;
  float *data;
  size_t size;
  size_t capacity;
  struct TimeLog *next;  // Points to the next TimeLog
} TimeLog;

// Store statistics for a single function
typedef struct {
  const char *name;
  size_t count;
  float total_time;
  float min_time;
  float max_time;
  float avg_time;
  float std_dev;  // Standard Deviation
} CPROF_Stats;

// New: Used to declare a single static TimeLog variable
#define CPROF_DECLARE_LOG(func_name)                                         \
  static TimeLog func_name##_log = {.data = NULL, .size = 0, .capacity = 0}; \
  static int __cprof_registered_##func_name =                                \
      0;  // Used for registration tracking

// New: Registration logic (must check on every CPROF_SCOPE call)
#define CPROF_REGISTER_LOG(func_name)                                        \
  do {                                                                       \
    if (__cprof_registered_##func_name == 0) {                               \
      func_name##_log.name = #func_name;                                     \
      func_name##_log.next =                                                 \
          __cprof_log_root;                /* New node points to old root */ \
      __cprof_log_root = &func_name##_log; /* Root points to new node */     \
      __cprof_registered_##func_name = 1;                                    \
    }                                                                        \
  } while (0)

// Independent static log list start point for each C file (internal linkage)
static TimeLog *__cprof_log_root = NULL;

#ifdef __linux__
#if 1 == 1
static TimePoint CPROF_StartProfile(void) {
  TimePoint start;
  if (clock_gettime(CLOCK_MONOTONIC, &start) == 0) return start;
  return (TimePoint){0, 0};
}

static TimePoint CPROF_StopProfile(void) {
  TimePoint end;
  if (clock_gettime(CLOCK_MONOTONIC, &end) == 0) return end;
  return (TimePoint){0, 0};
}

static int64_t TimeDiff(const TimePoint *timeA_p, const TimePoint *timeB_p) {
  return (timeA_p->tv_sec - timeB_p->tv_sec) * 1000000000L +
         (timeA_p->tv_nsec - timeB_p->tv_nsec);
}

// Core timing macro:
// It uses a C block ({...}) to ensure timing variables are only valid within
// that block. func_name: Unique name of the function (usually the function name
// itself) code: The original code to be executed by the function
#define CPROF_SCOPE(func_name, code)                                          \
  do {                                                                        \
    /* 1. Ensure registration (now only responsible for registration, not     \
     * variable definition) */                                                \
    CPROF_REGISTER_LOG(func_name);                                            \
    /* 2. Start timing */                                                     \
    TimePoint start_time = CPROF_StartProfile();                              \
                                                                              \
    /* 3. Execute code */                                                     \
    code;                                                                     \
                                                                              \
    /* 4. End timing */                                                       \
    TimePoint end_time = CPROF_StopProfile();                                 \
                                                                              \
    /* 5. Calculate duration (convert to seconds) */                          \
    float duration = (float)TimeDiff(&end_time, &start_time);                 \
                                                                              \
    /* 6. Write to dedicated TimeLog variable */                              \
    TimeLog_push(&func_name##_log, duration);                                 \
    /* [NEW] Debug output: Use #func_name to print function name string */    \
    /* Note: We use #func_name to convert variable name to string */          \
    printf("CPROF_DEBUG: %s execution time: %.3f microseconds\n", #func_name, \
           duration / 1000.0f);                                               \
  } while (0)
#else
// TODO: if linux w/o monotonic clock
#endif
#else
// TODO: other platform than linux
#endif

static inline void TimeLog_Init(TimeLog *log) {
  log->size = 0;
  log->capacity = 1024;  // Initial capacity
  log->data = malloc(log->capacity * sizeof(float));
  if (log->data == NULL) log->capacity = 0;  // Allocation failed
}

static inline size_t TimeLog_Resize(TimeLog *log) {
  const size_t new_capacity = log->capacity * 2;
  float *new_data = realloc(log->data, new_capacity * sizeof(float));
  if (new_data == NULL) {
    free(log->data);
    fprintf(stderr, "CPROF: Memory allocation failed!\n");
    return 0;  // Reallocation failed
  }
  log->data = new_data;
  log->capacity = new_capacity;
  return new_capacity;
}

// 2. Core operation functions (implementing Dynamic Array push)
// These must be defined as static inline to ensure they are only compiled
// within each C file, and the compiler will try to inline them to reduce
// function call overhead.
static inline void TimeLog_push(TimeLog *log, const float duration) {
  if (!log) return;
  if (log->capacity == 0) TimeLog_Init(log);
  if (log->size == log->capacity) {
    const size_t new_capacity = TimeLog_Resize(log);
    if (new_capacity == 0) return;  // Resize failed
  }
  log->data[log->size++] = duration;
}

// Calculate and fill statistics for a single TimeLog
static inline void CPROF_calculate_stats(const TimeLog *log,
                                         CPROF_Stats *stats) {
  if (log->size == 0) {
    // If no data, set to default values
    stats->count = 0;
    stats->total_time = 0.0f;
    stats->min_time = 0.0f;
    stats->max_time = 0.0f;
    stats->avg_time = 0.0f;
    stats->std_dev = 0.0f;
    return;
  }

  stats->name = log->name;
  stats->count = log->size;
  stats->total_time = 0.0f;
  stats->min_time = log->data[0];
  stats->max_time = log->data[0];

  // --- First traversal: Calculate sum, Min and Max ---
  for (size_t i = 0; i < log->size; i++) {
    float t = log->data[i];
    stats->total_time += t;
    if (t < stats->min_time) stats->min_time = t;
    if (t > stats->max_time) stats->max_time = t;
  }

  stats->avg_time = stats->total_time / (float)log->size;

  // --- Second traversal: Calculate standard deviation (Variance) ---
  float sum_of_sq_diff = 0.0f;
  for (size_t i = 0; i < log->size; i++) {
    const float diff = log->data[i] - stats->avg_time;
    sum_of_sq_diff += diff * diff;
  }

  // Use N-1 as denominator (sample standard deviation)
  const float variance =
      (log->size > 1) ? sum_of_sq_diff / (float)(log->size - 1) : 0.0f;
  stats->std_dev = sqrtf(variance);
}

// Static inline function for outputting all logs collected in a single C file
static inline void CPROF_dump_to_file(const char *filename) {
  FILE *f = fopen(filename, "w");
  if (!f) {
    fprintf(stderr, "CPROF: Failed to open file %s\n", filename);
    return;
  }
  // Traverse the static log list of current C file
  TimeLog *current = __cprof_log_root;
  // CSV Header
  fprintf(f, "Function,Count,Total(us),Avg(us),Min(us),Max(us),StdDev(us)\n");
  while (current != NULL) {
    CPROF_Stats stats;
    // [KEY] Calculate statistics
    CPROF_calculate_stats(current, &stats);
    // Convert nanoseconds (ns) to microseconds (us) for output (because your
    // TimeDiff outputs nanoseconds)
    const float us_factor = 1000.0f;  // 1000 ns = 1 us
    // Write results to file
    fprintf(f, "%s,%zu,%.1f,%.1f,%.1f,%.1f,%.1f\n", stats.name, stats.count,
            stats.total_time / us_factor,  // Total Time (us)
            stats.avg_time / us_factor,    // Avg Time (us)
            stats.min_time / us_factor,    // Min Time (us)
            stats.max_time / us_factor,    // Max Time (us)
            stats.std_dev / us_factor      // StdDev (us)
    );
    // Move to next log
    current = current->next;
  }
  fclose(f);
}

// Static inline function for memory cleanup
static inline void CPROF_cleanup(void) {
  TimeLog *current = __cprof_log_root;
  while (current != NULL) {
    if (current->data != NULL) {
      free(current->data);  // Free Dynamic Array data within TimeLog
    }
    current->data = NULL;
    current->size = 0;
    current->capacity = 0;
    current =
        current->next;  // Only need to move pointer here, because TimeLog
                        // structure itself is a static variable, no free needed
  }
  __cprof_log_root = NULL;
}

#endif  // TINYCPERF_C_PROFILER_H
