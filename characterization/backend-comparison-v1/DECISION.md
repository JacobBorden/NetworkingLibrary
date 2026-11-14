# Backend comparison for review

Task `networking-backend-comparison-v1`; October 11, 2026 UTC / America/Halifax. This completes a bounded milestone-1 specification, not backend approval or transport qualification. NetworkingLibrary source: master `928837c83c3a72d8102ee8f3ffd0703a0cd44018`. Earlier contract/baseline files remain immutable.

Recommendation: retain the proposed thin native Linux backend for the first reviewed synchronous TCP IPv4 experiment. Keep standalone Asio as the concrete alternative if native deadline/lifetime complexity exceeds the limits below. This recommendation is an engineering inference from the consumer inventory and primary documentation. Neither candidate has passed repaired-transport acceptance. No dependency is added today.

## Inputs and evidence limits

Compare the proposed native design in [CONTRACT.md](../contract-baseline-v1/CONTRACT.md) with standalone Asio **1.36.0**, tag `asio-1-36-0`, peeled commit `231cb29bab30f82712fcd54faaea42424cc6e710` (tag object `366dfc44640182cb21c1ebf7efb658a6bec13f5a`). This is a reproducible evaluation pin, not a latest-version assertion or accepted package version. The primary sources below link to versioned documentation and exact-commit upstream files. The historical local source-excerpt inventory is not part of this draft; it is not needed to resolve those published sources.

Immutable http-server `86305e4fb61d2ba06dda4523ebb64e9b65c30521` uses SERVER/server.h/logger.h, copyable ClientConnection, text Send, vector Receive and detached client workers. Its unpinned FetchContent previously resolved this library to the source SHA above; its separate gitlink is `2b2f8528319a64a93355b81b84b153fe6edc00a8`. Re-inspection confirms the unsafe NUL-based conversion and missing durable pin. Prior configure failed on absent Google Test, with zero consumer tests; no rerun or consumer acceptance is claimed. Its current steward owns consumer adaptation and dependency setup.

## Concrete comparison

| Criterion | Proposed native Linux backend | Standalone Asio 1.36.0 alternative |
| --- | --- | --- |
| Byte API | span length passed to nonblocking send/recv; implement accumulation ourselves | Explicit sized buffers; read/write composed operations retain transferred counts [R,W] |
| Bounded synchronous calls | poll under one steady deadline, at most 10 ms cancellation polling; no event loop | Async socket operations plus steady timer and a private executor driven until all operation/timer handlers drain; blocking read/write alone does not supply this contract [T] |
| Cancellation | std::stop_token checked before nonempty operations and each wait; no cross-thread close | Adapter must bridge stop_token onto serialized executor work; partial cancellation can retain progress, but support depends on the underlying operation [C,R,W] |
| Ownership | Implement move-only fd owner, idempotent noexcept close, failed-acquisition cleanup and listener/accepted-owner separation | Hide moveable Asio socket/acceptor behind the same owner API; keep executor alive through completion. Still must prevent raw ownership escape [S] |
| Consumer fit | New namespace can coexist with unchanged CLIENT/SERVER; old methods still need explicit migration | Backend can stay private, but copying ClientConnection and NUL-based framing still need the same consumer migration; Asio does not solve HTTP parsing |
| Toolchains | Current local evidence is GCC 15.2.1/Linux only; WinSock design remains separate | Upstream lists Linux GCC/Clang and Windows Visual C++ among tested platforms; that is upstream evidence, not our C++20/MSVC acceptance [U] |
| Dependency cost | No external package; POSIX platform links and our maintenance burden | Header-only by default; choose standalone mode, pin headers, keep them private. Optional SSL/coroutine/regex features have extra dependencies and are outside this comparison [U,S] |
| License | Existing repository MIT; preserve current notice | Boost Software License 1.0; preserve its required notice for any future source distribution. No dependency approval inferred [L] |
| Maintenance | Own readiness races, errno capture, SIGPIPE, partial transfers and Windows port | Upstream supplies reactor/platform machinery; we still own deadline arbitration, adapter lifetime, cancellation mapping, pin upgrades and qualification |

## Same public contract, two possible implementations

Keep the proposed `Networking::tcp` span/Transfer/Deadline/stop_token API independent of backend types. Empty spans finish before cancellation/descriptor checks, as already proposed; nonempty closed owners report bad_file_descriptor. EOF, timeout, cancellation and error remain distinct. `bytes` always records actual progress; errors never resize caller storage. No backend gets to infer framing from a short read.

| Operation | Native mapping | Asio mapping requiring qualification |
| --- | --- | --- |
| connect / accept | Numeric sockaddr_in; nonblocking readiness and SO_ERROR; loopback listener on port 0 | Numeric tcp endpoint; async_connect / async_accept, timer and serialized cancellation; accepted socket gets independent ownership |
| send_some / receive_some | One successful bounded syscall after readiness; EINTR/EAGAIN retry under the same deadline | async_write_some / async_read_some using asio::buffer(span.data(), min(size, 64 KiB)) |
| send_all / receive_exact | Offset over successive some calls; preserve bytes plus terminal status | Repeated bounded some calls, or composed async_write/async_read with a 64 KiB completion-condition cap. Built-in all-buffer calls alone do not establish our chunk limit [R,W] |
| close / move | Exactly one fd closer; moved-from closed; listener does not close accepted sockets | Private socket/acceptor ownership; drain pending handlers before destroying operation state; wrap repeated close to our idempotent policy |

The pinned upstream timeout example uses io_context::run_for, closes the socket on expiry, and then drains completion. Its DNS resolution occurs before that timed run, and its line framing is outside our contract. It demonstrates an executor-backed blocking facade, not our proposed resumable timer/stop adapter.

For an Asio adapter, each synchronous facade call would own operation state on the heap, start one I/O operation and one absolute steady timer, register a stop callback that posts to the executor, then drive that executor until both handlers settle. Unregister the stop callback and drain posted cancellation work before returning: no caller span may outlive its facade call. Do not translate every operation_aborted into timeout. Record whether timeout or stop won on the serialized executor, retain the I/O completion count, and map uncategorized aborts to error. Readiness/deadline races need an explicit deterministic tie policy in both implementations. This sketch is uncompiled and unapproved.

Asio cancellation signals do not remember requests made before initiation. Check stop_requested before starting nonempty I/O; emit partial cancellation for resumable composed transfers rather than requiring total cancellation. A single cancellation slot cannot serve concurrent operations, and emission must obey executor/threading rules. These details make a naive stop_callback that directly invokes socket.cancel from another thread unsuitable [C].

## Consumer adapter boundary

A safe migration keeps CLIENT/SERVER and their headers unchanged while qualifying a separate transport target that declares cxx_std_20. Do not reinterpret raw ClientConnection as an owning Socket. The steward must replace detached workers with bounded/joined ownership, receive into an explicit buffer and construct strings with returned length, keep HTTP framing above transport, and send responses with explicit length. Receive-exact cannot substitute for reading an unknown-size HTTP request. LogToFile remains application behavior. Consumer source changes, a durable dependency pin, and actual request/response smoke tests remain with the existing owner. No binary ABI preservation or drop-in compatibility is promised.

## Decision and qualification gates

Approve native only for numeric loopback TCP IPv4, externally serialized owners and synchronous bounded I/O. Reopen the backend decision before adding DNS deadlines, asynchronous multiplexing, TLS, protocol framing, concurrent operations on one owner, or platform-specific duplicated retry logic. Prefer reviewing an Asio adapter if satisfying those requirements introduces an executor/reactor into our custom layer. Do not silently switch backends.

Before milestone 2: independent review must settle the public status/tie policy, native-versus-Asio recommendation, consumer pin/adapter boundary and ownership. An Asio choice additionally needs exact pinned-header compilation and license/dependency acceptance; upstream support lists are insufficient. Linux first: GCC 15.2.1/CMake 3.31.0 is the observed local matrix. Windows/MSVC exact versions remain unselected and unqualified.

The first transport suite must use stateful scripted fixtures that consume input and collect output across retries. Required cases: exact bidirectional bytes for empty/NUL/all-byte/511/512/513/large payloads; fragmentation, short writes, EINTR, would-block and zero progress; partial EOF/error/timeout/cancel; refused connect and accept timeout; moves, repeated close, listener close with surviving accepted sockets and fd balance. Each run needs an independent watchdog; 100 repetitions, sanitizer lanes and restored strlen/resize-before-error mutations must demonstrate failures at the relevant regressions. Connect/accept/read/write deadline and cancellation tolerance remains 100 ms on a responsive host. Windows and pinned consumer CI are independent later gates.

This document has **zero runtime tests**. Prior characterization results are historical evidence, not a claim about later reruns. Completion means the comparison and review criteria are delivered. Backend/API, consumer, transport, platforms and package acceptance remain open. Scoped branch publication and a draft PR were authorized on November 1, 2026; publication does not approve either backend or establish any of those acceptance claims.

## Primary sources

- [U: versioned build/platform/dependency documentation](https://think-async.com/Asio/asio-1.36.0/doc/asio/using.html)
- [C: per-operation cancellation guarantees and threading](https://think-async.com/Asio/asio-1.36.0/doc/asio/overview/core/cancellation.html)
- [R: async_read counts, lifetime and cancellation](https://think-async.com/Asio/asio-1.36.0/doc/asio/reference/async_read/overload1.html)
- [W: async_write counts, lifetime and cancellation](https://think-async.com/Asio/asio-1.36.0/doc/asio/reference/async_write/overload1.html)
- [T: upstream timeout example at exact comparison commit](https://github.com/chriskohlhoff/asio/blob/231cb29bab30f82712fcd54faaea42424cc6e710/asio/src/examples/cpp11/timeouts/blocking_tcp_client.cpp)
- [S: upstream socket implementation](https://github.com/chriskohlhoff/asio/blob/231cb29bab30f82712fcd54faaea42424cc6e710/asio/include/asio/basic_socket.hpp)
- [L: exact upstream license](https://github.com/chriskohlhoff/asio/blob/231cb29bab30f82712fcd54faaea42424cc6e710/asio/LICENSE_1_0.txt)
