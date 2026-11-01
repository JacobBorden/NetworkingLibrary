# Proposed synchronous TCP boundary and race policy

Task networking-status-policy-v1 addresses the October 11 independent steward's precedence finding. This is a milestone-1 proposal over source master 928837c83c3a72d8102ee8f3ffd0703a0cd44018, not an accepted API or implemented socket backend. It supplements CONTRACT.md and DECISION.md without editing their historical evidence. Consumer approval and backend approval remain open.

## Observable boundaries

An operation has one serialized owner and one caller span. At entry and before every new native attempt, take a fresh observation in this order: committed count, owner validity, stop request, steady-clock time. The decision is based on those observations, not a reconstruction of global wall-clock event order. A stop arriving after its sample can be seen at the next boundary; it cannot roll back a syscall. Deadline equality (`now >= deadline`) counts as expired. Deadlines are absolute and never restart after interruption, readiness, or partial progress.

| Before an attempt, first applicable condition | Outcome |
| --- | --- |
| Empty span, or all bytes already committed | complete, with the committed count; no syscall |
| Nonempty closed/moved-from owner | error/bad_file_descriptor, retaining committed count |
| Stop observed | cancelled, retaining committed count |
| Deadline observed expired | timeout, retaining committed count |
| None of the above | Readiness wait or one bounded nonblocking attempt is permitted |

The validity-first rule resolves the original unspecified closed-plus-stop case. Stop wins when both stop and expiry are observed at the same boundary. This rule offers deterministic outcomes for an observation; it does not promise that thread scheduling produces the same observations on every run. Concurrent close/move/I/O on an owner remains unsupported, so invalidation after partial progress is a defensive model case rather than permission to close across threads.

Readiness is permission to try, never an outcome. Re-sample stop and time after a readiness wake before issuing the syscall. An expired deadline defeats ready data at that boundary; no speculative final read is made. A timeout wake alone is insufficient until the steady-clock comparison confirms expiry. Interrupted/would-block calls commit no bytes and return to the same gate. Poll waits remain capped at 10 ms and the remaining deadline as proposed previously. Scheduler delay and a boundary-to-syscall race prevent a hard real-time promise.

## Commit the attempt result

After an attempt returns, capture its native outcome immediately, before consulting newly arrived stop/expiry intent. Receive zero maps to EOF; nonempty send zero maps to error/no_progress. A positive successful result commits its count exactly once. Some operations return progress even if that one attempt fills the supplied span. All/exact return complete only when successful committed bytes fill the span; otherwise advance the span and enter the next boundary. EOF/error terminate with all prior bytes. A combined count-plus-error (possible in a future composed adapter) retains count and error even if the count reaches requested size: byte completion does not erase a reported error.

Thus a final successful fragment wins over stop/expiry arriving during that attempt. A successful some result similarly returns progress. A nonfinal fragment followed by a boundary observing stop and expiry returns cancelled with the fragment's count. An EOF/error already returned by the attempt wins over intent first observed afterward. A retry result, however, returns to the gate where stop wins over expiry. No result ever claims bytes not actually reported by an attempt, and no terminal outcome discards partial progress.

For numeric connect and accept, apply the nonempty preflight order before attempts; successful acquisition observed from the attempt wins over later intent. Failure publishes no owner, and temporary descriptors must be cleaned up. Exact SO_ERROR interpretation, acquired-owner lifetime and cancellation latency still require transport tests; this byte-model harness does not exercise them.

## Backend and consumer limits

This policy is proposed for the bounded synchronous native Linux experiment only. It defines neither an asynchronous event loop nor an accepted Asio facade. If Asio is chosen, a separate executable adapter must define serialized callback arbitration, count-plus-error mapping, losing-timer cancellation, stop callback/slot detachment and full handler drain before returning or destroying operation state. The October 11 review's Asio lifetime finding stays open; this synchronous model cannot qualify it.

CLIENT/SERVER remain untouched. The existing http-server steward retains its unpinned FetchContent dependency, explicit-length/framing adaptation, worker lifetime and real consumer tests. No consumer or shared dependency is changed, and no Matrix dependency is introduced. Independent review must accept or revise this policy before it governs milestone 2.

## Executable qualification of this proposal

Run `python3 characterization/status-policy-v1/run.py --output <absolute evidence directory>` in the canonical repository. The runner bounds each process at ten seconds, records the exact command/cwd/log/exit status and requires five unittest groups. The independent expected table contains 10 preflight cases and 14 attempt cases, followed by 10 sequential race assertions and 9 invalid-observation cases: 46 assertions total including three before/equal/after deadline checks. Mutation checks deliberately reverse stop-versus-expiry precedence and ignore full completion; each must be rejected at the exact expected test labels and failure count (three and two respectively), with no unrelated errors. Mutated copies live only in this run's work directory.

These checks exercise a pure Python state model. They have zero native socket tests and zero C++ builds, and establish no timing, binary transfer, ownership, sanitizer, Windows, remote CI or consumer acceptance. The existing legacy baseline remains valid for unchanged inputs and is not rerun. Future backend tests must inject stateful partial attempts and synchronized stop/clock events at these boundaries, test exact equality at the deadline and full/partial result races, then qualify real loopback behavior with an independent watchdog and the previously specified timing tolerance.
