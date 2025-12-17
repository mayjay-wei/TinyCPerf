//
// Created by TCWei on 12/16/25.
//

#ifndef TINYCPERF_C_PROFILER_H
#define TINYCPERF_C_PROFILER_H
#include <math.h>
#include <stdlib.h>
#include <string.h>
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
#define CPROF_SCOPE(func_name, code)                                         \
  do {                                                                       \
    /* 1. 建立一個靜態指標，它在該程式碼位置是唯一的 */                      \
    /* 但它指向的 TimeLog 則可以與其他位置共享 */                            \
    static TimeLog *__local_log_ptr = NULL;                                  \
    /* 2. 如果指標還沒初始化，去全域尋找或建立一個同名的 Log */              \
    if (__local_log_ptr == NULL) {                                           \
      __local_log_ptr = CPROF_GetOrCreateLog(#func_name);                    \
    }                                                                        \
    if (__local_log_ptr) {                                                   \
      /* 2. Start timing */                                                  \
      TimePoint start_time = CPROF_StartProfile();                           \
                                                                             \
      /* 3. Execute code */                                                  \
      code;                                                                  \
                                                                             \
      /* 4. End timing */                                                    \
      TimePoint end_time = CPROF_StopProfile();                              \
                                                                             \
      /* 5. Calculate duration (convert to seconds) */                       \
      float duration = (float)TimeDiff(&end_time, &start_time);              \
                                                                             \
      /* 6. Write to dedicated TimeLog variable */                           \
      TimeLog_push(__local_log_ptr, duration);                               \
      /* [NEW] Debug output: Use #func_name to print function name string */ \
      /* Note: We use #func_name to convert variable name to string */       \
      printf("CPROF_DEBUG: %s execution time: %.3f microseconds\n",          \
             #func_name, duration / 1000.0f);                                \
    }                                                                        \
  } while (0)
#else
#define CPROF_SCOPE(func_name, code) \
  /* Execute code */                 \
  do {                               \
    code;                            \
  } while (0)
#endif

static inline void TimeLog_Init(TimeLog *log) {
  log->size = 0;
  log->capacity = 1024;  // Initial capacity
  log->data = malloc(log->capacity * sizeof(float));
  if (log->data == NULL) log->capacity = 0;  // Allocation failed
}

static inline TimeLog *CPROF_GetOrCreateLog(const char *name) {
  // A. 搜尋現有的 Log 是否有同名的
  for (size_t i = 0; i < __cprof_entry_count; i++) {
    // 使用 strcmp 比較名稱 (需 #include <string.h>)
    if (strcmp(__cprof_log_entries[i]->name, name) == 0) {
      return __cprof_log_entries[i];  // 找到同名的，直接回傳
    }
  }

  // B. 如果沒找到，且陣列還有空間，則建立新的
  if (__cprof_entry_count < CPROF_MAX_ENTRIES) {
    TimeLog *new_log = (TimeLog *)malloc(sizeof(TimeLog));
    if (new_log) {
      new_log->name = name;
      TimeLog_Init(new_log);
      __cprof_log_entries[__cprof_entry_count++] = new_log;
      return new_log;
    }
  }
  return NULL;  // 空間不足或分配失敗
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
  if (log->size == 0) {
    // If no data, set to default values
    return stats;
  }

  stats.name = log->name;
  stats.count = log->size;
  stats.total_time = 0.0f;
  stats.min_time = log->data[0];
  stats.max_time = log->data[0];

  // --- First traversal: Calculate sum, Min and Max ---
  for (size_t i = 0; i < log->size; i++) {
    const float t = log->data[i];
    stats.total_time += t;
    if (t < stats.min_time) stats.min_time = t;
    if (t > stats.max_time) stats.max_time = t;
  }

  stats.avg_time = stats.total_time / (float)log->size;

  // --- Second traversal: Calculate standard deviation (Variance) ---
  float sum_of_sq_diff = 0.0f;
  for (size_t i = 0; i < log->size; i++) {
    const float diff = log->data[i] - stats.avg_time;
    sum_of_sq_diff += diff * diff;
  }

  // Use N-1 as denominator (sample standard deviation)
  const float variance =
      (log->size > 1) ? sum_of_sq_diff / (float)(log->size - 1) : 0.0f;
  stats.std_dev = sqrtf(variance);
  return stats;
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
  fprintf(f, "Function,Count,Total(us),Avg(us),Min(us),Max(us),StdDev(us)\n");
  for (size_t func = 0; func < __cprof_entry_count; func++) {
    // Traverse the static log list of current C file
    const TimeLog *current = __cprof_log_entries[func];
    // [KEY] Calculate statistics
    const CPROF_Stats stats = CPROF_calculate_stats(current);
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
  }
  fclose(f);
#else
  ; // do nothing
#endif
}

// Static inline function for memory cleanup
static inline void CPROF_cleanup(void) {
#if defined(PROFILING)
  for (size_t func = 0; func < __cprof_entry_count; func++) {
    // Traverse the static log list of current C file
    TimeLog *current = __cprof_log_entries[func];
    if (!current) return;
    // Free Dynamic Array data within TimeLog
    if (current->data) free(current->data);
    free(current);
    // 3. 將指標清空，避免懸空指標 (Dangling Pointer)
    __cprof_log_entries[func] = NULL;
  }
  __cprof_entry_count = 0;
#else
  ;  // do nothing
#endif
}

#endif  // TINYCPERF_C_PROFILER_H
