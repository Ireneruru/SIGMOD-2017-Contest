# SIGMOD-2017-Contest

## ACM SIGMOD 2017 Programming Contest

### Group ID : Mccree

### Brief Explanation
	- Basically, our solution is on hash and trie basis.
	- We compute the hash ID of every word and use it as the global unique ID.

	- ADD and DELETE operations
	    - We treat add and delete operations in the same way. We use several threads to maintain several sub-trie
	    structure. In a batch, we first do all the add and delete operations. Then, add the phases into the trie
	    structure and record the timestamp accordingly. If it is a delete operation, we mark the node of the trie structure
	    and modify the timestamp of it.

	- QUERY operations
	    - We have two strategies to query the words in a batch.
	        - In the first one, we assign different query to different threads. All the threads work concurrently. Then, we
	          sort the answer that we have found and output them.
	        - In the second one, we concatenate all the query sentences, cut them into the same length on character level.
	          Then, we assign them to the different threads and do the query operations separately. In the end, we collect
	          the answers, integrate them into a correct one and outputs the final answer.

### Three party code
    - Boost 1.58
    - Intel TBB 4.4





