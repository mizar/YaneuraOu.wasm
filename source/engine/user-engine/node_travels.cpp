#include "node_travels.hpp"

#include <variant>

#include "../../mate/mate.h"
#include "move_picker.hpp"
#include "proof_hand.hpp"
#include "transposition_table.hpp"
#include "ttentry.hpp"

namespace {
constexpr int kNonProven = -1;
constexpr int kRepetitionNonProven = -2;

template <bool kOrNode>
inline Hand OrHand(const Position& n) {
  if constexpr (kOrNode) {
    return n.hand_of(n.side_to_move());
  } else {
    return n.hand_of(~n.side_to_move());
  }
}

template <bool kOrNode>
inline void DeclareWin(const komori::LookUpQuery& query, Hand hand) {
  if constexpr (kOrNode) {
    query.SetProven(hand);
  } else {
    query.SetDisproven(hand);
  }
}

template <bool kOrNode>
inline Hand ProperChildHand(const Position& n, Move move, komori::TTEntry* child_entry) {
  if constexpr (kOrNode) {
    Hand after_hand = child_entry->ProperHand(komori::AfterHand(n, move, OrHand<kOrNode>(n)));
    return komori::BeforeHand(n, move, after_hand);
  } else {
    return child_entry->ProperHand(OrHand<kOrNode>(n));
  }
}

/// OrNode で move が近接王手となるか判定する
bool IsStepCheck(const Position& n, Move move) {
  auto us = n.side_to_move();
  auto them = ~us;
  auto king_sq = n.king_square(them);
  Piece pc = n.moved_piece_after(move);
  PieceType pt = type_of(pc);

  return komori::StepEffect(pt, us, to_sq(move)).test(king_sq);
}

}  // namespace

namespace komori {
NodeTravels::NodeTravels(TranspositionTable& tt) : tt_{tt} {
  or_pickers_.reserve(kMaxNumMateMoves);
  and_pickers_.reserve(kMaxNumMateMoves);
}

template <bool kOrNode>
TTEntry* NodeTravels::LeafSearch(Position& n, Depth depth, Depth remain_depth, const LookUpQuery& query) {
  if (kOrNode && !n.in_check()) {
    if (auto move = Mate::mate_1ply(n); move != MOVE_NONE) {
      HandSet proof_hand = HandSet::Zero();
      auto curr_hand = OrHand<kOrNode>(n);

      StateInfo st_info;
      n.do_move(move, st_info);
      proof_hand |= BeforeHand(n, move, AddIfHandGivesOtherEvasions(n, HAND_ZERO));
      n.undo_move(move);

      proof_hand &= curr_hand;
      query.SetProven(proof_hand.Get());
      return query.LookUpWithCreation();
    }
  }

  // stack消費を抑えるために、vectorの中にMovePickerを構築する
  // 関数から抜ける前に、必ず pop_back() しなければならない
  auto& move_picker = PushMovePicker<kOrNode>(n);
  {
    if (move_picker.empty() || depth > kMaxNumMateMoves) {
      if constexpr (kOrNode) {
        Hand disproof_hand = RemoveIfHandGivesOtherChecks(n, CollectHand(n));
        query.SetDisproven(disproof_hand);
      } else {
        Hand proof_hand = AddIfHandGivesOtherEvasions(n, HAND_ZERO);
        query.SetProven(proof_hand);
      }
      goto SEARCH_FOUND;
    }

    if (remain_depth <= 1) {
      goto SEARCH_NOT_FOUND;
    }

    bool unknown_flag = false;
    HandSet lose_hand = kOrNode ? HandSet::Full() : HandSet::Zero();
    for (const auto& move : move_picker) {
      auto child_query = tt_.GetChildQuery<kOrNode>(n, move.move, depth + 1);
      auto child_entry = child_query.LookUpWithoutCreation();

      if (!query.DoesStored(child_entry) || child_entry->IsFirstVisit()) {
        // 近接王手以外は時間の無駄なので無視する
        if (kOrNode && !IsStepCheck(n, move)) {
          unknown_flag = true;
          continue;
        }

        // まだ FirstSearch していなさそうな node なら掘り進めてみる
        DoMove(n, move.move, depth);
        child_entry = LeafSearch<!kOrNode>(n, depth + 1, remain_depth - 1, child_query);
        UndoMove(n, move.move);
      }

      if ((kOrNode && child_entry->IsProvenNode()) || (!kOrNode && child_entry->IsNonRepetitionDisprovenNode())) {
        // win
        DeclareWin<kOrNode>(query, ProperChildHand<kOrNode>(n, move.move, child_entry));
        goto SEARCH_FOUND;
      } else if ((!kOrNode && child_entry->IsProvenNode()) ||
                 (kOrNode && child_entry->IsNonRepetitionDisprovenNode())) {
        // lose
        if (!unknown_flag) {
          // unknown のときは lose_hand を真面目に更新する必要がない
          if constexpr (kOrNode) {
            lose_hand &= ProperChildHand<kOrNode>(n, move.move, child_entry);
          } else {
            lose_hand |= ProperChildHand<kOrNode>(n, move.move, child_entry);
          }
        }
      } else {
        // unknown
        unknown_flag = true;
      }
    }

    if (unknown_flag) {
      goto SEARCH_NOT_FOUND;
    } else {
      if constexpr (kOrNode) {
        lose_hand |= OrHand<kOrNode>(n);
        auto disproof_hand = RemoveIfHandGivesOtherChecks(n, lose_hand.Get());
        query.SetDisproven(disproof_hand);
      } else {
        lose_hand &= OrHand<kOrNode>(n);
        auto proof_hand = AddIfHandGivesOtherEvasions(n, lose_hand.Get());
        query.SetProven(proof_hand);
      }
    }
  }

  // passthrough
SEARCH_FOUND:
  PopMovePicker<kOrNode>();
  return query.LookUpWithCreation();

SEARCH_NOT_FOUND:
  PopMovePicker<kOrNode>();
  static TTEntry entry;
  entry = {query.HashHigh(), query.GetHand(), 1, 1, depth};
  return &entry;
}

template <bool kOrNode>
void NodeTravels::MarkDeleteCandidates(Position& n,
                                       Depth depth,
                                       std::unordered_set<Key>& parents,
                                       const LookUpQuery& query,
                                       TTEntry* entry) {
  entry->MarkDeleteCandidate();
  if (depth > kMaxNumMateMoves) {
    return;
  }

  auto& move_picker = PushMovePicker<kOrNode>(n);
  for (const auto& move : move_picker) {
    auto child_query = tt_.GetChildQuery<kOrNode>(n, move.move, depth + 1);
    auto child_entry = child_query.LookUpWithoutCreation();

    if (!query.DoesStored(child_entry) || child_entry->IsProvenNode() || child_entry->IsNonRepetitionDisprovenNode()) {
      continue;
    }

    DoMove(n, move.move, depth);
    if (parents.find(n.key()) == parents.end()) {
      parents.insert(n.key());
      MarkDeleteCandidates<!kOrNode>(n, depth + 1, parents, child_query, child_entry);
      parents.erase(n.key());
    }
    UndoMove(n, move.move);
  }
  PopMovePicker<kOrNode>();
}

template <bool kOrNode>
int NodeTravels::MateMovesSearch(std::unordered_map<Key, Move>& memo, Position& n, int depth) {
  auto key = n.key();
  if (auto itr = memo.find(key); itr != memo.end() || depth > kMaxNumMateMoves) {
    return kRepetitionNonProven;
  }

  if (kOrNode && !n.in_check()) {
    if (auto move = Mate::mate_1ply(n); move != MOVE_NONE) {
      memo[key] = move;
      return 1;
    }
  }

  memo[key] = Move::MOVE_NONE;
  Move curr_move = Move::MOVE_NONE;
  Depth curr_depth = kOrNode ? kMaxNumMateMoves : 0;

  auto& move_picker = PushMovePicker<kOrNode>(n);
  for (const auto& move : move_picker) {
    auto child_query = tt_.GetChildQuery<kOrNode>(n, move.move, depth + 1);
    auto child_entry = child_query.LookUpWithoutCreation();
    if (!child_entry->IsProvenNode()) {
      continue;
    }

    DoMove(n, move.move, depth);
    auto child_depth = MateMovesSearch<!kOrNode>(memo, n, depth + 1);
    UndoMove(n, move.move);

    if (child_depth >= 0) {
      if constexpr (kOrNode) {
        if (curr_depth > child_depth + 1) {
          curr_move = move.move;
          curr_depth = child_depth + 1;
        }
      } else {
        if (curr_depth < child_depth + 1) {
          curr_move = move.move;
          curr_depth = child_depth + 1;
        }
      }
    }
  }
  PopMovePicker<kOrNode>();

  if (kOrNode && curr_depth == kMaxNumMateMoves) {
    memo.erase(key);
    return kRepetitionNonProven;
  }

  memo[key] = curr_move;
  return curr_depth;
}

template <>
MovePicker<true>& NodeTravels::PushMovePicker<true>(Position& n) {
  return or_pickers_.emplace_back(n);
}

template <>
MovePicker<false>& NodeTravels::PushMovePicker<false>(Position& n) {
  return and_pickers_.emplace_back(n);
}

template <>
void NodeTravels::PopMovePicker<true>() {
  or_pickers_.pop_back();
}

template <>
void NodeTravels::PopMovePicker<false>() {
  and_pickers_.pop_back();
}

template TTEntry* NodeTravels::LeafSearch<false>(Position& n, Depth depth, Depth max_depth, const LookUpQuery& query);
template TTEntry* NodeTravels::LeafSearch<true>(Position& n, Depth depth, Depth max_depth, const LookUpQuery& query);
template void NodeTravels::MarkDeleteCandidates<false>(Position& n,
                                                       Depth depth,
                                                       std::unordered_set<Key>& parents,
                                                       const LookUpQuery& query,
                                                       TTEntry* entry);
template void NodeTravels::MarkDeleteCandidates<true>(Position& n,
                                                      Depth depth,
                                                      std::unordered_set<Key>& parents,
                                                      const LookUpQuery& query,
                                                      TTEntry* entry);
template int NodeTravels::MateMovesSearch<false>(std::unordered_map<Key, Move>& memo, Position& n, int depth);
template int NodeTravels::MateMovesSearch<true>(std::unordered_map<Key, Move>& memo, Position& n, int depth);
}  // namespace komori