# mini-oems

FIX プロトコル、価格時間優先マッチング、TWAP/VWAP 執行アルゴリズムを備えた、モダン C++ によるミニマルな注文・執行管理システムです。

## 前提条件

- [Podman](https://podman.io/)
- [just](https://github.com/casey/just)

## コマンド

すべての操作は `just` を入口にし、内部では Podman コンテナを使います。依存関係はコンテナ内で Nix が管理し、ビルドは CMake で行います。

```bash
just dev               # 開発用イメージをビルド（初回以降はキャッシュ利用）
just configure         # cmake configure
just test              # 全テスト実行（unit + integration + system）
just unit-test         # unit test のみ
just integration-test  # integration test のみ
just system-test       # HTTP 経由の system test
just coverage          # gcov + lcov HTML レポートを build-cov/coverage-html/ に生成
just bench             # Google Benchmark を build-bench/ で実行
just docs              # EN/JA の Doxygen HTML を生成し、日本語の入口ページを開く
just api-gen           # docs/openapi.yaml から src/api-gen/ を再生成
just api-gen-check     # src/api-gen/ の差分が残っていれば失敗（CI 用）
just openapi-validate  # docs/openapi.yaml を検証
just openapi-preview   # openapi.yaml を :8081 の Swagger UI でプレビュー
just fmt               # コード整形（clang-format）
just lint              # 静的解析（clang-tidy）
just check             # fmt + lint + test
just build             # 本番用イメージをビルド
just run               # コンテナで実行
```

## API（spec-first）

HTTP API の仕様は [`docs/openapi.yaml`](docs/openapi.yaml)（OpenAPI 3.0.3）にあり、これを単一の正本として扱います。サーバー側 C++ コード（DTO と [oatpp](https://oatpp.io) の抽象コントローラ）は [openapi-generator](https://openapi-generator.tech) でここから再生成し、`src/api-gen/` に配置しています。手書きの `src/core/api/oems_controllers.{hpp,cpp}` が、生成された抽象 API とドメイン層の橋渡しをします。

エンドポイントを追加・変更するときは次の流れです。

1. `docs/openapi.yaml` を編集
2. `just api-gen` を実行
3. 変更された抽象コントローラに合わせて実装を更新
4. `just test` で確認

## ライセンス

Apache-2.0。詳細は [LICENSE](LICENSE) を参照してください。

## アーキテクチャ

v1 設計の全体像は [docs/architecture.ja.md](docs/architecture.ja.md) を参照してください。実装は次の順序で組み上げています。

| モジュール | パス | 役割 |
|-----------|------|------|
| Foundation types | `src/core/types/` | OrderId, Price, Symbol, Result<T>, errors |
| Matching engine | `src/core/matching/` | 価格時間優先オーダーブック |
| Risk manager | `src/core/risk/` | プレトレード制御 |
| Order manager | `src/core/order/` | ライフサイクル、イベント、調停 |
| Persistence | `src/core/persistence/` | SQLite による永続化 |
| HTTP/JSON API | `src/core/api/` + `src/api-gen/` | oatpp サーバー、spec-first |
| FIX gateway | `src/core/fix/` | FIX4 セッションとアプリ変換 |
| Execution algos | `src/core/algo/` | TWAP / VWAP のスライス生成 |
| Market data | `src/core/market_data/` | BBO スナップショット、参照価格 |
| Server | `src/main.cc` | モジュールを組み立て、HTTP サーバーを起動 |
| CLI | `src/cli/main.cc` | 開発者向けコマンドラインクライアント |

## CLI 利用例

サーバー起動後（`./mini-oems 8080 oems.db`）、次のように操作できます。

```bash
oems-cli server-status
oems-cli new-order --symbol AAPL --side buy --type limit --price 10000 --qty 100
oems-cli show-book --symbol AAPL
oems-cli show-orders --status Accepted
oems-cli cancel-order --order-id 1
oems-cli show-trades
```
