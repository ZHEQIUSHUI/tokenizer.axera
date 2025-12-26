#ifndef TOKENIZER_hf_hpp
#define TOKENIZER_hf_hpp

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <fstream>

#include "hf_tokenizer_c.h"

class Tokenizer {
public:
    Tokenizer() = default;
    virtual ~Tokenizer() = default;
    static Tokenizer* createTokenizer(const std::string& filename);

    bool is_stop(int token);
    bool is_special(int token);
    std::vector<int> get_stop_tokens();
    std::vector<int> encode(const std::string& str);
    virtual std::string decode(int id) = 0;

protected:
    virtual void load_special(std::ifstream& file);
    virtual bool load_vocab(std::ifstream& file) = 0;
    virtual void encode(const std::string& str, std::vector<int>& ids) = 0;

    std::vector<int> special_tokens_;
    std::vector<int> stop_tokens_;
    std::vector<int> prefix_tokens_;

    // 内部加速，不影响你对外 API
    std::unordered_set<int> special_set_;
    std::unordered_set<int> stop_set_;
};

#endif
