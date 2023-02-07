#include "lexer.h"

#include <algorithm>
#include <charconv>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

Token Lexer::ParseIndent() {
    is_need_to_parse_indent_ = false;
    size_t this_indent = 0;
    char c;
    while (input_.get(c) && c == ' ') {
        this_indent++;
    }
    if (c == '\n') {
        input_.putback(c);
        return GetTokenFromIstream(input_);
    }
    input_.putback(c);
    if (indent_ > this_indent) {
        size_t buff_size = indent_ / 2 - this_indent / 2;
        for (size_t i = 0; i < buff_size; ++i) {
            token_buffer_.push_back(token_type::Dedent{});
        }
        indent_ -= 2 * buff_size;
        return NextToken();
    } else if (indent_ < this_indent) {
        indent_ += 2;
        return token_type::Indent{};
    } else {
        return GetTokenFromIstream(input_);
    }
}

Token Lexer::ParseConstString() {

    char string_start = input_.get();
    auto it = std::istreambuf_iterator<char>(input_);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    while (true) {
        if (it == end) {
            throw invalid_argument(""s);
        }
        const char ch = *it;
        if (ch == string_start) {
            ++it;
            break;
        } else if (ch == '\\') {
            ++it;
            if (it == end) {
                throw invalid_argument(""s);
            }
            const char escaped_char = *(it);
            switch (escaped_char) {
                case 'n':
                    s.push_back('\n');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case '"':
                    s.push_back('"');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                case '\'':
                    s.push_back('\'');
                    break;
                default:
                    throw invalid_argument(""s);
            }
        } else if (ch == '\n' || ch == '\r') {
            throw invalid_argument(""s);
        } else {
            s.push_back(ch);
        }
        ++it;
    }
    return token_type::String({s});
}

Token Lexer::LoadNumberIdKeywordBool() {
    string id;
    char current_char;
    while (input_.get(current_char) && current_char != ' ' && current_char != '\n' && unexpected_symbols.count(current_char) == 0) {
        id.push_back(current_char);
    }
    input_.putback(current_char);
    if (key_words_.count(id)) {
        if (id == "return") {
            return token_type::Return{};
        }
        if (id == "class") {
            return token_type::Class{};
        }
        if (id == "if") {
            return token_type::If{};
        }
        if (id == "else") {
            return token_type::Else{};
        }
        if (id == "def") {
            return token_type::Def{};
        }
        if (id == "print") {
            return token_type::Print{};
        }
        if (id == "or") {
            return token_type::Or{};
        }
        if (id == "None") {
            return token_type::None{};
        }
        if (id == "True") {
            return token_type::True{};
        }
        if (id == "False") {
            return token_type::False{};
        }
        if (id == "not") {
            return token_type::Not{};
        }
        if (id == "and") {
            return token_type::And{};
        }
    }
    try {
        auto number = stoi(id);
        return token_type::Number({number});
    } catch(...) {
        return token_type::Id({id});
    }
}

Token Lexer::GetTokenFromIstream(std::istream& input) {
    if (!token_buffer_.empty()) {
        token_buffer_.pop_back();
        return token_type::Dedent{};
    }
    if (is_need_to_parse_indent_) {
        return ParseIndent();
    }
    char c;
    while (input.get(c)) {
        switch (c) {
            case '_':
                input_.putback(c);
                return LoadNumberIdKeywordBool();
            case '-':
                return token_type::Char({c});
            case '\n':

                if (tokens_.empty()) continue;
                is_new_line = true;
                is_need_to_parse_indent_ = true;
                if (tokens_.at(tokens_.size() - 1) == token_type::Newline{}) return ParseIndent();

                return token_type::Newline();
            case '.':
                return token_type::Char({c});
            case ',':
                return token_type::Char({c});
            case '(':
                return token_type::Char({c});
            case '+':
                return token_type::Char({c});
            case ')':
                return token_type::Char({c});
            case ' ':
                break;
            case '\"':
                input_.putback(c);
                return ParseConstString();
            case '\'':
                input_.putback(c);
                return ParseConstString();
            case '#':
                while (c != '\n' && input_) {
                    input_.get(c);
                }

                if (is_token_in_last_line_) {
                    is_new_line = true;
                    is_token_in_last_line_ = false;
                    is_need_to_parse_indent_ = true;
                    return token_type::Newline{};
                }
                is_new_line = true;
                if (tokens_.empty()) break;
                return GetTokenFromIstream(input_);
            case '=':
                if (!input_) {
                    return token_type::Char({c});
                }
                char new_char;
                input_.get(new_char);
                if (new_char == ' ') {
                    return token_type::Char({c});
                }
                if (new_char == '=') {
                    return token_type::Eq{};
                }
                input_.putback(new_char);
                return token_type::Char({c});
            case '<':
                if (!input_) {
                    return token_type::Char({c});
                }
                input_.get(new_char);

                if (new_char == '=') {
                    return token_type::LessOrEq{};
                }
                input_.putback(new_char);
                return token_type::Char({c});

            case '!':
                input_.get();
                return token_type::NotEq{};
            case '*':
                return token_type::Char({c});
            case '/':
                return token_type::Char({c});
            case ':':
                return token_type::Char({c});
            case '>':
                if (!input_) {
                    return token_type::Char({c});
                }
                input_.get(new_char);
                if (new_char == '=') {
                    return token_type::GreaterOrEq{};
                }
                input_.putback(new_char);
                return token_type::Char({c});
                //throw std::logic_error("");
            default:
                input_.putback(c);
                return LoadNumberIdKeywordBool();
        }
    }

    if (!is_new_line) {
        is_new_line = true;
        is_need_to_parse_indent_ = true;
        return token_type::Newline{};
    }
    return token_type::Eof{};
}

Lexer::Lexer(std::istream& input) : input_(input) {
    char c;
    input_.get(c);
    if (input_.eof()) {
        tokens_.push_back(token_type::Eof{});
    } else {
        input.putback(c);
        tokens_.push_back(GetTokenFromIstream(input_));
    }

}

const Token& Lexer::CurrentToken() const {
    return tokens_.at(tokens_.size() - 1);
}

Token Lexer::NextToken() {
    if (CurrentToken() == token_type::Eof{}) return  CurrentToken();
    if (!token_buffer_.empty()) {
        token_buffer_.pop_back();
        return token_type::Dedent{};
    }
    if (CurrentToken() != token_type::Newline{} && CurrentToken() != token_type::Indent{}) {
        is_token_in_last_line_ = true;
    } else {
        is_token_in_last_line_ = false;
    }
    tokens_.push_back(GetTokenFromIstream(input_));

    if (CurrentToken() != token_type::Newline{} && CurrentToken() != token_type::Eof{} && CurrentToken() != token_type::Dedent{} && CurrentToken() != token_type::Indent{}) {
        is_new_line = false;
    }
    return tokens_.at(tokens_.size() - 1);
}

}  // namespace parse