"""The enumeration must not hold the Python GIL.

An Omaha preflop call runs for seconds. The Django service runs one Granian
worker per pod, so a call that holds the GIL stops that pod answering anything,
health probes included. Only the extension can release the GIL, so no caller
can work around it.

How the test proves it: a second thread sleeps for `TICK_SECONDS`, then counts,
then sleeps again. `time.sleep` gives the GIL up, so after every sleep the
thread must take the GIL back. While the extension holds the GIL the thread
cannot take it back, so the count stops. While the extension has released it
the count runs.

The sleep is what makes the test honest. Each count costs 10 ms of real time,
so the operating system cannot inflate the number by descheduling the main
thread for a moment around the call.
"""

import threading
import time

import eqcalc

#: A PLO4 preflop spot: 1,086,008 boards, about 1.3e8 hand evaluations.
#: It is the cheapest spot that runs long enough to measure.
SLOW_SPOT = ["Ah5h7s7d", "QcJcJh2d"]

#: How long the counting thread sleeps between counts.
TICK_SECONDS = 0.01

#: The call takes far longer than 50 ms, so a free thread passes this easily.
#: A blocked thread reaches 0.
MINIMUM_TICKS = 5


class SleepingTicker:
    """Counts once per `TICK_SECONDS`, taking the GIL back each time."""

    def __init__(self) -> None:
        self.ticks = 0
        self._stop = False
        self._thread = threading.Thread(target=self._run, daemon=True)

    def _run(self) -> None:
        while not self._stop:
            time.sleep(TICK_SECONDS)
            self.ticks += 1

    def __enter__(self) -> "SleepingTicker":
        self._thread.start()
        return self

    def __exit__(self, *_: object) -> None:
        self._stop = True
        self._thread.join(timeout=5)


def test_the_enumeration_releases_the_gil():
    with SleepingTicker() as ticker:
        # Let the thread reach its first sleep before the call starts.
        time.sleep(TICK_SECONDS * 2)

        before = ticker.ticks
        start = time.perf_counter()
        eqcalc.exact_equity_detailed_from_string(hands=SLOW_SPOT, board="")
        elapsed = time.perf_counter() - start
        moved = ticker.ticks - before

    assert elapsed > TICK_SECONDS * MINIMUM_TICKS, (
        f"the call took only {elapsed:.3f}s, which is too short to measure. "
        "Choose a slower spot."
    )
    assert moved >= MINIMUM_TICKS, (
        f"another thread counted only {moved} times in {elapsed:.3f}s, and each "
        f"count costs {TICK_SECONDS}s of sleep. The binding is holding the GIL."
    )
