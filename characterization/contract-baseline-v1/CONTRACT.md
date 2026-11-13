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

## Error after progress characterization, November 5, 2026

A finite recv seam supplies 512 binary bytes then returns -1 with ECONNRESET.
The legacy implementation resizes the accumulated vector to 511 before handling
that error, closes the accepted socket, and returns the truncated vector without
an explicit error result. Two Release-active checks require those exact bytes,
two native calls, the consumed offset/bounds, and EBADF from the closed descriptor.
The fixture relinquishes descriptor ownership after observing closure.

Acceptance: Debug/Release plus 100 Debug repetitions pass 17 groups per execution
(1,734 passes); proposed-contract runs retain exactly five original failures
(12 passes/five failures). These two new checks preserve legacy defect evidence
in both modes; they do not assert that reset behavior satisfies the proposed
contract. This is injected error evidence, not a real TCP reset, EINTR, deadline,
cancellation, ownership repair or consumer qualification. Production is unchanged.
The consumer remains at 86305e4fb61d2ba06dda4523ebb64e9b65c30521 with no
GIT_TAG in its fetched CMake; no new resolved dependency or consumer build exists.

## Interrupted receive characterization — November 7, 2026

The finite recv seam supplies 512 binary bytes, one EINTR, then three bytes. It checks all three calls request 512 bytes, all 515 fixture bytes are consumed, but the returned vector contains only the first 511 bytes: the recursive recovery result is discarded, and switch fallthrough closes the accepted descriptor. A separate ownership check and the final descriptor inventory detect cleanup errors. This is deterministic error injection on a real loopback connection, not native signal-delivery or repaired EINTR acceptance. The suite now has 19 legacy groups; both proposed-contract runs retain five expected failures (14 passes). Production code and consumer integration remain unchanged.

## Peer half-close characterization — November 8, 2026

After the real loopback fixture drains 1,024 binary bytes and observes persistent
EOF, two checks establish that Receive retains the accepted descriptor and the
reverse direction still transfers all 256 byte values across 1,024 bytes. Native
explicit-length send/recv isolate shutdown ownership from legacy Send defects;
poll deadlines and the CTest watchdog bound execution. The final descriptor
inventory still verifies cleanup. Acceptance is Debug/Release plus 100 Debug
repetitions at 21 groups (2,142 passes), with exactly five original proposed-
contract failures in each mode (16 passes). This qualifies this legacy half-close
path only; full-close/error ownership, cancellation, Windows and consumer
compatibility remain open. Production code is unchanged.

## Terminal send error characterization — November 9, 2026

A finite send seam injects ECONNRESET on a real IPv4 loopback connection and
verifies one call with the exact descriptor, six payload bytes and flags zero.
Three checks establish a -1 return, consumed descriptor ownership, and native
peer EOF without payload after the legacy handler shuts down and closes. Poll
and CTest watchdogs bound the fixture; final descriptor inventory checks cleanup.
This is injected-error characterization, not a genuine network reset or repaired
ownership contract. Production is unchanged. Debug/Release plus 100 repetitions
require 24 groups (2,448 passes); proposed controls retain five expected gaps
(19 passes each). Native EINTR timing and full transport acceptance remain open.

## Zero-progress send characterization — November 10, 2026

A finite native send seam returns zero for a six-byte nonempty request. Three
Release-active checks require one exact native call, a zero legacy return with
no retry, retained accepted descriptor ownership, and a real loopback peer that
remains open without payload (nonblocking recv reports EAGAIN/EWOULDBLOCK).
This distinguishes zero progress from the terminal error path without a timing
sleep. The existing watchdog and descriptor inventory bound/check cleanup.
Production is unchanged; the proposed contract requires a no-progress error,
so these checks characterize a gap rather than accept send-all behavior.
Debug/Release plus 100 repetitions require 27 groups (2,754 passes); both
proposed controls retain exactly five original failures (22 passes each).


November 11 characterization: a finite recv seam returns 512 binary bytes,
one EAGAIN, then three recoverable bytes. Two active checks bind the exact
four calls (including an extra EOF read via EINTR fallthrough), 512-byte requested bounds, consumed 515-byte fixture, returned
511-byte prefix, and closed descriptor. Legacy EAGAIN switch fallthrough
discards the recursive result and disconnects after recovery, as with EINTR.
This is injected would-block evidence, not native timeout/deadline acceptance.
The loopback fixture and final descriptor inventory retain resource checks.


## Native empty nonblocking receive — November 12, 2026

A real IPv4 loopback accepted socket uses O_NONBLOCK; a native MSG_PEEK
preflight verifies EAGAIN/EWOULDBLOCK with no data and no peer shutdown.
With all linker seams disabled, Receive throws length_error while resizing
before error handling. Two active checks require that exception and retained
descriptor/connected-peer ownership. Final descriptor inventory checks cleanup.
Debug/Release plus 100 repeats require 31 groups (3,162 passes); both proposed
controls retain the same five expected failures (26 passes). This records the
native no-progress defect, not deadline/cancellation or repaired API acceptance.
Production and consumer code remain unchanged.


## Native nonblocking receive after progress — November 13, 2026

A real loopback peer queues exactly 512 binary bytes. A blocking MSG_PEEK /
MSG_WAITALL preflight verifies the full chunk before enabling O_NONBLOCK.
Without receive seams, the legacy call consumes the chunk, encounters EAGAIN,
and recursively calls Receive; the empty recursive buffer resize throws
length_error. Three active checks require the exception, consumed queued bytes
with retained descriptor, and successful later three-byte binary receive after
peer half-close. The original 512 bytes never reach the caller. This differs
from the injected recovery fixture, which supplies bytes to recursion. CTest
watchdogs bound setup; final descriptor inventory checks cleanup. Production
remains unchanged. Debug/Release plus 100 repeats require 34 groups (3,468
passes); both proposed controls retain five expected failures (29 passes).
This characterizes progress loss, not repaired transport or consumer acceptance.
