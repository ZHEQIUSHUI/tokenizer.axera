#pragma once
#include <sstream>
#include <algorithm>

#include "BaseTokenizer.hpp"
#include "utils/object_register.hpp"
#include "utils/sample_log.h"
#include "tokenizer/tokenizer.hpp"

template <ContentType... Types>
class Qwen3Tokenizer : public BaseTokenizer
{
protected:
    std::shared_ptr<MNN::Transformer::Tokenizer> tokenizer;
    bool add_generation_prompt = true;
    std::string video_pad_token = "<|video_pad|>";
    std::string image_pad_token = "<|image_pad|>";

    std::string img_start_token = "<|vision_start|>";
    std::string img_end_token = "<|vision_end|>";

    static constexpr ContentType support_types[] = {Types...};

public:
    bool load(const std::string tokenizer_path) override
    {
        tokenizer.reset(MNN::Transformer::Tokenizer::createTokenizer(tokenizer_path));
        return tokenizer != nullptr;
    }

    bool support(ContentType type) override
    {
        return std::find(std::begin(support_types), std::end(support_types), type) != std::end(support_types);
    }

    bool is_stop(int token) override
    {
        return tokenizer->is_stop(token);
    }

    std::vector<int> encode(const std::vector<Content> &contents) override
    {
        // check contents type
        for (const auto &content : contents)
        {
            if (!support(content.type))
            {
                ALOGE("unsupport content type: %d", content.type);
                return {};
            }
        }

        std::stringstream text;
        for (const auto &content : contents)
        {
            if (content.role == SYSTEM)
            {
                text << "<|im_start|>system\n"
                     << content.data << "<|im_end|>\n";
            }
            else if (content.role == USER)
            {
                switch (content.type)
                {
                case TEXT:
                    text << "<|im_start|>user\n"
                         << content.data << "<|im_end|>\n";
                    break;
                case IMAGE:
                    text << "<|im_start|>user\n" << img_start_token;
                    for (int i = 0; i < content.num_media * content.num_media_tokens; i++)
                    {
                        text << image_pad_token;
                    }
                    text << img_end_token;
                    text << content.data << "<|im_end|>\n";
                    break;
                case VIDEO:
                    text << "<|im_start|>user\n" << img_start_token;
                    for (int i = 0; i < content.num_media * content.num_media_tokens; i++)
                    {
                        text << video_pad_token;
                    }
                    text << img_end_token;
                    text << content.data << "<|im_end|>\n";
                    break;

                default:
                    break;
                }
            }
            else if (content.role == ASSISTANT)
            {
                if (!think_in_prompt)
                {
                    auto cleaned_data = remove_thinking(content.data);
                    text << "<|im_start|>assistant\n"
                         << cleaned_data << "<|im_end|>\n";
                }
                else
                {
                    text << "<|im_start|>assistant\n"
                         << content.data << "<|im_end|>\n";
                }
            }
        }

        if (contents.back().role == USER && add_generation_prompt)
        {
            text << "<|im_start|>assistant\n";
        }

        ALOGD("text: \n%s", text.str().c_str());
        return tokenizer->encode(text.str());
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
};
using qwen3vl_tokenizer = Qwen3Tokenizer<TEXT, IMAGE, VIDEO>;
REGISTER(Qwen3VL, qwen3vl_tokenizer)
using qwen3_tokenizer = Qwen3Tokenizer<TEXT>;
REGISTER(Qwen3, qwen3_tokenizer)
using qwen2_5_tokenizer = Qwen3Tokenizer<TEXT>;
REGISTER(Qwen2_5, qwen2_5_tokenizer)
