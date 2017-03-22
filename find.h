#include "trie.h"

struct Answer {
    char *start, *end;
    TrieRoot* word;
    TrieHash* ngram;
    char pos;
};

struct Ans {
    vector<char> ans;
    atomic_bool done;
    Ans() : ans(max_answer_size) {};
    Ans(Ans &&a) : ans(a.ans) {
        done.store(a.done);
    }
};

bool operator< (const Answer& a, const Answer& b) {
    return a.start < b.start || (a.start == b.start && a.end < b.end);
}

sem_t find_start_sem[find_thread_number], find_done_sem;
sem_t print_start_sem, print_done_sem;

char *find_string;
unsigned total_len;
atomic_uint next_pos;

tbb::concurrent_vector<Answer> *answer;

vector<Ans>* ans;
vector<unsigned>* last_found;

vector<pair<char*,unsigned>> sentences;

void print_thread() {
    while (true) {
        sem_wait(&print_start_sem);
        unsigned now = 0;
        while (now < sentences.size()) {
            if (!ans->data()[now].done) {
                continue;
            }
            fputs_unlocked(ans->data()[now].ans.data(), stdout);
            ans->data()[now].done.store(false);
            now++;
        }
        sem_post(&print_done_sem);
    }
}

void find_thread(int num) {
    while (true) {
        sem_wait(&find_start_sem[num]);
        if (small_work) {
            while (true) {
                unsigned query = next_pos++;
                if (query >= sentences.size()) {
                    break;
                }
                vector<unsigned> &last = last_found[num];
                vector<char> &ans_ = (*ans)[query].ans;
                ptr_t ans_p = 0;

                unsigned timestamp = sentences[query].second;
                bool first = true;
                for (char *p = sentences[query].first, *q = p; *p != '\n'; p = ++q) {
                    unsigned first_hash = Trie::string_hash(q);
                    unsigned trie_num = thread_map[first_hash & 127];
                    Trie &T = *tries[trie_num];

                    char *r = p, *s = r;
                    ptr_t token = T.get(s, first_hash);
                    s = q;
                    if (token == null) {
                        continue;
                    }
                    if (T.trie_root[token].ngram) {
                        TrieRoot &tr = T.trie_root[token];
                        if (last[tr.ngram] < timestamp && tr.check(timestamp)) {
                            if (ans_p * 1.5 > ans_.size()) {
                                ans_.resize(ans_.size() * 2);
                            }
                            if (!first) {
                                ans_[ans_p++] = '|';
                            }
                            memcpy(ans_.data() + ans_p, p, s - p);
                            ans_p += s - p;
                            first = false;
                            last[tr.ngram] = timestamp;
                        }
                    }
                    r = ++s;
                    if (*r != '\n') {
                        ptr_t node = ~token;
                        char pos = 0;
                        do {
                            token = T.get(s, false);
                            if (token == null) {
                                break;
                            }
                            bool found = false;
                            if (node < T.next_node && pos + 1 < T.trie_node[node].size && T.trie_node[node].token[pos + 1] == token) {
                                ++pos;
                                found = true;
                            } else {
                                unsigned hash = T.trie_hash(node, pos, token);
                                for (ptr_t t = hash;
                                     t != null && T.trie_node[t].from != null; t = T.trie_node[t].next) {
                                    if (T.trie_node[t].from == node && T.trie_node[t].token[0] == token) {
                                        node = t;
                                        pos = 0;
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (!found) break;
                            if (T.trie_node[node].ngram[pos]) {
                                TrieHash &tr = T.trie_node[node];
                                if (last[tr.ngram[pos]] < timestamp && tr.check(pos, timestamp)) {
                                    if (ans_p * 1.5 > ans_.size()) {
                                        ans_.resize(ans_.size() * 2);
                                    }
                                    if (!first) {
                                        ans_[ans_p++] = '|';
                                    }
                                    memcpy(ans_.data() + ans_p, p, s - p);
                                    ans_p += s - p;
                                    first = false;
                                    last[tr.ngram[pos]] = timestamp;
                                }
                            }
                            r = ++s;
                        } while (*r != '\n');
                    }
                }
                if (first) {
                    strcpy(ans_.data() + ans_p, "-1\n");
                }
                else {
                    strcpy(ans_.data() + ans_p, "\n");
                }
                (*ans)[query].done.store(true);
            }
        }
        else {
            Answer ans;
            while (true) {
                unsigned start = next_pos.fetch_add(find_batch_size);
                unsigned end = start + find_batch_size;
                if (start >= total_len) {
                    break;
                }
                if (end >= total_len) end = total_len - 1;
                char *start_p = find_string + start, *end_p = find_string + end;
                while (*(end_p - 1) != ' ') ++end_p;
                while (start_p != find_string && *(start_p - 1) != ' ' && *start_p != '\n') ++start_p;

                for (char *p = start_p, *q = p; p != end_p; p = ++q) {
                    if (*p == '\n') continue;

                    unsigned first_hash = Trie::string_hash(q);
                    unsigned trie_num = thread_map[first_hash & 127];
                    Trie &T = *tries[trie_num];

                    char *r = p, *s = r;
                    ptr_t token = T.get(s, first_hash);
                    s = q;
                    if (token == null) {
                        continue;
                    }
                    if (T.trie_root[token].ngram) {
                        ans.start = p;
                        ans.end = s;
                        ans.word = &T.trie_root[token];
                        ans.ngram = NULL;
                        answer->push_back(ans);
                    }
                    r = ++s;
                    if (*r != '\n') {
                        ptr_t node = ~token;
                        char pos = 0;
                        do {
                            token = T.get(s, false);
                            if (token == null) {
                                break;
                            }
                            bool found = false;
                            if (node < T.next_node && pos + 1 < T.trie_node[node].size &&
                                T.trie_node[node].token[pos + 1] == token) {
                                ++pos;
                                found = true;
                            } else {
                                unsigned hash = T.trie_hash(node, pos, token);
                                for (ptr_t t = hash;
                                     t != null && T.trie_node[t].from != null; t = T.trie_node[t].next) {
                                    if (T.trie_node[t].from == node && T.trie_node[t].token[0] == token) {
                                        node = t;
                                        pos = 0;
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (!found) break;
                            if (T.trie_node[node].ngram[pos]) {
                                ans.start = p;
                                ans.end = s;
                                ans.word = NULL;
                                ans.ngram = &T.trie_node[node];
                                ans.pos = pos;
                                answer->push_back(ans);
                            }
                            r = ++s;
                        } while (*r != '\n');
                    }
                }
            }
        }
        sem_post(&find_done_sem);
    }
}

inline void find(char* string, unsigned len) {
    total_len = len;
    find_string = string;
    next_pos = 0;
    answer->clear();
    for (int i = 0; i < find_work_number; ++i) {
        sem_post(&find_start_sem[i]);
    }
    if (small_work) {
        sem_post(&print_start_sem);
    }
    for (int i = 0; i < find_work_number; ++i) {
        sem_wait(&find_done_sem);
    }
    if (small_work) {
        sem_wait(&print_done_sem);
    }
}

inline void update_find_size() {
    unsigned max_ngram = 0;
    for (int i = 0; i < trie_thread_number; ++i) {
        if (tries[i]->next_ngram > max_ngram) {
            max_ngram = tries[i]->next_ngram;
        }
    }
    for (int i = 0; i <= find_thread_number; ++i) {
        if (max_ngram * 1.5 > last_found[i].size()) {
            last_found[i].resize(last_found[i].size() * 2);
        }
    }
}

inline void init_find() {
    answer = new tbb::concurrent_vector<Answer>;
    answer->reserve(max_answer_number);
    sentences.resize(max_query_number);
    sem_init(&find_done_sem, 0, 0);
    ans = new vector<Ans>;
    ans->resize(max_query_number);
    last_found = new vector<unsigned>[find_thread_number + 1];
    for (int i = 0; i < find_thread_number; ++i) {
        last_found[i].resize(total_ngram_number);
        sem_init(&find_start_sem[i], 0, 0);
    }
    last_found[find_thread_number].resize(total_ngram_number);
    for (int i = 0; i < find_init_number; ++i) {
        new thread(find_thread, i);
    }
    find_work_number = find_init_number;
    sem_init(&print_start_sem, 0, 0);
    sem_init(&print_done_sem, 0, 0);
    new thread(print_thread);
}
