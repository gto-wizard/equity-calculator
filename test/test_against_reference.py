"""Cross-checks the extension against an independent Python enumeration.

The expected values in the other test files are constants. A constant proves
nothing on its own, because it can come from the same wrong algorithm it is
meant to check. ``reference.py`` re-derives the answer from the rules of poker
and shares no code with the extension.

The reference is slow, so every spot here starts from a flop or a turn.
"""

import eqcalc
import pytest

import reference
from util import results_to_dict

TOLERANCE = 1e-9


def assert_matches_reference(hands: list[str], board: str, dead_cards: str = "") -> None:
    actual = results_to_dict(
        eqcalc.exact_equity_detailed_from_string(hands=hands, board=board, dead_cards=dead_cards)
    )
    expected = reference.exact_equity(hands, board, dead_cards)

    assert actual.win == pytest.approx(expected.win, abs=TOLERANCE)
    assert actual.tie == pytest.approx(expected.tie, abs=TOLERANCE)
    assert actual.equity == pytest.approx(expected.equity, abs=TOLERANCE)


def test_hold_em_turn_matches_reference():
    """The control. Hold'em already shipped, so this must agree."""
    assert_matches_reference(["Ah5h", "KsQc"], "QsJh2h")


def test_plo4_flop_matches_reference():
    """A flop runs the two-card recursion, which is where the board is mutated in place."""
    assert_matches_reference(["Ah5h7s7d", "QcJcJh2d"], "QhAs3c")


def test_plo4_turn_three_handed_matches_reference():
    assert_matches_reference(["Ah5h7s7d", "QcJcJh2d", "9d9c8h7h"], "QhAs3cTd")


def test_plo5_turn_three_handed_matches_reference():
    assert_matches_reference(["Ah5h7s7dJs", "QcJcJh2d3s", "9d9s8h7hTs"], "QhAs3c4d")


def test_plo6_turn_matches_reference():
    assert_matches_reference(["Ah5h7s7dTc9s", "Ks3d2sQs5dJs"], "QhAs3c4d")


def test_plo6_turn_three_handed_with_ties_matches_reference():
    """Three identical-strength draws, so the tie branch carries real weight."""
    assert_matches_reference(["AhKh2c3c4d5d", "AsKs2d3d4c5c", "AcKc2h3h4s5s"], "AdKd7h8s")


def test_plo4_preflop_matches_reference():
    """The only cross-check of the five-card recursion.

    A preflop enumeration normally runs over a million boards, which is far
    beyond the Python reference. Thirty-two dead cards leave a deck of twelve
    and 792 boards, and the recursion still fills all five slots.
    """
    assert_matches_reference(
        ["AhKhQhJh", "AsKsQsJs"],
        "",
        "2c3c4c5c6c7c8c9cTcJcQcKcAc2d3d4d5d6d7d8d9dTdJdQdKdAdTh9h8hTs9s8s",
    )


def test_plo5_turn_with_dead_cards_matches_reference():
    """Dead cards must leave the deck, not merely be absent from the hands."""
    assert_matches_reference(
        ["Ah2hJsTs2c", "KsQsJcTc5s", "KcQcJdTd5c"],
        "Th9s3s8h",
        "KhQhJh9h7h6h5h4h3h",
    )
