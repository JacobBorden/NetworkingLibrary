"""Executable proposal only: boundary arbitration, with no socket implementation."""
from dataclasses import dataclass
from enum import Enum

class Status(str, Enum):
    COMPLETE = 'complete'
    PROGRESS = 'progress'
    EOF = 'eof'
    TIMEOUT = 'timeout'
    CANCELLED = 'cancelled'
    ERROR = 'error'

@dataclass(frozen=True)
class Outcome:
    bytes: int
    status: Status
    error: str = ''

def deadline_expired(observed_steady_time, absolute_deadline):
    return observed_steady_time >= absolute_deadline

# None means an attempt is permitted, not that the descriptor is ready.
def before_attempt(size, transferred, valid=True, stopped=False, expired=False):
    if not 0 <= transferred <= size:
        raise ValueError('progress must lie within the caller span')
    if transferred == size:
        return Outcome(transferred, Status.COMPLETE)
    if not valid:
        return Outcome(transferred, Status.ERROR, 'bad_file_descriptor')
    if stopped:
        return Outcome(transferred, Status.CANCELLED)
    if expired:
        return Outcome(transferred, Status.TIMEOUT)
    return None

# This boundary commits one attempt's observed result before consulting new intent.
# RETRY covers interruption/would-block and performs no transfer.
def after_attempt(size, transferred, count, event, *, some=False, sending=False):
    if not 0 <= transferred <= size or not 0 <= count <= size - transferred:
        raise ValueError('attempt count exceeds remaining span')
    if event not in ('data', 'eof', 'error', 'retry'):
        raise ValueError('unknown attempt event')
    if event == 'retry':
        if count:
            raise ValueError('retry cannot consume bytes')
        return None
    total = transferred + count
    if event == 'error':
        return Outcome(total, Status.ERROR, 'native_error')
    if event == 'eof':
        if sending or count:
            raise ValueError('EOF is a zero-count receive result')
        return Outcome(total, Status.EOF)
    if not count:
        return Outcome(total, Status.ERROR, 'no_progress')
    if some:
        return Outcome(total, Status.PROGRESS)
    if total == size:
        return Outcome(total, Status.COMPLETE)
    return None  # Caller commits total and takes another before_attempt snapshot.
