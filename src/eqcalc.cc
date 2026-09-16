#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "poker_hand.h"

namespace py = pybind11;

namespace gtow {

/// The outcome of one hand in an exact equity enumeration.
/// @details `win` is the fraction of runouts the hand wins alone. `tie` is the
/// fraction of runouts the hand chops, with any number of opponents, and it
/// carries the FULL weight of the runout, not the share. `equity` is the
/// expected share of the pot, so a chop adds `1 / number_of_winners`.
/// Therefore `win + tie` is the probability of not losing, and
/// `win <= equity <= win + tie`.
struct EquityResult {
  double win;
  double tie;
  double equity;
};

namespace {

/// The number of cards on a complete board.
inline constexpr std::size_t FULL_BOARD_SIZE = 5;

/// The number of hole cards a Hold'em hand holds.
inline constexpr std::size_t HOLDEM_HAND_SIZE = 2;

/// The largest number of players an Omaha enumeration accepts.
/// @details Six is the table size the product deals.
inline constexpr std::size_t MAX_OMAHA_PLAYERS = 6;

/// The largest number of hands any enumeration accepts.
/// @details Each runout adds a weight of `factorial(n)` to a `std::uint64_t`
/// accumulator, and the total is `factorial(n)` times the board count. That
/// product overflows below 18 hands, which would report wrong equities rather
/// than fail. Twelve is above every real table, so no caller loses a spot it
/// can play.
inline constexpr std::size_t MAX_PLAYERS = 12;

/// The number of ways to choose the two board slots an Omaha hand replaces.
inline constexpr std::size_t BOARD_SLOT_PAIRS = 10;

/// A complete board, held in place across the enumeration recursion.
using BoardCards = std::array<card_t, FULL_BOARD_SIZE>;

/// The board with two of its five cards removed, one entry per slot pair.
/// @details An Omaha hand plays exactly three board cards, so these are the
/// only board fragments it can use. Building them once per runout replaces a
/// five-card hand build in the innermost loop with a two-hand merge.
using BoardTriples = std::array<PokerHand, BOARD_SLOT_PAIRS>;

constexpr std::uint64_t factorial(int n) {
  std::uint64_t result = 1;
  for (int i = 2; i <= n; ++i) {
    result *= i;
  }
  return result;
}

constexpr std::uint64_t combinations(int n, int k) {
  std::uint64_t result = 1;
  for (int i = 0; i < k; ++i) {
    result *= n - i;
    result /= i + 1;
  }
  return result;
}

constexpr bool is_valid_hand_size(std::size_t size) noexcept {
  return size == 2 || size == 4 || size == 5 || size == 6;
}

constexpr bool is_valid_board_size(std::size_t size) noexcept {
  return size == 0 || size == 3 || size == 4 || size == 5;
}

/// @brief Rejects the input shapes the enumeration cannot evaluate.
/// @details The module is importable, so a bad shape must not reach the
/// enumeration. The hackathon build wrote the Omaha loop as
/// `i < size() - 1`, which wraps to `SIZE_MAX` on an empty hand and reads
/// past the end of the vector. That is undefined behaviour, and it ends the
/// process rather than the one call.
void validate_inputs(const std::vector<std::vector<card_t>>& hands_cards,
                     std::size_t board_size) {
  if (!is_valid_board_size(board_size)) [[unlikely]] {
    throw std::invalid_argument("exact_equity: The board size must be 0, 3, 4, or 5");
  }

  const auto hand_size = hands_cards.front().size();
  if (!is_valid_hand_size(hand_size)) [[unlikely]] {
    throw std::invalid_argument("exact_equity: Each hand must contain 2, 4, 5, or 6 cards");
  }

  for (const auto& hand : hands_cards) {
    if (hand.size() != hand_size) [[unlikely]] {
      throw std::invalid_argument("exact_equity: Every hand must contain the same number of cards");
    }
    for (const auto card : hand) {
      // `PokerHand` reads its lookup table with `at`, which reports an
      // out-of-range card as a different exception type. Reject it here so
      // every bad input raises the same one.
      if (card >= detail::NUM_CARDS) [[unlikely]] {
        throw std::invalid_argument("exact_equity: A card must be in the range 0 to 51");
      }
    }
  }

  if (hands_cards.size() > MAX_PLAYERS) [[unlikely]] {
    throw std::invalid_argument("exact_equity: An enumeration accepts at most 12 hands");
  }

  if (hand_size != HOLDEM_HAND_SIZE && hands_cards.size() > MAX_OMAHA_PLAYERS) [[unlikely]] {
    throw std::invalid_argument("exact_equity: An Omaha enumeration accepts at most 6 hands");
  }
}

/// @brief Returns the board with each pair of its five slots removed.
/// @details One build per runout, shared by every hand at that runout.
BoardTriples board_triples_of(const PokerHand& board_hand, const BoardCards& board) {
  BoardTriples triples;
  std::size_t next = 0;
  for (std::size_t slot_1 = 0; slot_1 + 1 < FULL_BOARD_SIZE; ++slot_1) {
    for (std::size_t slot_2 = slot_1 + 1; slot_2 < FULL_BOARD_SIZE; ++slot_2) {
      triples[next++] = board_hand - PokerHand{board[slot_1], board[slot_2]};
    }
  }
  return triples;
}

/// @brief Returns the best five-card strength an Omaha hand makes on a board.
/// @details Omaha fixes the split: exactly two hole cards and exactly three
/// board cards. Every three-card board fragment arrives already built, so the
/// inner loop merges two hands rather than building five cards.
detail::strength_t omaha_strength(const std::vector<card_t>& hand_cards,
                                  const BoardTriples& board_triples) {
  detail::strength_t best = 0;
  const auto hand_size = hand_cards.size();

  for (std::size_t hole_1 = 0; hole_1 + 1 < hand_size; ++hole_1) {
    for (std::size_t hole_2 = hole_1 + 1; hole_2 < hand_size; ++hole_2) {
      const PokerHand hole{hand_cards[hole_1], hand_cards[hole_2]};
      for (const auto& triple : board_triples) {
        best = std::max(best, (triple + hole).evaluate());
      }
    }
  }

  return best;
}

/// @brief Enumerates every completion of the board, in place.
/// @details The board is carried by reference and mutated at `depth`, so the
/// recursion allocates nothing. A copied `std::vector` here costs roughly one
/// million malloc/free pairs per preflop call.
template <int N, class F>
void enumerate_all_boards(const std::vector<card_t>& deck, F& f, BoardCards& board,
                          std::size_t depth, std::size_t index = 0) {
  if constexpr (N == 0) {
    f(board);
  } else {
    for (; index < deck.size(); ++index) {
      board[depth] = deck[index];
      enumerate_all_boards<N - 1>(deck, f, board, depth + 1, index + 1);
    }
  }
}

/// @brief Finds the winners of one runout and adds its weight to the counters.
/// @param strength_of Returns the strength of hand `i` on this runout.
template <class F>
void score_runout(std::size_t n, F strength_of, std::vector<std::uint64_t>& win_counts,
                  std::vector<std::uint64_t>& tie_counts,
                  std::vector<std::uint64_t>& equity_counts,
                  std::vector<unsigned>& winner_buffer, std::uint64_t fact) {
  auto max_strength = strength_of(0);
  unsigned num_winners = 1;
  winner_buffer[0] = 0;

  for (unsigned i = 1; i < n; ++i) {
    const auto strength = strength_of(i);
    if (strength > max_strength) {
      max_strength = strength;
      num_winners = 1;
      winner_buffer[0] = i;
    } else if (strength == max_strength) {
      winner_buffer[num_winners++] = i;
    }
  }

  if (num_winners == 1) {
    win_counts[winner_buffer[0]] += fact;
    equity_counts[winner_buffer[0]] += fact;
  } else {
    const auto addend = fact / num_winners;
    for (unsigned i = 0; i < num_winners; ++i) {
      tie_counts[winner_buffer[i]] += fact;
      equity_counts[winner_buffer[i]] += addend;
    }
  }
}

/// @brief Scores one runout for one variant.
/// @details The variant is a template parameter, not a run-time flag, so the
/// Hold'em path never builds the board fragments that only Omaha reads. A
/// Hold'em preflop call visits 1.7 million runouts, and the fragments are
/// 160 bytes each.
template <bool IsOmaha>
void update_stats(const std::vector<std::vector<card_t>>& hands_cards,
                  const std::vector<PokerHand>& hands, const BoardCards& board,
                  std::vector<std::uint64_t>& win_counts, std::vector<std::uint64_t>& tie_counts,
                  std::vector<std::uint64_t>& equity_counts, std::vector<unsigned>& winner_buffer,
                  std::uint64_t fact) {
  const auto n = hands.size();
  const PokerHand board_hand(board);

  if constexpr (IsOmaha) {
    const BoardTriples board_triples = board_triples_of(board_hand, board);
    score_runout(
        n, [&](std::size_t i) { return omaha_strength(hands_cards[i], board_triples); },
        win_counts, tie_counts, equity_counts, winner_buffer, fact);
  } else {
    // Hold'em plays the best five of the seven cards, which the evaluator
    // does on the merged hand.
    score_runout(
        n, [&](std::size_t i) { return (board_hand + hands[i]).evaluate(); }, win_counts,
        tie_counts, equity_counts, winner_buffer, fact);
  }
}

}  // namespace

std::vector<card_t> cards_from_string(std::string_view cards) {
  if (cards.size() % 2 != 0) [[unlikely]] {
    throw std::invalid_argument("cards_from_string: The card string must be of even length");
  }

  std::vector<card_t> result;
  result.reserve(cards.size() / 2);

  for (unsigned i = 0; i < cards.size(); i += 2) {
    const auto rank = detail::RANK_CHARS.find(cards[i]);
    const auto suit = detail::SUIT_CHARS.find(cards[i + 1]);
    if (rank == std::string_view::npos || suit == std::string_view::npos) [[unlikely]] {
      throw std::invalid_argument("cards_from_string: The card string contains invalid characters");
    }
    result.push_back(detail::make_card(rank, suit));
  }

  return result;
}

std::vector<EquityResult> exact_equity_detailed(const std::vector<std::vector<card_t>>& hands_cards,
                                                const std::vector<card_t>& board_cards = {},
                                                const std::vector<card_t>& dead_cards = {}) {
  const auto n = hands_cards.size();
  if (n == 0) {
    return {};
  }

  validate_inputs(hands_cards, board_cards.size());

  // Building the hands rejects a card that repeats inside one hand, so it
  // must happen before the single-hand answer.
  std::vector<PokerHand> hands;
  hands.reserve(n);
  for (const auto& hand : hands_cards) {
    hands.emplace_back(hand);
  }

  if (n == 1) {
    return std::vector<EquityResult>(1, {1.0, 0.0, 1.0});
  }

  const PokerHand board(board_cards);

  PokerHand combined = board;
  for (const auto& hand : hands) {
    combined += hand;
  }
  combined += PokerHand(dead_cards);

  std::vector<card_t> deck;
  deck.reserve(detail::NUM_CARDS - combined.size());
  for (card_t card = 0; card < detail::NUM_CARDS; ++card) {
    if (!combined.contains(card)) {
      deck.push_back(card);
    }
  }

  const auto cards_to_come = FULL_BOARD_SIZE - board.size();
  if (deck.size() < cards_to_come) [[unlikely]] {
    throw std::invalid_argument(
        "exact_equity: Too few cards remain in the deck to complete the board");
  }

  const auto fact = factorial(static_cast<int>(n));
  std::vector<std::uint64_t> win_counts(n);
  std::vector<std::uint64_t> tie_counts(n);
  std::vector<std::uint64_t> equity_counts(n);
  std::vector<unsigned> winner_buffer(n);

  BoardCards running_board{};
  std::ranges::copy(board_cards, running_board.begin());

  // Every hand holds the same number of cards, so one variant runs per call.
  const bool is_omaha = hands_cards.front().size() != HOLDEM_HAND_SIZE;
  auto lambda = [&](const BoardCards& river_board) {
    if (is_omaha) {
      update_stats<true>(hands_cards, hands, river_board, win_counts, tie_counts, equity_counts,
                         winner_buffer, fact);
    } else {
      update_stats<false>(hands_cards, hands, river_board, win_counts, tie_counts, equity_counts,
                          winner_buffer, fact);
    }
  };

  switch (board.size()) {
    case 0:
      enumerate_all_boards<5>(deck, lambda, running_board, 0);
      break;
    case 3:
      enumerate_all_boards<2>(deck, lambda, running_board, 3);
      break;
    case 4:
      enumerate_all_boards<1>(deck, lambda, running_board, 4);
      break;
    case 5:
      lambda(running_board);
      break;
    default:
      // `validate_inputs` has already rejected every other size.
      throw std::invalid_argument("exact_equity: The board size must be 0, 3, 4, or 5");
  }

  const auto comb =
      combinations(static_cast<int>(deck.size()), static_cast<int>(cards_to_come));
  const double denom = static_cast<double>(fact * comb);

  std::vector<EquityResult> result;
  result.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    result.push_back({static_cast<double>(win_counts[i]) / denom,
                      static_cast<double>(tie_counts[i]) / denom,
                      static_cast<double>(equity_counts[i]) / denom});
  }

  return result;
}

std::vector<EquityResult> exact_equity_detailed_from_string(
    const std::vector<std::string>& hands_string, const std::string& board_string = "",
    const std::string& dead_cards_string = "") {
  std::vector<std::vector<card_t>> hands_cards;
  hands_cards.reserve(hands_string.size());
  for (const auto& hand_string : hands_string) {
    hands_cards.push_back(cards_from_string(hand_string));
  }

  return exact_equity_detailed(hands_cards, cards_from_string(board_string),
                               cards_from_string(dead_cards_string));
}

std::vector<double> exact_equity(const std::vector<std::vector<card_t>>& hands_cards,
                                 const std::vector<card_t>& board_cards = {}) {
  const auto detailed = exact_equity_detailed(hands_cards, board_cards);

  std::vector<double> result;
  result.reserve(detailed.size());
  for (const auto& entry : detailed) {
    result.push_back(entry.equity);
  }

  return result;
}

std::vector<double> exact_equity_from_string(const std::vector<std::string>& hands_string,
                                             const std::string& board_string = "") {
  std::vector<std::vector<card_t>> hands_cards;
  hands_cards.reserve(hands_string.size());
  for (const auto& hand_string : hands_string) {
    hands_cards.push_back(cards_from_string(hand_string));
  }

  return exact_equity(hands_cards, cards_from_string(board_string));
}

}  // namespace gtow

PYBIND11_MODULE(_core, m) {
  m.doc() = "Hand equity calculator for Texas Hold'em and Omaha poker";

  py::class_<gtow::EquityResult>(m, "EquityResult")
      .def_readonly("win", &gtow::EquityResult::win, "The fraction of runouts the hand wins alone")
      .def_readonly("tie", &gtow::EquityResult::tie,
                    "The fraction of runouts the hand chops, at full weight")
      .def_readonly("equity", &gtow::EquityResult::equity, "The expected share of the pot")
      .def("__repr__", [](const gtow::EquityResult& r) {
        return "<EquityResult: win=" + std::to_string(r.win) + ", tie=" + std::to_string(r.tie) +
               ", equity=" + std::to_string(r.equity) + ">";
      });

  // Every equity function releases the GIL. An Omaha enumeration runs for
  // seconds, and the Django service runs one Granian worker per pod, so a
  // held GIL would stop that pod answering anything, health probes included.
  // None of these functions touches a Python object, and pybind11 restores
  // the GIL before it converts the result.
  m.def("cards_from_string", &gtow::cards_from_string, py::arg("cards"),
        "Converts a string of cards to a vector");
  m.def("exact_equity", &gtow::exact_equity, py::arg("hands"),
        py::arg("board") = std::vector<gtow::card_t>{},
        py::call_guard<py::gil_scoped_release>(), "Calculates the exact equity of each hand");
  m.def("exact_equity_from_string", &gtow::exact_equity_from_string, py::arg("hands"),
        py::arg("board") = "", py::call_guard<py::gil_scoped_release>(),
        "Calculates the exact equity of each hand from a string representation");
  m.def("exact_equity_detailed", &gtow::exact_equity_detailed, py::arg("hands"),
        py::arg("board") = std::vector<gtow::card_t>{},
        py::arg("dead_cards") = std::vector<gtow::card_t>{},
        py::call_guard<py::gil_scoped_release>(),
        "Calculates the exact win, tie and equity of each hand");
  m.def("exact_equity_detailed_from_string", &gtow::exact_equity_detailed_from_string,
        py::arg("hands"), py::arg("board") = "", py::arg("dead_cards") = "",
        py::call_guard<py::gil_scoped_release>(),
        "Calculates the exact win, tie and equity of each hand from a string representation");
}
