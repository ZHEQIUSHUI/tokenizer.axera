#include "Qwen3Tokenizer.hpp"

class InternVL3Tokenizer : public Qwen3Tokenizer<TEXT, IMAGE>
{
public:
    InternVL3Tokenizer() {
        image_pad_token = "<IMG_CONTEXT>";
        img_start_token = "<img>";
        img_end_token   = "</img>";
    }
};
REGISTER(InternVL3, InternVL3Tokenizer)
using InternVL2_5Tokenizer = InternVL3Tokenizer;
REGISTER(InternVL2_5, InternVL2_5Tokenizer)
