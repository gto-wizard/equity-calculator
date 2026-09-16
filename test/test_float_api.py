"""The two float functions are public API with callers outside this repository.

`exact_equity` and `exact_equity_from_string` must keep returning plain floats
and must keep their signatures. A caller does `Decimal(str(equities[i]))`, which
raises on an object, and another types the result as `list[float]`.
"""

import eqcalc
import pytest


def test_exact_equity_from_string_returns_floats():
    result = eqcalc.exact_equity_from_string(hands=["Ah5h", "KsQc"], board="")
    assert all(type(value) is float for value in result)
    assert result == pytest.approx([0.6061, 0.3939], abs=0.001)


def test_exact_equity_returns_floats():
    hands = [eqcalc.cards_from_string("Ah5h"), eqcalc.cards_from_string("KsQc")]
    result = eqcalc.exact_equity(hands=hands, board=[])
    assert all(type(value) is float for value in result)
    assert result == pytest.approx([0.6061, 0.3939], abs=0.001)


def test_exact_equity_accepts_a_board():
    hands = [eqcalc.cards_from_string("Ah5h"), eqcalc.cards_from_string("KsQc")]
    board = eqcalc.cards_from_string("QsJh2h")
    assert eqcalc.exact_equity(hands=hands, board=board) == pytest.approx(
        [0.4535, 0.5465], abs=0.001
    )


def test_float_result_equals_the_detailed_equity():
    hands = ["Ah5h", "As9h"]
    board = "QsJh2h"
    floats = eqcalc.exact_equity_from_string(hands=hands, board=board)
    detailed = eqcalc.exact_equity_detailed_from_string(hands=hands, board=board)
    assert floats == [result.equity for result in detailed]


def test_equity_result_reports_all_three_fields():
    result = eqcalc.exact_equity_detailed_from_string(hands=["Ah5h", "As9h"], board="QsJh2h")
    for entry in result:
        assert isinstance(entry, eqcalc.EquityResult)
        assert entry.win <= entry.equity <= entry.win + entry.tie
        assert "EquityResult" in repr(entry)


def test_omaha_reaches_the_float_api_too():
    result = eqcalc.exact_equity_from_string(hands=["Ah5h7s7d", "QcJcJh2d"], board="QhAs3c")
    assert result == pytest.approx([0.6841, 0.3159], abs=0.001)
