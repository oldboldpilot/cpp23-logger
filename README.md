# C++23 Logger

A production-grade, thread-safe logging system with Mustache-style named templates and fluent API.

## Features

- **Mustache Templates**: Named placeholders `{param_name}` for context-rich logging
- **Fluent API**: Method chaining for ergonomic configuration
- **Thread-Safe**: Concurrent logging without mutex contention
- **C++23 Modules**: Modern module system with clean namespace separation
- **ANSI Colors**: Color-coded console output (optional)
- **Timezone-Aware**: ISO 8601 timestamps with UTC offset
- **JSONL Records**: `LogFormat::JSON` writes caller-serialised records one per line, for machines and AI agents
- **Real Rotation**: `max_file_bytes` and `max_backup_files` bound the disk used
- **Console Stream**: `console_stream` sends console output to stdout (default) or stderr
- **Zero Dependencies**: Self-contained, no external dependencies
- **Smart Pointers**: RAII-compliant resource management

## Requirements

- **Compiler**: Clang 17+, GCC 14+, or MSVC 19.34+ (VS 2022 17.4)
- **Build System**: CMake 3.28+ or Bazel 7.0+
- **C++ Standard**: C++23 with modules support

## Quick Start

### Installation via CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    cpp23_logger
    GIT_REPOSITORY https://github.com/oldboldpilot/cpp23-logger.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(cpp23_logger)

# Link to your target
target_link_libraries(your_target PRIVATE logger)
```

### Embedding in Large Projects (Avoiding Conflicts)

When integrating cpp23-logger into a larger project that may have multiple dependencies using this library, you can customize the build to prevent target name collisions and control library placement:

```cmake
include(FetchContent)

# Configure BEFORE FetchContent_MakeAvailable
# 1. Add a prefix to the target name (e.g., "myproject_logger")
set(CPP23_LOGGER_TARGET_PREFIX "myproject_" CACHE STRING "" FORCE)

# 2. Build as shared or static library
set(CPP23_LOGGER_BUILD_SHARED ON CACHE BOOL "" FORCE)  # or OFF for static

# 3. Control output directory
set(CPP23_LOGGER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib/myproject" CACHE PATH "" FORCE)

FetchContent_Declare(
    cpp23_logger
    GIT_REPOSITORY https://github.com/oldboldpilot/cpp23-logger.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(cpp23_logger)

# Link using the configured target name
target_link_libraries(your_target PRIVATE ${CPP23_LOGGER_LIBRARY_TARGET})

# Or use the alias (always named 'logger')
target_link_libraries(your_target PRIVATE logger)
```

#### Available Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `CPP23_LOGGER_TARGET_PREFIX` | `""` | Prefix for target name (e.g., `"sgee_"` → `sgee_logger`) |
| `CPP23_LOGGER_BUILD_SHARED` | `OFF` | Build as shared library (`ON`) or static (`OFF`) |
| `CPP23_LOGGER_OUTPUT_DIR` | `${CMAKE_BINARY_DIR}/lib` | Output directory for library files |

#### Exported Variables

After `FetchContent_MakeAvailable`, these variables are set:

| Variable | Description |
|----------|-------------|
| `CPP23_LOGGER_TARGET_NAME` | Actual target name (e.g., `myproject_logger`) |
| `CPP23_LOGGER_LIBRARY_TARGET` | Same as above, for linking |
| `LOGGER_STD_PCM` | Path to prebuilt `std.pcm` (for module compatibility) |
| `LOGGER_STD_PCM_DIR` | Directory containing PCM files |

### Installation via Bazel

In your `WORKSPACE` file:

```python
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "cpp23_logger",
    remote = "https://github.com/YOUR_USERNAME/cpp23-logger.git",
    tag = "v1.0.0",
)
```

In your `BUILD.bazel` file:

```python
cc_binary(
    name = "my_app",
    srcs = ["main.cpp"],
    deps = ["@cpp23_logger//:logger"],
)
```

Build your project:

```bash
bazel build //:my_app
```

### Basic Usage

```cpp
import logger;

int main() {
    auto& log = logger::Logger::getInstance()
        .initialize("logs/app.log")
        .setLevel(logger::LogLevel::DEBUG)
        .enableColors(true);

    // Positional formatting
    log.info("Application started");
    log.error("Connection failed: {}, retrying in {}s", error_msg, 5);

    // Named template formatting
    log.info("User {user_id} logged in from {ip}",
        {{"user_id", 12345}, {"ip", "192.168.1.1"}});

    log.warn("Trade rejected: Symbol={symbol} Size={size}",
        logger::Logger::named_args("symbol", "AAPL", "size", 1000));

    return 0;
}
```

### JSONL Records, Rotation and the Console Stream

```cpp
import logger;

logger::LoggerConfig config;
config.log_file_path = "logs/events.jsonl";
config.format = logger::LogFormat::JSON;          // text methods are refused and counted
config.console_stream = logger::ConsoleStream::Stderr;   // keep stdout free (default: Stdout)
config.announce_initialization = false;           // no "Logger initialized" line (default: true)
config.max_file_bytes = 10 * 1024 * 1024;         // rotate here (0: max_file_size_mb)
config.max_backup_files = 3;                      // events.jsonl.1 .. .3; 0 truncates instead

auto& log = logger::Logger::getInstance().initialize(config);
auto const result = log.writeRecord(logger::LogLevel::INFO, R"({"msg":"started"})");
```

`writeRecord()` never throws and returns `RecordWrite::Written`, `Filtered`, `Rejected` (the line
holds a newline) or `SinkFailed`. `getSinkStats()` reports the counters and the last error. Existing
code that sets none of these options behaves exactly as before: a TEXT-format log behaves exactly
as it did before JSON records existed, and never rotates, unless `LoggerConfig::rotate_text` is set
to true. It is off by default so that upgrading does not start rotating (and discarding entries past
`max_backup_files`) logs that grew without bound before. JSON records always rotate, and an explicit
`rotate()` call rotates either format. See [docs/API.md](docs/API.md).

`SinkStats::written` counts every record or entry that reached its sink, including a console-only write
(`file_output` false). `SinkStats::rotations` counts only rotations in which every step worked. After a
failed rotation the next attempt waits one second, so a file that cannot be renamed is not retried on every
write; the file is still kept within its size bound meanwhile. `initialize()` and a successful `rotate()`
clear that wait. A write whose code throws is counted in `SinkStats::failed`, keeps its
reason in `SinkStats::last_error` and falls back to `std::cerr` when the console is off.

### Building from Source

```bash
git clone https://github.com/YOUR_USERNAME/cpp23-logger.git
cd cpp23-logger
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

## API Reference

### Log Levels

```cpp
enum class LogLevel {
    TRACE,     // Extremely detailed debugging
    DEBUG,     // Detailed diagnostic information
    INFO,      // General informational messages
    WARN,      // Warning messages
    ERROR,     // Error messages
    CRITICAL   // Critical system failures
};
```

### Fluent API

```cpp
auto& logger = Logger::getInstance()
    .initialize("logs/app.log")
    .setLevel(LogLevel::DEBUG)
    .enableColors(true)
    .setConsoleOutput(true);
```

### Positional Formatting

```cpp
logger.info("Value: {}, Count: {}", 42, 100);
logger.debug("Processing {}/{} items", current, total);
```

### Named Template Formatting

```cpp
// Method 1: Initializer list
logger.info("User {user_id} at {time}",
    {{"user_id", 123}, {"time", "14:23"}});

// Method 2: named_args helper
logger.warn("Error {code}: {message}",
    Logger::named_args("code", 404, "message", "Not found"));

// Method 3: Complex multi-parameter
logger.debug("Simulation[{sim_id}] Step[{step}]: Temp={temp}°C",
    {
        {"sim_id", simulation_id},
        {"step", current_step},
        {"temp", temperature}
    });
```

## Thread Safety

The logger is fully thread-safe:

- `Logger::format()` is static with no shared state
- `Logger::processTemplate()` is static with no shared state
- All logging methods are mutex-protected
- Multiple threads can log concurrently without corruption
- `setLevel()` / `getLevel()` are safe while other threads log (the level is atomic)
- Zero contention for formatting operations

## Performance

- ~5-30μs per log call (buffered writes)
- Stack-allocated formatting (minimal heap allocations)
- Named template lookup: O(1) average via hash map
- Immediate flush to disk for reliability

## Examples

See the [examples/](examples/) directory for complete working examples:

- `basic_usage.cpp` - Simple positional formatting
- `named_templates.cpp` - Mustache-style templates

## Testing

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

`tests/test_logger_records.cpp` covers the rotation-failure accounting, console-only counting and the exception path.
`tests/test_jsonl_sink.cpp` covers the JSONL sink, rotation, the console stream and the level race.
It needs Clang and the std module PCM. To make its concurrency check detect a data race, configure
with `-DCMAKE_CXX_FLAGS=-fsanitize=thread` and link with the same flag.

## License

BSD 4-Clause License with Author's Special Rights. See [LICENSE](LICENSE) for details.

## Author

Olumuyiwa Oluwasanmi

## Version

1.0.0 (November 2025)
