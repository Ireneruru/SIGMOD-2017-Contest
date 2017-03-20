#include "constant.h"

struct TokenHash {
    ptr_t string;
    ptr_t next;
    TokenHash() : string(null), next(null) {}
};

struct Token {
    vector<TokenHash> token_node;
    vector<char> token_string;
    ptr_t next_token, string_top;

    static unsigned token_hash(char* &key) {
        unsigned hash = 0;
        while (*key != ' ') {
            hash = hash * 16777619 ^ *(key++) + 131;
        }
        return hash & (token_hash_number - 1);
    }

    static bool str_cmp(char* key1, char* key2) {
        while (*key1 == *key2 && *key1 != ' ') {
            key1++, key2++;
        }
        return *key1 == ' ' && *key2 == ' ';
    }

    token_t get(char* &str, bool add) {
        char *q = str;
        unsigned hash = token_hash(q), last = null, t;
        for (t = hash; t != null && token_node[t].string != null; last = t, t = token_node[t].next) {
            if (str_cmp(token_string.data() + token_node[t].string, str)) {
                str = q;
                return t;
            }
        }
        if (!add) {
            return null;
        }
        if (t == null) {
            t = next_token++;
            token_node[last].next = t;
        }
        char* s = token_string.data() + string_top;
        memcpy(s, str, q - str + 1);
        token_node[t].string = string_top;
        string_top += q - str + 1;
        str = q;
        return t;
    }

    void update_size() {
        if (string_top * 1.5 > token_string.size()) {
            token_string.resize(token_string.size() * 2);
        }
        if (next_token * 1.5 > token_node.size()) {
            token_node.resize(token_node.size() * 2);
        }
    }

    Token() {
        token_string.resize(token_string_size);
        token_node.resize(token_node_size);
        string_top = 0;
        next_token = token_hash_number;
    }
};
