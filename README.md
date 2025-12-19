# TinyCPerf

![C](https://img.shields.io/badge/language-C-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)

A lightweight, header-only C library for high-precision function profiling with minimal overhead. TinyCPerf provides an easy-to-use interface for measuring execution time of code blocks with nanosecond precision using Linux's `CLOCK_MONOTONIC`, outputting results in microseconds for readability.

## Features

- 🚀 **Header-only**: Single include, no separate compilation needed
- ⚡ **Minimal overhead**: Static inline functions with compiler optimization
- 📊 **Statistical analysis**: Automatic calculation of min, max, average, and standard deviation
- � **Histogram analysis**: Automatic generation of timing distribution histograms (10 bins)
- �💾 **CSV export**: Easy data analysis with spreadsheet applications
- 🔒 **Thread-safe**: Per-file static storage with no global conflicts
- 🧪 **Multi-testcase support**: Profile multiple test scenarios for the same function
- 📝 **Simple API**: Just 2 macros to get started

## Quick Start

### Basic Usage

```c
#define PROFILING  // Enable profiling (should be defined before including header)
#include "c_profiler.h"

void my_function(int n) {
    // Your original code here
    for (int i = 0; i < n; i++) {
        // Some computation
    }
}

int main() {
    // 1. Profile function calls using CPROF_SCOPE
    // No need to declare logs - they're created automatically!
    CPROF_SCOPE(my_function, {
        my_function(1000);
    });
    
    // 2. Profile multiple test cases for the same function
    CPROF_SCOPE_TAG(my_function, "small", {
        my_function(100);  // Small dataset test
    });
    
    CPROF_SCOPE_TAG(my_function, "large", {
        my_function(10000); // Large dataset test
    });
    
    CPROF_SCOPE(another_function, {
        // Profile any code block
        int sum = 0;
        for (int i = 0; i < 10000; i++) {
            sum += i * i;
        }
    });
    
    // 3. Export results to CSV file
    CPROF_dump_to_file("profiling_report.csv");
    
    // 4. Clean up allocated memory
    CPROF_cleanup();
    
    return 0;
}
```

### Sample Output

The library generates a CSV file with detailed timing statistics and histogram data:

```csv
Function,Count,Total(us),Avg(us),Min(us),Max(us),Max(count),StdDev(us)
my_function@0,5,1250.5,250.1,245.2,255.8,2,4.2
my_function@small,3,180.2,60.1,58.5,62.1,1,1.8
my_function@large,2,2150.8,1075.4,1070.1,1080.7,0,7.5
another_function@0,3,875.3,291.8,289.1,295.4,1,3.2

# --- Histogram Data ---
Function,step(us),"Bin(Start,End](us)",Count
my_function@0,Underflow,(<237.7),0
my_function@0,2.5,"(237.7,240.2]",1
my_function@0,2.5,"(240.2,242.7]",0
my_function@0,2.5,"(242.7,245.2]",1
my_function@0,2.5,"(245.2,247.7]",0
my_function@0,2.5,"(247.7,250.2]",1
my_function@0,2.5,"(250.2,252.7]",1
my_function@0,2.5,"(252.7,255.2]",0
my_function@0,2.5,"(255.2,257.7]",1
my_function@0,2.5,"(257.7,260.2]",0
my_function@0,2.5,"(260.2,262.5]",0
my_function@0,Overflow,(>262.5),0
```

**Output Format:**
- **Statistics Section**: Basic timing statistics in microseconds
- **Histogram Section**: Distribution analysis with 10 bins based on avg ± 2×std deviation range
- **Max(count)**: Index of the measurement with the maximum execution time

### Data Visualization

The CSV output format is designed to be **Microsoft Excel friendly** for easy data visualization:

1. **Import the CSV file** directly into Excel
2. **Select the histogram data** (Function, Bin ranges, Count columns)
3. **Create charts** using Excel's built-in chart tools:
   - Column charts for timing distribution
   - Line charts for trend analysis
   - Scatter plots for performance correlation

The histogram data format with clearly labeled bin ranges makes it simple to generate professional timing distribution charts for performance analysis reports.

**Example Excel Chart Output:**

![Excel Histogram Demo](data/excel_histogram_demo.png)

*The above chart shows a typical timing distribution histogram generated from TinyCPerf data in Microsoft Excel, displaying the frequency distribution of execution times across different bins.*

## API Reference

### Core Macros

#### `CPROF_SCOPE(func_name, code_block)`
Profiles the execution time of the given code block. The profiling log is created automatically - no need to declare it beforehand!

**Parameters:**
- `func_name`: Unique identifier for the function (typically the actual function name)
- `code_block`: Code to be profiled, enclosed in braces `{}`

**Example:**
```c
CPROF_SCOPE(matrix_multiply, {
    matrix_multiply(matA, matB, result, size);
});

// You can use the same function name multiple times
// - timing data will be aggregated automatically
CPROF_SCOPE(matrix_multiply, {
    matrix_multiply(matC, matD, result2, size);
});
```

#### `CPROF_SCOPE_TAG(func_name, tag, code_block)`
Profiles the execution time with a specific tag to distinguish different test cases for the same function.

**Parameters:**
- `func_name`: Base function identifier
- `tag`: String or integer tag to distinguish test cases (e.g., "small", "large", 1, 2)
- `code_block`: Code to be profiled, enclosed in braces `{}`

**Example:**
```c
// Different test scenarios for the same function
CPROF_SCOPE_TAG(sort_algorithm, "100_items", {
    sort_algorithm(array_100, 100);
});

CPROF_SCOPE_TAG(sort_algorithm, "1000_items", {
    sort_algorithm(array_1000, 1000);
});

CPROF_SCOPE_TAG(sort_algorithm, 1, {
    sort_algorithm(random_data, size);
});
```

#### `CPROF_dump_to_file(filename)`
Exports all profiling data from the current compilation unit to a CSV file with statistical analysis and histogram distribution.

**Parameters:**
- `filename`: Path to the output CSV file

**Example:**
```c
CPROF_dump_to_file("performance_report.csv");
```

**Output includes:**
- Basic timing statistics (count, total, avg, min, max, std dev)
- Timing distribution histogram with 10 bins
- Overflow/underflow counts for values outside the histogram range

#### `CPROF_cleanup()`
Releases all dynamically allocated memory used by the profiling system. Call this before program termination.

**Example:**
```c
CPROF_cleanup();
```

## Building

### Prerequisites

- C11 compatible compiler (GCC, Clang)
- Linux system with POSIX.1-2001 support
- CMake 4.1+ (for provided build configuration)

### Compilation Flags

**Important**: To enable profiling, you must define `PROFILING` before including the header:

```c
#define PROFILING  // Must be defined to enable profiling
#include "c_profiler.h"
```

Or pass it as a compiler flag:
```bash
gcc -DPROFILING -std=c11 -O2 -I./inc src/main.c -lm -o tinycperf_example
```

### Using CMake

```bash
mkdir build
cd build
cmake ..
make
```

### Manual Compilation

```bash
# With profiling enabled
gcc -DPROFILING -std=c11 -O2 -I./inc src/main.c -lm -o tinycperf_example

# Without profiling (CPROF_SCOPE becomes no-op)
gcc -std=c11 -O2 -I./inc src/main.c -lm -o tinycperf_example
```

## Architecture

### Per-File Profiling

TinyCPerf uses file-scoped static variables, meaning:
- ✅ Each `.c` file maintains its own independent profiling data
- ✅ No naming conflicts between different compilation units
- ❌ Cross-file timing aggregation is not currently supported

### Memory Management

- **Automatic log creation**: Profiling logs are created automatically when first used
- Dynamic arrays automatically resize as needed (initial capacity: 1024 entries)
- All memory is allocated per-log and cleaned up with `CPROF_cleanup()`
- Static variables ensure no memory leaks between profiling sessions

### Precision

- Timing precision: Nanosecond (using `clock_gettime(CLOCK_MONOTONIC)`)
- Output precision: Microsecond (for readability)
- Statistical calculations use sample standard deviation (N-1 denominator)
- Histogram bins: 10 bins by default, covering avg ± 2×std deviation range

## Limitations

1. **Entry limit**: Maximum 1024 total profiling entries across all functions and test cases
2. **Single compilation unit**: Profiling data is isolated per `.c` file
3. **Linux only**: Currently depends on `CLOCK_MONOTONIC`
4. **Function naming**: Log names must be valid C identifiers
5. **Static storage**: Cannot profile recursively without separate log names
6. **Compilation flag**: Must define `PROFILING` to enable profiling functionality

## Contributing

Contributions are welcome! Areas for improvement:

- [ ] Windows platform support
- [ ] Optimize on profiling loading

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**Author**: TCWei  
**Created**: December 16, 2025