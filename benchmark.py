"""Measures the cost of one exact equity call, per spot.

A caller that answers a request synchronously needs to know which spots it can
afford. Preflop Omaha is not one of them: the board count and the per-hand work
both grow, so the call costs hundreds of times a Hold'em preflop call.

The table also reports the number of five-card evaluations each spot needs, and
the rate that implies. A caller can estimate any other spot from the rate:

    boards    = C(52 - cards_used, 5 - board_size)
    per_hand  = 1                        for Hold'em
    per_hand  = C(hole_cards, 2) * 10    for Omaha
    total     = boards * players * per_hand

Omaha uses exactly two of its hole cards and exactly three of the five board
cards, and C(5, 3) is 10.

Run it with `python benchmark.py`. The CI workflow writes the table into the
run summary.
"""

import math
import platform
import time

import eqcalc

FULL_BOARD_SIZE = 5
HOLDEM_HAND_SIZE = 2
#: The number of ways to choose three of the five board cards.
BOARD_TRIPLES = 10

#: (label, hands, board, how many times to repeat the call)
CELLS = [
    ("NLHE 2-way preflop", ["Ah5h", "KsQc"], "", 5),
    ("PLO4 2-way preflop", ["Ah5h7s7d", "QcJcJh2d"], "", 3),
    ("PLO6 2-way preflop", ["Ah5h7s7dTc9s", "Ks3d2sQs5dJs"], "", 3),
    ("PLO6 3-way flop", ["Ah5h7s7dTc9s", "Ks3d2sQs5dJs", "2c4c6d8dTdJd"], "QhAs3c", 5),
]


def evaluation_count(hands: list[str], board: str) -> int:
    """How many five-card hands the enumeration ranks for this spot."""
    hole_cards = len(hands[0]) // 2
    board_cards = len(board) // 2
    deck = 52 - hole_cards * len(hands) - board_cards
    boards = math.comb(deck, FULL_BOARD_SIZE - board_cards)
    if hole_cards == HOLDEM_HAND_SIZE:
        per_hand = 1
    else:
        per_hand = math.comb(hole_cards, 2) * BOARD_TRIPLES
    return boards * len(hands) * per_hand


def measure(hands: list[str], board: str, repeats: int) -> float:
    """The fastest of `repeats` calls, in seconds."""
    timings = []
    for _ in range(repeats):
        start = time.perf_counter()
        eqcalc.exact_equity_detailed_from_string(hands=hands, board=board)
        timings.append(time.perf_counter() - start)
    return min(timings)


def main() -> None:
    print(f"Machine: {platform.platform()}")
    print()
    print("| Spot | Best of N | Evaluations | Rate |")
    print("| --- | --- | --- | --- |")
    for label, hands, board, repeats in CELLS:
        fastest = measure(hands, board, repeats)
        evaluations = evaluation_count(hands, board)
        rate = evaluations / fastest / 1e6
        print(
            f"| {label} | {fastest * 1000:.1f} ms (N={repeats}) "
            f"| {evaluations:,} | {rate:.0f} M/s |"
        )


if __name__ == "__main__":
    main()
