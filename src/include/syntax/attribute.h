/*license*/

// attribute - define attribute

namespace utils {
    namespace syntax {
        enum class Attribute {
            repeat = 0x01,
            fatal = 0x02,
            ifexist = 0x04,
            adjacent = 0x08,
        };
    }
}  // namespace utils