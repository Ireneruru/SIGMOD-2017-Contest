#include "find.h"
#include <sys/time.h>
#include <unistd.h>

char *input_buffer, *query_buffer;
unsigned timestamp;

void init() {
    input_buffer = new char[input_buffer_size];
    query_buffer = new char[query_buffer_size];
    init_trie();
    init_find();
}

void read_initial_ngram() {
    start_insert();
    char* input_top = input_buffer;
    while (true) {
        unsigned len = (unsigned int) getline(&input_top, &line_limit_size, stdin);
        if (strcmp(input_top, "S\n") == 0) {
            break;
        }
        input_top[len - 1] = ' ';
        input_top[len] = '\n';
        insert(input_top, 0, true);
        input_top += len + 1;
    }
    end_insert();
    fputs_unlocked("R\n", stdout);
    fflush(stdout);
}

struct timeval time_start, time_end, time_start2, time_end2;
unsigned insert_time, find_time, collect_time;

void do_batch() {
    unsigned len;
    char op;
    char *input_top = input_buffer, *query_top = query_buffer;
    sentences.clear();
    update_trie_size();
    update_find_size();
    gettimeofday(&time_start, NULL);
    start_insert();
    while (true) {
        while ((op = (char) getchar_unlocked()) != EOF) {
            getchar_unlocked();
            if (op == 'F') break;
            timestamp += 1;
            switch (op) {
                case 'A':
                case 'D':
                    len = (unsigned int) getline(&input_top, &line_limit_size, stdin);
                    input_top[len - 1] = ' ';
                    input_top[len] = '\n';
                    insert(input_top, timestamp, op == 'A');
                    input_top += len + 1;
                    break;
                case 'Q':
                    len = (unsigned int) getline(&query_top, &line_limit_size, stdin);
                    query_top[len - 1] = ' ';
                    query_top[len] = '\n';
                    sentences.push_back(make_pair(query_top, timestamp));
                    query_top += len + 1;
                    break;
                default:
                    break;
            }
        }
        if (op == EOF) break;

        if (sentences.size() == 0) {
            continue;
        }

        end_insert();

        gettimeofday(&time_end, NULL);
        insert_time += (time_end.tv_sec - time_start.tv_sec) * 1000000 + time_end.tv_usec - time_start.tv_usec;



        gettimeofday(&time_start, NULL);

        small_work = (sentences.size() > find_thread_number);
        if (small_work && find_work_number < find_thread_number) {
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
        }
        else {
            find(query_buffer, (unsigned int) (query_top - query_buffer));

            gettimeofday(&time_start2, NULL);

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
                    TrieRoot* word = it->word;
                    TrieHash* ngram = it->ngram;
                    char pos = it->pos;
                    bool ok = false;
                    if (word && last[word->ngram] < timestamp && word->check(timestamp)) {
                        last[word->ngram] = timestamp;
                        ok = true;
                    }
                    if (ngram && last[ngram->ngram[it->pos]] < timestamp && ngram->check(pos, timestamp)) {
                        last[ngram->ngram[pos]] = timestamp;
                        ok = true;
                    }
                    if (ok)
                    {
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
            gettimeofday(&time_end2, NULL);
            collect_time += (time_end2.tv_sec - time_start2.tv_sec) * 1000000 + time_end2.tv_usec - time_start2.tv_usec;
        }

        gettimeofday(&time_end, NULL);
        find_time += (time_end.tv_sec - time_start.tv_sec) * 1000000 + time_end.tv_usec - time_start.tv_usec;

        fflush(stdout);

	input_top = input_buffer, query_top = query_buffer;
	sentences.clear();
	update_trie_size();
	update_find_size();
	gettimeofday(&time_start, NULL);
	start_insert();
    }
}

int main()
{
    for (int i = 0; i < 128; ++i) {
	thread_map[i] = i % trie_thread_number;
    }
    init();
    read_initial_ngram();
    do_batch();
    fprintf(stderr, "insert %u  find %u  collect %u\n", insert_time, find_time, collect_time);
    exit(0);
}
