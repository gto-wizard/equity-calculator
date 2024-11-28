#pragma once

#include "precalculated_tables.h"

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <ranges>
#include <stdexcept>
#include <string_view>

namespace gtow {

/// The type that represents a card. The value is given by `rank * 4 + suit`.
using card_t = std::uint8_t;

/// A struct representing a set of cards, designed to efficiently merge and
/// evaluate hands.
class PokerHand;

namespace detail {

inline constexpr std::string_view RANK_CHARS = "23456789TJQKA";
inline constexpr std::string_view SUIT_CHARS = "cdhs";

inline constexpr auto NUM_RANKS = RANK_CHARS.size();
inline constexpr auto NUM_SUITS = SUIT_CHARS.size();
inline constexpr auto NUM_CARDS = NUM_RANKS * NUM_SUITS;

constexpr unsigned rank_of_card(card_t card) noexcept {
  return card / NUM_SUITS;
}

constexpr unsigned suit_of_card(card_t card) noexcept {
  return card % NUM_SUITS;
}

constexpr card_t make_card(unsigned rank, unsigned suit) noexcept {
  return rank * NUM_SUITS + suit;
}

/// Basis for each suit.
inline constexpr std::array<std::uint16_t, NUM_SUITS> SUIT_BASIS = {0x1000, 0x100, 0x10, 0x1};

/// Creates an array of `PokerHand` objects for each card singleton.
constexpr std::array<PokerHand, NUM_CARDS> init_eval_hand_array() noexcept;

}  // namespace detail

class PokerHand {
 public:
  /// The maximum number of cards in a hand that can be evaluated.
  static constexpr std::uint8_t MAX_SIZE = 7;

  /// Constructs an empty hand (default constructor).
  constexpr PokerHand() noexcept : misc_data_{0}, suitwise_bitset_{0} {}

  /// @brief Constructs a hand from a set of cards. Throws an exception if
  /// `cards` contains duplicate cards or values outside of the range [0, 51].
  constexpr PokerHand(std::initializer_list<card_t> cards);

  /// @brief Constructs a hand from a set of cards. Throws an exception if
  /// `cards` contains duplicate cards or values outside of the range [0, 51].
  template <std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, card_t>
  explicit constexpr PokerHand(R&& cards);

  /// @brief Constructs a hand from a given string. Throws an exception if the
  /// string contains duplicate cards or invalid characters.
  explicit constexpr PokerHand(std::string_view card_string);

  /// Returns the number of cards in the hand.
  constexpr std::uint8_t size() const noexcept {
    return static_cast<std::uint8_t>(misc_data_ >> 32);
  }

  /// Returns whether the hand is empty.
  constexpr bool empty() const noexcept { return size() == 0; }

  /// Returns whether the hand contains a given card.
  constexpr bool contains(card_t card) const;

  /// Returns whether the hand contains a given hand.
  constexpr bool contains(const PokerHand& other) const noexcept {
    return (suitwise_bitset_ & other.suitwise_bitset_) == other.suitwise_bitset_;
  }

  /// Returns whether the hand collides with a given hand.
  constexpr bool collides_with(const PokerHand& other) const noexcept {
    return (suitwise_bitset_ & other.suitwise_bitset_) != 0;
  }

  /// @brief Returns the strength of the hand (higher is better). Throws an
  /// exception if the hand contains more than 7 cards.
  template <bool CheckSize = true>
  detail::strength_t evaluate() const {
    constexpr std::uint64_t suit_counter_offset = 0x3333'0000'0000'0000;
    constexpr std::uint64_t flush_mask = 0x8888'0000'0000'0000;

    if constexpr (CheckSize) {
      if (size() > MAX_SIZE) [[unlikely]] {
        throw std::runtime_error("PokerHand: Cannot evaluate a hand with more than 7 cards");
      }
    }

    const auto flush_test = (misc_data_ + suit_counter_offset) & flush_mask;

    if (flush_test) {
      const auto shift_count = std::countl_zero(flush_test) * 4;
      const auto flush_suit_bitset = static_cast<std::uint16_t>(suitwise_bitset_ >> shift_count);
      return detail::FLUSH_LOOKUP_TABLE[flush_suit_bitset];
    } else {
      const auto rank_key = static_cast<detail::rank_key_t>(misc_data_);
      const auto offset = detail::RANK_KEY_OFFSET_TABLE[rank_key >> detail::RANK_KEY_OFFSET_SHIFT];
      return detail::NONFLUSH_LOOKUP_TABLE[rank_key + offset];
    }
  }

  /// Merges two hands together. Throws an exception if the two hands contain
  /// the same card.
  template <bool CheckCollision = true>
  constexpr PokerHand& operator+=(const PokerHand& other) {
    if constexpr (CheckCollision) {
      if (collides_with(other)) [[unlikely]] {
        throw std::invalid_argument("PokerHand: Cannot merge two hands that contain the same card");
      }
    }
    misc_data_ += other.misc_data_;
    suitwise_bitset_ += other.suitwise_bitset_;
    return *this;
  }

  /// @brief Subtracts a hand from another hand. Throws an exception if the left
  /// hand does not contain the right hand.
  template <bool CheckContain = true>
  constexpr PokerHand& operator-=(const PokerHand& other) {
    if constexpr (CheckContain) {
      if (!contains(other)) [[unlikely]] {
        throw std::invalid_argument("PokerHand: Cannot subtract a hand that is not contained");
      }
    }
    misc_data_ -= other.misc_data_;
    suitwise_bitset_ -= other.suitwise_bitset_;
    return *this;
  }

  constexpr bool operator==(const PokerHand& other) const noexcept = default;

 private:
  /// An integer that stores miscellaneous data.
  /// @details This data member contains the following information:
  ///   - Bits  0-31 (32 bits): The unique key representing the combination of
  ///   card ranks.
  ///   - Bits 32-39 ( 8 bits): The number of cards in the hand (see `size()`).
  ///   - Bits 40-47 ( 8 bits): Padding.
  ///   - Bits 48-63 (16 bits): The number of cards in each suit (see
  ///   `detail::SUIT_BASIS`).
  /// @note Ideally, this should be a union, but I prefer not to use a union
  /// here for portability.
  std::uint64_t misc_data_;

  /// Concatenation of four 16-bit integers, each representing the rank bitset
  /// of a suit.
  std::uint64_t suitwise_bitset_;

  friend constexpr std::array<PokerHand, detail::NUM_CARDS> detail::init_eval_hand_array() noexcept;
};

/// Merges two hands together. Throws an exception if the two hands contain the
/// same card.
constexpr PokerHand operator+(const PokerHand& lhs, const PokerHand& rhs) {
  PokerHand result(lhs);
  result += rhs;
  return result;
}

/// @brief Subtracts a hand from another hand. Throws an exception if the left
/// hand does not contain the right hand.
constexpr PokerHand operator-(const PokerHand& lhs, const PokerHand& rhs) {
  PokerHand result(lhs);
  result -= rhs;
  return result;
}

namespace detail {

constexpr std::array<PokerHand, NUM_CARDS> init_eval_hand_array() noexcept {
  std::array<PokerHand, NUM_CARDS> result;
  for (card_t card = 0; card < NUM_CARDS; ++card) {
    const auto suit = suit_of_card(card);
    const auto rank = rank_of_card(card);
    result[card].misc_data_ = (static_cast<std::uint64_t>(SUIT_BASIS[suit]) << 48) +
                              (std::uint64_t{1} << 32) + RANK_BASIS[rank];
    result[card].suitwise_bitset_ = std::uint64_t{1} << (suit * 16 + rank);
  }
  return result;
}

/// An array of `PokerHand` objects for each card singleton.
inline constexpr std::array<PokerHand, NUM_CARDS> EVAL_HAND_ARRAY = init_eval_hand_array();

}  // namespace detail

constexpr PokerHand::PokerHand(std::initializer_list<card_t> cards) : PokerHand() {
  std::ranges::for_each(cards, [&](auto card) { *this += detail::EVAL_HAND_ARRAY.at(card); });
}

template <std::ranges::input_range R>
  requires std::same_as<std::ranges::range_value_t<R>, card_t>
constexpr PokerHand::PokerHand(R&& cards) : PokerHand() {
  std::ranges::for_each(cards, [&](auto card) { *this += detail::EVAL_HAND_ARRAY.at(card); });
}

constexpr PokerHand::PokerHand(std::string_view card_string) : PokerHand() {
  if (card_string.size() % 2 != 0) [[unlikely]] {
    throw std::invalid_argument("PokerHand: The card string must be of even length");
  }
  for (unsigned i = 0; i < card_string.size(); i += 2) {
    const auto rank = detail::RANK_CHARS.find(card_string[i]);
    const auto suit = detail::SUIT_CHARS.find(card_string[i + 1]);
    if (rank == std::string_view::npos || suit == std::string_view::npos) [[unlikely]] {
      throw std::invalid_argument("PokerHand: The card string contains invalid characters");
    }
    *this += detail::EVAL_HAND_ARRAY[detail::make_card(rank, suit)];
  }
}

constexpr bool PokerHand::contains(card_t card) const {
  return suitwise_bitset_ & detail::EVAL_HAND_ARRAY.at(card).suitwise_bitset_;
}

}  // namespace gtow
