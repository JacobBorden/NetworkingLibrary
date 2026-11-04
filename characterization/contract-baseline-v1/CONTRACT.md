# Proposed TCP IPv4 contract and legacy baseline

This is a reviewable contract proposal and executable characterization of NetworkingLibrary master `928837c83c3a72d8102ee8f3ffd0703a0cd44018`. It does not repair or accept the legacy transport. The bounded task is `networking-contract-baseline-v1`; broader milestone 1 acceptance still requires independent API/backend review.

## Consumer inventory

http-server master resolved through GitHub on October 10, 2026 to `86305e4fb61d2ba06dda4523ebb64e9b65c30521`. Its unchanged CMake source declares FetchContent SERVER from this library without GIT_TAG, SOURCE_DIR `../dependencies/my_networking_lib`, and links SERVER and Threads::Threads into http_server and server.exe. A clean configure in this project's isolated work inventory populated NetworkingLibrary at `928837c83c3a72d8102ee8f3ffd0703a0cd44018`. That is this run's actual resolution, not a durable consumer pin. The separately verified networking gitlink is `2b2f8528319a64a93355b81b84b153fe6edc00a8`; it does not establish which source FetchContent compiled.

The configure then failed at consumer CMake line 45: dependencies/googletest does not contain CMakeLists.txt. No consumer executable was built or run. Preserve this failure; full consumer compatibility requires the steward's dependency setup, explicit NetworkingLibrary pin and independent smoke/regression checks. No consumer checkout was modified; downloaded immutable source and FetchContent output reside inside this library's work directory.

src/main.cpp includes server.h and logger.h from dependencies/my_networking_lib/src/server. It uses Networking::Server(80, IPv4), ServerIsRunning, Accept returning copyable ClientConnection, Receive(ClientConnection), GetClientIPAddress, LogToFile, Send(const char*, ClientConnection), and DisconnectClient. A global server outlives detached per-client threads only because main never exits; no coordinated worker shutdown exists. `std::string request = &server.Receive(client)[0]` assumes nonempty NUL-terminated storage without explicit length; responses pass `&response[0]` to strlen-based Send. Consumer parsing, framing, worker ownership and adaptation remain with the current http-server steward. No SimpliDFS consumer or Matrix dependency is inferred.

## API sketch for review

Proposed namespace Networking::tcp with move-only Socket and Listener owners, no owning raw-handle conversion. A moved-from owner is closed. Destruction and close() are noexcept and idempotent; each acquired descriptor has one closer. Closing Listener closes only the listener; independently owned accepted sockets remain valid. Applications join their workers before destroying borrowed objects. The library neither exits the process nor recursively retries.

Use C++20 std::span<const std::byte> for sends and std::span<std::byte> for receives, std::chrono::steady_clock::time_point for an absolute deadline, and std::stop_token for cancellation. Proposed signatures:

```cpp
enum class Status { complete, progress, eof, timeout, cancelled, error };
struct Transfer { std::size_t bytes; Status status; std::error_code error; };
Transfer send_some(std::span<const std::byte>, Deadline, std::stop_token);
Transfer send_all(std::span<const std::byte>, Deadline, std::stop_token);
Transfer receive_some(std::span<std::byte>, Deadline, std::stop_token);
Transfer receive_exact(std::span<std::byte>, Deadline, std::stop_token);
```

An empty span completes with zero bytes without a syscall, even when closed; cancellation is checked after this empty fast path. A nonempty operation on a closed/moved-from owner returns error/bad_file_descriptor without a syscall. Some operations return progress after any positive count, bounded by the supplied span. All/exact operations return complete only when the whole span is transferred, otherwise the accumulated count plus the terminal outcome. A short receive is progress, never message completion. Positive progress followed by EOF, timeout, cancellation or error retains every byte count. Orderly recv zero is EOF; send zero for a nonempty request is an error/no-progress condition, never a busy loop. SIGPIPE must be suppressed on Linux sends. Capture errno immediately; interrupted calls retry under the same deadline.

No implicit strlen, NUL termination, vector growth or whole-message accumulation. Helpers split native calls into chunks no larger than 64 KiB and platform count limits; caller spans bound allocation. The application chooses overall buffer/message limits. EOF is distinct from reset/error, timeout and cancellation. Socket acquisition failures return a typed error without a partially usable owner. Numeric IPv4 connect accepts ports 1..65535; listener accepts 0..65535 and reports the assigned ephemeral port. Validate before socket creation; use the actual sockaddr pointer and length, and SO_ERROR after nonblocking connection readiness. Initial listener policy permits numeric loopback only. DNS, IPv6, UDP and file-transfer helpers are deferred.

Use nonblocking native descriptors and poll with one monotonic deadline across retries. At each retry check cancellation first, then readiness/deadline; a cancellation already requested before the first nonempty call returns cancelled with zero progress. Poll waits at most min(remaining deadline, 10 ms), providing cooperative cancellation without cross-thread descriptor close. Promised boundedness covers numeric connect, accept and byte I/O, not DNS. Tests allow deadline/cancellation tolerance of 100 ms on an otherwise responsive machine and use an independent hard watchdog. Data already transferred cannot be rolled back.

Externally serialize operations, close and moves on each owner; sharing one Socket for simultaneous read/write is initially unsupported. A stop_source may be used concurrently to request cancellation. Separate socket owners can run in separate threads. Listener accept transfers sole ownership to a new Socket; no shared mutable server client vector is part of the proposed core.

## Backend and compatibility decision

Propose retaining a thin synchronous native-socket backend for the first Linux slice: existing consumer already links SERVER, no extra package is needed, and the contract is bounded connect/accept/byte I/O rather than an event loop. poll plus nonblocking calls can express the proposed Linux deadlines and cooperative stop checks. The current recursive retry/server registry is not part of that candidate. An established backend adapter becomes a separate reviewed choice if requirements expand to asynchronous dispatch, TLS or a protocol stack; this slice neither evaluates nor introduces an external backend dependency.

Do not silently replace legacy Send/Receive. Keep CLIENT/SERVER and current headers intact until an explicit adapter and consumer migration are qualified. An eventual legacy text Send adapter must document text-only length and error/count semantics; Receive requires framing owned by the consumer and cannot promise one TCP read per HTTP request. The old copyable raw ClientConnection cannot masquerade as the new owning Socket. Rebuild consumers against an accepted exact package revision; no binary ABI compatibility is promised.

| Platform/toolchain | Evidence now | Next gate |
| --- | --- | --- |
| Linux/WSL GCC 15.2.1, CMake 3.31.0 | Native legacy targets build; C++20 characterization harness | New backend, lifecycle/deadline tests and exact-head CI |
| Windows/MSVC | No executable qualification | WinSock owner/type/link adaptation; nonzero Debug/Release CI |
| macOS, IPv6, UDP | Unqualified/deferred | Separate demand and contracts |

Only the harness requests cxx_std_20. The untouched legacy targets still have no declared standard. This is not a portfolio C++20 migration acceptance.

## Executable regression baseline

Run `python3 characterization/contract-baseline-v1/run.py --output <absolute-evidence-directory>` from the canonical repository. The runner records commands, cwd, exit codes and logs. Root CMake remains untouched. Legacy characterization asserts known outcomes; proposed_binary_contract deliberately fails on five unmet expectations. Do not invert its expected failure into feature acceptance. The short-write/short-read comparisons express the gap to proposed send-all/receive-exact helpers; they do not assert that legacy Send promised send-all semantics. The runner verifies the exact five gap labels and result count, so a crash or unrelated test failure cannot satisfy the expected-failure gate.

Eleven groups cover empty send, 511/512/513 ASCII byte equality over real ephemeral TCP IPv4 loopback, embedded NUL truncation, all-byte payload beginning with NUL, scripted short read, scripted short write, real orderly EOF, invalid descriptor resize-before-error, and explicit harness cleanup restoring /proc/self/fd count. GNU linker send/recv wrappers force two partial-transfer cases independently of scheduling. The harness explicitly closes its native descriptors and listener; the fd check does not establish library destructor correctness. A 10-second CTest watchdog bounds every execution. No real partial-transfer, large-buffer, reset, timeout, cancellation or Windows support is claimed.

Initial harness compilation failed because SERVER exports no include usage requirement; corrected only the harness include path. Initial CTest had two Not Run entries, zero executed tests. Those logs remain in work alongside the corrected run; no production file was changed.

## First repair acceptance list

1. Empty spans, embedded NUL, all 256 byte values, 511/512/513 lengths, and payload larger than socket buffers, in both directions with exact equality.
2. Scripted fragmented reads, short writes, EINTR, zero progress and errors after progress; independent real TCP loopback; numeric ports and readiness synchronization.
3. EOF after partial data, reset, invalid/moved-from owner, refused connection, connect/accept/read/write deadlines and cancellation, measured with documented tolerances.
4. Failed construction, moves, repeated close, listener close with accepted owners, repeated connect/disconnect; exactly-once closes, fd baseline and ASan/UBSan.
5. One hundred complete suite repetitions with no hangs or resource growth; restored strlen and resize-before-error mutations must fail their tests.
6. Linux first; Windows and pinned http-server acceptance remain independent later gates with the current consumer steward. Full package and release acceptance remain outside this characterization task. Scoped branch publication and a draft PR were authorized on November 1, 2026; publication does not establish transport or consumer acceptance.

## Binary drain and EOF characterization, November 2, 2026

The loopback fixture now sends 1,024 bytes (all 256 byte values repeated four times) with explicit native send lengths, then half-closes the sender. It accumulates legacy Receive chunks through EOF and compares the complete byte vector, then checks persistent EOF. This is real IPv4 loopback evidence without send/recv seams enabled. The fixture owns its descriptors, binds only 127.0.0.1 on an ephemeral port, uses readiness waits, and runs under the existing ten-second CTest timeout. Comparisons remain active in Release. It characterizes Receive; native sending does not qualify legacy Send or repair the five known gaps.

Acceptance: Debug and Release legacy runs and 100 Debug repetitions pass 13 groups each (1,326 total); each proposed-contract run passes eight groups and fails exactly the same five known gaps. The runner rejects any changed failure list. The library API and backend remain unchanged.

Consumer recheck: http-server master remains 86305e4fb61d2ba06dda4523ebb64e9b65c30521. Its current CMake still lacks a GIT_TAG; main still constructs a string from Receive's data without a length and uses detached workers. The October 10 clean configure resolution above is historical evidence, not a new resolved pin. No new consumer build, compatibility acceptance or consumer edit is claimed.

## Stateful fragmented receive characterization, November 4, 2026

A byte-consuming recv seam supplies 515 binary bytes as a full 512-byte read
followed by a three-byte short read. The fixture checks exact output equality,
consumed offset, native buffer bounds and call counts; the next Receive observes
EOF without consuming more bytes. This detects lost or overwritten chunks and
unwanted reads beyond a short read. Unlike the original constant short-read
seam, the stream has finite state and preserves embedded NUL bytes.

Acceptance requires Debug/Release and 100 Debug repetitions at 15 groups each
(1,530 group passes), with exactly the original five known failures in each
proposed-contract run (10 passes, five failures). This is scripted receive
characterization; real loopback evidence remains the November 2 fixture. No
EINTR, reset, timeout, cancellation, send-all or consumer acceptance follows.
