#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <boost/lockfree/spsc_queue.hpp>
#include <tbb/concurrent_vector.h>
#include <semaphore.h>

using namespace std;

typedef unsigned token_t;
typedef unsigned ptr_t;

const ptr_t null = ~0U;
const ptr_t root = null - 1;

bool small_work;
int find_work_number;

// won't vary
const unsigned input_buffer_size = 1 << 28;  // 256M
const unsigned query_buffer_size = 1 << 28;  // 256M
const unsigned max_insert_number = 1 << 24;  // 16M

const unsigned token_hash_number = 1 << 22;  // 8M
const unsigned trie_hash_number = 1 << 23;   // 8M
const int trie_node_len = 8;

const int trie_thread_number = 8;
const int find_thread_number = 20;
const int find_init_number = 10;
const unsigned find_batch_size = 1 << 9;  // 512


// will grow when meet large batch
const unsigned max_query_number = 1 << 10;  // 1K
const unsigned max_answer_number = 1 << 20; // 1M
const unsigned max_answer_size = 1 << 16;  // 4K


// will grow during program running
// will fail when meet very large batch
const unsigned token_node_size = 1 << 23;  // 4M
const unsigned total_ngram_number = 1 << 26;  // 64M

const unsigned token_string_size = token_hash_number * 2;
const unsigned trie_node_size = trie_hash_number * 2;
