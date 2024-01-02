#!/bin/bash

total_size=0
dir="apps"

# appsディレクトリ内の各サブディレクトリをループ処理
for subdir in "$dir"/*; do
    if [ -d "$subdir" ]; then
        app_name=$(basename "$subdir")
        executable="$subdir/$app_name"

        # 同名の実行ファイルが存在するか確認
        if [ -f "$executable" ] && [ -x "$executable" ]; then
            # ファイルサイズを取得し、合計サイズに加算
            size=$(stat -c%s "$executable")
            total_size=$((total_size + size))
        fi
    fi
done

echo "合計サイズ: $total_size バイト"