# Hand Equity Calculator

## Build

```bash
make
```

### Requirements

- C++ compiler (GCC 13 on Linux, Clang 19 on macOS)
- CMake 3.28+

## Example

```
>>> import eqcalc
>>> eqcalc.cards_from_string("Ah5h")
[50, 14]
>>> eqcalc.exact_equity(hands_cards=[[50, 14], eqcalc.cards_from_string("KsQc")], board_cards=[])
[0.6060912665040787, 0.39390873349592126]
>>> eqcalc.exact_equity_from_string(hands_string=["Ah5h", "KsQc"], board_string="")
[0.6060912665040787, 0.39390873349592126]
```
