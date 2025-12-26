#include "BaseTokenizer.hpp"
#include <memory>
#include <algorithm>
#include "BaseMixinTokenizer.hpp"

class AutoTokenizer : public BaseMixinTokenizer<TEXT, IMAGE, VIDEO, AUDIO> {
    // 不 override apply_chat_template：默认走 HF 模板
};

std::shared_ptr<BaseTokenizer> create_tokenizer(std::string model_dir)
{
    auto t = std::make_shared<AutoTokenizer>();
    if (!t->load(model_dir)) {
        throw std::runtime_error("load tokenizer failed");
    }
    return t;
}