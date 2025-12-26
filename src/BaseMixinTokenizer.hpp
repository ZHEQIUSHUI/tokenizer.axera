#pragma once
#include "BaseTokenizer.hpp"

#include "tokenizer/tokenizer_hf.hpp"

#include "utils/json.hpp"

template <ContentType... Types>
class BaseMixinTokenizer : public BaseTokenizer
{
protected:
    std::string model_dir_;             // ✅ 新增：load(path) 存下来
    bool add_generation_prompt_ = true; // 需要的话可开放 set

    std::shared_ptr<Tokenizer> tokenizer;
    static constexpr ContentType support_types[] = {Types...};

    std::vector<int> addition_stop_tokens = {};

    std::string vision_start_token_;
    std::string vision_end_token_;
    std::string image_pad_token_;
    std::string video_pad_token_;

    static inline void trim_inplace(std::string &s)
    {
        // 去掉前面空白
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                        { return !std::isspace(ch); }));
        // 去掉后面空白
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                             { return !std::isspace(ch); })
                    .base(),
                s.end());
    }

    // 从文本中移除 end_think_token 之前的内容,并 trim 空格和换行符
    static std::string remove_thinking(const std::string &text, std::string end_think_token = "</think>", bool trim = true)
    {
        // 查找 token
        size_t pos = text.find(end_think_token);

        // 找不到 token：返回原文本（按需可更改）
        if (pos == std::string::npos)
        {
            std::string result = text;
            if (trim)
                trim_inplace(result);
            return result;
        }

        // 提取 token 之后的内容
        std::string result = text.substr(pos + end_think_token.size());

        // 可选 trim
        if (trim)
            trim_inplace(result);

        return result;
    }

    bool pick_first_existing(const std::vector<std::string> &cands, std::string &out)
    {
        for (auto &s : cands)
        {
            if (this->tokenizer->encode(s).size() == 1)
            { // 你如果没有 backend_，就直接调 hf_tok_token_to_id
                out = s;
                return true;
            }
        }
        return false;
    }

    static inline const char *role_to_str(RoleType r)
    {
        switch (r)
        {
        case SYSTEM:
            return "system";
        case USER:
            return "user";
        case ASSISTANT:
            return "assistant";
        default:
            return "user";
        }
    }

    static inline const char *type_to_str(ContentType t)
    {
        switch (t)
        {
        case TEXT:
            return "text";
        case IMAGE:
            return "image";
        case VIDEO:
            return "video";
        case AUDIO:
            return "audio";
        default:
            return "text";
        }
    }

public:
    bool load(const std::string tokenizer_path) override
    {
        model_dir_ = tokenizer_path;
        tokenizer.reset(Tokenizer::createTokenizer(tokenizer_path));
        if (tokenizer == nullptr)
        {
            return false;
        }

        if (!pick_first_existing({"<|vision_start|>", "<vision_start>", "<|vision_start|>\n"}, vision_start_token_))
            throw std::runtime_error("vision_start token not found in tokenizer");

        if (!pick_first_existing({"<|vision_end|>", "<vision_end>"}, vision_end_token_))
            throw std::runtime_error("vision_end token not found in tokenizer");

        if (!pick_first_existing({"<|image_pad|>", "<image>", "<|image|>"}, image_pad_token_))
            throw std::runtime_error("image pad token not found in tokenizer");

        if (!pick_first_existing({"<|video_pad|>", "<video>", "<|video|>"}, video_pad_token_))
            throw std::runtime_error("video pad token not found in tokenizer");

        return true;
    }

    bool support(ContentType type) override
    {
        return std::find(std::begin(support_types), std::end(support_types), type) != std::end(support_types);
    }

    bool is_stop(int token) override
    {
        return tokenizer->is_stop(token) || std::find(addition_stop_tokens.begin(), addition_stop_tokens.end(), token) != addition_stop_tokens.end();
    }

    void add_stop_token(int token) override
    {
        addition_stop_tokens.push_back(token);
    }
    bool add_stop_token(std::string stop_token) override
    {
        auto tokens = tokenizer->encode(stop_token);
        if (tokens.size() != 1)
        {
            return false;
        }
        addition_stop_tokens.push_back(tokens[0]);
        return true;
    }
    void clear_addition_stop_tokens() override
    {
        addition_stop_tokens.clear();
    }
    std::vector<int> get_stop_tokens() override
    {
        std::vector<int> stop_tokens = tokenizer->get_stop_tokens();
        stop_tokens.insert(stop_tokens.end(), addition_stop_tokens.begin(), addition_stop_tokens.end());
        return stop_tokens;
    }

    // ✅ 默认实现：走 HF chat_template
    std::string apply_chat_template(const std::vector<Content> &contents) override
    {
        if (model_dir_.empty())
        {
            throw std::runtime_error("apply_chat_template: tokenizer not loaded (model_dir_ empty)");
        }
        if (contents.empty())
        {
            throw std::runtime_error("apply_chat_template: contents empty");
        }

        auto repeat = [](const std::string &tok, int n)
        {
            std::string out;
            if (n <= 0)
                return out;
            out.reserve(tok.size() * (size_t)n);
            for (int i = 0; i < n; ++i)
                out += tok;
            return out;
        };

        nlohmann::json messages = nlohmann::json::array();
        for (const auto &c : contents)
        {
            if (!support(c.type))
            {
                throw std::runtime_error("apply_chat_template: unsupported content type");
            }

            nlohmann::json msg;
            msg["role"] = role_to_str(c.role);

            // TEXT: 直接字符串
            if (c.type == IMAGE || c.type == VIDEO)
            {
                const std::string &pad = (c.type == IMAGE) ? image_pad_token_ : video_pad_token_;
                int n = c.num_media * c.num_media_tokens;

                std::string block;
                block += vision_start_token_;
                block += repeat(pad, n);
                block += vision_end_token_;
                block += c.data; // 你的 caption/后续文本

                msg["content"] = block;
            }
            else if (c.type == TEXT)
            {
                if (c.role == ASSISTANT && !this->think_in_prompt)
                {
                    msg["content"] = remove_thinking(c.data);
                }
                else
                {
                    msg["content"] = c.data;
                }
            }
            else
            {
                // 多模态：用 HF 常见的 “content array” 结构（模板能否支持取决于模型）
                // [{"type":"image"}, ... , {"type":"text","text":"..."}]
                nlohmann::json arr = nlohmann::json::array();
                for (int i = 0; i < c.num_media; ++i)
                {
                    nlohmann::json media;
                    media["type"] = type_to_str(c.type);
                    // 也可放一些额外信息：media["num_tokens"]=c.num_media_tokens;
                    arr.push_back(media);
                }
                if (!c.data.empty())
                {
                    nlohmann::json t;
                    t["type"] = "text";
                    t["text"] = c.data;
                    arr.push_back(t);
                }
                msg["content"] = arr;
            }

            messages.push_back(msg);
        }

        // 通常最后一条是 user，就加 generation prompt
        bool add_gen = add_generation_prompt_;
        if (contents.back().role != USER)
            add_gen = false;

        char *out = nullptr;
        int rc = hf_chat_apply_template_from_dir(
            model_dir_.c_str(),
            messages.dump().c_str(),
            add_gen ? 1 : 0,
            &out);

        if (rc != 0 || out == nullptr)
        {
            const char *e = hf_last_error_message();
            throw std::runtime_error(std::string("hf_chat_apply_template_from_dir failed: ") + (e ? e : "unknown"));
        }

        std::string prompt(out);
        hf_string_free(out);
        return prompt;
    }

    std::vector<int> encode(const std::string &text) override
    {
        return tokenizer->encode(text);
    }

    std::string decode(const std::vector<int> &ids) override
    {
        std::stringstream text;
        for (auto id : ids)
        {
            text << tokenizer->decode(id);
        }
        return text.str();
    }

    std::string decode(int id) override
    {
        return tokenizer->decode(id);
    }
};