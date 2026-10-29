/**
 * Test suite for the JSONL record sink, rotation, the console stream option and the
 * level-change race fix.
 *
 * Each scenario re-initialises the Logger singleton on its own scratch directory and
 * asserts on the files and on SinkStats. std::cout / std::cerr are captured by swapping
 * their stream buffers, which is exactly what the logger writes to.
 *
 * The concurrency scenario can only FAIL deterministically under a race detector:
 *   cmake -DCMAKE_CXX_FLAGS=-fsanitize=thread ...
 * In a normal build it still checks that no record is lost, torn or double counted.
 */

#include <cassert>

import std;
import logger;

using namespace logger;
namespace fs = std::filesystem;

namespace {

auto readLines(fs::path const& p) -> std::vector<std::string> {
    std::ifstream in(p, std::ios::binary);
    std::vector<std::string> lines;
    for (std::string l; std::getline(in, l);) {
        lines.push_back(l);
    }
    return lines;
}

auto readAll(fs::path const& p) -> std::string {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

auto backup(fs::path const& p, int n) -> fs::path {
    return fs::path(p.string() + "." + std::to_string(n));
}

/// Redirects a standard stream into a string for the lifetime of the object.
class Capture {
  public:
    explicit Capture(std::ostream& s) : stream_{s}, old_{s.rdbuf(buf_.rdbuf())} {}
    ~Capture() { stream_.rdbuf(old_); }
    Capture(Capture const&) = delete;
    auto operator=(Capture const&) -> Capture& = delete;
    [[nodiscard]] auto str() const -> std::string { return buf_.str(); }

  private:
    std::ostream& stream_;
    std::ostringstream buf_;
    std::streambuf* old_;
};

auto scratchDir() -> fs::path {
    auto const dir = fs::temp_directory_path() /
                     std::format("cpp23_logger_jsonl_{}",
                                 std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(dir);
    return dir;
}

auto jsonConfig(fs::path const& file) -> LoggerConfig {
    LoggerConfig c;
    c.log_file_path = file.string();
    c.min_level = LogLevel::INFO;
    c.format = LogFormat::JSON;
    c.console_output = false;
    c.announce_initialization = false;
    return c;
}

auto seqLine(int i) -> std::string {
    return std::format("{{\"seq\":{:04},\"pad\":\"xxxx\"}}", i); // 25 bytes + '\n'
}

auto test_jsonl_written_verbatim(fs::path const& dir) -> void {
    std::cout << "Testing JSONL lines written verbatim...\n";
    auto const file = dir / "verbatim.jsonl";
    auto& log = Logger::getInstance();
    log.initialize(jsonConfig(file));

    std::vector<std::string> const records{
        R"({"ts":"2026-10-29T10:00:00Z","level":"INFO","msg":"first"})",
        R"({"msg":"quote \" and backslash \\ and unicode é"})",
        R"({"k":[1,2,3],"nested":{"a":null}})"};
    for (auto const& r : records) {
        assert(log.writeRecord(LogLevel::INFO, r) == RecordWrite::Written);
    }
    assert(readLines(file) == records);
    assert(readAll(file).back() == '\n');

    auto expected_bytes = std::size_t{0};
    for (auto const& r : records) {
        expected_bytes += r.size() + 1;
    }
    assert(fs::file_size(file) == expected_bytes);
    assert(log.getCurrentLogFileSize() == expected_bytes);
    assert(log.getTotalLogsWritten() == 3);
    auto const stats = log.getSinkStats();
    assert(stats.written == 3 && stats.failed == 0 && stats.rejected == 0);

    // Below the minimum level: Filtered, nothing written.
    assert(log.writeRecord(LogLevel::DEBUG, R"({"msg":"quiet"})") == RecordWrite::Filtered);
    assert(log.getSinkStats().filtered == 1);
    assert(readLines(file) == records);
    std::cout << "  ✓ passed\n";
}

auto test_text_methods_refused_in_json(fs::path const& dir) -> void {
    std::cout << "Testing text methods refused and counted in JSON format...\n";
    auto const file = dir / "refused.jsonl";
    auto& log = Logger::getInstance();
    log.initialize(jsonConfig(file));

    log.info("plain text must not reach the file");
    log.warn("value {}", 42);
    log.error("named {x}", {{"x", "y"}});
    auto const stats = log.getSinkStats();
    assert(stats.rejected == 3);
    assert(stats.written == 0);
    assert(readAll(file).empty());

    assert(log.writeRecord(LogLevel::INFO, R"({"ok":true})") == RecordWrite::Written);
    assert(readLines(file) == std::vector<std::string>{R"({"ok":true})"});
    std::cout << "  ✓ passed\n";
}

auto test_newline_rejected(fs::path const& dir) -> void {
    std::cout << "Testing a line holding a newline is rejected...\n";
    auto const file = dir / "newline.jsonl";
    auto& log = Logger::getInstance();
    log.initialize(jsonConfig(file));

    assert(log.writeRecord(LogLevel::INFO, "{\"a\":1}\n{\"b\":2}") == RecordWrite::Rejected);
    assert(log.writeRecord(LogLevel::INFO, "\n") == RecordWrite::Rejected);
    assert(log.getSinkStats().rejected == 2);
    assert(log.getSinkStats().written == 0);
    assert(readAll(file).empty());
    std::cout << "  ✓ passed\n";
}

auto writeSeq(Logger& log, int first, int last) -> void {
    for (int i = first; i <= last; ++i) {
        assert(log.writeRecord(LogLevel::INFO, seqLine(i)) == RecordWrite::Written);
    }
}

auto test_rotation_keeps_backups(fs::path const& dir) -> void {
    std::cout << "Testing rotation at the byte bound with backups...\n";
    auto const file = dir / "rotate.jsonl";
    auto& log = Logger::getInstance();
    auto cfg = jsonConfig(file);
    cfg.max_file_bytes = 60; // two 26-byte records fit, a third does not
    cfg.max_backup_files = 2;
    log.initialize(cfg);

    writeSeq(log, 1, 11);
    assert(readLines(file) == std::vector<std::string>{seqLine(11)});
    assert((readLines(backup(file, 1)) == std::vector<std::string>{seqLine(9), seqLine(10)}));
    assert((readLines(backup(file, 2)) == std::vector<std::string>{seqLine(7), seqLine(8)}));
    assert(!fs::exists(backup(file, 3))); // N backups kept, the oldest removed
    for (auto const& f : {file, backup(file, 1), backup(file, 2)}) {
        assert(fs::file_size(f) <= 60);
    }
    assert(log.getSinkStats().rotations == 5);
    assert(log.getSinkStats().written == 11);
    assert(log.getCurrentLogFileSize() == 26);

    // Explicit rotate(): everything shifts up, the live file is empty.
    log.rotate();
    assert(fs::file_size(file) == 0 && log.getCurrentLogFileSize() == 0);
    assert(readLines(backup(file, 1)) == std::vector<std::string>{seqLine(11)});
    assert((readLines(backup(file, 2)) == std::vector<std::string>{seqLine(9), seqLine(10)}));
    assert(!fs::exists(backup(file, 3)));
    assert(log.getSinkStats().rotations == 6);

    // A line longer than the bound goes into an empty file instead of rotating forever.
    auto const big = std::string(R"({"big":")") + std::string(100, 'z') + R"("})";
    assert(log.writeRecord(LogLevel::INFO, big) == RecordWrite::Written);
    assert(readLines(file) == std::vector<std::string>{big});
    std::cout << "  ✓ passed\n";
}

auto test_rotation_zero_backups_truncates(fs::path const& dir) -> void {
    std::cout << "Testing rotation with zero backups truncates...\n";
    auto const file = dir / "truncate.jsonl";
    auto& log = Logger::getInstance();
    auto cfg = jsonConfig(file);
    cfg.max_file_bytes = 60;
    cfg.max_backup_files = 0;
    log.initialize(cfg);

    writeSeq(log, 1, 11);
    assert(readLines(file) == std::vector<std::string>{seqLine(11)});
    assert(!fs::exists(backup(file, 1)));
    assert(!fs::exists(backup(file, 0)));
    assert(fs::file_size(file) == 26 && log.getCurrentLogFileSize() == 26);
    assert(log.getSinkStats().rotations == 5);
    std::cout << "  ✓ passed\n";
}

auto test_torn_line_repair(fs::path const& dir) -> void {
    std::cout << "Testing torn-line repair on open...\n";
    auto const file = dir / "torn.jsonl";
    {
        std::ofstream out(file, std::ios::binary);
        out << "{\"a\":1}\n{\"b\":"; // crash mid-record: no trailing newline
    }
    auto& log = Logger::getInstance();
    log.initialize(jsonConfig(file));
    assert(log.writeRecord(LogLevel::INFO, R"({"c":3})") == RecordWrite::Written);
    assert((readLines(file) ==
            std::vector<std::string>{R"({"a":1})", R"({"b":)", R"({"c":3})"}));
    assert(log.getCurrentLogFileSize() == fs::file_size(file));

    // A clean file (ends in '\n') gets no extra blank line.
    log.initialize(jsonConfig(file));
    assert(log.writeRecord(LogLevel::INFO, R"({"d":4})") == RecordWrite::Written);
    assert(readLines(file).size() == 4);
    std::cout << "  ✓ passed\n";
}

auto test_sink_failure_is_a_value(fs::path const& dir) -> void {
    std::cout << "Testing sink failure returns SinkFailed without throwing...\n";
    auto const blocker = dir / "blocker";
    {
        std::ofstream(blocker) << "a regular file where a directory is needed";
    }
    auto& log = Logger::getInstance();
    static_assert(noexcept(log.writeRecord(LogLevel::INFO, std::string_view{})));

    auto cfg = jsonConfig(blocker / "sub" / "x.jsonl"); // parent is a file: cannot be created
    {
        Capture init_noise(std::cerr); // initialize() reports the failure on stderr
        log.initialize(cfg);
    }
    auto const failed_after_init = log.getSinkStats().failed;
    assert(failed_after_init >= 1);

    auto const line = std::string(R"({"msg":"must survive"})");
    {
        Capture err(std::cerr);
        assert(log.writeRecord(LogLevel::INFO, line) == RecordWrite::SinkFailed);
        // Console is off, so the line is written to stderr exactly once as a fallback.
        auto const text = err.str();
        assert(text == line + "\n");
    }
    auto const stats = log.getSinkStats();
    assert(stats.failed == failed_after_init + 1);
    assert(!stats.last_error.empty());
    assert(stats.last_error.find("must survive") == std::string::npos); // never record content
    assert(stats.written == 0);

    // The directory exists but the path is itself a directory: only open() fails, so
    // last_error comes from the open failure rather than from create_directories().
    {
        auto const as_dir = dir / "is_a_directory";
        fs::create_directories(as_dir);
        Capture init_noise(std::cerr);
        log.initialize(jsonConfig(as_dir));
        auto const open_stats = log.getSinkStats();
        assert(open_stats.failed == 1);
        assert(!open_stats.last_error.empty());
        assert(log.writeRecord(LogLevel::INFO, line) == RecordWrite::SinkFailed);
        assert(log.getSinkStats().failed == 2);
    }

    // With the console on there is no second copy on stderr.
    cfg.console_output = true;
    {
        Capture init_noise(std::cerr);
        Capture out(std::cout);
        log.initialize(cfg);
        Capture err(std::cerr);
        assert(log.writeRecord(LogLevel::INFO, line) == RecordWrite::SinkFailed);
        assert(err.str().empty());
        assert(out.str() == line + "\n");
    }
    std::cout << "  ✓ passed\n";
}

auto test_reopen_at_most_once_per_second(fs::path const& dir) -> void {
    std::cout << "Testing the file is reopened at most once per second after a failure...\n";
    auto const gate = dir / "gate";
    {
        std::ofstream(gate) << "blocks the directory";
    }
    auto const file = gate / "x.jsonl";
    auto& log = Logger::getInstance();
    {
        Capture init_noise(std::cerr);
        log.initialize(jsonConfig(file));
    }
    // The obstacle goes away, but a reopen was just attempted by initialize().
    fs::remove(gate);
    fs::create_directory(gate);
    {
        Capture err(std::cerr);
        assert(log.writeRecord(LogLevel::INFO, R"({"n":1})") == RecordWrite::SinkFailed);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    assert(log.writeRecord(LogLevel::INFO, R"({"n":2})") == RecordWrite::Written);
    assert(readLines(file) == std::vector<std::string>{R"({"n":2})"});
    std::cout << "  ✓ passed\n";
}

auto test_console_stream_stderr(fs::path const& dir) -> void {
    std::cout << "Testing console_stream Stderr writes nothing to stdout...\n";
    auto& log = Logger::getInstance();
    LoggerConfig cfg;
    cfg.log_file_path = (dir / "console_err.log").string();
    cfg.console_output = true;
    cfg.console_color = false;
    cfg.console_stream = ConsoleStream::Stderr;
    cfg.min_level = LogLevel::INFO;

    Capture out(std::cout);
    Capture err(std::cerr);
    log.initialize(cfg); // announcement is on by default: it must follow the stream
    log.info("text on stderr {}", 7);
    auto json_cfg = cfg;
    json_cfg.format = LogFormat::JSON;
    json_cfg.log_file_path = (dir / "console_err.jsonl").string();
    log.initialize(json_cfg);
    assert(log.writeRecord(LogLevel::INFO, R"({"rec":"on stderr"})") == RecordWrite::Written);

    assert(out.str().empty());
    assert(err.str().find("Logger initialized:") != std::string::npos);
    assert(err.str().find("text on stderr 7") != std::string::npos);
    assert(err.str().find(R"({"rec":"on stderr"})") != std::string::npos);
    std::cout.flush();
    std::cout << "  ✓ passed\n";
}

auto test_announce_initialization_false(fs::path const& dir) -> void {
    std::cout << "Testing announce_initialization false prints nothing...\n";
    auto& log = Logger::getInstance();
    LoggerConfig cfg;
    cfg.log_file_path = (dir / "quiet.log").string();
    cfg.console_output = false;
    cfg.announce_initialization = false;

    Capture out(std::cout);
    Capture err(std::cerr);
    log.initialize(cfg);
    cfg.console_stream = ConsoleStream::Stderr;
    log.initialize(cfg);
    cfg.file_output = false;
    log.initialize(cfg);
    assert(out.str().empty());
    assert(err.str().empty());
    std::cout << "  ✓ passed\n";
}

auto test_file_output_false(fs::path const& dir) -> void {
    std::cout << "Testing file_output false opens nothing...\n";
    auto const file = dir / "never" / "created.log";
    auto& log = Logger::getInstance();
    auto cfg = jsonConfig(file);
    cfg.file_output = false;
    log.initialize(cfg);
    assert(log.writeRecord(LogLevel::INFO, R"({"x":1})") == RecordWrite::Written);
    assert(!fs::exists(file) && !fs::exists(file.parent_path()));
    assert(log.getSinkStats().failed == 0);
    std::cout << "  ✓ passed\n";
}

auto test_old_defaults_unchanged(fs::path const& dir) -> void {
    std::cout << "Testing the old defaults are unchanged...\n";
    LoggerConfig const defaults;
    assert(defaults.console_stream == ConsoleStream::Stdout);
    assert(defaults.announce_initialization);
    assert(defaults.file_output);
    assert(defaults.max_file_bytes == 0);
    assert(defaults.format == LogFormat::TEXT);
    assert(defaults.console_output && defaults.console_color);
    assert(defaults.max_file_size_mb == 100 && defaults.max_backup_files == 5);

    auto& log = Logger::getInstance();
    {
        Capture out(std::cout);
        Capture err(std::cerr);
        LoggerConfig cfg;
        cfg.log_file_path = (dir / "defaults.log").string();
        cfg.console_color = false;
        log.initialize(cfg);
        log.info("hello stdout");
        assert(out.str().find("Logger initialized: " + cfg.log_file_path +
                              " (UTF-8, Color: OFF)") != std::string::npos);
        assert(out.str().find("hello stdout") != std::string::npos);
        assert(err.str().empty());
    }
    {
        // The legacy overload keeps announcing on stdout too.
        Capture out(std::cout);
        log.initialize((dir / "legacy.log").string(), LogLevel::INFO, true);
        assert(out.str().find("Logger initialized:") != std::string::npos);
    }
    auto const lines = readLines(dir / "defaults.log");
    assert(lines.size() == 1 && lines[0].find("hello stdout") != std::string::npos);
    assert(log.getTotalLogsWritten() == 0); // initialize() restarted the counters
    std::cout << "  ✓ passed\n";
}

auto test_level_changes_under_concurrent_logging(fs::path const& dir) -> void {
    std::cout << "Testing level changes are safe under concurrent logging...\n";
    auto const file = dir / "concurrent.jsonl";
    auto& log = Logger::getInstance();
    log.initialize(jsonConfig(file));

    constexpr int threads = 4;
    constexpr int per_thread = 3000;
    auto const record = std::string(R"({"msg":"concurrent record","n":12345})");
    std::atomic<std::uint64_t> written{0};
    std::atomic<std::uint64_t> filtered{0};
    std::atomic<bool> go{false};

    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&] {
            while (!go.load()) {
            }
            for (int i = 0; i < per_thread; ++i) {
                switch (log.writeRecord(LogLevel::INFO, record)) {
                    case RecordWrite::Written:
                        ++written;
                        break;
                    case RecordWrite::Filtered:
                        ++filtered;
                        break;
                    default:
                        assert(false && "unexpected result");
                }
                (void)log.getLevel();
            }
        });
    }
    go = true;
    for (int i = 0; i < 4000; ++i) {
        log.setLevel(i % 2 == 0 ? LogLevel::ERROR : LogLevel::INFO);
    }
    for (auto& w : workers) {
        w.join();
    }

    assert(written + filtered == std::uint64_t{threads} * per_thread);
    auto const stats = log.getSinkStats();
    assert(stats.written == written && stats.filtered == filtered && stats.failed == 0);
    auto const lines = readLines(file);
    assert(lines.size() == written);
    for (auto const& l : lines) {
        assert(l == record);
    }
    log.setLevel(LogLevel::INFO);
    std::cout << "  ✓ passed\n";
}

} // namespace

auto main() -> int {
    auto const dir = scratchDir();
    test_jsonl_written_verbatim(dir);
    test_text_methods_refused_in_json(dir);
    test_newline_rejected(dir);
    test_rotation_keeps_backups(dir);
    test_rotation_zero_backups_truncates(dir);
    test_torn_line_repair(dir);
    test_sink_failure_is_a_value(dir);
    test_reopen_at_most_once_per_second(dir);
    test_console_stream_stderr(dir);
    test_announce_initialization_false(dir);
    test_file_output_false(dir);
    test_old_defaults_unchanged(dir);
    test_level_changes_under_concurrent_logging(dir);

    Logger::getInstance().initialize(
        LoggerConfig{.log_file_path = (dir / "final.log").string(), .announce_initialization = false});
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::cout << "All JSONL sink tests passed\n";
    return 0;
}
