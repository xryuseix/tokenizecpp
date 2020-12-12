#include <iostream>
using namespace std;
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

const set<string> value{"nullptr"};
const set<string> type{
    "int",      "long",     "short",        "signed",   "unsigned", "float",
    "double",   "bool",     "true",         "false",    "char",     "wchar",
    "char16_t", "char32_t", "void",         "auto",     "class",    "struct",
    "union",    "enum",     "const",        "volatile", "extern",   "register",
    "static",   "mutable",  "thread_local", "friend",   "typedef",  "constexpr",
    "explicit", "inline",   "virtual"};
const set<string> class_word{"public", "protected", "private", "operator",
                             "this"};
const set<string> sentence{"if",   "else",   "for",   "while",
                           "do",   "switch", "break", "continue",
                           "goto", "return", "try",   "catch"};
const set<string> formula{"new",          "delete",     "dynamic_cast",
                          "static_cast",  "const_cast", "reinterpret_cast",
                          "alignof",      "decltype",   "sizeof",
                          "typeid",       "throw",      "noexcept",
                          "static_assert"};
const set<string> template_word{"template", "typename", "export"};
const set<string> namespace_word{"namespace", "using"};
const set<string> inline_assembler{"asm"};
const set<string> attribute{"alignas"};
const set<string> contextual{"final", "override"};
const set<string> logical{"and",    "and_eq", "bitand", "bitor", "compl", "not",
                          "not_eq", "or",     "or_eq",  "xor",   "xor_eq"};
const set<string> op{"+",  "-",  "*",  "/",   "++",  "--", "+=", "-=", "*=",
                     "/=", "<",  "<=", ">",   ">=",  "!=", "==", "!",  "&&",
                     "||", "<<", ">>", "<<=", ">>=", "~",  "&",  "&=", "|",
                     "|=", "^",  "^=", "=",   "->",  "?",  ":",  "::"};
const set<string> paren{"(", ")", "{", "}", "[", "]"};

vector<string> tokenize(const string &str, vector<string> &token) {
    // 空白，改行，タブ，セミコロンの削除
    // (){}などでsplit
    const set<string> remove_string{"\r", "\n", " ", "\t"};

    set<string> break_string = {";", ","};
    // break_stringにopとparenを追加
    set_union(begin(op), end(op), begin(paren), end(paren),
              inserter(break_string, end(break_string)));
    for (int i = 0; i < (int)str.size(); i++) {
        if (str[i] == '\n' || remove_string.count(string(1, str[i]))) {
            // 改行,タブ,空白なので無視
            continue;
        } else {
            string tmp;
            int j = 0;
            int reserve_string = 0;
            while (i + j < (int)str.size()) {
                if (remove_string.count(string(1, str[i + j]))) {
                    // 「str;」の;が次のもじのとき，strだけをtokenに入れる
                    break;
                }
                bool string_break_flag = false;  // ここで切るかのフラグ
                // tmpの次の3,2,1文字が予約語かチェック
                for (int prog = 3; prog > 0; prog--) {
                    if (i + j + prog - 1 >= (int)str.size()) {
                        continue;
                    }
                    if (break_string.count(str.substr(i + j, prog))) {
                        string_break_flag = true;
                        reserve_string = prog;
                        break;
                    }
                }
                if (!string_break_flag) {
                    // この先好きなくとも1文字は予約語ではない場合
                    tmp += str[i + j];
                    j++;
                } else {
                    break;
                }
            }
            if ((int)tmp.size()) token.push_back(tmp);
            if (reserve_string) {
                // AAA++のとき，AAAはこの上の行で追加，++はここで追加
                token.push_back(str.substr(i + j, reserve_string));
                i += reserve_string;
            }
            i += max(j - 1, 0);
        }
    }
    return token;
}

void add_label(const vector<string> &token, vector<string> &labeled_token) {
    map<string, set<string>> keyword{{"VALUE", value},
                                     {"TYPE", type},
                                     {"CLASS", class_word},
                                     {"SENTENCE", sentence},
                                     {"FORMULA", formula},
                                     {"TEMPLATE", template_word},
                                     {"NAMESPACE", namespace_word},
                                     {"ASSEMBLER", inline_assembler},
                                     {"ATTRIBUTE", attribute},
                                     {"CONTEXTUAL", contextual},
                                     {"OP", logical}};

    bool passed_typedef = false;
    for (int i = 0; i < (int)token.size(); i++) {
        const string word = token[i];
        // 型を設定
        bool add_label = false;
        if (word == "typedef") {
            passed_typedef = true;
        } else if (word == ";") {
            passed_typedef = false;
        }
        for (auto it = keyword.begin(); it != keyword.end(); it++) {
            if (it->second.count(word)) {
                labeled_token.push_back(it->first);
                add_label = true;
                break;
            }
        }
        if (add_label) {
            continue;
        } else if (!add_label && passed_typedef) {
            labeled_token.push_back("TYPE");
            keyword["TYPE"].insert(word);
            continue;
        }

        if (regex_match(word, regex("(\".*\")"))) {  // 文字列
            labeled_token.push_back("STR");
        } else if (regex_match(word, regex("\\d+"))) {  // 数列
            labeled_token.push_back("NUM");
        } else if (op.count(word)) {  // 演算子
            labeled_token.push_back("OP");
        } else if (paren.count(word)) {  // 括弧
            labeled_token.push_back("PAREN");
        } else if (word == ";" || word == ",") {  // 区切り文字
            labeled_token.push_back("PUNC");
        } else if (i + 1 < (int)token.size() && token[i + 1] == "(") {  // 関数
            labeled_token.push_back("FUNC");
        } else {
            labeled_token.push_back("VAR");  // 変数
        }
    }
}

string remove_comment(string s) {
    const regex comment1{R"(/\*[\s\S]*?\*/)"};
    s = regex_replace(s, comment1, "");
    const regex comment2{R"(//.*)"};
    s = regex_replace(s, comment2, "");
    const regex include{R"(#include.*)"};
    s = regex_replace(s, include, "");
    const regex std{"std::"};
    s = regex_replace(s, std, "");
    const regex crlf{"\r\n"};
    s = regex_replace(s, crlf, "\n");
    for (int c = 0; c < (int)s.size(); c++) {
        if (!isprint(s[c]) && !iscntrl(s[c])) {
            s[c] = ' ';
        }
    }
    return s;
}

string to_tokenize(string code) {
    code = remove_comment(code);
    vector<string> token, labeled_token;
    tokenize(code, token);
    add_label(token, labeled_token);
    string res = "";
    for (auto s : labeled_token) {
        res += s;
        res += " ";
    }
    res.erase(--res.end());
    return res;
}


PYBIND11_MODULE(tokenizecpp, m) {
    m.def("to_tokenize", &to_tokenize);
    m.def("remove_comment", &remove_comment);
}
