#include "tokenizer_hf.hpp"
#include <filesystem>
#include <stdexcept>

// 需要你提供 nlohmann/json.hpp
#include "../utils/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

static json load_json_if_exists(const fs::path& p) {
    if (!fs::exists(p)) return json();
    std::ifstream f(p);
    if (!f) return json();
    json j;
    try { f >> j; } catch (...) { return json(); }
    return j;
}

static void push_unique(std::vector<std::string>& v, const std::string& s) {
    if (s.empty()) return;
    for (auto& x : v) if (x == s) return;
    v.push_back(s);
}

static void push_unique_id(std::vector<int>& v, std::unordered_set<int>& setv, int id) {
    if (setv.insert(id).second) v.push_back(id);
}

class HfTokenizer final : public Tokenizer {
public:
    // input 可以是文件 tokenizer.json，也可以是模型目录
    explicit HfTokenizer(const std::string& input_path) {
        resolve_paths(input_path);
        handle_ = hf_tok_load_from_file(tokenizer_json_.string().c_str());
        if (!handle_) {
            const char* e = hf_last_error_message();
            throw std::runtime_error(std::string("hf_tok_load_from_file failed: ") + (e ? e : "unknown"));
        }

        // 1) special：优先从 tokenizer.json 内部（通过 hf api 枚举 special=true）
        init_special_from_tokenizer_json();

        // 2) special：再从 tokenizer_config/special_tokens_map 补齐（可能有没被标 special 的）
        init_special_from_side_json();

        // 3) stop：优先从 generation_config.json 的 eos_token_id（int 或 list）
        init_stop_from_generation_config();

        // 4) stop：兜底用 eos_token 字符串转 id（来自 side json）
        if (stop_tokens_.empty()) init_stop_from_eos_token_string();
    }

    ~HfTokenizer() override {
        if (handle_) {
            hf_tok_free(handle_);
            handle_ = nullptr;
        }
    }

    std::string decode(int id) override {
        char* out = nullptr;
        int rc = hf_tok_decode_id(handle_, (uint32_t)id, /*skip_special=*/0, &out);
        if (rc != 0) {
            const char* e = hf_last_error_message();
            throw std::runtime_error(std::string("hf_tok_decode_id failed: ") + (e ? e : "unknown"));
        }
        std::string s(out ? out : "");
        hf_string_free(out);
        return s;
    }

protected:
    bool load_vocab(std::ifstream&) override { return true; }
    void load_special(std::ifstream&) override {}

    void encode(const std::string& str, std::vector<int>& ids) override {
        uint32_t* out_ids = nullptr;
        size_t out_len = 0;
        int rc = hf_tok_encode(handle_, str.c_str(), /*add_special=*/1, &out_ids, &out_len);
        if (rc != 0) {
            const char* e = hf_last_error_message();
            throw std::runtime_error(std::string("hf_tok_encode failed: ") + (e ? e : "unknown"));
        }
        ids.clear();
        ids.reserve(out_len);
        for (size_t i = 0; i < out_len; ++i) ids.push_back((int)out_ids[i]);
        hf_free_ptr(out_ids);
    }

private:
    HfTokenizerHandle* handle_{nullptr};

    fs::path model_dir_;
    fs::path tokenizer_json_;
    fs::path tokenizer_config_;
    fs::path special_map_;
    fs::path generation_config_;

    void resolve_paths(const std::string& input) {
        fs::path p(input);

        if (fs::is_regular_file(p)) {
            tokenizer_json_ = p;
            model_dir_ = p.parent_path();
        } else if (fs::is_directory(p)) {
            model_dir_ = p;
            tokenizer_json_ = model_dir_ / "tokenizer.json";
        } else {
            throw std::runtime_error("input path is neither file nor directory: " + input);
        }

        if (!fs::exists(tokenizer_json_)) {
            throw std::runtime_error("tokenizer.json not found: " + tokenizer_json_.string());
        }

        tokenizer_config_ = model_dir_ / "tokenizer_config.json";
        special_map_ = model_dir_ / "special_tokens_map.json";
        generation_config_ = model_dir_ / "generation_config.json";
    }

    void init_special_from_tokenizer_json() {
        char** arr = nullptr;
        size_t n = 0;
        int rc = hf_tok_list_special_tokens(handle_, &arr, &n);
        if (rc != 0) return;

        for (size_t i = 0; i < n; ++i) {
            if (!arr[i]) continue;
            uint32_t id = 0; int found = 0;
            if (hf_tok_token_to_id(handle_, arr[i], &id, &found) == 0 && found) {
                push_unique_id(special_tokens_, special_set_, (int)id);
            }
        }
        hf_tok_free_string_array(arr, n);
    }

    void init_special_from_side_json() {
        json tc = load_json_if_exists(tokenizer_config_);
        json sm = load_json_if_exists(special_map_);

        std::vector<std::string> specials_str;

        auto collect_one = [&](const json& j, const char* key) {
            if (!j.is_object()) return;
            if (!j.contains(key)) return;
            const auto& v = j.at(key);
            if (v.is_string()) {
                push_unique(specials_str, v.get<std::string>());
            } else if (v.is_object() && v.contains("content") && v.at("content").is_string()) {
                // 有些配置会把 token 写成 { "content": "...", ... }
                push_unique(specials_str, v.at("content").get<std::string>());
            }
        };

        // 常见 special keys
        for (auto key : {"bos_token","eos_token","unk_token","pad_token","sep_token","cls_token","mask_token"}) {
            collect_one(tc, key);
            collect_one(sm, key);
        }

        // additional_special_tokens
        auto collect_list = [&](const json& j, const char* key) {
            if (!j.is_object() || !j.contains(key)) return;
            const auto& v = j.at(key);
            if (!v.is_array()) return;
            for (auto& it : v) {
                if (it.is_string()) push_unique(specials_str, it.get<std::string>());
                else if (it.is_object() && it.contains("content") && it.at("content").is_string())
                    push_unique(specials_str, it.at("content").get<std::string>());
            }
        };
        collect_list(tc, "additional_special_tokens");
        collect_list(sm, "additional_special_tokens");

        // 转成 id
        for (auto& s : specials_str) {
            uint32_t id = 0; int found = 0;
            if (hf_tok_token_to_id(handle_, s.c_str(), &id, &found) == 0 && found) {
                push_unique_id(special_tokens_, special_set_, (int)id);
            }
        }
    }

    void init_stop_from_generation_config() {
        json gc = load_json_if_exists(generation_config_);
        if (!gc.is_object()) return;
        if (!gc.contains("eos_token_id")) return;

        const auto& v = gc.at("eos_token_id");
        if (v.is_number_integer()) {
            push_unique_id(stop_tokens_, stop_set_, v.get<int>());
        } else if (v.is_array()) {
            for (auto& it : v) {
                if (it.is_number_integer()) push_unique_id(stop_tokens_, stop_set_, it.get<int>());
            }
        }
    }

    void init_stop_from_eos_token_string() {
        json tc = load_json_if_exists(tokenizer_config_);
        json sm = load_json_if_exists(special_map_);

        auto get_token_str = [&](const json& j) -> std::string {
            if (!j.is_object() || !j.contains("eos_token")) return {};
            const auto& v = j.at("eos_token");
            if (v.is_string()) return v.get<std::string>();
            if (v.is_object() && v.contains("content") && v.at("content").is_string())
                return v.at("content").get<std::string>();
            return {};
        };

        std::string eos = get_token_str(tc);
        if (eos.empty()) eos = get_token_str(sm);
        if (eos.empty()) return;

        uint32_t id = 0; int found = 0;
        if (hf_tok_token_to_id(handle_, eos.c_str(), &id, &found) == 0 && found) {
            push_unique_id(stop_tokens_, stop_set_, (int)id);
        }
    }
};

// ------------------ 保持你原 API 不变：只是输入变得更智能 ------------------

Tokenizer* Tokenizer::createTokenizer(const std::string& filename) {
    try {
        return new HfTokenizer(filename);
    } catch (const std::exception& e) {
        std::cerr << "createTokenizer failed: " << e.what() << std::endl;
        return nullptr;
    }
}

bool Tokenizer::is_stop(int token) {
    return stop_set_.find(token) != stop_set_.end();
}

bool Tokenizer::is_special(int token) {
    return special_set_.find(token) != special_set_.end();
}

std::vector<int> Tokenizer::get_stop_tokens() {
    return stop_tokens_;
}

std::vector<int> Tokenizer::encode(const std::string& str) {
    std::vector<int> ids;
    this->encode(str, ids);
    return ids;
}

void Tokenizer::load_special(std::ifstream&) {
    // 默认无操作；HF 实现已在构造函数中完成 special/stop 初始化
}
