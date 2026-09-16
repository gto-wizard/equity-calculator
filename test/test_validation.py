"""The input guards.

The extension is importable by any caller, so it validates its own input
rather than trusting a boundary above it. An unvalidated hand is not a wrong
answer, it is a crash: the hackathon build looped to ``hand.size() - 1``, which
wraps to ``SIZE_MAX`` on an empty hand and reads past the end of the vector.
"""

import eqcalc
import pytest


def test_empty_hand_string_is_rejected():
    with pytest.raises(ValueError, match="2, 4, 5, or 6 cards"):
        eqcalc.exact_equity_detailed_from_string(hands=["", ""], board="")


def test_three_card_hand_is_rejected():
    with pytest.raises(ValueError, match="2, 4, 5, or 6 cards"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhKsQd", "2c3c4c"], board="")


def test_seven_card_hand_is_rejected():
    with pytest.raises(ValueError, match="2, 4, 5, or 6 cards"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhKsQd2c3c4c5c", "6c7c8c9cTcJcQc"])


def test_mixed_hand_lengths_are_rejected():
    with pytest.raises(ValueError, match="same number of cards"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhKs", "2c3c4c5c"], board="")


def test_too_many_omaha_hands_are_rejected():
    hands = ["Ah2h3h4h", "As2s3s4s", "Ac2c3c4c", "Ad2d3d4d", "Kh5h6h7h", "Ks5s6s7s", "Kc5c6c7c"]
    with pytest.raises(ValueError, match="at most 6 hands"):
        eqcalc.exact_equity_detailed_from_string(hands=hands, board="")


def test_eight_hold_em_hands_are_accepted():
    """A hand history can seat more than six players, so Hold'em keeps no six-hand cap."""
    hands = ["AcAs", "JcTh", "9h4c", "Ks9d", "Jh6s", "6c6h", "9s2s", "8h5c"]
    result = eqcalc.exact_equity_detailed_from_string(hands=hands, board="Ad2d2c")
    assert len(result) == len(hands)


def test_thirteen_hands_are_rejected():
    """The per-runout weight is `factorial(n)`, which overflows near 18 hands."""
    hands = [
        "AcAs", "JcTh", "9h4c", "Ks9d", "Jh6s", "6c6h", "9s2s",
        "8h5c", "2c3c", "4d5d", "7s8s", "TdJd", "QhKh",
    ]
    with pytest.raises(ValueError, match="at most 12 hands"):
        eqcalc.exact_equity_detailed_from_string(hands=hands, board="AdQd2c")


def test_card_value_out_of_range_is_rejected():
    """A raw card list is public API, so it must raise the documented error."""
    with pytest.raises(ValueError, match="range 0 to 51"):
        eqcalc.exact_equity_detailed(hands=[[60, 3], [0, 1]])


def test_repeated_card_inside_a_single_hand_is_rejected():
    with pytest.raises(ValueError, match="same card"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhAh"], board="")


def test_two_card_board_is_rejected():
    with pytest.raises(ValueError, match="board size must be 0, 3, 4, or 5"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhKs", "2c3c"], board="QsJh")


def test_six_card_board_is_rejected():
    with pytest.raises(ValueError, match="board size must be 0, 3, 4, or 5"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhKs", "2c3c"], board="QsJh2h3d4d5d")


def test_odd_length_card_string_is_rejected():
    with pytest.raises(ValueError, match="even length"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhK", "2c3c"], board="")


def test_invalid_card_character_is_rejected():
    with pytest.raises(ValueError, match="invalid characters"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhXz", "2c3c"], board="")


def test_duplicate_card_across_hands_is_rejected():
    with pytest.raises(ValueError, match="same card"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhKs", "AhQc"], board="")


def test_duplicate_card_between_hand_and_board_is_rejected():
    with pytest.raises(ValueError, match="same card"):
        eqcalc.exact_equity_detailed_from_string(hands=["AhKs", "2c3c"], board="AhJh4d")


def test_too_few_cards_left_to_complete_the_board_is_rejected():
    """Six six-card hands take 36 cards and 12 dead cards take 12 more.

    Four cards remain, and a board needs five.
    """
    hands = [
        "2c2d2h2s3c3d",
        "3h3s4c4d4h4s",
        "5c5d5h5s6c6d",
        "6h6s7c7d7h7s",
        "8c8d8h8s9c9d",
        "9h9sTcTdThTs",
    ]
    dead_cards = "JcJdJhJsQcQdQhQsKcKdKhKs"
    with pytest.raises(ValueError, match="Too few cards"):
        eqcalc.exact_equity_detailed_from_string(hands=hands, board="", dead_cards=dead_cards)


def test_single_hand_returns_certainty():
    result = eqcalc.exact_equity_detailed_from_string(hands=["AhKs"], board="")
    assert len(result) == 1
    assert result[0].win == 1.0
    assert result[0].tie == 0.0
    assert result[0].equity == 1.0


def test_no_hands_returns_nothing():
    assert eqcalc.exact_equity_detailed_from_string(hands=[], board="") == []
