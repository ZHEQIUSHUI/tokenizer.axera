#!/bin/bash
mkdir build
cd build

tokenizer_url="https://github.com/ZHEQIUSHUI/hf_tokenizer_ffi/releases/download/v0.1.0/hf-tokenizer-sdk-x86_64.tar.gz"
sha_url="${tokenizer_url}.sha256"

tokenizer_folder="hf-tokenizer-sdk-x86_64"
archive="${tokenizer_folder}.tar.gz"
sha_file="${archive}.sha256"

download_files() {
  echo "Downloading: $tokenizer_url"
  wget "$tokenizer_url" -O "$archive"
  echo "Downloading: $sha_url"
  wget "$sha_url" -O "$sha_file"
}

verify_sha() {
  # .sha256 文件内容形如："<hash>  <filename>"
  # 我们把 filename 固定为当前 archive，避免上游写的文件名不一致导致校验失败
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
if [ ! -d "$tokenizer_folder" ]; then
  echo "Extracting $archive"
  tar -xf "$archive"
else
  echo "$tokenizer_folder already exists"
fi



cmake \
-DHF_TOKENIZER_DIR=$PWD/$tokenizer_folder \
-DCMAKE_BUILD_TYPE=Release \
..

cmake \
-DHF_TOKENIZER_DIR=$PWD/$tokenizer_folder \
-DCMAKE_BUILD_TYPE=Release \
..

make -j16
make install