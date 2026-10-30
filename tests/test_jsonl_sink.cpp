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

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

import std;
import logger;

using namespace logger;
namespace fs = std::filesystem;

namespace {

/// Aborts with the call site when `ok` is false. Unlike assert() it cannot be compiled out.
auto check(bool ok, std::source_location const loc = std::source_location::current()) -> void {
    if (!ok) {
        std::cerr << "CHECK FAILED at line " << loc.line() << '\n';
        std::abort();
    }
}

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
        check(log.writeRecord(LogLevel::INFO, r) == RecordWrite::Written);
    }
    check(readLines(file) == records);
    check(readAll(file).back() == '\n');

    auto expected_bytes = std::size_t{0};
    for (auto const& r : records) {
        expected_bytes += r.size() + 1;
    }
    check(fs::file_size(file) == expected_bytes);
    check(log.getCurrentLogFileSize() == expected_bytes);
    check(log.getTotalLogsWritten() == 3);
    auto const stats = log.getSinkStats();
    check(stats.written == 3 && stats.failed == 0 && stats.rejected == 0);

    // Below the minimum level: Filtered, nothing written.
    check(log.writeRecord(LogLevel::DEBUG, R"({"msg":"quiet"})") == RecordWrite::Filtered);
    check(log.getSinkStats().filtered == 1);
    check(readLines(file) == records);
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
    check(stats.rejected == 3);
    check(stats.written == 0);
    check(readAll(file).empty());

    check(log.writeRecord(LogLevel::INFO, R"({"ok":true})") == RecordWrite::Written);
    check(readLines(file) == std::vector<std::string>{R"({"ok":true})"});
    std::cout << "  ✓ passed\n";
}

auto test_newline_rejected(fs::path const& dir) -> void {
    std::cout << "Testing a line holding a newline is rejected...\n";
    auto const file = dir / "newline.jsonl";
    auto& log = Logger::getInstance();
    log.initialize(jsonConfig(file));

    check(log.writeRecord(LogLevel::INFO, "{\"a\":1}\n{\"b\":2}") == RecordWrite::Rejected);
    check(log.writeRecord(LogLevel::INFO, "\n") == RecordWrite::Rejected);
    check(log.getSinkStats().rejected == 2);
    check(log.getSinkStats().written == 0);
    check(readAll(file).empty());
    std::cout << "  ✓ passed\n";
}

auto writeSeq(Logger& log, int first, int last) -> void {
    for (int i = first; i <= last; ++i) {
        check(log.writeRecord(LogLevel::INFO, seqLine(i)) == RecordWrite::Written);
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
    check(readLines(file) == std::vector<std::string>{seqLine(11)});
    check((readLines(backup(file, 1)) == std::vector<std::string>{seqLine(9), seqLine(10)}));
    check((readLines(backup(file, 2)) == std::vector<std::string>{seqLine(7), seqLine(8)}));
    check(!fs::exists(backup(file, 3))); // N backups kept, the oldest removed
    for (auto const& f : {file, backup(file, 1), backup(file, 2)}) {
        check(fs::file_size(f) <= 60);
    }
    check(log.getSinkStats().rotations == 5);
    check(log.getSinkStats().written == 11);
    check(log.getCurrentLogFileSize() == 26);

    // Explicit rotate(): everything shifts up, the live file is empty.
    log.rotate();
    check(fs::file_size(file) == 0 && log.getCurrentLogFileSize() == 0);
    check(readLines(backup(file, 1)) == std::vector<std::string>{seqLine(11)});
    check((readLines(backup(file, 2)) == std::vector<std::string>{seqLine(9), seqLine(10)}));
    check(!fs::exists(backup(file, 3)));
    check(log.getSinkStats().rotations == 6);

    // A line longer than the bound goes into an empty file instead of rotating forever.
    auto const big = std::string(R"({"big":")") + std::string(100, 'z') + R"("})";
    check(log.writeRecord(LogLevel::INFO, big) == RecordWrite::Written);
    check(readLines(file) == std::vector<std::string>{big});
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
    check(readLines(file) == std::vector<std::string>{seqLine(11)});
    check(!fs::exists(backup(file, 1)));
    check(!fs::exists(backup(file, 0)));
    check(fs::file_size(file) == 26 && log.getCurrentLogFileSize() == 26);
    check(log.getSinkStats().rotations == 5);
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
    check(log.writeRecord(LogLevel::INFO, R"({"c":3})") == RecordWrite::Written);
    check((readLines(file) ==
            std::vector<std::string>{R"({"a":1})", R"({"b":)", R"({"c":3})"}));
    check(log.getCurrentLogFileSize() == fs::file_size(file));

    // A clean file (ends in '\n') gets no extra blank line.
    log.initialize(jsonConfig(file));
    check(log.writeRecord(LogLevel::INFO, R"({"d":4})") == RecordWrite::Written);
    check(readLines(file).size() == 4);
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
    check(log.getSinkStats().last_error == std::make_error_code(std::errc::not_a_directory).message());
    check(failed_after_init >= 1);

    auto const line = std::string(R"({"msg":"must survive"})");
    {
        Capture err(std::cerr);
        check(log.writeRecord(LogLevel::INFO, line) == RecordWrite::SinkFailed);
        // Console is off, so the line is written to stderr exactly once as a fallback.
        auto const text = err.str();
        check(text == line + "\n");
    }
    auto const stats = log.getSinkStats();
    check(stats.failed == failed_after_init + 1);
    check(!stats.last_error.empty());
    check(stats.last_error.find("must survive") == std::string::npos); // never record content
    check(stats.written == 0);

    // The directory exists but the path is itself a directory: only open() fails, so
    // last_error comes from the open failure rather than from create_directories().
    {
        auto const as_dir = dir / "is_a_directory";
        fs::create_directories(as_dir);
        Capture init_noise(std::cerr);
        log.initialize(jsonConfig(as_dir));
        auto const open_stats = log.getSinkStats();
        check(open_stats.failed == 1);
        check(!open_stats.last_error.empty());
        check(open_stats.last_error == std::make_error_code(std::errc::is_a_directory).message());
        check(log.writeRecord(LogLevel::INFO, line) == RecordWrite::SinkFailed);
        check(log.getSinkStats().failed == 2);
    }

    // A directory the process may not write to: open() reports permission denied, not a bare I/O error.
    // Root ignores directory permissions, so the check is skipped there.
#if defined(__unix__) || defined(__APPLE__)
    if (::geteuid() != 0) {
        auto const locked = dir / "locked";
        fs::create_directories(locked);
        fs::permissions(locked, fs::perms::owner_read | fs::perms::owner_exec);
        {
            Capture init_noise(std::cerr);
            log.initialize(jsonConfig(locked / "x.jsonl"));
        }
        auto const denied = log.getSinkStats().last_error;
        fs::permissions(locked, fs::perms::owner_all);
        check(denied == std::make_error_code(std::errc::permission_denied).message());
    }
#endif

    // With the console on there is no second copy on stderr.
    cfg.console_output = true;
    {
        Capture init_noise(std::cerr);
        Capture out(std::cout);
        log.initialize(cfg);
        Capture err(std::cerr);
        check(log.writeRecord(LogLevel::INFO, line) == RecordWrite::SinkFailed);
        check(err.str().empty());
        check(out.str() == line + "\n");
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
        check(log.writeRecord(LogLevel::INFO, R"({"n":1})") == RecordWrite::SinkFailed);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    check(log.writeRecord(LogLevel::INFO, R"({"n":2})") == RecordWrite::Written);
    check(readLines(file) == std::vector<std::string>{R"({"n":2})"});
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
    check(log.writeRecord(LogLevel::INFO, R"({"rec":"on stderr"})") == RecordWrite::Written);

    check(out.str().empty());
    check(err.str().find("Logger initialized:") != std::string::npos);
    check(err.str().find("text on stderr 7") != std::string::npos);
    check(err.str().find(R"({"rec":"on stderr"})") != std::string::npos);
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
    check(out.str().empty());
    check(err.str().empty());
    std::cout << "  ✓ passed\n";
}

auto test_file_output_false(fs::path const& dir) -> void {
    std::cout << "Testing file_output false opens nothing...\n";
    auto const file = dir / "never" / "created.log";
    auto& log = Logger::getInstance();
    auto cfg = jsonConfig(file);
    cfg.file_output = false;
    log.initialize(cfg);
    check(log.writeRecord(LogLevel::INFO, R"({"x":1})") == RecordWrite::Written);
    check(!fs::exists(file) && !fs::exists(file.parent_path()));
    check(log.getSinkStats().failed == 0);
    std::cout << "  ✓ passed\n";
}

auto test_old_defaults_unchanged(fs::path const& dir) -> void {
    std::cout << "Testing the old defaults are unchanged...\n";
    LoggerConfig const defaults;
    check(defaults.console_stream == ConsoleStream::Stdout);
    check(defaults.announce_initialization);
    check(defaults.file_output);
    check(defaults.max_file_bytes == 0);
    check(defaults.format == LogFormat::TEXT);
    check(defaults.console_output && defaults.console_color);
    check(!defaults.rotate_text);
    check(defaults.max_file_size_mb == 100 && defaults.max_backup_files == 5);

    auto& log = Logger::getInstance();
    {
        Capture out(std::cout);
        Capture err(std::cerr);
        LoggerConfig cfg;
        cfg.log_file_path = (dir / "defaults.log").string();
        cfg.console_color = false;
        log.initialize(cfg);
        log.info("hello stdout");
        check(out.str().find("Logger initialized: " + cfg.log_file_path +
                              " (UTF-8, Color: OFF)") != std::string::npos);
        check(out.str().find("hello stdout") != std::string::npos);
        check(err.str().empty());
    }
    {
        // The legacy overload keeps announcing on stdout too.
        Capture out(std::cout);
        log.initialize((dir / "legacy.log").string(), LogLevel::INFO, true);
        check(out.str().find("Logger initialized:") != std::string::npos);
    }
    auto const lines = readLines(dir / "defaults.log");
    check(lines.size() == 1 && lines[0].find("hello stdout") != std::string::npos);
    check(log.getTotalLogsWritten() == 0); // initialize() restarted the counters
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
                        check(false && "unexpected result");
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

    check(written + filtered == std::uint64_t{threads} * per_thread);
    auto const stats = log.getSinkStats();
    check(stats.written == written && stats.filtered == filtered && stats.failed == 0);
    auto const lines = readLines(file);
    check(lines.size() == written);
    for (auto const& l : lines) {
        check(l == record);
    }
    log.setLevel(LogLevel::INFO);
    std::cout << "  ✓ passed\n";
}

auto textConfig(fs::path const& file, bool rotate_text) -> LoggerConfig {
    LoggerConfig c;
    c.log_file_path = file.string();
    c.min_level = LogLevel::INFO;
    c.console_output = false;
    c.announce_initialization = false;
    c.max_file_bytes = 200;
    c.max_backup_files = 2;
    c.rotate_text = rotate_text;
    return c;
}

auto test_text_rotation_is_opt_in(fs::path const& dir) -> void {
    std::cout << "Testing TEXT rotation is opt-in...\n";
    auto& log = Logger::getInstance();

    auto const off = dir / "text_off.log";
    log.initialize(textConfig(off, false));
    for (int i = 0; i < 40; ++i) {
        log.info("text line {}", i);
    }
    check(fs::file_size(off) > 200);
    check(!fs::exists(backup(off, 1)));
    check(log.getSinkStats().rotations == 0);

    auto const on = dir / "text_on.log";
    log.initialize(textConfig(on, true));
    for (int i = 0; i < 40; ++i) {
        log.info("text line {}", i);
    }
    check(fs::exists(backup(on, 1)));
    check(fs::file_size(on) <= 200);
    check(log.getSinkStats().rotations > 0);

    // Explicit rotate() rotates a TEXT log even with the flag off.
    auto const manual = dir / "text_manual.log";
    log.initialize(textConfig(manual, false));
    log.info("one line");
    log.rotate();
    check(fs::exists(backup(manual, 1)));
    std::cout << "  ✓ passed\n";
}

auto test_json_rotates_regardless_of_rotate_text(fs::path const& dir) -> void {
    std::cout << "Testing JSON records rotate regardless of rotate_text...\n";
    auto& log = Logger::getInstance();
    for (bool const flag : {false, true}) {
        auto const file = dir / std::format("json_flag_{}.jsonl", flag);
        auto cfg = jsonConfig(file);
        cfg.max_file_bytes = 60;
        cfg.max_backup_files = 2;
        cfg.rotate_text = flag;
        log.initialize(cfg);
        writeSeq(log, 1, 11);
        check(fs::exists(backup(file, 1)));
        check(log.getSinkStats().rotations == 5);
    }
    std::cout << "  ✓ passed\n";
}

auto countOpenHandles(fs::path const& file) -> int {
    auto n = 0;
    std::error_code ec;
    for (auto const& e : fs::directory_iterator("/proc/self/fd", ec)) {
        std::error_code lec;
        if (fs::read_symlink(e.path(), lec) == file && !lec) {
            ++n;
        }
    }
    return n;
}

auto test_reinit_without_file_releases_handle(fs::path const& dir) -> void {
    std::cout << "Testing re-initialising with file_output=false releases the old file...\n";
    auto& log = Logger::getInstance();
    auto const file = fs::canonical(dir) / "handle.jsonl";
    log.initialize(jsonConfig(file));
    check(log.writeRecord(LogLevel::INFO, R"({"a":1})") == RecordWrite::Written);
    check(countOpenHandles(file) == 1);
    auto cfg = jsonConfig(file);
    cfg.file_output = false;
    log.initialize(cfg);
    check(countOpenHandles(file) == 0);
    std::cout << "  ✓ passed\n";
}

auto test_failed_rename_truncates_live_file(fs::path const& dir) -> void {
    std::cout << "Testing a failed rename truncates the live file...\n";
    auto& log = Logger::getInstance();
    auto const file = dir / "norename.jsonl";
    // backup .1 is a non-empty directory: it can neither be removed nor renamed onto.
    fs::create_directories(backup(file, 1));
    std::ofstream(backup(file, 1) / "keep") << "x";
    auto cfg = jsonConfig(file);
    cfg.max_file_bytes = 60;
    cfg.max_backup_files = 1;
    log.initialize(cfg);
    writeSeq(log, 1, 11);
    // After a failed rotation the next attempt waits a second, but the live file is still truncated at the bound.
    check(fs::file_size(file) <= 60); // bounded, not growing past the bound
    check(log.getSinkStats().rotations == 0); // no rotation succeeded
    check(log.getSinkStats().failed > 0);
    check(fs::exists(backup(file, 1) / "keep"));
    std::cout << "  ✓ passed\n";
}

auto test_rotation_failure_counted_once(fs::path const& dir) -> void {
    std::cout << "Testing a rotation failure is counted once...\n";
    auto& log = Logger::getInstance();
    auto const file = dir / "countonce.jsonl";
    auto cfg = jsonConfig(file);
    cfg.max_file_bytes = 60;
    cfg.max_backup_files = 0; // rotation = truncate
    log.initialize(cfg);
    writeSeq(log, 1, 2);
    fs::permissions(file, fs::perms::owner_read);
    if (std::ofstream(file, std::ios::app).is_open()) {
        std::cout << "  (skipped: file permissions are not enforced for this user)\n";
        fs::permissions(file, fs::perms::owner_read | fs::perms::owner_write);
        return;
    }
    // Truncate and reopen both fail, so the file ends closed and the write is refused.
    check(log.writeRecord(LogLevel::INFO, seqLine(3)) == RecordWrite::SinkFailed);
    check(log.getSinkStats().failed == 1);
    check(log.getSinkStats().rotations == 0);
    fs::permissions(file, fs::perms::owner_read | fs::perms::owner_write);
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
    test_text_rotation_is_opt_in(dir);
    test_json_rotates_regardless_of_rotate_text(dir);
    test_reinit_without_file_releases_handle(dir);
    test_failed_rename_truncates_live_file(dir);
    test_rotation_failure_counted_once(dir);
    test_old_defaults_unchanged(dir);
    test_level_changes_under_concurrent_logging(dir);

    Logger::getInstance().initialize(
        LoggerConfig{.log_file_path = (dir / "final.log").string(), .announce_initialization = false});
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::cout << "All JSONL sink tests passed\n";
    return 0;
}
