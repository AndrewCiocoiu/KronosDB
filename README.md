KronosDB ⚡KronosDB is a high-performance, multithreaded key-value storage engine and TCP network server built from scratch in Modern C++20 on Linux.Designed with an emphasis on low-latency concurrency and crash-resilient durability, KronosDB combines fine-grained in-memory lock striping, a custom zero-idle-CPU worker thread pool, an asynchronous Group Commit Binary Write-Ahead Log (WAL), atomic Snapshot Checkpointing, and an event-driven epoll TCP server.🏗️ Architecture & Core Components                  +----------------------------------------------+
                  |         TCP Clients (nc / custom driver)     |
                  +----------------------------------------------+
                                         │
                                         ▼
                     +---------------------------------------+
                     |        Linux epoll Event Loop         |
                     |         (Non-blocking sockets)        |
                     +---------------------------------------+
                                         │
                                         ▼
                         +-------------------------------+
                         |      ThreadPool (Workers)     |
                         +-------------------------------+
                                  /             \
                                 /               \
       (In-Memory Partition)    /                 \   (Durability Log)
                               v                   v
              +--------------------+         +------------------------+
              | Sharded Store (16) |         | Group Commit WAL       |
              | [Shard 0] [mtx]    |         | [Queue] [Swap]         |
              | [Shard 1] [mtx]    |         +------------------------+
              | ...                |                     │ (Single batch flush)
              | [Shard N] [mtx]    |                     ▼
              +--------------------+            +-----------------+
                        │                       | logs/Kronos.wal |
                        │ (Periodic Compaction) +-----------------+
                        ▼
              +-------------------------+
              | logs/Kronos.snapshot    |
              +-------------------------+
1. Sharded In-Memory Storage Engine (Lock Striping)Instead of protecting a single global hash map with one coarse-grained mutex—which serializes all threads under write contention—KronosDB partitions the keyspace across $N$ independent shards (std::vector<std::unique_ptr<Shard>>).Keys are deterministically routed via $\text{hash}(\text{key}) \pmod{\text{shard\_count}}$.Operations on disjoint shards execute with true hardware parallelism without acquiring shared locks.2. Custom Asynchronous Thread PoolKronosDB dispatches background tasks and network commands through a dedicated thread pool built on C++20 std::jthread:Zero-CPU Idle Waiting: Worker threads block on a std::condition_variable when work is unavailable, maintaining $0\%$ CPU utilization until notified.Non-Blocking Futures: Supports generic task submission returning std::future<T> via std::packaged_task for thread-safe asynchronous result retrieval.3. Binary Write-Ahead Log (WAL) & Two-Phase Crash RecoveryTo guarantee durability across restarts and crashes without delimiter parsing bugs (such as raw newlines or binary payloads), KronosDB implements a length-prefixed binary wire protocol:SET Operation:
+---------------+-------------------+--------------+---------------------+----------------+
| OpCode (1 B)  | Key Length (4 B)  | Key Data     | Value Length (4 B)  | Value Data     |
| uint8_t (1)   | uint32_t          | Raw Bytes    | uint32_t            | Raw Bytes      |
+---------------+-------------------+--------------+---------------------+----------------+

DEL (Tombstone) Operation:
+---------------+-------------------+--------------+
| OpCode (1 B)  | Key Length (4 B)  | Key Data     |
| uint8_t (2)   | uint32_t          | Raw Bytes    |
+---------------+-------------------+--------------+
Tombstone Deletions: DEL records write an append-only tombstone (OpCode = 2), avoiding costly in-place disk mutations.Torn-Write Protection: During startup replay, the recovery engine inspects stream state flags after every record header and payload chunk, safely halting at the exact boundary of an incomplete record caused by sudden power loss.Two-Phase Bootstrapping: On startup, the engine reads logs/Kronos.snapshot first to restore baseline state, followed by replaying subsequent incremental modifications recorded in logs/Kronos.wal.4. Asynchronous Group Commit PipelineSynchronous per-write disk flushes degrade throughput to mechanical storage speeds. KronosDB resolves this with an asynchronous pipeline:Worker threads enqueue a LogRecord alongside a std::promise<bool> into a staging deque and await the paired std::future<bool>.A dedicated flusher thread wakes via condition variable, performs an $O(1)$ pointer swap (std::deque::swap) into an isolated local batch, and releases the queue lock immediately.The flusher sequentially writes the batch, calls .flush() once for the entire group, and resolves all promises in parallel, waking waiting callers simultaneously.5. Snapshot Checkpointing & WAL CompactionTo prevent unbounded log expansion, KronosDB triggers a full state checkpoint every $50\text{k}$ operations (tracked via lock-free std::atomic<size_t>):Multi-Shard Freeze: Acquires unique locks sequentially across every shard to capture a consistent state snapshot.Atomic Temp File Flushing: Streams active key-value pairs into a .snapshot.tmp binary file.Atomic Promotion: Commits the snapshot via std::filesystem::rename (.snapshot.tmp $\to$ .snapshot), ensuring corrupted files are never read during recovery.Coordinated WAL Truncation: Synchronizes with the background flusher via condition variable barriers (queue.empty() && !is_writing_batch), truncating active log files safely to $0$ bytes.6. Non-Blocking epoll Network ServerKronosDB provides a high-throughput TCP network interface built using Linux epoll:Edge/Level Socket Management: Configures listening and client sockets in non-blocking mode (O_NONBLOCK).Burst-Safe Accept Loop: Handles incoming connection spikes with draining loops checking for EAGAIN/EWOULDBLOCK.Worker Offloading: Reads incoming raw request buffers and delegates command parsing and execution to the thread pool, sending responses with MSG_NOSIGNAL to prevent process termination on client disconnects.📊 Performance BenchmarksBenchmarked on 100,000 write operations on Linux across incremental architectural milestones ($200 \times 500$ work chunks):In-Memory Concurrency Scaling (Lock Striping)ConfigurationThreadsTotal OperationsExecution TimeSpeedupUnsharded (Single Mutex)1100,00095 ms1.0xUnsharded (Single Mutex)4100,00098 ms0.97x (Lock Contention)Sharded (4 Shards)4100,00055 ms1.7xSharded (16 Shards)16100,00025 ms3.8xSharded (32 Shards)32100,00016 ms6.0xThreadpool + Optimal Chunking (200 × 500)Hardware100,00015 ms6.3xDurable Disk Logging Throughput (100,000 Durable Writes)WAL StrategyFramingExecution TimePerformance NotesSynchronous Per-Op FlushPlain-Text447 msHigh syscall count; susceptible to delimiter collisionsSynchronous Per-Op FlushBinary712 msRobust binary wire layout; blocked on $5\times$ stream writes per opGroup Commit (Batch Swap + Single Flush)Binary313 ms> 2.2x faster than synchronous binary I/O📡 TCP Protocol SpecificationClients interact with KronosDB over standard TCP connections (port 8081 by default) using newline-terminated text commands:CommandSyntaxResponseDescriptionPINGPINGPONG\nHealthcheck and connection verificationSETSET <key> <value>OK\nStores a key-value pair across memory and WALGETGET <key><value>\n or (nil)\nRetrieves the value of a keyDELDEL <key>OK\nRemoves a key and appends a WAL tombstone📂 Project Structurekronosdb/
├── CMakeLists.txt
├── README.md
├── include/
│   └── kronos/
│       ├── benchmark.hpp     # Multi-threaded throughput & latency suite
│       ├── engine.hpp        # Sharded storage engine & checkpoint coordination
│       ├── server.hpp        # Non-blocking epoll TCP network server
│       ├── threadpool.hpp    # Worker thread pool with condition variables
│       └── wal.hpp           # Binary Write-Ahead Log & Group Commit flusher
├── src/
│   ├── benchmark.cpp
│   ├── engine.cpp
│   ├── main.cpp              # Entry point with signal handling (SIGINT/SIGTERM)
│   ├── server.cpp
│   ├── threadpool.cpp
│   └── wal.cpp
└── tests/
    └── kronos_test.cpp       # GoogleTest integration & regression test suite
🚀 Getting StartedPrerequisitesLinux environment (Kernel 5.x+ for native epoll support)A C++20 compatible compiler (GCC 11+ or Clang 13+)CMake 3.20+build-essentialsudo apt update && sudo apt install -y build-essential cmake
Build# 1. Configure in Release mode
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 2. Build executables and test suite in parallel
cmake --build build -j$(nproc)
Run Unit Tests (GoogleTest)The test suite runs unit and regression tests covering basic operations, binary crash recovery, and multithreaded checkpointing:ctest --test-dir build --output-on-failure
# Or invoke the test binary directly:
./build/kronos_tests
Run the Server./build/kronos_server
Connect via Netcat (nc)From a second terminal:nc localhost 8081
PING
PONG
SET user:42 Andrei Ciocoiu
OK
GET user:42
Andrei Ciocoiu
DEL user:42
OK
GET user:42
(nil)
Pipe one-off commands directly:printf "SET sensor_id 90210\nGET sensor_id\n" | nc -N localhost 8081
🗺️ Completed Milestones[x] Multi-threaded worker pool with std::jthread and non-blocking futures[x] Fine-grained shard lock striping[x] Length-prefixed Binary Write-Ahead Log (WAL) with torn-write detection[x] Asynchronous Group Commit batch swap pipeline[x] Snapshot Checkpointing with atomic rename & WAL compaction[x] Two-phase startup recovery: Snapshot baseline + incremental WAL replay[x] Non-blocking TCP network server (epoll on Linux)[x] Automated test suite integrated with GoogleTest