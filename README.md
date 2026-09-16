# Hand Equity Calculator

Exact equity for Texas Hold'em and for Omaha with 4, 5 or 6 hole cards. The
calculator enumerates every runout, so the answer is exact and there is no
sampling error. It accepts a board, dead cards, and 2 or more hands.

## Build

```bash
make
```

### Requirements

- C++ compiler (GCC 13 on Linux, Clang 19 on macOS)
- CMake 3.28+

The CI workflow builds with GCC 12 inside `python:3.12-slim-bookworm`, because
that is the image the Django service builds the extension in.

## Test

```bash
pip install -e .
pip install pytest
pytest test/
```

## Functions

| Function | Returns |
| --- | --- |
| `cards_from_string(cards)` | `list[int]` |
| `exact_equity(hands, board)` | `list[float]` |
| `exact_equity_from_string(hands, board)` | `list[float]` |
| `exact_equity_detailed(hands, board, dead_cards)` | `list[EquityResult]` |
| `exact_equity_detailed_from_string(hands, board, dead_cards)` | `list[EquityResult]` |

The two float functions are unchanged. Callers that read a plain equity keep
working. The detailed functions add the win and tie split, and dead cards.

### What `EquityResult` reports

| Field | Meaning |
| --- | --- |
| `win` | The fraction of runouts the hand wins alone. |
| `tie` | The fraction of runouts the hand chops, with any number of opponents. A chop adds the FULL weight, not the share. |
| `equity` | The expected share of the pot. A chop between `k` hands adds `1 / k`. |

Therefore `win + tie` is the probability of not losing, and
`win <= equity <= win + tie`.

### Input rules

- Each hand holds 2, 4, 5 or 6 cards, and every hand holds the same number.
- Hold'em (2 cards) plays the best five of seven.
- Omaha (4, 5 or 6 cards) plays exactly two hole cards and exactly three board
  cards.
- An Omaha enumeration accepts at most 6 hands, which is the table size the
  product deals. Hold'em accepts up to 12, because a hand history can seat more
  players than a table of six.
- The board holds 0, 3, 4 or 5 cards.
- No card can repeat across the hands, the board and the dead cards.

- Every card value is in the range 0 to 51.

An input that breaks a rule raises `ValueError`.

## Cost

Preflop Omaha is expensive. Run `python benchmark.py` for the numbers on your
machine. A caller that answers a request synchronously should refuse the
preflop Omaha spots rather than run them.

## Example

```
>>> import eqcalc
>>> eqcalc.cards_from_string("Ah5h")
[50, 14]
>>> eqcalc.exact_equity(hands=[[50, 14], eqcalc.cards_from_string("KsQc")], board=[])
[0.6060912665040787, 0.39390873349592126]
>>> eqcalc.exact_equity_from_string(hands=["Ah5h", "KsQc"], board="")
[0.6060912665040787, 0.39390873349592126]
>>> eqcalc.exact_equity_from_string(hands=["Ah5h", "KsQc"], board="QsJh2h")
[0.4535353535353535, 0.5464646464646464]
>>> eqcalc.exact_equity_detailed_from_string(hands=["Ah5h", "KsQc"], board="")
[<EquityResult: win=0.603832, tie=0.004518, equity=0.606091>, <EquityResult: win=0.391649, tie=0.004518, equity=0.393909>]
>>> eqcalc.exact_equity_detailed_from_string(hands=["Ah5h7s7d", "QcJcJh2d"], board="QhAs3c")
[<EquityResult: win=0.684146, tie=0.000000, equity=0.684146>, <EquityResult: win=0.315854, tie=0.000000, equity=0.315854>]
>>> eqcalc.exact_equity_detailed_from_string(hands=["AhAc", "KsKc"], board="", dead_cards="AsAd")
[<EquityResult: win=0.780383, tie=0.004859, equity=0.782813>, <EquityResult: win=0.214758, tie=0.004859, equity=0.217187>]
```
