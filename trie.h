#include "constant.h"

struct Ngram {
    vector<unsigned> time;
    unsigned tc;

    void add(unsigned timestamp) {
        time.push_back(timestamp);
        tc++;
    }

    bool check(unsigned timestamp) {
        timestamp *= 2;
        for (int i = 1; i < tc && time[i] < timestamp; ++i) {
            if ((i == tc - 1 || time[i + 1] > timestamp) && (time[i] & 1)) {
                return false;
            }
        }
        return true;
    }
};

vector<Ngram>* ngrams;

struct TrieRoot {
    ptr_t string;
    ptr_t next;
    unsigned ngram, time[2], tc;

    void add(unsigned timestamp) {
        if (tc >= 2) {
            Ngram &ng = ngrams->data()[ngram];
            if (tc == 2) {
                ng.add(time[0]);
                ng.add(time[1]);
            }
            ng.add(timestamp);
            tc++;
            return;
        }
        time[tc++] = timestamp;
    }

    bool check(unsigned timestamp) {
        if (tc > 2) {
            Ngram &ng = ngrams->data()[ngram];
            return ng.check(timestamp);
        }
        timestamp *= 2;
        if (time[0] > timestamp) {
            return false;
        }
        return tc == 1 || !(time[1] & 1) || time[1] > timestamp;
    }

    TrieRoot() : string(null), next(null) {}
};

struct TrieHash {
    ptr_t from, next;
    ptr_t token[trie_node_len];
    unsigned ngram[trie_node_len], time[trie_node_len * 2];
    unsigned tc[trie_node_len];
    char size;

    void add(char pos, unsigned timestamp) {
        if (tc[pos] >= 2) {
            Ngram &ng = ngrams->data()[ngram[pos]];
            if (tc[pos] == 2) {
                ng.add(time[pos + pos]);
                ng.add(time[pos + pos + 1]);
            }
            ng.add(timestamp);
            tc[pos]++;
            return;
        }
        time[pos + pos + tc[pos]] = timestamp;
        tc[pos]++;
    }

    bool check(char pos, unsigned timestamp) {
        if (tc[pos] > 2) {
            Ngram &ng = ngrams->data()[ngram[pos]];
            return ng.check(timestamp);
        }
        timestamp *= 2;
        if (time[pos + pos] > timestamp) {
            return false;
        }
        return tc[pos] == 1 || !(time[pos + pos + 1] & 1) || time[pos + pos + 1] > timestamp;
    }

    TrieHash() : from(null), next(null) {}
};

struct Task {
    char* ngram;
    unsigned timestamp;
    Task() {}
    Task(char* _ngram, unsigned _timestamp) {
        ngram = _ngram;
        timestamp = _timestamp;
    }
};

typedef boost::lockfree::spsc_queue<Task, boost::lockfree::capacity<max_insert_number>> spsc_que;
sem_t trie_done_sem;
bool batch_done;

struct Trie {
    unsigned number;
    vector<TrieHash> trie_node;
    vector<TrieRoot> trie_root;
    vector<char> root_string;
    ptr_t next_node, next_root, next_ngram, string_top;
    spsc_que queue;
    sem_t trie_start_sem;

    static unsigned string_hash(char* &key) {
        unsigned hash = 0;
        while (*key != ' ') {
            hash = hash * 16777619 ^ *(key++) + 131;
        }
        return hash & (string_hash_number - 1);
    }

    static unsigned trie_hash(ptr_t node, char pos, ptr_t token) {
        return (node * 10000007 + pos * 10007 + token) & (trie_hash_number - 1);
    }

    static bool str_cmp(char* key1, char* key2) {
        while (*key1 == *key2 && *key1 != ' ') {
            key1++, key2++;
        }
        return *key1 == ' ' && *key2 == ' ';
    }

    ptr_t get(char* &str, unsigned hash) {
        for (ptr_t t = hash; t != null && trie_root[t].string != null; t = trie_root[t].next) {
            if (str_cmp(root_string.data() + trie_root[t].string, str)) {
                return t;
            }
        }
        return null;
    }

    ptr_t get(char* &str, bool add) {
        char *q = str;
        if (!add) {
            unsigned hash = string_hash(q), t;
            for (t = hash; t != null && trie_root[t].string != null; t = trie_root[t].next) {
                if (str_cmp(root_string.data() + trie_root[t].string, str)) {
                    str = q;
                    return t;
                }
            }
            str = q;
            return null;
        }
        else {
            unsigned hash = string_hash(q), last = null, t;
            for (t = hash; t != null && trie_root[t].string != null; last = t, t = trie_root[t].next) {
                if (str_cmp(root_string.data() + trie_root[t].string, str)) {
                    str = q;
                    return t;
                }
            }
            if (t == null) {
                t = next_root++;
                trie_root[last].next = t;
            }
            char* s = root_string.data() + string_top;
            memcpy(s, str, q - str + 1);
            trie_root[t].string = string_top;
            string_top += q - str + 1;
            str = q;
            return t;
        }
    }

    void trie_thread() {
        Task task;
        while (true) {
            sem_wait(&trie_start_sem);
            bool pop;
            while ((pop = queue.pop(task)) || !batch_done || (pop = queue.pop(task))) {
                if (!pop) {
                    continue;
                }
                char pos = 0;
                char *p = task.ngram, *q = p;
                ptr_t token = get(q, true);
                p = ++q;
                if (*p == '\n') {
                    TrieRoot &tr = trie_root[token];
                    if (tr.ngram == 0) {
                        tr.ngram = next_ngram;
                        next_ngram += trie_thread_number;
                    }
                    tr.add(task.timestamp);
                }
                else {
                    ptr_t node = ~token, t;
                    do {
                        token = get(q, true);
                        if (node < next_node && pos + 1 < trie_node_len) {
                            while (pos + 1 < trie_node[node].size && trie_node[node].token[pos + 1] == token) {
                                ++pos;
                                p = ++q;
                                if (*p == '\n') break;
                                token = get(q, true);
                            }
                            if (*p != '\n' && pos + 1 == trie_node[node].size) {
                                while (pos + 1 < trie_node_len) {
                                    trie_node[node].token[++pos] = token;
                                    trie_node[node].size++;
                                    p = ++q;
                                    if (*p == '\n') break;
                                    token = get(q, true);
                                }
                            }
                        }
                        if (*p != '\n') {
                            bool found = false;
                            unsigned hash = trie_hash(node, pos, token), last = null;
                            for (t = hash; t != null && trie_node[t].from != null; last = t, t = trie_node[t].next) {
                                if (trie_node[t].from == node && trie_node[t].token[0] == token) {
                                    node = t;
                                    pos = 0;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                if (t == null) {
                                    t = next_node++;
                                    trie_node[last].next = t;
                                }
                                trie_node[t].from = node;
                                trie_node[t].token[0] = token;
                                trie_node[t].size = 1;
                                node = t;
                                pos = 0;
                            }
                        }
                        else {
                            break;
                        }
                        p = ++q;
                    }
                    while (*p != '\n');
                    TrieHash &tr = trie_node[node];
                    if (tr.ngram[pos] == 0) {
                        tr.ngram[pos] = next_ngram;
                        next_ngram += trie_thread_number;
                    }
                    tr.add(pos, task.timestamp);
                }
            }
            sem_post(&trie_done_sem);
        }
    }

    void update_size() {
        if (string_top * 1.5 > root_string.size()) {
            root_string.resize(root_string.size() * 2);
        }
        if (next_root * 1.5 > trie_root.size()) {
            trie_root.resize(trie_root.size() * 2);
        }
        if (next_node * 1.5 > trie_node.size()) {
            trie_node.resize(trie_node.size() * 2);
        }
    }

    Trie(unsigned num) {
        number = num;
        root_string.resize(root_string_size);
        trie_root.resize(trie_root_size);
        trie_node.resize(trie_node_size);
        string_top = 0;
        next_root = string_hash_number;
        next_node = trie_hash_number;
        next_ngram = num;
        sem_init(&trie_start_sem, 0, 0);
        new thread(&Trie::trie_thread, this);
    }
};

Trie* tries[trie_thread_number];

inline void insert(char* ngram, unsigned timestamp, bool ins) {
    Task task(ngram, (timestamp << 1) + !ins);
    char* p = ngram;
    unsigned i = thread_map[Trie::string_hash(p) & 127];
    while (!tries[i]->queue.push(task));
}

inline void start_insert() {
    batch_done = false;
    for (int i = 0; i < trie_thread_number; ++i) {
        sem_post(&tries[i]->trie_start_sem);
    }
}

inline void end_insert() {
    batch_done = true;
    for (int i = 0; i < trie_thread_number; ++i) {
        sem_wait(&trie_done_sem);
    }
}

inline void update_trie_size() {
    unsigned max_ngram = 0;
    for (int i = 0; i < trie_thread_number; ++i) {
        tries[i]->update_size();
        if (tries[i]->next_ngram > max_ngram) {
            max_ngram = tries[i]->next_ngram;
        }
    }
    if (max_ngram * 1.5 > ngrams->size()) {
        ngrams->resize(ngrams->size() * 2);
    }
}

void init_trie() {
    ngrams = new vector<Ngram>;
    ngrams->resize(total_ngram_number);
    sem_init(&trie_done_sem, 0, 0);
    for (unsigned i = 0; i < trie_thread_number; ++i) {
        tries[i] = new Trie(i + 1);
    }
}
