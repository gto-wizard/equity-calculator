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
>>> eqcalc.exact_equity(hands=[[50, 14], eqcalc.cards_from_string("KsQc")], board=[])
[0.6060912665040787, 0.39390873349592126]
>>> eqcalc.exact_equity_from_string(hands=["Ah5h", "KsQc"], board="")
[0.6060912665040787, 0.39390873349592126]
>>> eqcalc.exact_equity_from_string(hands=["Ah5h", "KsQc"], board="QsJh2h")
[0.4535353535353535, 0.5464646464646464]
```
