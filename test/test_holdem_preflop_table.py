"""A heads-up Hold'em preflop call reads a precomputed table.

The table must return what the enumeration returns, bit for bit, for every
matchup, in either seat order and under any suit relabeling.
"""

import itertools
import random
import time
from types import SimpleNamespace

import eqcalc
import pytest
from eqcalc import _core

from util import results_to_dict

#: The matchups the sample compares with the enumeration. Each costs one
#: preflop enumeration, about 10 ms.
SAMPLE_SIZE = 150


def all_matchups() -> list[tuple[list[int], list[int]]]:
    hands = list(itertools.combinations(range(52), 2))
    return [
        (list(first), list(second))
        for first, second in itertools.combinations(hands, 2)
        if not set(first) & set(second)
    ]


def as_string(hand: list[int]) -> str:
    ranks, suits = "23456789TJQKA", "cdhs"
    return "".join(ranks[card // 4] + suits[card % 4] for card in hand)


def enumerated(hands: list[str], dead_cards: str = "") -> SimpleNamespace:
    """The enumeration's answer, from its own counters, without the table."""
    counts = _core._count_runouts_from_string(hands=hands, dead_cards=dead_cards)
    return SimpleNamespace(
        win=[win / counts.total for win in counts.win],
        tie=[tie / counts.total for tie in counts.tie],
        equity=[(win + chop) / counts.total for win, chop in zip(counts.win, counts.chop_equity)],
    )


def assert_same(actual_results: list, expected: SimpleNamespace) -> None:
    actual = results_to_dict(actual_results)
    assert actual.win == expected.win
    assert actual.tie == expected.tie
    assert actual.equity == expected.equity


def test_every_matchup_is_in_the_table():
    # 812,175 matchups, both seat orders. A missing key raises.
    for first, second in all_matchups():
        eqcalc.exact_equity_detailed(hands=[first, second])
        eqcalc.exact_equity_detailed(hands=[second, first])


def test_the_table_matches_the_enumeration():
    matchups = random.Random(1003).sample(all_matchups(), SAMPLE_SIZE)
    for first, second in matchups:
        hands = [as_string(first), as_string(second)]
        assert_same(
            eqcalc.exact_equity_detailed_from_string(hands=hands),
            enumerated(hands),
        )


@pytest.mark.parametrize(
    "hands",
    [
        ["Ah5h", "KsQc"],
        ["KsQc", "Ah5h"],
        ["AsAh", "AdAc"],
        ["AsKs", "AhKh"],
        ["AsKh", "AhKs"],
        ["7c2d", "7d2c"],
        ["QsQd", "AhKh"],
        ["2c2d", "3c3d"],
    ],
)
def test_named_matchups_match_the_enumeration(hands):
    assert_same(
        eqcalc.exact_equity_detailed_from_string(hands=hands),
        enumerated(hands),
    )


def test_the_float_api_reads_the_table():
    hands = ["Ah5h", "KsQc"]
    assert eqcalc.exact_equity_from_string(hands=hands) == enumerated(hands).equity


def test_dead_cards_skip_the_table():
    hands = ["AhAc", "KsKc"]
    assert_same(
        eqcalc.exact_equity_detailed_from_string(hands=hands, dead_cards="AsAd"),
        enumerated(hands, dead_cards="AsAd"),
    )
    with_dead = eqcalc.exact_equity_detailed_from_string(hands=hands, dead_cards="AsAd")
    without = eqcalc.exact_equity_detailed_from_string(hands=hands)
    assert with_dead[0].equity != without[0].equity


def test_a_table_call_is_fast():
    hands = ["Ah5h", "KsQc"]
    start = time.perf_counter()
    for _ in range(1000):
        eqcalc.exact_equity_detailed_from_string(hands=hands)
    per_call = (time.perf_counter() - start) / 1000
    # The enumeration takes about 10 ms. A lookup takes microseconds.
    assert per_call < 0.001


def test_the_table_still_validates_its_input():
    with pytest.raises(ValueError):
        eqcalc.exact_equity_detailed_from_string(hands=["AhAh", "KsQc"])
    with pytest.raises(ValueError):
        eqcalc.exact_equity_detailed_from_string(hands=["Ah5h", "Ah5h"])
