from types import SimpleNamespace


def results_to_dict(results: list) -> SimpleNamespace:
    """Turns a list of EquityResult objects into three parallel lists."""
    return SimpleNamespace(
        win=[res.win for res in results],
        tie=[res.tie for res in results],
        equity=[res.equity for res in results],
    )
