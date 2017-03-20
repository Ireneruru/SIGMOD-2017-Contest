#include "token.h"

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

struct TrieHash {
    ptr_t from, next;
    token_t token[trie_node_len];
    unsigned ngram[trie_node_len], time[trie_node_len * 2];
    unsigned tc[trie_node_len];
    char size;

    void add(char pos, unsigned timestamp) {
        if (tc[pos] >= 2) {
            Ngram &ng = ngrams->data()[ngram[pos]];
            if (tc[pos] == 2) {
                ng.add(time[0]);
                ng.add(time[1]);
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
atomic_uint ngram_number(1);
sem_t trie_done_sem;
bool batch_done;

struct Trie {
    Token* token_table;
    vector<TrieHash> trie_node;
    ptr_t next_node;
    spsc_que queue;
    sem_t trie_start_sem;

    static unsigned trie_hash(ptr_t node, char pos, token_t token) {
        return (node * 10000007 + pos * 10007 + token) & (trie_hash_number - 1);
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
                ptr_t node = root, t;
                char pos = 0;
                for (char *p = task.ngram, *q = p; *p != '\n'; p = ++q) {
                    token_t token = token_table->get(q, true);
                    if (node != root && pos + 1 < trie_node_len) {
                        while (pos + 1 < trie_node[node].size && trie_node[node].token[pos + 1] == token) {
                            ++pos;
                            p = ++q;
                            if (*p == '\n') break;
                            token = token_table->get(q, true);
                        }
                        if (*p != '\n' && pos + 1 == trie_node[node].size) {
                            while (pos + 1 < trie_node_len) {
                                trie_node[node].token[++pos] = token;
                                trie_node[node].size++;
                                p = ++q;
                                if (*p == '\n') break;
                                token = token_table->get(q, true);
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
                }
                TrieHash &p = trie_node[node];
                if (p.ngram[pos] == 0) p.ngram[pos] = ngram_number++;
                p.add(pos, task.timestamp);
            }
            sem_post(&trie_done_sem);
        }
    }

    void update_size() {
        token_table->update_size();
        if (next_node * 1.5 > trie_node.size()) {
            trie_node.resize(trie_node.size() * 2);
        }
    }

    Trie() {
        token_table = new Token();
        trie_node.resize(trie_node_size);
        next_node = trie_hash_number;
        sem_init(&trie_start_sem, 0, 0);
        new thread(&Trie::trie_thread, this);
    }
};

Trie* tries[trie_thread_number];

inline void insert(char* ngram, unsigned timestamp, bool ins) {
    Task task(ngram, (timestamp << 1) + !ins);
    char* p = ngram;
    unsigned i = Token::token_hash(p) & (trie_thread_number - 1);
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
    for (int i = 0; i < trie_thread_number; ++i) {
        tries[i]->update_size();
    }
    if (ngram_number * 1.5 > ngrams->size()) {
        ngrams->resize(ngrams->size() * 2);
    }
}

void init_trie() {
    ngrams = new vector<Ngram>;
    ngrams->resize(total_ngram_number);
    sem_init(&trie_done_sem, 0, 0);
    for (int i = 0; i < trie_thread_number; ++i) {
        tries[i] = new Trie();
    }
}
