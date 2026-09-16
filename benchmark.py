"""Measures the cost of one exact equity call, per spot.

A caller that runs this synchronously needs to know which spots are affordable.
Preflop Omaha is not: the board count and the per-hand work both grow, so the
call costs hundreds of times a Hold'em preflop call.

Run it with `python benchmark.py`. The CI workflow writes the table into the
run summary.
"""

import platform
import time

import eqcalc

#: (label, hands, board, how many times to repeat the call)
CELLS = [
    ("NLHE 2-way preflop", ["Ah5h", "KsQc"], "", 5),
    ("PLO4 2-way preflop", ["Ah5h7s7d", "QcJcJh2d"], "", 3),
    ("PLO6 2-way preflop", ["Ah5h7s7dTc9s", "Ks3d2sQs5dJs"], "", 3),
    ("PLO6 3-way flop", ["Ah5h7s7dTc9s", "Ks3d2sQs5dJs", "2c4c6d8dTdJd"], "QhAs3c", 5),
]


def measure(hands: list[str], board: str, repeats: int) -> tuple[float, float]:
    timings = []
    for _ in range(repeats):
        start = time.perf_counter()
        eqcalc.exact_equity_detailed_from_string(hands=hands, board=board)
        timings.append(time.perf_counter() - start)
    return min(timings), sum(timings) / len(timings)


def main() -> None:
    print(f"Machine: {platform.platform()}")
    print()
    print("| Spot | Best | Mean | Calls timed |")
    print("| --- | --- | --- | --- |")
    for label, hands, board, repeats in CELLS:
        fastest, mean = measure(hands, board, repeats)
        print(f"| {label} | {fastest * 1000:.1f} ms | {mean * 1000:.1f} ms | {repeats} |")


if __name__ == "__main__":
    main()
