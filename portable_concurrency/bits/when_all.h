#pragma once

#include <atomic>
#include <type_traits>
#include <utility>

#include "fwd.h"

#include "concurrency_type_traits.h"
#include "future.h"
#include "future_sequence.h"
#include "make_future.h"
#include "shared_future.h"
#include "shared_state.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {

namespace detail {

template <typename Sequence>
class when_all_state final : public future_state<Sequence> {
public:
  when_all_state(Sequence&& futures)
      : futures_(std::move(futures)), operations_remains_(sequence_traits<Sequence>::size(futures_) + 1) {}

  static std::shared_ptr<future_state<Sequence>> make(Sequence&& futures) {
    auto state = std::make_shared<when_all_state<Sequence>>(std::move(futures));
    sequence_traits<Sequence>::for_each(
        state->futures_, [state](auto& f) { state_of(f)->continuations().push([state] { state->notify(); }); });
    state->notify();
    return state;
  }

  void notify() {
    if (--operations_remains_ != 0)
      return;
    continuations_.execute();
  }

  Sequence& value_ref() override {
    assert(continuations_.executed());
    return futures_;
  }

  std::exception_ptr exception() override {
    assert(continuations_.executed());
    return nullptr;
  }

  continuations_stack& continuations() final { return continuations_; }

private:
  Sequence futures_;
  std::atomic<size_t> operations_remains_;
  continuations_stack continuations_;
};

} // namespace detail

future<std::tuple<>> when_all();

template <typename... Futures>
auto when_all(Futures&&... futures) -> std::enable_if_t<detail::are_futures<std::decay_t<Futures>...>::value,
    future<std::tuple<std::decay_t<Futures>...>>> {
  using Sequence = std::tuple<std::decay_t<Futures>...>;
  return {detail::when_all_state<Sequence>::make(Sequence{std::forward<Futures>(futures)...})};
}

template <typename InputIt>
auto when_all(InputIt first, InputIt last)
    -> std::enable_if_t<detail::is_unique_future<typename std::iterator_traits<InputIt>::value_type>::value,
        future<std::vector<typename std::iterator_traits<InputIt>::value_type>>> {
  using Sequence = std::vector<typename std::iterator_traits<InputIt>::value_type>;
  if (first == last)
    return make_ready_future(Sequence{});
  return {
      detail::when_all_state<Sequence>::make(Sequence{std::make_move_iterator(first), std::make_move_iterator(last)})};
}

template <typename InputIt>
auto when_all(InputIt first, InputIt last)
    -> std::enable_if_t<detail::is_shared_future<typename std::iterator_traits<InputIt>::value_type>::value,
        future<std::vector<typename std::iterator_traits<InputIt>::value_type>>> {
  using Sequence = std::vector<typename std::iterator_traits<InputIt>::value_type>;
  if (first == last)
    return make_ready_future(Sequence{});
  return {detail::when_all_state<Sequence>::make(Sequence{first, last})};
}

} // namespace cxx14_v1
} // namespace portable_concurrency
