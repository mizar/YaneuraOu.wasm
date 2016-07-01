﻿#include "../shogi.h"

// 学習関係のルーチン
// 1) 棋譜の自動生成
// 2) 生成した棋譜からの評価関数パラメーターの学習
// 3) 定跡の自動生成
// 4) 局後自動検討モード
// etc..


#if defined(EVAL_LEARN) && defined(YANEURAOU_2016_MID_ENGINE)

#include <sstream>
#include <fstream>

#include "../misc.h"
#include "../thread.h"
#include "../position.h"

using namespace std;

extern void is_ready(Position& pos);

namespace Learner
{

// いまのところ、やねうら王2016Midしか、このスタブを持っていない。
extern pair<Value, vector<Move> >  search(Position& pos, Value alpha, Value beta, int depth);
extern pair<Value, vector<Move> > qsearch(Position& pos, Value alpha, Value beta);


// -----------------------------------
//    局面のファイルへの書き出し
// -----------------------------------

// Sfenを書き出して行くためのヘルパクラス
struct SfenWriter
{
  SfenWriter(int thread_num,u64 sfen_count_limit_)
  {
    sfen_count_limit = sfen_count_limit_;
    sfen_buffers.resize(thread_num);
    fs.open("generated_kifu.sfen", ios::out);
  }

  // この行数ごとにファイルにflushする。
  // 探索深さ3なら1スレッドあたり1秒で1000局面ほど作れる。
  // 40コア80HTで秒間10万局面。1日で80億ぐらい作れる。
  // 80億=8G , 1局面100バイトとしたら 800GB。
  
  const size_t FILE_WRITE_INTERVAL = 1000;

  // 停止信号。これがtrueならslave threadは終了。
  bool is_stop() const
  {
    return sfen_count > sfen_count_limit;
  }

  // 局面を1行書き出す
  void write(size_t thread_id,string line)
  {
    // スレッドごとにbufferを持っていて、そこに追加する。
    // bufferが溢れたら、ファイルに書き出す。

    auto& buf = sfen_buffers[thread_id];
    buf.push_back(line);
    if (buf.size() >= FILE_WRITE_INTERVAL)
    {
      std::unique_lock<Mutex> lk(mutex);

      for (auto line : buf)
        fs << line;

      fs.flush();
      sfen_count += buf.size();
      buf.clear();

      // 棋譜を書き出すごとに'.'を出力。
      cout << ".";
    }
  }

private:
  // ファイルに書き出す前に排他するためのmutex
  Mutex mutex;

  fstream fs;

  // ファイルに書き出す前のバッファ
  vector<vector<string>> sfen_buffers;

  // 生成したい局面の数
  u64 sfen_count_limit;

  // 生成した局面の数
  u64 sfen_count;

};

// -----------------------------------
//  棋譜を生成するworker(スレッドごと)
// -----------------------------------

//  thread_id    = 0..Threads.size()-1
//  search_depth = 通常探索の探索深さ
void gen_sfen_worker(size_t thread_id , int search_depth , SfenWriter& sw)
{
  const int MAX_PLY = 256; // 256手までテスト

  StateInfo state[MAX_PLY + 64]; // StateInfoを最大手数分 + SearchのPVでleafにまで進めるbuffer
  Move moves[MAX_PLY]; // 局面の巻き戻し用に指し手を記憶
  int ply; // 初期局面からの手数

  PRNG prng(20160101);

  auto& pos = Threads[thread_id]->rootPos;

  pos.set_hirate();

  // Positionに対して従属スレッドの設定が必要。
  // 並列化するときは、Threads (これが実体が vector<Thread*>なので、
  // Threads[0]...Threads[thread_num-1]までに対して同じようにすれば良い。
  auto th = Threads[thread_id];
  pos.set_this_thread(th);

  // loop_maxになるまで繰り返し
  while(true)
  {
    for (ply = 0; ply < MAX_PLY; ++ply)
    {
      // mate1ply()の呼び出しのために必要。
      pos.check_info_update();

      if (pos.is_mated())
        break;

      // 3手読みの評価値とPV(最善応手列)
      auto pv_value1 = Learner::search(pos, -VALUE_INFINITE, VALUE_INFINITE, search_depth);
      auto value1 = pv_value1.first;
      auto pv1 = pv_value1.second;

#if 0
      // 0手読み(静止探索のみ)の評価値とPV(最善応手列)
      auto pv_value2 = Learner::qsearch(pos, -VALUE_INFINITE, VALUE_INFINITE);
      auto value2 = pv_value2.first;
      auto pv2 = pv_value2.second;
#endif

      // 上のように、search()の直後にqsearch()をすると、search()で置換表に格納されてしまって、
      // qsearch()が置換表にhitして、search()と同じ評価値が返るので注意。

      // 局面のsfen,3手読みでの最善手,0手読みでの評価値
      // これをファイルか何かに書き出すと良い。
      //      cout << pos.sfen() << "," << value1 << "," << value2 << "," << endl;

#if 0
      // sfenのpack test
      // pack()してunpack()したものが元のsfenと一致するのかのテスト。

      u8 data[32];
      pos.sfen_pack(data);
      auto sfen = pos.sfen_unpack(data);
      auto pos_sfen = pos.sfen();

      // 手数の部分の出力がないので異なる。末尾の数字を消すと一致するはず。
      auto trim = [](std::string& s)
      {
        while (true)
        {
          auto c = *s.rbegin();
          if (c < '0' || '9' < c)
            break;
          s.pop_back();
        }
      };
      trim(sfen);
      trim(pos_sfen);

      if (sfen != pos_sfen)
      {
        cout << "Error: sfen packer error\n" << sfen << endl << pos_sfen << endl;
      }
#endif


      string line = pos.sfen() + "," + to_string(value1) + "\n";
      sw.write(thread_id,line);
      
#if 0
      // デバッグ用に局面と読み筋を表示させてみる。
      cout << pos;
      cout << "search() PV = ";
      for (auto pv_move : pv1)
        cout << pv_move << " ";
      cout << endl;

      // 静止探索のpvは存在しないことがある。(駒の取り合いがない場合など)　その場合は、現局面がPVのleafである。
      cout << "qsearch() PV = ";
      for (auto pv_move : pv2)
        cout << pv_move << " ";
      cout << endl;

#endif

#if 1
      // デバッグ用の検証として、
      // PVの指し手でleaf nodeまで進めて、非合法手が混じっていないかをテストする。
      auto go_leaf_test = [&](auto pv) {
        int ply2 = ply;
        for (auto m : pv)
        {
          // 非合法手はやってこないはずなのだが。
          if (!pos.pseudo_legal(m) || !pos.legal(m))
          {
            cout << pos << m << endl;
            ASSERT_LV3(false);
          }
          pos.do_move(m, state[ply2++]);
          pos.check_info_update();
        }
        // leafに到達
        //      cout << pos;

        // 巻き戻す
        auto pv_r = pv;
        std::reverse(pv_r.begin(), pv_r.end());
        for (auto m : pv_r)
          pos.undo_move(m);
      };

      go_leaf_test(pv1); // 通常探索のleafまで行くテスト
//      go_leaf_test(pv2); // 静止探索のleafまで行くテスト
#endif

      // 3手読みの指し手で局面を進める。
      auto m = pv1[0];

      pos.do_move(m, state[ply]);
      moves[ply] = m;
    }

    // 局面を最初まで巻き戻してみる(undo_moveの動作テストを兼ねて)
    while (ply > 0)
    {
      pos.undo_move(moves[--ply]);
    }
  }
}

// -----------------------------------
//    棋譜を生成するコマンド(master)
// -----------------------------------

// 棋譜を生成するコマンド
void gen_sfen(Position& pos, istringstream& is)
{
  // 生成棋譜の個数 default = 80億局面(Ponanza仕様)
  u64 loop_max = 8000000000UL;

  // スレッド数(これは、USIのsetoptionで与えられる)
  u32 thread_num = Options["Threads"];

  // 探索深さ
  int search_depth = 3;

  is >> search_depth >> loop_max;

  std::cout << "gen_sfen : "
    << "search_depth = " << search_depth
    << " , loop_max = " << loop_max
    << " , thread_num (set by USI setoption) = " << thread_num
    << endl;

  // 評価関数の読み込み等
  is_ready(pos);

  // 途中でPVの出力されると邪魔。
  Search::Limits.silent = true;

  SfenWriter sw(thread_num,loop_max);

  // スレッド数だけ作って実行。

  vector<std::thread> threads;
  for (size_t i = 0; i < thread_num; ++i)
  {
    threads.push_back( std::thread([i, search_depth,&sw] { gen_sfen_worker(i, search_depth, sw);  }));
  }

  // すべてのthreadの終了待ち

  for (auto& th:threads)
  {
    th.join();
  }

  std::cout << "gen_sfen finished." << endl;
}

} // namespace Learner

#endif // EVAL_LEARN
