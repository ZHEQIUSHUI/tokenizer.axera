#!/usr/bin/env bash
set -euo pipefail

mkdir -p build_mingw64
cd build_mingw64

tokenizer_url="https://github.com/ZHEQIUSHUI/hf_tokenizer_ffi/releases/download/v0.1.0/hf-tokenizer-sdk-windows-x86_64.zip"
sha_url="${tokenizer_url}.sha256"

tokenizer_folder="hf-tokenizer-sdk-windows-x86_64"
archive="${tokenizer_folder}.zip"
sha_file="${archive}.sha256"

download_files() {
  echo "Downloading: $tokenizer_url"
  curl -L --fail --retry 3 -o "$archive" "$tokenizer_url"
  echo "Downloading: $sha_url"
  curl -L --fail --retry 3 -o "$sha_file" "$sha_url"
}

verify_sha() {
  local expected
  expected="$(awk '{print $1}' "$sha_file")"
  echo "${expected}  ${archive}" | sha256sum -c - >/dev/null 2>&1
}

# 1) 确保文件存在
if [[ ! -f "$archive" || ! -f "$sha_file" ]]; then
  download_files
fi

# 2) 校验，不通过就更新下载再校验一次
if ! verify_sha; then
  echo "SHA256 mismatch. Re-downloading..."
  rm -f "$archive" "$sha_file"
  rm -rf "$tokenizer_folder"
  download_files

  if ! verify_sha; then
    echo "ERROR: SHA256 mismatch after re-download"
    echo "sha file:"
    cat "$sha_file" || true
    exit 1
  fi
fi

echo "SHA256 OK: $archive"

# 3) 解压（只在目录不存在时）
if [[ ! -d "$tokenizer_folder" ]]; then
  echo "Extracting $archive"
  unzip -q "$archive"
else
  echo "$tokenizer_folder already exists"
fi

# 4) 配置 & 编译 & 安装
cmake -G "MinGW Makefiles" \
  -DHF_TOKENIZER_DIR="$PWD/$tokenizer_folder" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/install" \
  ..

cmake --build . --config Release -j 8
cmake --install . --config Release
