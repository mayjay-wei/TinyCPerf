//
// Created by TCWei on 12/16/25.
//

#ifndef TINYCPERF_C_PROFILER_H
#define TINYCPERF_C_PROFILER_H
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
typedef struct timespec TimePoint;

typedef struct TimeLog {
  const char *name;
  uint64_t *data;
  size_t size;
  size_t capacity;
} TimeLog;

// 假設一個 C 檔案中最多有 1024 個不同的 Profiling 範圍
#define CPROF_MAX_ENTRIES 1024u
#define CPROF_NSEC_PER_USEC (1000u)

// 靜態陣列：每個 C 檔獨立的指標陣列 (內部連結)
// 這些指標初始為 NULL，用於儲存動態分配的 TimeLog 結構體
static TimeLog *__cprof_log_entries[CPROF_MAX_ENTRIES] = {NULL};
// 檔案中已註冊的 Log 總數 (也用於遍歷陣列)
static size_t __cprof_entry_count = 0;

// Store statistics for a single function
typedef struct {
  const char *name;
  size_t count;
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  size_t max_count;
  uint64_t avg_time;
  uint64_t std_dev;  // Standard Deviation
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

static uint64_t TimeDiff(const TimePoint *end_time,
                         const TimePoint *start_time) {
  int64_t sec_diff = end_time->tv_sec - start_time->tv_sec;
  int64_t nsec_diff = end_time->tv_nsec - start_time->tv_nsec;
  int64_t total_diff = sec_diff * 1000000000L + nsec_diff;

  // Prevent negative results from wrong argument order
  if (total_diff < 0) {
    fprintf(stderr,
            "CPROF Warning: Negative time difference detected. "
            "Check argument order in TimeDiff(end, start)\n");
    return 0;  // Return 0 instead of huge positive number
  }

  return (uint64_t)total_diff;
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
#define CPROF_SCOPE_TAG(func_name, tag, code)                                 \
  do {                                                                        \
    /* 1. Create a unique pointer in the code */                              \
    /* but the time log can be shared with other code */                      \
    static TimeLog *__local_log_ptr = NULL;                                   \
    /* 2. Find the pointer of the log in global or create one */              \
    /*if (!__local_log_ptr) { */                                              \
    char __combined_name[256];                                                \
    /* 判斷 tag 的型別並格式化 (這裡簡化處理，支援整數與字串) */              \
    /* 如果你的環境支援 C11，可以用 _Generic，否則用 printf 格式化 */         \
    if (__builtin_types_compatible_p(__typeof__(tag), char *) ||              \
        __builtin_types_compatible_p(__typeof__(tag), const char *)) {        \
      snprintf(__combined_name, sizeof(__combined_name), "%s@%s", #func_name, \
               (char *)(size_t)tag);                                          \
    } else {                                                                  \
      snprintf(__combined_name, sizeof(__combined_name), "%s@%lld",           \
               #func_name, (long long)tag);                                   \
    }                                                                         \
    __local_log_ptr = CPROF_GetOrCreateLog(__combined_name);                  \
    /*}*/                                                                     \
    if (__local_log_ptr) {                                                    \
      /* 2. Start timing */                                                   \
      TimePoint start_time = CPROF_StartProfile();                            \
      /* 3. Execute code */                                                   \
      {code} /* 4. End timing */                                              \
      TimePoint end_time = CPROF_StopProfile();                               \
      /* 5. Calculate duration (convert to nanoseconds) */                    \
      uint64_t duration = TimeDiff(&end_time, &start_time);                   \
      /* 6. Write to dedicated TimeLog variable */                            \
      TimeLog_push(__local_log_ptr, duration);                                \
    }                                                                         \
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
  log->data = malloc(log->capacity * sizeof(uint64_t));
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
  uint64_t *new_data = realloc(log->data, new_capacity * sizeof(uint64_t));
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
static inline void TimeLog_push(TimeLog *log, const uint64_t duration) {
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
  stats.total_time = 0;

  uint64_t first_val_ns = log->data[0];
  stats.max_time = stats.min_time = first_val_ns;
  stats.max_count = 0;

  // --- First traversal: Calculate sum, Min and Max ---
  for (size_t i = 0; i < log->size; i++) {
    const uint64_t t_ns = log->data[i];
    stats.total_time += t_ns;
    if (t_ns < stats.min_time) stats.min_time = t_ns;
    if (t_ns > stats.max_time) {
      stats.max_time = t_ns;
      stats.max_count = i;
    }
  }

  stats.avg_time = stats.total_time / log->size;

  // --- Second traversal: Calculate standard deviation (Variance) ---
  float sum_of_sq_diff = 0.0f;
  for (size_t i = 0; i < log->size; i++) {
    const float t_ns = (float)(log->data[i]);
    const float diff = t_ns - stats.avg_time;
    sum_of_sq_diff += diff * diff;
  }

  // Use N-1 as denominator (sample standard deviation)
  const float variance =
      (log->size > 1) ? sum_of_sq_diff / (float)(log->size - 1) : 0.0f;
  stats.std_dev = (uint64_t)sqrtf(variance);
  return stats;
}

#define CPROF_BINS (10u)
#define CPROF_LABEL_UNDERFLOW "Below range"
#define CPROF_LABEL_OVERFLOW "Above range"
static inline void CPROF_dump_histogram(FILE *f, const TimeLog *log,
                                        const CPROF_Stats *stats) {
  if (log->size == 0) return;

  // Stats and data are in nanoseconds; conversion to microseconds happens
  // during output

  // Use average ± 2 standard deviations as the core observation range
  uint64_t range_start = (stats->avg_time > (2 * stats->std_dev))
                             ? stats->avg_time - (2 * stats->std_dev)
                             : 0;
  if (range_start < stats->min_time) range_start = stats->min_time;

  uint64_t range_end = stats->avg_time + (2 * stats->std_dev);
  if (range_end > stats->max_time) range_end = stats->max_time;

  // If the computed range is empty or degenerate, treat all samples as a single
  // bin.
  // The second condition collapses very narrow ranges: when the total span is
  // ≤ CPROF_BINS microseconds (CPROF_NSEC_PER_USEC * CPROF_BINS in ns), using
  // CPROF_BINS bins would produce sub-microsecond (overly fine) buckets in the
  // output, which is reported in microseconds. In that case we prefer a single
  // aggregate bin for clearer, less noisy histograms.
  if (range_end <= range_start ||
      range_end - range_start <= CPROF_NSEC_PER_USEC * CPROF_BINS) {
    fprintf(f, "%s,1,\"(%ld,%ld]\",%zu\n", log->name,
            range_start / CPROF_NSEC_PER_USEC,
            range_start / CPROF_NSEC_PER_USEC + 1, log->size);
    return;
  }
  const uint64_t step = (range_end - range_start) / CPROF_BINS;
  if (step == 0) return;  // Prevent division by zero

  size_t counts[CPROF_BINS] = {0};
  size_t out_of_range_low = 0;
  size_t out_of_range_high = 0;

  for (size_t i = 0; i < log->size; i++) {
    uint64_t val_ns = log->data[i];
    if (val_ns < range_start) {
      out_of_range_low++;
    } else if (val_ns >= range_end) {
      out_of_range_high++;
    } else {
      uint64_t bin_idx = (val_ns - range_start) / step;
      if (bin_idx >= CPROF_BINS) bin_idx = CPROF_BINS - 1;
      counts[bin_idx]++;
    }
  }

  // Output histogram data to file (in microseconds)
  fprintf(f, "%s,%s,(<%ld),%zu\n", log->name, CPROF_LABEL_UNDERFLOW,
          range_start / CPROF_NSEC_PER_USEC, out_of_range_low);
  for (size_t bin = 0; bin < CPROF_BINS; bin++) {
    fprintf(
        f, "%s,%ld,\"(%ld,%ld]\",%zu\n", log->name, step / CPROF_NSEC_PER_USEC,
        (range_start + bin * step) / CPROF_NSEC_PER_USEC,
        (range_start + (bin + 1) * step) / CPROF_NSEC_PER_USEC, counts[bin]);
  }
  fprintf(f, "%s,%s,(>%ld),%zu\n", log->name, CPROF_LABEL_OVERFLOW,
          range_end / CPROF_NSEC_PER_USEC, out_of_range_high);
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
    fprintf(f, "%s,%zu,%ld,%ld,%ld,%ld,%zu,%ld\n", stat.name, stat.count,
            stat.total_time / CPROF_NSEC_PER_USEC,  // Total Time (us)
            stat.avg_time / CPROF_NSEC_PER_USEC,    // Avg Time (us)
            stat.min_time / CPROF_NSEC_PER_USEC,    // Min Time (us)
            stat.max_time / CPROF_NSEC_PER_USEC,    // Max Time (us)
            stat.max_count,                         // Max Occur (count)
            stat.std_dev / CPROF_NSEC_PER_USEC      // StdDev (us)
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
