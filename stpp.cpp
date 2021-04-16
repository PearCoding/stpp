#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <vector>

static void usage()
{
    std::cout << "stpp [options] in out \n"
              << "Available options:\n"
              << "    -h     --help                Shows this message\n"
              << "    -D     --definition          Define a tag\n"
              << std::flush;
}

static bool check_option(int i, int argc, char** argv)
{
    if (i + 1 >= argc) {
        std::cerr << "Missing argument for '" << argv[i] << "'. Aborting." << std::endl;
        return false;
    }
    return true;
}

struct Options {
    std::string Input;
    std::string Output;
    std::unordered_set<std::string> Tags;
};

bool parse_arguments(int argc, char** argv, Options& options, bool& help)
{
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage();
                help = true;
                return true;
            } else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--definition")) {
                if (!check_option(i++, argc, argv))
                    return false;
                std::string tag = argv[i];
                options.Tags.insert(tag);
            } else {
                std::cerr << "Unknown option '" << argv[i] << "'. Aborting." << std::endl;
                return false;
            }
        } else {
            if (options.Input == "") {
                options.Input = argv[i];
            } else if (options.Output == "") {
                options.Output = argv[i];
            } else {
                std::cerr << "More than two positional arguments given. Aborting." << std::endl;
                return false;
            }
        }
    }

    return true;
}

std::istream& open_input_stream(const Options& opts)
{
    if (opts.Input.empty() || opts.Input == "--")
        return std::cin;
    else {
        static std::unique_ptr<std::ifstream> stream;
        stream = std::make_unique<std::ifstream>(opts.Input);
        return *stream;
    }
}

std::ostream& open_output_stream(const Options& opts)
{
    if (opts.Output.empty() || opts.Output == "--")
        return std::cout;
    else {
        static std::unique_ptr<std::ofstream> stream;
        stream = std::make_unique<std::ofstream>(opts.Output);
        return *stream;
    }
}

bool parse(std::istream& in, std::ostream& out, const Options& options);

int main(int argc, char** argv)
{
    bool help = false;
    Options options;
    if (!parse_arguments(argc, argv, options, help))
        return EXIT_FAILURE;
    if (help)
        return EXIT_SUCCESS;

    std::istream& in = open_input_stream(options);
    if (!in.good()) {
        std::cerr << "Could not open input stream. Aborting." << std::endl;
        return EXIT_FAILURE;
    }

    std::ostream& out = open_output_stream(options);
    if (!out.good()) {
        std::cerr << "Could not open output stream. Aborting." << std::endl;
        return EXIT_FAILURE;
    }

    if (!parse(in, out, options))
        return EXIT_SUCCESS;

    return EXIT_SUCCESS;
}

constexpr char PP_START = '#';

enum class Operation {
    If,
    Elif,
    Else,
    Endif,
    Define,
    Undef,
    Unknown
};
Operation extract_operation(std::istream& in, const char*& name)
{
    constexpr size_t MAX_BUF_SIZE = 16;
    static char buffer[MAX_BUF_SIZE + 1];

    size_t counter = 0;
    bool started   = false;
    char c;
    while (counter < MAX_BUF_SIZE && in.get(c)) {
        if (c == '\n')
            break;
        if (!std::isspace(c)) {
            buffer[counter++] = c;
            started           = true;
        } else if (started) {
            break;
        }
    }

    buffer[counter] = 0;
    name            = buffer;

    if (strcmp("if", buffer) == 0)
        return Operation::If;
    else if (strcmp("elif", buffer) == 0)
        return Operation::Elif;
    else if (strcmp("else", buffer) == 0)
        return Operation::Else;
    else if (strcmp("endif", buffer) == 0)
        return Operation::Endif;
    else if (strcmp("define", buffer) == 0)
        return Operation::Define;
    else if (strcmp("undef", buffer) == 0)
        return Operation::Undef;
    else {
        // Silently ignore
        //std::cerr << "Got unknown operation " << buffer << std::endl;
        return Operation::Unknown;
    }
}

inline bool is_eof(std::istream& in)
{
    char c;
    return in.get(c).eof() || c == '\n';
}

struct Context {
    std::unordered_set<std::string> Tags;
    size_t Depth = 0;
};

bool handle_if(std::istream& in, std::ostream& out, Context& ctx, bool ignore);
bool handle_define(std::istream& in, Context& ctx);
bool handle_undef(std::istream& in, Context& ctx);

bool consume(std::istream& in, std::ostream& out, Context& context, bool ignore)
{
    char c;
    while (in.get(c)) {
        if (c == PP_START) {
            const char* name;
            const Operation op = extract_operation(in, name);
            switch (op) {
            case Operation::If:
                if (!handle_if(in, out, context, ignore))
                    return false;
                break;
            case Operation::Define:
                if (!ignore && !handle_define(in, context))
                    return false;
                break;
            case Operation::Undef:
                if (!ignore && !handle_undef(in, context))
                    return false;
                break;
            default:
            case Operation::Unknown:
                if (!ignore) {
                    out.put(PP_START);
                    out.write(name, strlen(name));// FIXME: We lose the whitespaces....
                }
            }
        } else if (!ignore) {
            out.put(c);
        }
    }
    return true;
}

bool consume_next(std::istream& in, std::ostream& out, Context& context, bool ignore, Operation& next)
{
    char c;
    while (in.get(c)) {
        if (c == PP_START) {
            const char* name;
            const Operation op = extract_operation(in, name);
            switch (op) {
            case Operation::If:
                if (!handle_if(in, out, context, ignore))
                    return false;
                break;
            case Operation::Elif:
            case Operation::Else:
            case Operation::Endif:
                next = op;
                return true;
            case Operation::Define:
                if (!ignore && !handle_define(in, context))
                    return false;
                break;
            case Operation::Undef:
                if (!ignore && !handle_undef(in, context))
                    return false;
                break;
            default:
            case Operation::Unknown:
                if (!ignore) {
                    out.put(PP_START);
                    out.write(name, strlen(name));
                }
            }
        } else if (!ignore) {
            out.put(c);
        }
    }
    return true;
}

bool parse(std::istream& in, std::ostream& out, const Options& options)
{
    Context context = Context{ options.Tags, 0 };
    return consume(in, out, context, false);
}

bool handle_condition(std::istream& in, const Context& ctx);
bool handle_if(std::istream& in, std::ostream& out, Context& ctx, bool ignore)
{
    bool condition = !ignore && handle_condition(in, ctx);
    bool once_true = false;
    ctx.Depth += 1;

    Operation current = Operation::If;
    while (consume_next(in, out, ctx, ignore || once_true || !condition, current)) {
        if (current == Operation::Endif)
            break;

        if (condition /* Previous condition */)
            once_true = true;

        if (once_true)
            condition = false;
        else if (current == Operation::Elif)
            condition = handle_condition(in, ctx);
        else
            condition = true;
    }

    ctx.Depth -= 1;

    return true;
}

std::string get_tag(std::istream& in)
{
    std::string buffer;
    bool started = false;
    char c;
    while (in.get(c)) {
        if (c == '\n')
            break;
        if (!std::isspace(c)) {
            buffer += c;
            started = true;
        } else if (started) {
            break;
        }
    }
    return buffer;
}

bool handle_define(std::istream& in, Context& ctx)
{
    const std::string tag = get_tag(in);
    if (tag.empty()) {
        std::cerr << "Define statement without tag" << std::endl;
        return false;
    } else {
        ctx.Tags.insert(tag);
        return true;
    }
}

bool handle_undef(std::istream& in, Context& ctx)
{
    const std::string tag = get_tag(in);
    if (tag.empty()) {
        std::cerr << "Undef statement without tag" << std::endl;
        return false;
    } else {
        ctx.Tags.erase(tag);
        return true;
    }
}

// Expressions
enum class TokenType {
    Tag,
    ParantheseOpen,
    ParantheseClose,
    And,
    Or,
    Xor,
    Not,
    EOS
};

struct Token {
    TokenType Type;
    std::string Tag;
};

class ExprLexer {
public:
    ExprLexer(std::istream& in)
        : mPosition(0)
    {
        std::string tag;
        auto checkAddTag = [&]() {
            if (!tag.empty())
                mTokens.push_back(Token{ TokenType::Tag, tag });
            tag.clear();
        };

        char c;
        while (in.get(c)) {
            if (c == '\n')
                break;
            else if (std::isspace(c)) {
                checkAddTag();
            } else if (c == '!') {
                checkAddTag();
                mTokens.push_back(Token{ TokenType::Not, "" });
            } else if (c == '^') {
                checkAddTag();
                mTokens.push_back(Token{ TokenType::Xor, "" });
            } else if (c == '(') {
                checkAddTag();
                mTokens.push_back(Token{ TokenType::ParantheseOpen, "" });
            } else if (c == ')') {
                checkAddTag();
                mTokens.push_back(Token{ TokenType::ParantheseClose, "" });
            } else if (c == '&') {
                checkAddTag();
                in.get(c);
                if (c == '&')
                    mTokens.push_back(Token{ TokenType::And, "" });
                else {
                    std::cerr << "And operator is && not &" << std::endl;
                    mTokens.push_back(Token{ TokenType::And, "" });
                    in.unget();
                }
            } else if (c == '|') {
                checkAddTag();
                in.get(c);
                if (c == '|')
                    mTokens.push_back(Token{ TokenType::Or, "" });
                else {
                    std::cerr << "Or operator is || not |" << std::endl;
                    mTokens.push_back(Token{ TokenType::Or, "" });
                    in.unget();
                }
            } else {
                tag += c;
            }
        }

        checkAddTag(); // Add possible trailing tags
    }

    bool accept(TokenType type)
    {
        const bool good = current().Type == type;
        if (!good)
            std::cerr << "Expected '" << tokenStr(type) << "' but got '" << tokenStr(current().Type) << "'" << std::endl;
        ++mPosition;
        return good;
    }

    void accept()
    {
        ++mPosition;
    }

    Token current()
    {
        if (mPosition >= mTokens.size())
            return Token{ TokenType::EOS, "" };
        else
            return mTokens[mPosition];
    }

private:
    static inline const char* tokenStr(TokenType type)
    {
        switch (type) {
        case TokenType::Tag:
            return "Tag";
        case TokenType::ParantheseOpen:
            return "(";
        case TokenType::ParantheseClose:
            return ")";
        case TokenType::And:
            return "&&";
        case TokenType::Or:
            return "||";
        case TokenType::Xor:
            return "^";
        case TokenType::Not:
            return "!";
        default:
        case TokenType::EOS:
            return "EOS";
        }
    }

    std::vector<Token> mTokens;
    size_t mPosition;
};

bool binary_condition(ExprLexer& lexer, const Context& ctx);
bool primary_condition(ExprLexer& lexer, const Context& ctx)
{
    if (lexer.current().Type == TokenType::ParantheseOpen) {
        lexer.accept();
        const bool a = binary_condition(lexer, ctx);
        if (!lexer.accept(TokenType::ParantheseClose))
            return false;
        return a;
    } else {
        Token tag = lexer.current();
        if (!lexer.accept(TokenType::Tag))
            return false;
        return ctx.Tags.count(tag.Tag) > 0;
    }
}

bool unary_condition(ExprLexer& lexer, const Context& ctx)
{
    if (lexer.current().Type == TokenType::Not) {
        lexer.accept();
        return !unary_condition(lexer, ctx);
    } else {
        return primary_condition(lexer, ctx);
    }
}

bool binary_condition(ExprLexer& lexer, const Context& ctx)
{
    const bool a = unary_condition(lexer, ctx);

    if (lexer.current().Type == TokenType::EOS || lexer.current().Type == TokenType::ParantheseClose) {
        lexer.accept();
        return a;
    } else if (lexer.current().Type == TokenType::And) {
        lexer.accept();
        const bool b = binary_condition(lexer, ctx);
        return a && b;
    } else if (lexer.current().Type == TokenType::Or) {
        lexer.accept();
        const bool b = binary_condition(lexer, ctx);
        return a || b;
    } else if (lexer.current().Type == TokenType::Xor) {
        lexer.accept();
        const bool b = binary_condition(lexer, ctx);
        return a ^ b;
    } else {
        return false; // TODO
    }
}

bool handle_condition(std::istream& in, const Context& ctx)
{
    ExprLexer lexer(in);
    if (lexer.current().Type == TokenType::EOS) {
        std::cerr << "Expected condition but got nothing" << std::endl;
        return false;
    }
    return binary_condition(lexer, ctx);
}