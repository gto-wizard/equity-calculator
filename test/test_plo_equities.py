import eqcalc
import pytest

from util import results_to_dict


def test_exact_equity_pre_flop_plo4():
    hands = ["Ah5h7s7d", "QcJcJh2d"]
    board = ""
    expected_equities = [0.4169, 0.5831]
    expected_wins = [0.4169, 0.5831]
    expected_ties = [0.0, 0.0]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_flop_plo4():
    hands = ["Ah5h7s7d", "QcJcJh2d"]
    board = "QhAs3c"
    expected_equities = [0.6841, 0.3159]
    expected_wins = [0.6841, 0.3159]
    expected_ties = [0.0, 0.0]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_pre_flop_plo4_3p():
    hands = ["Ah5h7s7d", "QcJcJh2d", "9d9c8h7h"]
    board = ""
    expected_wins = [0.2652, 0.4786, 0.2537]
    expected_ties = [0.0024, 0.0002, 0.0026]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_flop_plo4_3p():
    hands = ["Ah5h7s7d", "QcJcJh2d", "9d9c8h7h"]
    board = "QhAs3c"
    expected_wins = [0.5721, 0.3183, 0.1096]
    expected_ties = [0.0, 0.0, 0.0]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_pre_flop_plo5_3p():
    hands = ["Ah5h7s7dJs", "QcJcJh2d3s", "9d9s8h7hTs"]
    board = ""
    expected_wins = [0.2972, 0.3929, 0.3037]
    expected_ties = [0.006, 0.0062, 0.001]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_flop_plo5_3p():
    hands = ["Ah5h7s7dJs", "QcJcJh2d3s", "9d9s8h7hTs"]
    board = "QhAs3c"
    expected_wins = [0.3226, 0.5455, 0.1141]
    expected_ties = [0.0178, 0.0178, 0.0]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_pre_flop_plo6():
    hands = ["Ah5h7s7dTc9s", "Ks3d2sQs5dJs"]
    board = ""
    expected_equities = [0.5268, 0.4733]
    expected_wins = [0.5237, 0.4702]
    expected_ties = [0.0061, 0.0061]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_flop_plo6():
    hands = ["Ah5h7s7dTc9s", "Ks3d2sQs5dJs"]
    board = "QhAs3c"
    expected_equities = [0.4137, 0.5864]
    expected_wins = [0.4054, 0.5781]
    expected_ties = [0.0165, 0.0165]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_with_dead_cards():
    hands = ["Ah2hJsTs2c", "KsQsJcTc5s", "KcQcJdTd5c"]
    board = "Th9s3s8h"
    dead_cards = "KhQhJh9h7h6h5h4h3h"
    expected_equities = [0, 0.625, 0.375]
    expected_wins = [0, 0.25, 0]
    expected_ties = [0, 0.75, 0.75]
    res = results_to_dict(
        eqcalc.exact_equity_detailed_from_string(hands=hands, board=board, dead_cards=dead_cards)
    )
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_omaha_must_use_exactly_two_hole_cards():
    """A lone ace in the hand cannot play with four board cards.

    In Hold'em the nut flush here belongs to the first hand. In Omaha it does
    not, because a player must use exactly two hole cards and exactly three
    board cards, and the second hole card is an offsuit deuce.
    """
    hands = ["Ah2c3d4d", "KhQh5c6c"]
    board = "Jh8h7h2s"
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))

    # The second hand holds KhQh and makes a king-high flush on every runout,
    # so the first hand's lone Ah never wins.
    assert res.win[0] == pytest.approx(0.0, abs=1e-12)
    assert res.win[1] == pytest.approx(1.0, abs=1e-12)
