//
// Created by TCWei on 12/16/25.
//

#ifndef TINYCPERF_C_PROFILER_H
#define TINYCPERF_C_PROFILER_H
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
typedef struct timespec TimePoint;

typedef struct TimeLog {
  const char *name;
  float *data;
  size_t size;
  size_t capacity;
} TimeLog;

// 假設一個 C 檔案中最多有 1024 個不同的 Profiling 範圍
#define CPROF_MAX_ENTRIES 1024
#define CPROF_USEC_PER_NSEC (0.001f)

// 靜態陣列：每個 C 檔獨立的指標陣列 (內部連結)
// 這些指標初始為 NULL，用於儲存動態分配的 TimeLog 結構體
static TimeLog *__cprof_log_entries[CPROF_MAX_ENTRIES] = {NULL};
// 檔案中已註冊的 Log 總數 (也用於遍歷陣列)
static size_t __cprof_entry_count = 0;

// Store statistics for a single function
typedef struct {
  const char *name;
  size_t count;
  float total_time;
  float min_time;
  float max_time;
  size_t max_count;
  float avg_time;
  float std_dev;  // Standard Deviation
} CPROF_Stats;

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
#else
// TODO: if linux w/o monotonic clock
#endif
#else
// TODO: other platform than linux
#endif

// Core timing macro:
// It uses a C block ({...}) to ensure timing variables are only valid within
// that block. func_name: Unique name of the function (usually the function name
// itself) code: The original code to be executed by the function
#if defined(PROFILING)
#define CPROF_SCOPE_TAG(func_name, tag, code)                                            \
  do {                                                                                   \
    /* 1. Create a unique pointer in the code */                                         \
    /* but the time log can be shared with other code */                                 \
    static TimeLog *__local_log_ptr = NULL;                                              \
    /* 2. Find the pointer of the log in global or create one */                         \
    /*if (!__local_log_ptr) { */                                                         \
    char __combined_name[256];                                                           \
    /* 判斷 tag 的型別並格式化 (這裡簡化處理，支援整數與字串) */  \
    /* 如果你的環境支援 C11，可以用 _Generic，否則用 printf 格式化 */ \
    if (__builtin_types_compatible_p(__typeof__(tag), char *) ||                         \
        __builtin_types_compatible_p(__typeof__(tag), const char *)) {                   \
      snprintf(__combined_name, sizeof(__combined_name), "%s@%s", #func_name,            \
               (char *)(size_t)tag);                                                     \
    } else {                                                                             \
      snprintf(__combined_name, sizeof(__combined_name), "%s@%lld",                      \
               #func_name, (long long)tag);                                              \
    }                                                                                    \
    __local_log_ptr = CPROF_GetOrCreateLog(__combined_name);                             \
    /*}*/                                                                                \
    if (__local_log_ptr) {                                                               \
      /* 2. Start timing */                                                              \
      TimePoint start_time = CPROF_StartProfile();                                       \
      /* 3. Execute code */                                                              \
      {code} /* 4. End timing */                                                         \
      TimePoint end_time = CPROF_StopProfile();                                          \
      /* 5. Calculate duration (convert to seconds) */                                   \
      float duration = (float)TimeDiff(&end_time, &start_time);                          \
      /* 6. Write to dedicated TimeLog variable */                                       \
      TimeLog_push(__local_log_ptr, duration);                                           \
    }                                                                                    \
  } while (0)

#else
#define CPROF_SCOPE_TAG(func_name, tag, code) \
  /* Execute code */                          \
  do {                                        \
    {                                         \
      code                                    \
    }                                         \
  } while (0)
#endif

#define CPROF_SCOPE(func_name, code) CPROF_SCOPE_TAG(func_name, 0, code)

static inline void TimeLog_Init(TimeLog *log) {
  log->size = 0;
  log->capacity = 1024;  // Initial capacity
  log->data = malloc(log->capacity * sizeof(float));
  if (log->data == NULL) log->capacity = 0;  // Allocation failed
}

static inline TimeLog *CPROF_GetOrCreateLog(const char *name) {
  // A. Find if there is the same name log already exists
  for (size_t i = 0; i < __cprof_entry_count; i++) {
    // printf("CPROF_DEBUG: Entry count: %zu\n", __cprof_entry_count);
    // printf("CPROF_DEBUG: Comparing %s with %s\n",
    // __cprof_log_entries[i]->name,
    //        name);
    // printf("CPROF_DEBUG: strcmp result: %d\n",
    //        strcmp(__cprof_log_entries[i]->name, name));
    // using strcmp to compare names (needs to #include <string.h>)
    if (strcmp(__cprof_log_entries[i]->name, name) == 0) {
      return __cprof_log_entries[i];  // Found the same log, return it
    }
  }

  // B. If the name of the tag isn't found, create a new TimeLog
  if (__cprof_entry_count < CPROF_MAX_ENTRIES) {
    TimeLog *new_log = (TimeLog *)malloc(sizeof(TimeLog));
    if (new_log) {
      // Copy a name string to prevent pointing to a local variable
      size_t name_len = strlen(name) + 1;
      char *name_copy = (char *)malloc(name_len);
      if (name_copy) {
        strcpy(name_copy, name);
        new_log->name = name_copy;
      } else {
        free(new_log);
        return NULL;  // Failed to allocate memory
      }
      TimeLog_Init(new_log);
      __cprof_log_entries[__cprof_entry_count++] = new_log;
      return new_log;
    }
  }
  return NULL;  // No enough space or failed to allocate
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
static inline CPROF_Stats CPROF_calculate_stats(const TimeLog *log) {
  CPROF_Stats stats = {0};
  // If no data, set to default values
  if (log->size == 0) return stats;

  stats.name = log->name;
  stats.count = log->size;
  stats.total_time = 0.0f;

  // Convert first value to get initial min/max in microseconds
  float first_val_us = log->data[0] * CPROF_USEC_PER_NSEC;
  stats.max_time = stats.min_time = first_val_us;
  stats.max_count = 0;

  // --- First traversal: Calculate sum, Min and Max ---
  for (size_t i = 0; i < log->size; i++) {
    const float t_us =
        log->data[i] * CPROF_USEC_PER_NSEC;  // Convert to microseconds
    stats.total_time += t_us;
    if (t_us < stats.min_time) stats.min_time = t_us;
    if (t_us > stats.max_time) {
      stats.max_time = t_us;
      stats.max_count = i;
    }
  }

  stats.avg_time = stats.total_time / (float)log->size;

  // --- Second traversal: Calculate standard deviation (Variance) ---
  float sum_of_sq_diff = 0.0f;
  for (size_t i = 0; i < log->size; i++) {
    const float t_us =
        log->data[i] * CPROF_USEC_PER_NSEC;  // Convert to microseconds
    const float diff = t_us - stats.avg_time;
    sum_of_sq_diff += diff * diff;
  }

  // Use N-1 as denominator (sample standard deviation)
  const float variance =
      (log->size > 1) ? sum_of_sq_diff / (float)(log->size - 1) : 0.0f;
  stats.std_dev = sqrtf(variance);
  return stats;
}

#define CPROF_BINS (10)
static inline void CPROF_dump_histogram(FILE *f, const TimeLog *log,
                                        const CPROF_Stats *stats) {
  if (log->size == 0) return;

  // Both stats and data are already in microseconds

  // Use average ± 2 standard deviations as the core observation range
  float range_start = stats->avg_time - (2 * stats->std_dev);
  if (range_start < stats->min_time) range_start = stats->min_time;

  float range_end = stats->avg_time + (2 * stats->std_dev);
  if (range_end > stats->max_time) range_end = stats->max_time;

  float step = (range_end - range_start) / CPROF_BINS;
  // To prevent step being zero
  if (step <= 0) step = 0.1f;  // Use 0.1μs minimum step

  int counts[10] = {0};
  int out_of_range_low = 0;
  int out_of_range_high = 0;

  for (size_t i = 0; i < log->size; i++) {
    float val_us =
        log->data[i] * CPROF_USEC_PER_NSEC;  // Convert to microseconds
    if (val_us < range_start) {
      out_of_range_low++;
    } else if (val_us >= range_end) {
      out_of_range_high++;
    } else {
      int bin_idx = (int)((val_us - range_start) / step);
      if (bin_idx >= CPROF_BINS) bin_idx = CPROF_BINS - 1;
      counts[bin_idx]++;
    }
  }

  // Output histogram data to file (in microseconds)
  fprintf(f, "%s,Underflow,(<%.0f),%d\n", log->name, range_start,
          out_of_range_low);
  for (int i = 0; i < CPROF_BINS; i++) {
    fprintf(f, "%s,%.0f,\"(%.0f,%.0f]\",%d\n", log->name, step,
            (range_start + i * step), (range_start + (i + 1) * step),
            counts[i]);
  }
  fprintf(f, "%s,Overflow,(>%.0f),%d\n", log->name, range_end,
          out_of_range_high);
}

// Static inline function for outputting all logs collected in a single C file
static inline void CPROF_dump_to_file(const char *filename) {
#if defined(PROFILING)
  FILE *f = fopen(filename, "w");
  if (!f) {
    fprintf(stderr, "CPROF: Failed to open file %s\n", filename);
    return;
  }

  // CSV Header
  fprintf(f,
          "Function,Count,Total(us),Avg(us),Min(us),Max(us),Max(count),StdDev("
          "us)\n");
  CPROF_Stats stats[CPROF_MAX_ENTRIES] = {};
  for (size_t func = 0; func < __cprof_entry_count; func++) {
    // Traverse the static log list of current C file
    const TimeLog *current = __cprof_log_entries[func];
    if (!current) continue;
    // [KEY] Calculate statistics
    const CPROF_Stats stat = stats[func] = CPROF_calculate_stats(current);
    // Convert nanoseconds (ns) to microseconds (us) for output (because your
    // TimeDiff outputs nanoseconds)
    // Write results to file
    fprintf(f, "%s,%zu,%.0f,%.0f,%.0f,%.0f,%zu,%.0f\n", stat.name, stat.count,
            stat.total_time,  // Total Time (us)
            stat.avg_time,    // Avg Time (us)
            stat.min_time,    // Min Time (us)
            stat.max_time,    // Max Time (us)
            stat.max_count,   // Max Occur (count)
            stat.std_dev      // StdDev (us)
    );
  }
  fprintf(f, "\n# --- Histogram Data ---\n");
  fprintf(f, "Function,step(us),\"Bin(Start,End](us)\",Count\n");
  for (size_t func = 0; func < __cprof_entry_count; func++) {
    const TimeLog *current = __cprof_log_entries[func];
    if (!current) continue;
    // Use the same stats calculated above
    CPROF_dump_histogram(f, current, &stats[func]);
  }
  fclose(f);
#else
  ;  // do nothing
#endif
}

// Static inline function for memory cleanup
#if defined(PROFILING)
static inline void CPROF_cleanup(void) {
  for (size_t func = 0; func < __cprof_entry_count; func++) {
    // Traverse the static log list of current C file
    TimeLog *current = __cprof_log_entries[func];
    if (!current) continue;
    // Free Dynamic Array data within TimeLog
    if (current->data) free((void *)current->data);
    // Free the name string copy
    if (current->name) free((void *)current->name);
    free((void *)current);
    // Clear the pointer in the static array to avoid dangling pointer
    __cprof_log_entries[func] = NULL;
  }
  __cprof_entry_count = 0;
}
#else
static inline void CPROF_cleanup(void) {
  ;  // do nothing
}
#endif

#endif  // TINYCPERF_C_PROFILER_H
