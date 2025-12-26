#!/bin/bash
mkdir build
cd build

tokenizer_url="https://github.com/ZHEQIUSHUI/hf_tokenizer_ffi/releases/download/v0.1.0/hf-tokenizer-sdk-x86_64.tar.gz"
tokenizer_folder="hf-tokenizer-sdk-x86_64"
# 下载失败可以使用其他方式下载并放到在 $build_dir 目录，参考如下命令解压
if [ ! -f "$tokenizer_folder.tar.gz" ]; then
    # Download the file
    echo "Downloading $tokenizer_url"
    wget "$tokenizer_url" -O "$tokenizer_folder.tar.gz"
else
    echo "$tokenizer_folder.tar.gz already exists"
fi

# Check if the folder exists
if [ ! -d "$tokenizer_folder" ]; then
    # Extract the file
    echo "Extracting $tokenizer_folder.tar.gz"
    tar -xf "$tokenizer_folder.tar.gz"
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