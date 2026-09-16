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

/// @brief Enumerates every completion of the board, for Hold'em.
/// @details The hand is carried by value, which is two 64-bit words, and it is
/// merged one card at a time on the way down, so a leaf never rebuilds it.
/// This is the enumeration the Hold'em path has always used.
template <int N, class F>
void enumerate_holdem_boards(const std::vector<card_t>& deck, F f, PokerHand board = {},
                             unsigned index = 0) {
  if constexpr (N == 0) {
    f(board);
  } else {
    for (; index < deck.size(); ++index) {
      enumerate_holdem_boards<N - 1>(deck, f, board + PokerHand{deck[index]}, index + 1);
    }
  }
}

/// @brief Enumerates every completion of the board, for Omaha.
/// @details Omaha also needs the board as an array, because it picks three of
/// the five cards. Each level writes its own slot before it recurses, so the
/// array holds the current board at every leaf and the recursion allocates
/// nothing.
template <int N, class F>
void enumerate_omaha_boards(const std::vector<card_t>& deck, F f, BoardCards& board,
                            PokerHand board_hand, std::size_t depth, unsigned index = 0) {
  if constexpr (N == 0) {
    f(board, board_hand);
  } else {
    for (; index < deck.size(); ++index) {
      board[depth] = deck[index];
      enumerate_omaha_boards<N - 1>(deck, f, board, board_hand + PokerHand{deck[index]}, depth + 1,
                                    index + 1);
    }
  }
}

/// @brief Adds one runout to the counters.
/// @details `winner_buffer` holds the `num_winners` winners of the runout. A
/// chop adds the FULL weight to `tie_counts` and the share `1 / num_winners`
/// to `chop_equity`, which is what `EquityResult` documents.
/// @note A sole winner writes ONE counter, not two. Almost every runout has a
/// sole winner and a Hold'em preflop call visits 1.7 million runouts, so a
/// second store costs about 6 % of the call: 9.2 ms against 8.7 ms on one
/// runner. The equity of a hand is `win_counts + chop_equity`, so the second
/// counter carries nothing the first two do not.
inline void add_runout(const std::vector<unsigned>& winner_buffer, unsigned num_winners,
                       std::vector<std::uint64_t>& win_counts,
                       std::vector<std::uint64_t>& tie_counts,
                       std::vector<std::uint64_t>& chop_equity, std::uint64_t fact) {
  if (num_winners == 1) {
    win_counts[winner_buffer[0]] += fact;
  } else {
    const auto addend = fact / num_winners;
    for (unsigned i = 0; i < num_winners; ++i) {
      tie_counts[winner_buffer[i]] += fact;
      chop_equity[winner_buffer[i]] += addend;
    }
  }
}

/// @brief Scores one Hold'em runout.
/// @details Hold'em plays the best five of the seven cards, which the
/// evaluator does on the merged hand.
/// @note The winner loop is written out once per variant rather than taking
/// the strength as a callable. GCC 12 does not inline a callable into this
/// leaf, so each hand of each runout pays a function call: 10.9 ms against
/// 9.2 ms on one runner, about 20 %. Production already serves this path, so
/// it keeps the shape the Hold'em-only revision was measured in.
inline void update_holdem_stats(const std::vector<PokerHand>& hands, const PokerHand& board,
                                std::vector<std::uint64_t>& win_counts,
                                std::vector<std::uint64_t>& tie_counts,
                                std::vector<std::uint64_t>& chop_equity,
                                std::vector<unsigned>& winner_buffer, std::uint64_t fact) {
  const auto n = hands.size();

  auto max_strength = (hands[0] + board).evaluate();
  unsigned num_winners = 1;
  winner_buffer[0] = 0;

  for (unsigned i = 1; i < n; ++i) {
    const auto strength = (hands[i] + board).evaluate();
    if (strength > max_strength) {
      max_strength = strength;
      num_winners = 1;
      winner_buffer[0] = i;
    } else if (strength == max_strength) {
      winner_buffer[num_winners++] = i;
    }
  }

  add_runout(winner_buffer, num_winners, win_counts, tie_counts, chop_equity, fact);
}

/// @brief Scores one Omaha runout.
/// @details The board fragments are built once per runout and shared by every
/// hand at that runout. See `update_holdem_stats` for why the loop is written
/// out rather than shared.
inline void update_omaha_stats(const std::vector<std::vector<card_t>>& hands_cards,
                               const BoardTriples& board_triples,
                               std::vector<std::uint64_t>& win_counts,
                               std::vector<std::uint64_t>& tie_counts,
                               std::vector<std::uint64_t>& chop_equity,
                               std::vector<unsigned>& winner_buffer, std::uint64_t fact) {
  const auto n = hands_cards.size();

  auto max_strength = omaha_strength(hands_cards[0], board_triples);
  unsigned num_winners = 1;
  winner_buffer[0] = 0;

  for (unsigned i = 1; i < n; ++i) {
    const auto strength = omaha_strength(hands_cards[i], board_triples);
    if (strength > max_strength) {
      max_strength = strength;
      num_winners = 1;
      winner_buffer[0] = i;
    } else if (strength == max_strength) {
      winner_buffer[num_winners++] = i;
    }
  }

  add_runout(winner_buffer, num_winners, win_counts, tie_counts, chop_equity, fact);
}

/// @brief Runs the Hold'em enumeration over every board size.
void enumerate_and_score_holdem(const std::vector<card_t>& deck,
                                const std::vector<PokerHand>& hands, const PokerHand& board,
                                std::vector<std::uint64_t>& win_counts,
                                std::vector<std::uint64_t>& tie_counts,
                                std::vector<std::uint64_t>& chop_equity,
                                std::vector<unsigned>& winner_buffer, std::uint64_t fact) {
  const auto score = [&](const PokerHand& river_board) {
    update_holdem_stats(hands, river_board, win_counts, tie_counts, chop_equity, winner_buffer,
                        fact);
  };

  switch (board.size()) {
    case 0:
      enumerate_holdem_boards<5>(deck, score);
      break;
    case 3:
      enumerate_holdem_boards<2>(deck, score, board);
      break;
    case 4:
      enumerate_holdem_boards<1>(deck, score, board);
      break;
    default:
      // `validate_inputs` leaves only a complete board here.
      score(board);
      break;
  }
}

/// @brief Runs the Omaha enumeration over every board size.
void enumerate_and_score_omaha(const std::vector<card_t>& deck,
                               const std::vector<std::vector<card_t>>& hands_cards,
                               BoardCards& running_board, const PokerHand& board,
                               std::vector<std::uint64_t>& win_counts,
                               std::vector<std::uint64_t>& tie_counts,
                               std::vector<std::uint64_t>& chop_equity,
                               std::vector<unsigned>& winner_buffer, std::uint64_t fact) {
  const auto score = [&](const BoardCards& river_board, const PokerHand& river_hand) {
    update_omaha_stats(hands_cards, board_triples_of(river_hand, river_board), win_counts,
                       tie_counts, chop_equity, winner_buffer, fact);
  };

  switch (board.size()) {
    case 0:
      enumerate_omaha_boards<5>(deck, score, running_board, board, 0);
      break;
    case 3:
      enumerate_omaha_boards<2>(deck, score, running_board, board, 3);
      break;
    case 4:
      enumerate_omaha_boards<1>(deck, score, running_board, board, 4);
      break;
    default:
      // `validate_inputs` leaves only a complete board here.
      score(running_board, board);
      break;
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
  std::vector<std::uint64_t> chop_equity(n);
  std::vector<unsigned> winner_buffer(n);

  BoardCards running_board{};
  std::ranges::copy(board_cards, running_board.begin());

  // Every hand holds the same number of cards, so one variant runs per call.
  if (hands_cards.front().size() == HOLDEM_HAND_SIZE) {
    enumerate_and_score_holdem(deck, hands, board, win_counts, tie_counts, chop_equity,
                               winner_buffer, fact);
  } else {
    enumerate_and_score_omaha(deck, hands_cards, running_board, board, win_counts, tie_counts,
                              chop_equity, winner_buffer, fact);
  }

  const auto comb =
      combinations(static_cast<int>(deck.size()), static_cast<int>(cards_to_come));
  const double denom = static_cast<double>(fact * comb);

  std::vector<EquityResult> result;
  result.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    result.push_back({static_cast<double>(win_counts[i]) / denom,
                      static_cast<double>(tie_counts[i]) / denom,
                      static_cast<double>(win_counts[i] + chop_equity[i]) / denom});
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
