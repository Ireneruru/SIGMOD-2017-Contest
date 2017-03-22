#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <atomic>
#include <thread>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/thread/barrier.hpp>
#include <tbb/concurrent_vector.h>
#include <semaphore.h>

using namespace std;

typedef unsigned ptr_t;

const ptr_t null = ~0U;

bool small_work;
int find_work_number;

int thread_map[128];

// won't vary
const unsigned input_buffer_size = 1 << 28;  // 256M
const unsigned query_buffer_size = 1 << 28;  // 256M
const unsigned max_insert_number = 1 << 24;  // 16M
size_t line_limit_size = 1 << 24;  // 16M

const unsigned string_hash_number = 1 << 23;  // 8M
const unsigned trie_hash_number = 1 << 23;   // 8M
const int trie_node_len = 8;

const int trie_thread_number = 12;
const int find_thread_number = 30;
const int find_init_number = 10;
const unsigned find_batch_size = 1 << 9;  // 512

// will grow when meet large batch
const unsigned max_query_number = 1 << 10;  // 1K
const unsigned max_answer_number = 1 << 20; // 1M
const unsigned max_answer_size = 1 << 16;  // 4K

// will grow during program running
// will fail when meet very large batch
const unsigned trie_root_size = string_hash_number * 2;
const unsigned trie_node_size = trie_hash_number * 2;

const unsigned total_ngram_number = 1 << 26;  // 64M
const unsigned root_string_size = 1 << 26;  // 64M
