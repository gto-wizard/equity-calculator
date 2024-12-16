#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "poker_hand.h"

namespace py = pybind11;

namespace gtow {

namespace {

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

template <int N, class Container, class F>
void enumerate_all_boards(const Container& deck, F f, PokerHand board = {}, unsigned index = 0) {
  if constexpr (N == 0) {
    f(board);
  } else {
    for (; index < deck.size(); ++index) {
      const card_t card = deck[index];
      enumerate_all_boards<N - 1>(deck, f, board + PokerHand{card}, index + 1);
    }
  }
}

inline void update_equity(const std::vector<PokerHand>& hands, const PokerHand& board,
                          std::vector<std::uint64_t>& counts, std::vector<unsigned>& winner_buffer,
                          std::uint64_t fact) {
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

  if (num_winners == 1) {
    counts[winner_buffer[0]] += fact;
  } else {
    const auto addend = fact / num_winners;
    for (unsigned i = 0; i < num_winners; ++i) {
      counts[winner_buffer[i]] += addend;
    }
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

std::vector<double> exact_equity(const std::vector<std::vector<card_t>>& hands_cards,
                                 const std::vector<card_t>& board_cards = {}) {
  const auto n = hands_cards.size();
  if (n <= 1) {
    return std::vector<double>(n, 1.0);
  }

  std::vector<PokerHand> hands;
  hands.reserve(n);
  for (const auto& hand : hands_cards) {
    hands.emplace_back(hand);
  }

  if (std::ranges::any_of(hands, [](const PokerHand& hand) { return hand.size() != 2; })) {
    throw std::invalid_argument("exact_equity: Each hand must contain exactly 2 cards");
  }

  const PokerHand board(board_cards);

  PokerHand combined = board;
  for (const auto& hand : hands) {
    combined += hand;
  }

  std::vector<card_t> deck;
  deck.reserve(detail::NUM_CARDS - combined.size());
  for (card_t card = 0; card < detail::NUM_CARDS; ++card) {
    if (!combined.contains(card)) {
      deck.push_back(card);
    }
  }

  const auto fact = factorial(n);
  std::vector<std::uint64_t> counts(n);
  std::vector<unsigned> winner_buffer(n);

  const auto lambda = [&](const PokerHand& river_board) {
    update_equity(hands, river_board, counts, winner_buffer, fact);
  };

  switch (board.size()) {
    case 0:
      enumerate_all_boards<5>(deck, lambda);
      break;
    case 3:
      enumerate_all_boards<2>(deck, lambda, board);
      break;
    case 4:
      enumerate_all_boards<1>(deck, lambda, board);
      break;
    case 5:
      update_equity(hands, board, counts, winner_buffer, fact);
      break;
    default:
      throw std::invalid_argument("exact_equity: The board size must be 0, 3, 4, or 5");
  }

  const auto comb = combinations(deck.size(), 5 - board.size());
  const double denom = fact * comb;

  std::vector<double> result;
  result.reserve(n);
  for (const auto count : counts) {
    result.push_back(count / denom);
  }

  return result;
}

std::vector<double> exact_equity_from_string(const std::vector<std::string_view>& hands_string,
                                             const std::string_view& board_string = "") {
  std::vector<std::vector<card_t>> hands_cards;
  hands_cards.reserve(hands_string.size());
  for (const auto& hand_string : hands_string) {
    hands_cards.push_back(cards_from_string(hand_string));
  }

  std::vector<card_t> board_cards = cards_from_string(board_string);
  return exact_equity(hands_cards, board_cards);
}

}  // namespace gtow

PYBIND11_MODULE(_core, m) {
  m.doc() = "Hand equity calculator for Texas Hold'em poker";

  m.def("cards_from_string", &gtow::cards_from_string, py::arg("cards"),
        "Converts a string of cards to a vector");
  m.def("exact_equity", &gtow::exact_equity, py::arg("hands"),
        py::arg("board") = std::vector<gtow::card_t>{}, "Calculates the exact equity of each hand");
  m.def("exact_equity_from_string", &gtow::exact_equity_from_string, py::arg("hands"),
        py::arg("board") = "",
        "Calculates the exact equity of each hand from a string representation");
}
