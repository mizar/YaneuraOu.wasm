#ifndef TRANSPOSITION_TABLE_HPP_
#define TRANSPOSITION_TABLE_HPP_

#include <vector>

#include "typedefs.hpp"

namespace komori {

// forward declaration
class TTEntry;
// forward declaration
class TTCluster;

/**
 * @brief Cluster を LookUp するためのキャッシュするクラス
 *
 * 本クラスでは、 TTCluster を LookUp するための情報を保存保存しておき、同一局面に対する LookUp 呼び出しを簡略化したり、
 * 前回の LookUp 結果を再利用することで高速化することを目的とするクラスである。
 * 詰将棋の探索において、最も時間がかかる処理は置換表の LookUp である。本将棋の AI とは異なり、詰将棋の LookUp 時は
 * 証明駒・反証駒の探索を行わなければならないので、hash による一発表引きだけでは済まないことが処理負荷増加の一因と
 * なっている。
 */
class LookUpQuery {
 public:
  /// キャッシュに使用するため、デフォルトコンストラクタを有効にする
  LookUpQuery() = default;
  LookUpQuery(const LookUpQuery&) = delete;
  LookUpQuery(LookUpQuery&&) noexcept = default;
  LookUpQuery& operator=(const LookUpQuery&) = delete;
  LookUpQuery& operator=(LookUpQuery&&) noexcept = default;
  LookUpQuery(TTCluster* cluster, std::uint32_t hash_high, Hand hand, Depth depth);

  /// Query によるエントリ問い合わせを行う。もし見つからなかった場合は新規作成して cluster に追加する
  TTEntry* LookUpWithCreation() const;
  /**
   * @brief  Query によるエントリ問い合わせを行う。もし見つからなかった場合はダミーのエントリを返す。
   *
   * ダミーエントリが返されたかどうかは DoesStored() により判定可能である。このエントリは次回の LookUp までの間まで
   * 有効である。
   */
  TTEntry* LookUpWithoutCreation() const;

  /**
   * @brief entry が有効（前回呼び出しから移動していない）場合、それをそのまま帰す。
   *
   * entry が無効の場合、改めて LookUpWithCreation() する。
   */
  TTEntry* RefreshWithCreation(TTEntry* entry) const;
  /**
   * @brief entry が有効（前回呼び出しから移動していない）場合、それをそのまま帰す。
   *
   * entry が無効の場合、改めて LookUpWithoutCreation() する。
   */
  TTEntry* RefreshWithoutCreation(TTEntry* entry) const;

  /// 調べていた局面が証明駒 `proof_hand` で詰みであることを報告する
  void SetProven(Hand proof_hand) const;
  /// 調べていた局面が反証駒 `disproof_hand` で詰みであることを報告する
  void SetDisproven(Hand disproof_hand) const;
  /// `entry` が cluster に存在するエントリかを問い合わせる。（ダミーエントリのチェックに使用する）
  bool DoesStored(TTEntry* entry) const;
  /// `entry` が有効（前回呼び出しから移動していない）かどうかをチェックする
  bool IsValid(TTEntry* entry) const;

  /// query 時点の手駒を返す
  Hand GetHand() const { return hand_; }
  std::uint32_t HashHigh() const { return hash_high_; }

 private:
  TTCluster* cluster_;
  std::uint32_t hash_high_;
  Hand hand_;
  Depth depth_;
};

/**
 * @brief 詰将棋探索における置換表本体
 *
 * 高速化のために、直接 LookUp させるのではなく、LookUpQuery を返すことで結果のキャッシュが可能にしている。
 */
class TranspositionTable {
 public:
  TranspositionTable(void);

  /// ハッシュサイズを `hash_size_mb` （以下）に変更する。以前に保存されていた結果は削除される
  void Resize(std::uint64_t hash_size_mb);
  /// 以前の探索結果をすべて削除し、新たな探索をを始める
  void NewSearch();

  /// 局面 `n` で探索深さ `depth` のとき、LookUp 用の構造体を取得する
  template <bool kOrNode>
  LookUpQuery GetQuery(const Position& n, Depth depth);
  /// 局面 `n` から `move` で進めた局面で探索深さ `depth` の、LookUp 用の構造体を取得する
  /// なお、depth は 局面を進めた時点の探索深さを渡す必要がある。
  template <bool kOrNode>
  LookUpQuery GetChildQuery(const Position& n, Move move, Depth depth);

  /// ハッシュ使用率を返す（戻り値は千分率）
  int Hashfull() const;

 private:
  /// `board_key` に対応する cluster を返す
  TTCluster& ClusterOf(Key board_key);

  /// 置換表本体。キャッシュラインに乗せるために、実態より少し多めにメモリ確保を行う
  std::vector<uint8_t> tt_raw_{};
  /// 置換表配列にアクセスするためのポインタ
  TTCluster* tt_{nullptr};
  /// 置換表に保存されているクラスタ数
  std::uint64_t num_clusters_{0};
  /// `tt_` にアクセスするためのビットマスク
  std::uint64_t clusters_mask_{0};
};
}  // namespace komori
#endif  // TRANSPOSITION_TABLE_HPP_