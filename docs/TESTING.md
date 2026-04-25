# テストと品質（開発者向け）

ホスト上の自動テストと、失敗したときの扱い、実機チェックリストへの案内です。

## 1. 自動テストの概要

| 種類 | 内容 |
| :--- | :--- |
| **Google Test** | `tests/*.cpp` が lexer/parser/グラフィック/ファイル I/O/サウンド等を検証 |
| **実行ファイル** | `BUILD_TESTS=ON` で生成される `basic_tests` |
| **CI** | `main` / `master` への push と pull request で [`.github/workflows/ci.yml`](../.github/workflows/ci.yml) が Ubuntu 上で同じバイナリをビルド・実行 |

実機や Pico SDK は不要です（FetchContent で Google Test を取得します）。

## 2. ローカルでの実行（定期確認用）

リポジトリルートで:

```bash
cmake -S . -B build_host -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build_host --parallel
./build_host/basic_tests
```

ビルドディレクトリ名は任意です。変更した場合は最後のパスを合わせてください。

### 2.1 特定のテストだけ実行する

失敗したスイートを絞り込むとき:

```bash
./build_host/basic_tests --gtest_filter=LexerTest.*
./build_host/basic_tests --gtest_filter=ExecutionTest.ForNextLoop
```

`--gtest_list_tests` で一覧を表示できます。

### 2.2 ctest を使う場合

CTest にテストが登録されている環境では:

```bash
cd build_host && ctest --output-on-failure --parallel 4
```

登録が無い場合は `ctest` が 0 件になることがあるため、そのときは **`./basic_tests` を直接実行**してください。

## 3. CI が失敗したときの修正フロー

1. **GitHub Actions のログ**で失敗したジョブ（`host-tests`）を開き、赤くなったステップの標準出力を確認する。
2. **ローカルで再現**する（上記 §2 と同じ `cmake` / `build` / `./basic_tests`）。可能なら `--gtest_filter` で該当ケースのみ実行する。
3. **原因を切り分ける**  
   - 実装の退行 → `src/` または `tests/` を修正し、ホストで再度 `./basic_tests` がすべてパスすることを確認する。  
   - テストの期待値が仕様変更とずれた → テストか仕様ドキュメントのどちらを正とするか決め、両方を整合させる。
4. **コミット・push**する。CI が緑になるまで繰り返す。

原則として **CI を赤いまま放置しない**（main の保護ブランチを使う場合は merge されない設計にするとよい）。

## 4. 実機のみで確認する項目

USB・LCD・SD・タッチなどはホストのモックでは再現できないため、別紙のチェックリストで手動確認します。

👉 **[実機手動テストチェックリスト](./DEVICE_CHECKLIST.md)**

## 5. 関連ドキュメント

- [SETUP.md §8](../SETUP.md) — ホストテストの最短手順（macOS 向けガイド内）
- [COMMAND_STATUS.md](./COMMAND_STATUS.md) — 命令ごとの実装度
