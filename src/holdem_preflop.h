#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>

#include "poker_hand.h"

namespace gtow::holdem_preflop {

/// The exact outcome of one heads-up Hold'em preflop matchup.
/// @details The counts are runouts, out of `BOARD_COUNT`. A runout neither
/// hand wins alone is a chop, so the tie count is
/// `BOARD_COUNT - first_wins - second_wins`.
struct Matchup {
  std::uint32_t key;
  std::uint32_t first_wins;
  std::uint32_t second_wins;
};

/// The number of five-card boards two hands leave: C(48, 5).
inline constexpr std::uint32_t BOARD_COUNT = 1'712'304;

/// Every heads-up preflop matchup, one per class of suit relabeling, sorted
/// by `key`. `tools/generate_holdem_preflop_table.py` writes the rows.
inline constexpr Matchup MATCHUPS[] = {
#include "holdem_preflop_table.inc"
};

inline constexpr std::size_t MATCHUP_COUNT = std::size(MATCHUPS);

static_assert(MATCHUP_COUNT == 47'008);

struct CanonicalMatchup {
  std::uint32_t key;
  /// Whether the table stores the second hand first.
  bool swapped;
};

constexpr std::uint32_t hand_key(card_t card_1, card_t card_2) noexcept {
  return (std::max(card_1, card_2) << 6) | std::min(card_1, card_2);
}

/// @brief Returns the key of the matchup class and the hand order it stores.
/// @details Equity does not change when the suits are relabeled or the hands
/// change seats. The key is the smallest encoding over the 24 relabelings and
/// both seat orders, so every member of a class gets the same key.
constexpr CanonicalMatchup canonical_matchup(const std::array<card_t, 2>& first,
                                             const std::array<card_t, 2>& second) {
  std::array<unsigned, detail::NUM_SUITS> suit_map{0, 1, 2, 3};
  CanonicalMatchup best{std::numeric_limits<std::uint32_t>::max(), false};

  do {
    const auto relabel = [&](card_t card) {
      return detail::make_card(detail::rank_of_card(card), suit_map[detail::suit_of_card(card)]);
    };
    const auto first_key = hand_key(relabel(first[0]), relabel(first[1]));
    const auto second_key = hand_key(relabel(second[0]), relabel(second[1]));

    if (const auto key = (first_key << 12) | second_key; key < best.key) {
      best = {key, false};
    }
    if (const auto key = (second_key << 12) | first_key; key < best.key) {
      best = {key, true};
    }
  } while (std::ranges::next_permutation(suit_map).found);

  return best;
}

/// @brief Returns the table row of a matchup key.
/// @details The table holds every class, so a missing key is a bug in the
/// table or in `canonical_matchup`, never a bad input.
inline const Matchup& find_matchup(std::uint32_t key) {
  const auto* row = std::ranges::lower_bound(MATCHUPS, key, {}, &Matchup::key);
  if (row == std::end(MATCHUPS) || row->key != key) [[unlikely]] {
    throw std::logic_error("holdem_preflop: The matchup is missing from the table");
  }
  return *row;
}

}  // namespace gtow::holdem_preflop
