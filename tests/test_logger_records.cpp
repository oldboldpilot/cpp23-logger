/**
 * Tests of Logger::writeRecord() and the rotation accounting: a failed rotation is not counted and is not retried on
 * every write, a console-only write is counted, and an exception in the write path keeps its reason and falls back to
 * stderr.
 */

#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

import logger;

using namespace logger;

namespace {

// NDEBUG builds drop assert(): a check that exits.
auto check(bool condition, char const* what) -> void {
    if (!condition) {
        std::cerr << "FAILED: " << what << '\n';
        std::exit(1);
    }
}

// A fresh directory under the temp directory, unique per process and per call.
auto uniqueDir(std::string const& tag) -> std::filesystem::path {
    static int counter = 0;
    auto const dir = std::filesystem::temp_directory_path() /
                     ("cpp23-logger-records-" + tag + "-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
    std::filesystem::remove_all(dir);
    return dir;
}

auto release() -> void {
    LoggerConfig config;
    config.console_output = false;
    config.file_output = false;
    Logger::getInstance().initialize(config);
}

// A stream buffer that refuses every byte.
class ThrowingBuffer final : public std::streambuf {
  protected:
    auto overflow(int_type) -> int_type override { throw std::runtime_error("injected"); }
    auto xsputn(char const*, std::streamsize) -> std::streamsize override { throw std::runtime_error("injected"); }
};

auto test_failed_rotation_is_not_counted_or_hammered() -> void {
    auto const dir = uniqueDir("rotation");
    std::filesystem::create_directories(dir / "x.jsonl.1" / "keep"); // a non-empty directory: the rename must fail
    LoggerConfig config;
    config.log_file_path = (dir / "x.jsonl").string();
    config.format = LogFormat::JSON;
    config.console_output = false;
    config.console_color = false;
    config.max_file_bytes = 4096;
    config.max_backup_files = 1;
    Logger::getInstance().initialize(config);
    std::string const line = "{\"pad\":\"" + std::string(100, 'x') + "\"}";
    for (int i = 0; i < 1000; ++i) {
        (void)Logger::getInstance().writeRecord(LogLevel::INFO, line);
    }
    auto const stats = Logger::getInstance().getSinkStats();
    release();
    auto const live_size = std::filesystem::file_size(dir / "x.jsonl");
    std::filesystem::remove_all(dir);
    check(live_size <= 4096, "the file stays bounded while waiting");
    check(stats.rotations == 0, "a rotation whose rename failed is not counted");
    check(stats.failed > 0, "the failure is counted");
    check(stats.failed < 10, "a failed rotation is retried at most once a second, not on every write");
}

auto test_console_only_write_is_counted() -> void {
    LoggerConfig config;
    config.format = LogFormat::JSON;
    config.console_output = true;
    config.console_color = false;
    config.console_stream = ConsoleStream::Stderr;
    config.file_output = false;
    Logger::getInstance().initialize(config);
    std::ostringstream captured;
    auto* previous = std::cerr.rdbuf(captured.rdbuf());
    auto const result = Logger::getInstance().writeRecord(LogLevel::INFO, "{\"a\":1}");
    std::cerr.rdbuf(previous);
    auto const stats = Logger::getInstance().getSinkStats();
    release();
    check(result == RecordWrite::Written, "a console-only record is written");
    check(captured.str() == "{\"a\":1}\n", "and reaches the console");
    check(stats.written == 1, "and is counted");
}

auto test_exception_in_the_write_path_keeps_its_reason() -> void {
    LoggerConfig config;
    config.format = LogFormat::JSON;
    config.console_output = true;
    config.console_color = false;
    config.console_stream = ConsoleStream::Stderr;
    config.file_output = false;
    Logger::getInstance().initialize(config);
    ThrowingBuffer throwing;
    auto* previous = std::cerr.rdbuf(&throwing);
    auto const mask = std::cerr.exceptions();
    std::cerr.exceptions(std::ios::badbit);
    auto const result = Logger::getInstance().writeRecord(LogLevel::INFO, "{\"a\":1}");
    std::cerr.exceptions(mask);
    std::cerr.rdbuf(previous);
    std::cerr.clear();
    auto const stats = Logger::getInstance().getSinkStats();
    release();
    check(result == RecordWrite::SinkFailed, "an exception is SinkFailed, not an escape");
    check(stats.failed >= 1, "the failure is counted");
    check(!stats.last_error.empty(), "the reason is kept");
}

// A failed rotation delays the next attempt; initialize() must clear that delay, or a new run's first rotation waits.
auto test_initialize_clears_the_rotation_delay() -> void {
    auto const dir = uniqueDir("reinit");
    std::filesystem::create_directories(dir / "x.jsonl.1" / "keep");
    LoggerConfig config;
    config.log_file_path = (dir / "x.jsonl").string();
    config.format = LogFormat::JSON;
    config.console_output = false;
    config.console_color = false;
    config.max_file_bytes = 4096;
    config.max_backup_files = 1;
    Logger::getInstance().initialize(config);
    std::string const line = "{\"pad\":\"" + std::string(100, 'x') + "\"}";
    for (int i = 0; i < 100; ++i) {
        (void)Logger::getInstance().writeRecord(LogLevel::INFO, line);
    }
    // Second run: the obstacle is gone, so the first rotation must happen at once, inside the one-second delay.
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    Logger::getInstance().initialize(config);
    for (int i = 0; i < 100; ++i) {
        (void)Logger::getInstance().writeRecord(LogLevel::INFO, line);
    }
    auto const stats = Logger::getInstance().getSinkStats();
    release();
    std::filesystem::remove_all(dir);
    check(stats.rotations > 0, "after initialize() the first rotation is not delayed");
}

// A successful explicit rotate() ends the wait after a failed rotation: the next size-bound rotation is not delayed.
auto test_explicit_rotate_clears_the_rotation_delay() -> void {
    auto const dir = uniqueDir("explicit");
    std::filesystem::create_directories(dir / "x.jsonl.1" / "keep");
    LoggerConfig config;
    config.log_file_path = (dir / "x.jsonl").string();
    config.format = LogFormat::JSON;
    config.console_output = false;
    config.console_color = false;
    config.max_file_bytes = 4096;
    config.max_backup_files = 1;
    Logger::getInstance().initialize(config);
    std::string const line = "{\"pad\":\"" + std::string(100, 'x') + "\"}";
    for (int i = 0; i < 100; ++i) {
        (void)Logger::getInstance().writeRecord(LogLevel::INFO, line);
    }
    // The obstacle goes away and an explicit rotate() succeeds, inside the one-second delay.
    std::filesystem::remove_all(dir / "x.jsonl.1");
    Logger::getInstance().rotate();
    auto const after_rotate = Logger::getInstance().getSinkStats().rotations;
    for (int i = 0; i < 100; ++i) {
        (void)Logger::getInstance().writeRecord(LogLevel::INFO, line);
    }
    auto const stats = Logger::getInstance().getSinkStats();
    release();
    std::filesystem::remove_all(dir);
    check(after_rotate > 0, "the explicit rotate() worked");
    check(stats.rotations > after_rotate, "after a successful rotate() the next rotation is not delayed");
}

} // namespace

auto main() -> int {
    test_failed_rotation_is_not_counted_or_hammered();
    test_initialize_clears_the_rotation_delay();
    test_explicit_rotate_clears_the_rotation_delay();
    test_console_only_write_is_counted();
    test_exception_in_the_write_path_keeps_its_reason();
    std::cout << "test_logger_records: all passed\n";
    return 0;
}
