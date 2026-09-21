import eqcalc
import pytest

from util import results_to_dict


def test_exact_equity_pre_flop():
    hands = ["Ah5h", "KsQc"]
    board = ""
    expected_equities = [0.6061, 0.3939]
    expected_wins = [0.6038, 0.3916]
    expected_ties = [0.0045, 0.0045]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_flop():
    hands = ["Ah5h", "KsQc"]
    board = "QsJh2h"
    expected_equities = [0.4535, 0.5465]
    expected_wins = [0.4535, 0.5465]
    expected_ties = [0.0, 0.0]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_flop_ties():
    hands = ["Ah5h", "As9h"]
    board = "QsJh2h"
    expected_wins = [0.4333, 0.3848]
    expected_ties = [0.1818, 0.1818]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_3p_flop():
    hands = ["Ah5h", "KsQc", "9d9c"]
    board = "QsJh2h"
    expected_equities = [0.4563, 0.4895, 0.0543]
    expected_wins = [0.4563, 0.4895, 0.0543]
    expected_ties = [0.0, 0.0, 0.0]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_3p_flop_ties():
    hands = ["Ah5h", "Ad5s", "As9h"]
    board = "QsJh2h"
    expected_wins = [0.3411, 0.0, 0.3987]
    expected_ties = [0.2602, 0.2602, 0.1827]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_8p_flop():
    hands = ["AcAs", "JcTh", "9h4c", "Ks9d", "Jh6s", "6c6h", "9s2s", "8h5c"]
    board = "Ad2d2c"
    expected_equities = [0.9413, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0587, 0.0]
    expected_wins = [0.9413, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0587, 0.0]
    expected_ties = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
    res = results_to_dict(eqcalc.exact_equity_detailed_from_string(hands=hands, board=board))
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)


def test_exact_equity_with_dead_cards():
    hands = ["AhAc", "KsKc"]
    board = ""
    dead_cards = "AsAd"
    expected_equities = [0.78285, 0.21725]
    expected_wins = [0.7804, 0.2148]
    expected_ties = [0.0049, 0.0049]
    res = results_to_dict(
        eqcalc.exact_equity_detailed_from_string(hands=hands, board=board, dead_cards=dead_cards)
    )
    assert res.equity == pytest.approx(expected_equities, abs=0.001)
    assert res.win == pytest.approx(expected_wins, abs=0.001)
    assert res.tie == pytest.approx(expected_ties, abs=0.001)
