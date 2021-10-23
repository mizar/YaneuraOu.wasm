#include "leaf_search.hpp"

#include "move_picker.hpp"
#include "proof_hand.hpp"
#include "transposition_table.hpp"
#include "ttentry.hpp"

namespace {
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
template <bool kOrNode>
TTEntry* LeafSearch(TranspositionTable& tt, Position& n, Depth depth, Depth remain_depth, const LookUpQuery& query) {
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

  MovePicker<kOrNode> move_picker{n};
  if (move_picker.empty()) {
    if constexpr (kOrNode) {
      Hand disproof_hand = RemoveIfHandGivesOtherChecks(n, CollectHand(n));
      query.SetDisproven(disproof_hand);
    } else {
      Hand proof_hand = AddIfHandGivesOtherEvasions(n, HAND_ZERO);
      query.SetProven(proof_hand);
    }
    return query.LookUpWithCreation();
  }

  if (remain_depth <= 1) {
    static TTEntry entry;
    entry = {query.HashHigh(), query.GetHand(), 1, 1, depth};
    return &entry;
  }

  bool unknown_flag = false;
  HandSet lose_hand = kOrNode ? HandSet::Full() : HandSet::Zero();
  for (const auto& move : move_picker) {
    auto child_query = tt.GetChildQuery<kOrNode>(n, move.move, depth + 1);
    auto child_entry = child_query.LookUpWithoutCreation();

    if (!query.DoesStored(child_entry) || child_entry->IsFirstVisit()) {
      // 近接王手以外は時間の無駄なので無視する
      if (kOrNode && !IsStepCheck(n, move)) {
        unknown_flag = true;
        continue;
      }

      // まだ FirstSearch していなさそうな node なら掘り進めてみる
      StateInfo st_info;
      n.do_move(move.move, st_info);
      child_entry = LeafSearch<!kOrNode>(tt, n, depth + 1, remain_depth - 1, child_query);
      n.undo_move(move.move);
    }

    if ((kOrNode && child_entry->IsProvenNode()) || (!kOrNode && child_entry->IsNonRepetitionDisprovenNode())) {
      // win
      DeclareWin<kOrNode>(query, ProperChildHand<kOrNode>(n, move.move, child_entry));
      return query.LookUpWithCreation();
    } else if ((!kOrNode && child_entry->IsProvenNode()) || (kOrNode && child_entry->IsNonRepetitionDisprovenNode())) {
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
    static TTEntry entry;
    entry = {query.HashHigh(), query.GetHand(), 1, 1, depth};
    return &entry;
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
    return query.LookUpWithCreation();
  }
}

template TTEntry* LeafSearch<false>(TranspositionTable& tt,
                                    Position& n,
                                    Depth depth,
                                    Depth max_depth,
                                    const LookUpQuery& query);
template TTEntry* LeafSearch<true>(TranspositionTable& tt,
                                   Position& n,
                                   Depth depth,
                                   Depth max_depth,
                                   const LookUpQuery& query);
}  // namespace komori