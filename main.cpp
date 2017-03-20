#include "find.h"

char *input_buffer, *query_buffer;
unsigned timestamp, batch_count;

void init() {
    input_buffer = new char[input_buffer_size];
    query_buffer = new char[query_buffer_size];
    memset(input_buffer, 0, sizeof(char) * input_buffer_size);
    memset(query_buffer, 0, sizeof(char) * query_buffer_size);
    init_trie();
    init_find();
}

void read_initial_ngram() {
    start_insert();
    char* input_top = input_buffer;
    while (true) {
        char* read = fgets_unlocked(input_top, input_buffer_size, stdin);
        if (strcmp(read, "S\n") == 0) {
            break;
        }
        unsigned len = (unsigned int) strlen(read);
        read[len - 1] = ' ';
        read[len] = '\n';
        insert(read, 0, true);
        input_top += len + 1;
    }
    end_insert();
    fputs_unlocked("R\n", stdout);
    fflush(stdout);
}


void do_batch() {
    unsigned len;
    char op;
    while (true) {
        char *read, *input_top = input_buffer, *query_top = query_buffer;
        sentences.clear();
        if ((batch_count & 15) == 0) {
            update_trie_size();
            update_find_size();
        }
        batch_count++;

        start_insert();
        while ((op = (char) getchar_unlocked()) != EOF) {
            getchar_unlocked();
            if (op == 'F') break;
            timestamp += 1;
            switch (op) {
                case 'A':
                case 'D':
                    read = fgets_unlocked(input_top, input_buffer_size, stdin);
                    len = (unsigned int) strlen(read);
                    read[len - 1] = ' ';
                    read[len] = '\n';
                    insert(read, timestamp, op == 'A');
                    input_top += len + 1;
                    break;
                case 'Q':
                    read = fgets_unlocked(query_top, query_buffer_size, stdin);
                    len = (unsigned int) strlen(read);
                    read[len - 1] = ' ';
                    read[len] = '\n';
                    sentences.push_back(make_pair(query_top, timestamp));
                    query_top += len + 1;
                    break;
                default:
                    break;
            }
        }
        end_insert();
        if (op == EOF) break;
        if (sentences.size() == 0) {
            continue;
        }

        small_work = (sentences.size() > 2 * find_thread_number || query_top > query_buffer + (1 << 20));
        if (query_top > query_buffer + (1 << 18) && find_work_number < find_thread_number) {
            for (int i = find_work_number; i < find_thread_number; ++i) {
                new thread(find_thread, i);
            }
            find_work_number = find_thread_number;
        }
        if (small_work) {
            if (sentences.size() > ans->size()) {
                ans->resize(sentences.size() * 2);
            }
            find(query_buffer, (unsigned int) (query_top - query_buffer));
            for (int i = 0; i < sentences.size(); ++i) {
                fputs_unlocked((*ans)[i].ans.data(), stdout);
            }
        }
        else {
            find(query_buffer, (unsigned int) (query_top - query_buffer));
            vector<unsigned> &last = last_found[find_thread_number];
            sort(answer->begin(), answer->end());
            auto it = answer->begin();
            for (unsigned i = 0; i < sentences.size(); ++i) {
                char *end;
                if (i == sentences.size() - 1) end = query_top;
                else end = sentences[i + 1].first;
                unsigned timestamp = sentences[i].second;
                bool first = true;
                vector<char> &ans_ = (*ans)[0].ans;
                ptr_t ans_p = 0;
                while (it != answer->end() && it->start < end) {
                    TrieHash* word = it->word;
                    if (last[word->ngram[it->pos]] < timestamp && word->check(it->pos, timestamp)) {
                        last[word->ngram[it->pos]] = timestamp;
                        if (!first) ans_[ans_p++] = '|';
                        const char *s = it->start, *t = it->end;
                        memcpy(ans_.data() + ans_p, s, t - s);
                        ans_p += t - s;
                        first = false;
                        if (ans_p * 1.5 > ans_.size()) {
                            ans_.resize(ans_.size() * 2);
                        }
                    }
                    it++;
                }
                if (ans_p == 0) {
                    fputs_unlocked("-1\n", stdout);
                }
                else {
                    ans_[ans_p] = '\n';
                    ans_[ans_p + 1] = '\0';
                    fputs_unlocked(ans_.data(), stdout);
                }
            }
        }
        fflush(stdout);
    }
}

int main()
{
    init();
    read_initial_ngram();
    do_batch();
    exit(0);
}
