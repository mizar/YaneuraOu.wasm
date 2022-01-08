﻿#if !defined(USER_ENGINE)
#define USER_ENGINE
#endif

#include "komoring_heights.hpp"

#include <algorithm>
#include <cmath>

#include "../../mate/mate.h"
#include "children_cache.hpp"
#include "hands.hpp"
#include "move_picker.hpp"
#include "node_history.hpp"
#include "path_keys.hpp"

namespace komori {
namespace {
constexpr std::size_t kDefaultHashSizeMb = 1024;
constexpr std::int64_t kGcInterval = 3000;

/// TT の使用率が kGcHashfullThreshold を超えたら kGcHashfullRemoveRatio だけ削除する
constexpr int kGcHashfullThreshold = 850;
constexpr int kGcHashfullRemoveRatio = 300;

std::vector<std::pair<Move, SearchResult>> ExpandChildren(TranspositionTable& tt, const Node& n) {
  std::vector<std::pair<Move, SearchResult>> ret;
  if (n.IsOrNode()) {
    for (auto&& move : MovePicker{n.Pos(), NodeTag<true>{}}) {
      auto query = tt.GetChildQuery(n, move.move);
      auto entry = query.LookUpWithoutCreation();
      SearchResult result{*entry, query.GetHand()};
      ret.emplace_back(move.move, result);
    }
  } else {
    for (auto&& move : MovePicker{n.Pos(), NodeTag<false>{}}) {
      auto query = tt.GetChildQuery(n, move.move);
      auto entry = query.LookUpWithoutCreation();
      SearchResult result{*entry, query.GetHand()};
      ret.emplace_back(move.move, result);
    }
  }

  return ret;
}

/**
 * @brief move から始まる置換表に保存された手順を返す
 */
std::vector<Move> ExpandBranch(TranspositionTable& tt, Node& n, Move move) {
  std::vector<Move> branch;

  branch.emplace_back(move);
  n.DoMove(move);
  for (;;) {
    bool or_node = n.IsOrNode();
    Move move = tt.LookUpBestMove(n);
    if (move != MOVE_NONE && (!n.Pos().pseudo_legal(move) || !n.Pos().legal(move))) {
      // 現局面の持ち駒 <= 証明駒  なので、置換表に保存された手を指せない可能性がある
      // このときは、子局面の中から一番よさげな手を適当に選ぶ必要がある
      Move best_move = MOVE_NONE;
      Depth mate_len = 0;
      for (const auto& m2 : or_node ? MovePicker{n.Pos(), NodeTag<true>{}} : MovePicker{n.Pos(), NodeTag<false>{}}) {
        auto query = tt.GetChildQuery(n, m2.move);
        auto entry = query.LookUpWithoutCreation();
        if (entry->GetNodeState() != NodeState::kProvenState) {
          continue;
        }

        auto child_mate_len = entry->GetSolutionLen(n.OrHand());
        if ((or_node && child_mate_len + 1 < mate_len) || (!or_node && child_mate_len + 1 > mate_len)) {
          mate_len = child_mate_len;
          best_move = m2.move;
        }
      }
      move = best_move;
    }

    if (!n.Pos().pseudo_legal(move) || !n.Pos().legal(move) || n.IsRepetitionAfter(move)) {
      break;
    }

    n.DoMove(move);
    branch.emplace_back(move);
  }

  if (n.IsOrNode() && !n.Pos().in_check()) {
    // 高速1手詰めルーチンで解ける局面は置換表に登録されていないのでチェックする必要がある
    if (auto move = Mate::mate_1ply(n.Pos()); move != MOVE_NONE) {
      n.DoMove(move);
      branch.emplace_back(move);
    }
  }

  RollBack(n, branch);
  return branch;
}
}  // namespace

void SearchProgress::NewSearch() {
  start_time_ = std::chrono::system_clock::now();
  depth_ = 0;
  move_count_ = 0;
}

void SearchProgress::WriteTo(UsiInfo& output) const {
  auto curr_time = std::chrono::system_clock::now();
  auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - start_time_).count();
  time_ms = std::max(time_ms, decltype(time_ms){1});
  auto nps = move_count_ * 1000ULL / time_ms;

  output.Set(UsiInfo::KeyKind::kSelDepth, depth_)
      .Set(UsiInfo::KeyKind::kTime, time_ms)
      .Set(UsiInfo::KeyKind::kNodes, move_count_)
      .Set(UsiInfo::KeyKind::kNps, nps);
}

KomoringHeights::KomoringHeights() : tt_{kGcHashfullRemoveRatio} {}

void KomoringHeights::Init() {
  Resize(kDefaultHashSizeMb);
}

void KomoringHeights::Resize(std::uint64_t size_mb) {
  tt_.Resize(size_mb);
}

bool KomoringHeights::Search(Position& n, std::atomic_bool& stop_flag) {
  tt_.NewSearch();
  progress_.NewSearch();
  proof_tree_.Clear();
  stop_ = &stop_flag;
  gc_timer_.reset();
  last_gc_ = 0;
  best_moves_.clear();
  tree_size_ = 0;

  Node node{n};
  PnDn thpndn = 1;
  ChildrenCache cache{tt_, node, true, NodeTag<true>{}};
  SearchResult result = cache.CurrentResult(node);
  while (StripMaybeRepetition(result.GetNodeState()) == NodeState::kOtherState && !IsSearchStop()) {
    thpndn = std::max(2 * result.Pn(), 2 * result.Dn());
    thpndn = std::max(thpndn, result.Pn() + 1);
    thpndn = std::max(thpndn, result.Dn() + 1);
    thpndn = std::min(thpndn, kInfinitePnDn);
    score_ = Score::Unknown(result.Pn(), result.Dn());

    result = SearchImpl<true>(node, thpndn, thpndn, cache, false);
  }

  // <for-debug>
  auto query = tt_.GetQuery(node);
  switch (result.GetNodeState()) {
    case NodeState::kProvenState:
      query.SetProven(result.ProperHand(), result.BestMove(), result.GetSolutionLen(), node.GetMoveCount());
      break;
    case NodeState::kDisprovenState:
      query.SetDisproven(result.ProperHand(), result.BestMove(), result.GetSolutionLen(), node.GetMoveCount());
      break;
    case NodeState::kRepetitionState: {
      auto entry = query.LookUpWithCreation();
      query.SetRepetition(entry, node.GetMoveCount());
    } break;
    default:
      auto entry = query.LookUpWithCreation();
      entry->UpdatePnDn(result.Pn(), result.Dn(), node.GetMoveCount());
  }

  std::ostringstream oss;
  auto entry = query.LookUpWithCreation();
  oss << *entry;
  auto usi_output = UsiInfo::String(oss.str());
  progress_.WriteTo(usi_output);
  sync_cout << usi_output << sync_endl;
  // </for-debug>

  if (result.GetNodeState() == NodeState::kProvenState) {
    auto best_move = node.Pos().to_move(tt_.LookUpBestMove(node));
    proof_tree_.AddBranch(node, ExpandBranch(tt_, node, best_move));

    if (yozume_node_count_ > 0 && yozume_search_count_ > 0) {
      DigYozume(node);
    }
    best_moves_ = proof_tree_.GetPv(node);
    score_ = Score::Proven(static_cast<Depth>(best_moves_.size()));
    if (best_moves_.size() % 2 != 1) {
      sync_cout << "info string Failed to detect PV" << sync_endl;
    }
    return true;
  } else {
    score_ = Score::Disproven();
    return false;
  }
}

void KomoringHeights::DigYozume(Node& n) {
  auto best_move = n.Pos().to_move(tt_.LookUpBestMove(n));
  auto best_moves = ExpandBranch(tt_, n, best_move);
  RollForward(n, best_moves);

  std::uint64_t found_count = 0;
  Depth mate_len = kMaxNumMateMoves;
  while (!best_moves.empty()) {
    auto move = best_moves.back();
    best_moves.pop_back();

    n.UndoMove(move);
    proof_tree_.Update(n);
    if (IsSearchStop() || n.GetDepth() >= mate_len - 2 || found_count >= yozume_search_count_) {
      continue;
    }

    if (n.IsOrNode()) {
      // 最善手以外に詰み手順がないか探す
      for (auto&& m2 : MovePicker{n.Pos(), NodeTag<true>{}}) {
        if (proof_tree_.HasEdgeAfter(n, m2.move)) {
          // 既に木に追加されている
          continue;
        }

        auto query = tt_.GetChildQuery(n, m2.move);
        auto entry = query.LookUpWithoutCreation();
        if (entry->GetNodeState() == NodeState::kDisprovenState ||
            entry->GetNodeState() == NodeState::kRepetitionState || n.IsRepetitionAfter(m2.move)) {
          // 既に不詰が示されている
          continue;
        }

        if (StripMaybeRepetition(entry->GetNodeState()) == NodeState::kOtherState) {
          // 再探索
          n.DoMove(m2.move);
          auto max_search_node_org = max_search_node_;
          max_search_node_ = std::min(max_search_node_, n.GetMoveCount() + yozume_node_count_);
          ChildrenCache cache{tt_, n, false, NodeTag<false>{}};
          auto result = SearchImpl<false>(n, kInfinitePnDn, kInfinitePnDn, cache, false);
          max_search_node_ = max_search_node_org;
          n.UndoMove(m2.move);

          switch (result.GetNodeState()) {
            case NodeState::kProvenState:
              query.SetProven(result.ProperHand(), result.BestMove(), result.GetSolutionLen(), n.GetMoveCount());
              break;
            case NodeState::kDisprovenState:
              query.SetDisproven(result.ProperHand(), result.BestMove(), result.GetSolutionLen(), n.GetMoveCount());
              break;
            case NodeState::kRepetitionState: {
              entry = query.RefreshWithoutCreation(entry);
              query.SetRepetition(entry, n.GetMoveCount());
            } break;
            default:
              entry = query.RefreshWithoutCreation(entry);
              entry->UpdatePnDn(result.Pn(), result.Dn(), n.GetMoveCount());
          }

          entry = query.RefreshWithoutCreation(entry);
        }

        if (entry->GetNodeState() == NodeState::kProvenState) {
          // 新しく手を見つけた
          ++found_count;

          auto new_branch = ExpandBranch(tt_, n, m2.move);
          proof_tree_.AddBranch(n, new_branch);

          new_branch = proof_tree_.GetPv(n);
          RollForward(n, new_branch);
          std::copy(new_branch.begin(), new_branch.end(), std::back_inserter(best_moves));
          mate_len = std::min(mate_len, static_cast<Depth>(best_moves.size()));
          break;
        }
      }
    } else {
      // AND node
      // 余詰探索の結果、AND node の最善手が変わっている可能性がある
      // 現在の詰み手順よりも長く生き延びられる手があるなら、そちらの読みを進めてみる
      for (auto&& m2 : MovePicker{n.Pos(), NodeTag<false>{}}) {
        if (!proof_tree_.HasEdgeAfter(n, m2.move)) {
          auto branch = ExpandBranch(tt_, n, m2.move);
          proof_tree_.AddBranch(n, branch);
        }
      }

      if (auto new_mate_len = proof_tree_.MateLen(n) + n.GetDepth(); new_mate_len > mate_len) {
        // こっちに逃げたほうが手数が伸びる
        auto best_branch = proof_tree_.GetPv(n);

        // 千日手が絡むと、pv.size() と MateLen() が一致しないことがある
        // これは、pv の中に best_moves で一度通った局面が含まれるときに発生する
        // このような AND node は深く探索する必要がない。なぜなら、best_move の選び方にそもそも問題があるためである
        mate_len = new_mate_len;
        RollForward(n, best_branch);
        std::copy(best_branch.begin(), best_branch.end(), std::back_inserter(best_moves));
      }
    }
  }
}

void KomoringHeights::ShowValues(Position& n, const std::vector<Move>& moves) {
  auto depth_max = static_cast<Depth>(moves.size());
  Key path_key = 0;
  Node node{n};
  for (Depth depth = 0; depth < depth_max; ++depth) {
    path_key = PathKeyAfter(path_key, moves[depth], depth);
    node.DoMove(moves[depth]);
  }

  if (depth_max % 2 == 0) {
    for (auto&& move : MovePicker{n, NodeTag<true>{}}) {
      auto query = tt_.GetChildQuery(node, move.move);
      auto entry = query.LookUpWithoutCreation();
      sync_cout << move.move << " " << *entry << sync_endl;
    }
  } else {
    for (auto&& move : MovePicker{n, NodeTag<false>{}}) {
      auto query = tt_.GetChildQuery(node, move.move);
      auto entry = query.LookUpWithoutCreation();
      sync_cout << move.move << " " << *entry << sync_endl;
    }
  }

  static_assert(std::is_signed_v<Depth>);
  for (Depth depth = depth_max - 1; depth >= 0; --depth) {
    node.UndoMove(moves[depth]);
  }
}

void KomoringHeights::ShowPv(Position& n) {
  Node node{n};
  std::vector<Move> moves;

  for (;;) {
    auto children = ExpandChildren(tt_, node);
    std::sort(children.begin(), children.end(), [&](const auto& lhs, const auto& rhs) {
      if (node.IsOrNode()) {
        if (lhs.second.Pn() != rhs.second.Pn()) {
          return lhs.second.Pn() < rhs.second.Pn();
        } else if (lhs.second.Pn() == 0 && rhs.second.Pn() == 0) {
          return lhs.second.GetSolutionLen() < rhs.second.GetSolutionLen();
        }

        if (lhs.second.Dn() != rhs.second.Dn()) {
          return lhs.second.Dn() > rhs.second.Dn();
        } else if (lhs.second.Dn() == 0 && rhs.second.Dn() == 0) {
          return lhs.second.GetSolutionLen() > rhs.second.GetSolutionLen();
        }
        return false;
      } else {
        if (lhs.second.Dn() != rhs.second.Dn()) {
          return lhs.second.Dn() < rhs.second.Dn();
        } else if (lhs.second.Dn() == 0 && rhs.second.Dn() == 0) {
          return lhs.second.GetSolutionLen() < rhs.second.GetSolutionLen();
        }

        if (lhs.second.Pn() != rhs.second.Pn()) {
          return lhs.second.Pn() > rhs.second.Pn();
        } else if (lhs.second.Pn() == 0 && rhs.second.Pn() == 0) {
          return lhs.second.GetSolutionLen() > rhs.second.GetSolutionLen();
        }
        return false;
      }
    });

    std::ostringstream oss;
    oss << "[" << node.GetDepth() << "] ";
    for (const auto& child : children) {
      if (child.second.Pn() == 0) {
        oss << child.first << "(+" << child.second.GetSolutionLen() << ") ";
      } else if (child.second.Dn() == 0) {
        oss << child.first << "(-" << child.second.GetSolutionLen() << ") ";
      } else {
        oss << child.first << "(" << ToString(child.second.Pn()) << "/" << ToString(child.second.Dn()) << ") ";
      }
    }
    sync_cout << oss.str() << sync_endl;

    if (children.empty() || (children[0].second.Pn() == 1 && children[0].second.Dn() == 1)) {
      break;
    }
    auto best_move = children[0].first;
    node.DoMove(best_move);
    moves.emplace_back(best_move);
    if (node.IsRepetition()) {
      break;
    }
  }

  // 高速 1 手詰めルーチンで解ける局面は置換表に登録されていない可能性がある
  if (node.IsOrNode()) {
    if (Move move = Mate::mate_1ply(node.Pos()); move != MOVE_NONE) {
      node.DoMove(move);
      moves.emplace_back(move);
    }
  }

  sync_cout << sync_endl;
  std::ostringstream oss;
  for (const auto& move : moves) {
    oss << move << " ";
  }
  sync_cout << "pv: " << oss.str() << sync_endl;

  sync_cout << node.Pos() << sync_endl;
  for (auto itr = moves.crbegin(); itr != moves.crend(); ++itr) {
    node.UndoMove(*itr);
  }
}

UsiInfo KomoringHeights::Info() const {
  UsiInfo usi_output{};
  progress_.WriteTo(usi_output);
  usi_output.Set(UsiInfo::KeyKind::kHashfull, tt_.Hashfull()).Set(UsiInfo::KeyKind::kScore, score_);

  return usi_output;
}

void KomoringHeights::PrintDebugInfo() const {
  auto stat = tt_.GetStat();
  std::ostringstream oss;

  oss << "hashfull=" << stat.hashfull << " proven=" << stat.proven_ratio << " disproven=" << stat.disproven_ratio
      << " repetition=" << stat.repetition_ratio << " maybe_repetition=" << stat.maybe_repetition_ratio
      << " other=" << stat.other_ratio;

  sync_cout << UsiInfo::String(oss.str()) << sync_endl;
}

template <bool kOrNode>
SearchResult KomoringHeights::SearchImpl(Node& n, PnDn thpn, PnDn thdn, ChildrenCache& cache, bool inc_flag) {
  progress_.Visit(n.GetDepth(), n.GetMoveCount());

  if (print_flag_) {
    PrintProgress(n);
    print_flag_ = false;
  }

  // 深さ制限。これ以上探索を続けても詰みが見つかる見込みがないのでここで early return する。
  if (n.IsExceedLimit(max_depth_)) {
    return {NodeState::kRepetitionState, kMinimumSearchedAmount, kInfinitePnDn, 0, n.OrHand()};
  }

  auto curr_result = cache.CurrentResult(n);
  // 探索延長。浅い結果を参照している場合、無限ループになる可能性があるので少しだけ探索を延長する
  inc_flag = inc_flag || cache.DoesHaveOldChild();
  if (inc_flag && curr_result.Pn() > 0 && curr_result.Dn() > 0) {
    if constexpr (kOrNode) {
      thdn = Clamp(thdn, curr_result.Dn() + 1);
    } else {
      thpn = Clamp(thpn, curr_result.Pn() + 1);
    }
  }

  if (gc_timer_.elapsed() > last_gc_ + kGcInterval) {
    if (tt_.Hashfull() >= kGcHashfullThreshold) {
      tt_.CollectGarbage();
    }
    last_gc_ = gc_timer_.elapsed();
  }

  std::unordered_map<Move, ChildrenCache*> cache_cache;
  while (!IsSearchStop()) {
    if (curr_result.Pn() >= thpn || curr_result.Dn() >= thdn) {
      break;
    }

    // 最も良さげな子ノードを展開する
    auto best_move = cache.BestMove();
    bool is_first_search = cache.BestMoveIsFirstVisit();
    auto [child_thpn, child_thdn] = cache.ChildThreshold(thpn, thdn);
    if (is_first_search) {
      inc_flag = false;
    }

    auto move_count_org = n.GetMoveCount();
    n.DoMove(best_move);
    auto& child_cache = children_cache_.emplace(tt_, n, is_first_search, NodeTag<!kOrNode>{});
    SearchResult child_result;
    if (is_first_search) {
      child_result = child_cache.CurrentResult(n);
    } else {
      child_result = SearchImpl<!kOrNode>(n, child_thpn, child_thdn, child_cache, inc_flag);
    }

    children_cache_.pop();
    n.UndoMove(best_move);

    cache.UpdateFront(child_result, n.GetMoveCount() - move_count_org);
    curr_result = cache.CurrentResult(n);
  }

  return curr_result;
}

template <bool kOrNode>
std::pair<KomoringHeights::NumMoves, Depth> KomoringHeights::MateMovesSearchImpl(
    std::unordered_map<Key, MateMoveCache>& mate_table,
    std::unordered_map<Key, Depth>& search_history,
    Node& n,
    Depth max_start_depth,
    Depth max_depth) {
  auto key = n.Pos().key();
  tree_size_++;
  if (auto itr = search_history.find(key); itr != search_history.end()) {
    // 探索中の局面にあたったら、不詰を返す
    return {{}, itr->second};
  }

  if (n.GetDepth() > max_depth) {
    // これより短い詰みを知っているので、これ以上探索する意味がない
    return {{}, max_start_depth};
  }

  if (auto itr = mate_table.find(key); itr != mate_table.end()) {
    // 以前訪れたことがあるノードの場合、その結果をそのまま返す
    return {itr->second.num_moves, kNonRepetitionDepth};
  }

  if (kOrNode && !n.Pos().in_check()) {
    if (auto move = Mate::mate_1ply(n.Pos()); move != MOVE_NONE) {
      auto after_hand = AfterHand(n.Pos(), move, n.OrHand());
      NumMoves num_moves = {1, CountHand(after_hand)};
      mate_table[key] = {move, num_moves};
      return {num_moves, kNonRepetitionDepth};
    }
  }

  search_history.insert(std::make_pair(key, n.GetDepth()));
  auto& move_picker = pickers_.emplace(n.Pos(), NodeTag<kOrNode>{});
  auto picker_is_empty = move_picker.empty();

  MateMoveCache curr{};
  curr.num_moves.num = kOrNode ? kMaxNumMateMoves : 0;
  Depth rep_start = kNonRepetitionDepth;

  for (const auto& move : move_picker) {
    auto child_query = tt_.GetChildQuery(n, move.move);
    auto child_entry = child_query.LookUpWithoutCreation();
    if (child_entry->GetNodeState() != NodeState::kProvenState) {
      continue;
    }

    auto child_capture = n.Pos().capture(move.move);
    n.DoMove(move.move);
    auto [child_num_moves, child_rep_start] =
        MateMovesSearchImpl<!kOrNode>(mate_table, search_history, n, max_start_depth, max_depth);
    n.UndoMove(move.move);

    rep_start = std::min(rep_start, child_rep_start);
    if (child_num_moves.num >= 0) {
      curr.Update<kOrNode>(move.move, child_num_moves.num + 1, child_num_moves.surplus + (child_capture ? 1 : 0));

      if constexpr (kOrNode) {
        if (max_depth > n.GetDepth() + curr.num_moves.num) {
          max_start_depth = n.GetDepth();
          max_depth = n.GetDepth() + curr.num_moves.num;
        }
      }
    } else if (!kOrNode) {
      // nomate
      curr = {};
      break;
    }
  }
  search_history.erase(key);
  pickers_.pop();

  if (!kOrNode && picker_is_empty) {
    curr.num_moves.num = 0;
    curr.num_moves.surplus = n.OrHand();
  }

  if (rep_start >= n.GetDepth()) {
    mate_table[key] = curr;
    if (rep_start == n.GetDepth() && curr.num_moves.num >= 0) {
      n.DoMove(curr.move);
      std::unordered_map<Key, Depth> new_search_history;
      MateMovesSearchImpl<!kOrNode>(mate_table, new_search_history, n, max_start_depth, max_depth);
      n.UndoMove(curr.move);
    }
  }

  return {curr.num_moves, rep_start};
}

void KomoringHeights::PrintProgress(const Node& n) const {
  auto usi_output = Info();

  usi_output.Set(UsiInfo::KeyKind::kDepth, n.GetDepth());
#if defined(KEEP_LAST_MOVE)
  usi_output.Set(UsiInfo::KeyKind::kPv, n.Pos().moves_from_start());
#endif

  sync_cout << usi_output << sync_endl;
}

bool KomoringHeights::IsSearchStop() const {
  return progress_.MoveCount() > max_search_node_ || *stop_;
}

template SearchResult KomoringHeights::SearchImpl<true>(Node& n,
                                                        PnDn thpn,
                                                        PnDn thdn,
                                                        ChildrenCache& cache,
                                                        bool inc_flag);
template SearchResult KomoringHeights::SearchImpl<false>(Node& n,
                                                         PnDn thpn,
                                                         PnDn thdn,
                                                         ChildrenCache& cache,
                                                         bool inc_flag);
template std::pair<KomoringHeights::NumMoves, Depth> KomoringHeights::MateMovesSearchImpl<false>(
    std::unordered_map<Key, MateMoveCache>& mate_table,
    std::unordered_map<Key, Depth>& search_history,
    Node& n,
    Depth max_start_depth,
    Depth max_depth);
template std::pair<KomoringHeights::NumMoves, Depth> KomoringHeights::MateMovesSearchImpl<true>(
    std::unordered_map<Key, MateMoveCache>& mate_table,
    std::unordered_map<Key, Depth>& search_history,
    Node& n,
    Depth max_start_depth,
    Depth max_depth);
}  // namespace komori
