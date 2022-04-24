[![Make CI (for WebAssembly)](https://github.com/mizar/YaneuraOu.wasm/actions/workflows/make-wasm.yml/badge.svg?branch=komoringheights_wasm&event=push)](https://github.com/mizar/YaneuraOu.wasm/actions/workflows/make-wasm.yml)

# About this project

KomoringHeights は、df-pn+アルゴリズムを用いたやねうら王ベースの詰将棋ソルバーです。
中〜長編の詰将棋を高速に解くことを目指して開発をしています。

KomoringHeights 本体は `source/engine/user-engine` 以下に格納されています。

# How to use

[Releases](https://github.com/komori-n/KomoringHeights/releases) からお使いのOSに合ったバイナリをダウンロードしてください。
KomoringHeightsを動かすには、[将棋所](http://shogidokoro.starfree.jp/)、[ShogiGUI](http://shogigui.siganus.com/)、
[ShogiDroid](http://shogidroid.siganus.com/)などのUSIプロトコルに対応したGUIを利用してください。

エンジンオプションについては [EngineOptions](docs/EngineOptions.txt) を参照。

# 技術解説

* [詰将棋に対するdf-pnアルゴリズムの解説 | コウモリのちょーおんぱ](https://komorinfo.com/blog/df-pn-basics/)
* [詰将棋探索における証明駒／反証駒の活用方法 | コウモリのちょーおんぱ](https://komorinfo.com/blog/proof-piece-and-disproof-piece/)
* [高速な詰将棋ソルバー『KomoringHeights』v0.4.0を公開した | コウモリのちょーおんぱ](https://komorinfo.com/blog/komoring-heights-v040/)

# Contributing

バグの報告や機能要望などはIssueへお願いします。
Pull Requestも大歓迎です。

# ライセンス

Licensed under GPLv3.

やねうら王プロジェクトのソースコードはStockfishをそのまま用いている部分が多々あり、Apery/SilentMajorityを参考にしている部分もありますので、やねうら王プロジェクトは、それらのプロジェクトのライセンス(GPLv3)に従うものとします。

「リゼロ評価関数ファイル」については、やねうら王プロジェクトのオリジナルですが、一切の権利は主張しませんのでご自由にお使いください。
