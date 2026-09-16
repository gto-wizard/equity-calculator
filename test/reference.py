"""An independent, slow reference implementation of exact equity.

This module shares no code with the C++ extension. It parses cards into
``(rank, suit)`` tuples, ranks five-card hands from first principles, and
enumerates every runout in Python. The tests use it to cross-check the
extension, so a table of constants copied from the same algorithm cannot make
a test pass.

It is too slow for a preflop Omaha spot. Use it on a flop, a turn or a river.
"""

import itertools
from collections import Counter
from types import SimpleNamespace

RANKS = "23456789TJQKA"
SUITS = "cdhs"

#: The rank of the highest card of the wheel, A-5-4-3-2.
WHEEL_HIGH = RANKS.index("5")

FULL_BOARD_SIZE = 5
HOLDEM_HAND_SIZE = 2
#: How many cards in a row make a straight.
STRAIGHT_LENGTH = 5
OMAHA_HOLE_CARDS_USED = 2
OMAHA_BOARD_CARDS_USED = 3


def parse_cards(card_string: str) -> list[tuple[int, int]]:
    """Converts "AhKs" into [(12, 2), (11, 3)]."""
    if len(card_string) % 2 != 0:
        raise ValueError(f"The card string {card_string!r} must be of even length")
    cards = []
    for i in range(0, len(card_string), 2):
        cards.append((RANKS.index(card_string[i]), SUITS.index(card_string[i + 1])))
    return cards


FULL_DECK = [(rank, suit) for rank in range(len(RANKS)) for suit in range(len(SUITS))]


def rank_five(cards: tuple[tuple[int, int], ...]) -> tuple[int, ...]:
    """Ranks exactly five cards. A larger tuple is a stronger hand."""
    ranks = sorted((rank for rank, _ in cards), reverse=True)
    is_flush = len({suit for _, suit in cards}) == 1

    distinct = sorted(set(ranks), reverse=True)
    straight_high = None
    if len(distinct) == STRAIGHT_LENGTH:
        if distinct[0] - distinct[4] == 4:
            straight_high = distinct[0]
        elif distinct == [RANKS.index("A"), 3, 2, 1, 0]:
            straight_high = WHEEL_HIGH

    if is_flush and straight_high is not None:
        return (8, straight_high)

    counts = Counter(ranks)
    # Sort by how many of a rank there are first, then by the rank itself, so
    # the tuple compares in the order a poker hand does.
    groups = sorted(counts.items(), key=lambda item: (item[1], item[0]), reverse=True)
    shape = [count for _, count in groups]
    ordered = tuple(rank for rank, _ in groups)

    if shape[0] == 4:
        return (7, *ordered)
    if shape[:2] == [3, 2]:
        return (6, *ordered)
    if is_flush:
        return (5, *ranks)
    if straight_high is not None:
        return (4, straight_high)
    if shape[0] == 3:
        return (3, *ordered)
    if shape[:2] == [2, 2]:
        return (2, *ordered)
    if shape[0] == 2:
        return (1, *ordered)
    return (0, *ranks)


def best_holdem(hole: list, board: list) -> tuple[int, ...]:
    """The best five of the seven cards a Hold'em player holds."""
    return max(rank_five(five) for five in itertools.combinations(hole + board, FULL_BOARD_SIZE))


def best_omaha(hole: list, board: list) -> tuple[int, ...]:
    """The best hand from exactly two hole cards and exactly three board cards."""
    return max(
        rank_five(two + three)
        for two in itertools.combinations(hole, OMAHA_HOLE_CARDS_USED)
        for three in itertools.combinations(board, OMAHA_BOARD_CARDS_USED)
    )


def best_hand(hole: list, board: list) -> tuple[int, ...]:
    if len(hole) == HOLDEM_HAND_SIZE:
        return best_holdem(hole, board)
    return best_omaha(hole, board)


def exact_equity(hands: list[str], board: str = "", dead_cards: str = "") -> SimpleNamespace:
    """Enumerates every runout and returns win, tie and equity per hand.

    ``tie`` carries the full weight of a chopped runout. ``equity`` carries the
    share. This matches what the extension reports.
    """
    hole_cards = [parse_cards(hand) for hand in hands]
    board_cards = parse_cards(board)
    used = set(board_cards) | set(parse_cards(dead_cards))
    for hole in hole_cards:
        used |= set(hole)

    deck = [card for card in FULL_DECK if card not in used]
    player_count = len(hands)
    wins = [0] * player_count
    ties = [0] * player_count
    equity = [0.0] * player_count
    runouts = 0

    for extra in itertools.combinations(deck, FULL_BOARD_SIZE - len(board_cards)):
        full_board = board_cards + list(extra)
        strengths = [best_hand(hole, full_board) for hole in hole_cards]
        best = max(strengths)
        winners = [i for i, strength in enumerate(strengths) if strength == best]
        runouts += 1

        if len(winners) == 1:
            wins[winners[0]] += 1
            equity[winners[0]] += 1.0
        else:
            for i in winners:
                ties[i] += 1
                equity[i] += 1.0 / len(winners)

    return SimpleNamespace(
        win=[count / runouts for count in wins],
        tie=[count / runouts for count in ties],
        equity=[value / runouts for value in equity],
    )
