# KronosDB ⚡

**KronosDB** is a high-performance, multithreaded key-value storage engine built from scratch in **Modern C++20**. 

Designed with a focus on low-latency concurrency and crash-resilient durability, KronosDB combines fine-grained in-memory lock striping, a custom zero-idle-CPU worker thread pool, and an asynchronous **Group Commit Binary Write-Ahead Log (WAL)**.

---

## 🏗️ Architecture & Key Features

### 1. Sharded In-Memory Storage Engine (Lock Striping)
Instead of protecting a single global hash map with one coarse-grained mutex—which serializes all threads under heavy contention—KronosDB partitions the keyspace across **$N$ independent shards** (`std::vector<std::unique_ptr<Shard>>`).
* Keys are deterministically routed via `std::hash<std::string>{}(key) % shard_count`.
* Operations on different shards execute in true hardware parallelism without blocking one another.

### 2. Custom Asynchronous Thread Pool
Rather than spawning and destroying OS threads per request, KronosDB dispatches work through a custom thread pool built on C++20 `std::jthread`:
* **Zero-CPU Idle Waiting:** Worker threads sleep on a `std::condition_variable` when the queue is empty, consuming `0%` CPU until woken by `notify_one()`.
* **Non-Blocking Futures:** Supports generic task submission returning `std::future<T>` via `std::packaged_task` for asynchronous result retrieval.

### 3. Binary Write-Ahead Log (WAL) & Crash Recovery
To guarantee durability across restarts and power failures without suffering from text-delimiter parsing bugs (such as embedded newlines or spaces), KronosDB implements a custom **length-prefixed binary wire format**:

```text
+---------------+-------------------+--------------+---------------------+----------------+
| OpCode (1 B)  | Key Length (4 B)  | Key Data     | Value Length (4 B)  | Value Data     |
| uint8_t       | uint32_t          | Raw Bytes    | uint32_t            | Raw Bytes      |
+---------------+-------------------+--------------+---------------------+----------------+
```

* **Tombstone Deletions:** `DEL` operations append lightweight tombstone records (`OpCode = 2` + `Key Length` + `Key Data`), avoiding expensive random disk seeks.
* **Torn-Write Protection:** During startup recovery, the engine validates stream state bits after every frame segment, safely halting recovery if it encounters a truncated record caused by a sudden power loss.

### 4. Group Commit Pipeline
Disk I/O is orders of magnitude slower than RAM. To prevent individual threads from stalling on synchronous per-operation disk flushes:
1. Worker threads append `LogRecord` items containing a `std::promise<bool>` into a staging queue and block on the corresponding `std::future<bool>`.
2. A dedicated background flusher thread wakes up, performs an **$O(1)$ pointer swap** (`std::deque::swap`) to drain the entire pending queue into a local batch, and immediately releases the queue mutex.
3. The flusher writes all batched records sequentially, executes a **single** `.flush()` system call for the entire batch, and fulfills all caller promises simultaneously.

---

## 📊 Performance Benchmarks

Benchmarked on **100,000 write operations** across varying concurrency models and durability configurations:

### In-Memory Concurrency Scaling (Lock Striping)
| Configuration | Threads | Total Operations | Execution Time | Speedup |
| :--- | :---: | :---: | :---: | :---: |
| **Unsharded (Single Mutex)** | 1 | 100,000 | `95 ms` | 1.0x |
| **Unsharded (Single Mutex)** | 4 | 100,000 | `98 ms` | 0.97x *(Lock Contention)* |
| **Sharded (4 Shards)** | 4 | 100,000 | `55 ms` | 1.7x |
| **Sharded (16 Shards)** | 16 | 100,000 | `25 ms` | 3.8x |
| **Sharded (32 Shards)** | 32 | 100,000 | `16 ms` | **6.0x** |
| **Threadpool + Optimal Batching (200 × 500)** | Hardware | 100,000 | **`15 ms`** | **6.3x** |

### Durable Disk Logging Throughput (100,000 Durable Writes)
| WAL Strategy | Framing | Execution Time | Notes |
| :--- | :---: | :---: | :--- |
| **Synchronous Per-Op Flush** | Plain-Text | `447 ms` | Vulnerable to newline/delimiter corruption |
| **Synchronous Per-Op Flush** | Binary | `712 ms` | Safe framing, high per-op syscall overhead |
| **Group Commit (Batch Swap + Single Flush)** | Binary | **`313 ms`** | **> 2.2x faster** than synchronous binary I/O |

---

## 📂 Project Structure

```text
kronosdb/
├── CMakeLists.txt
├── include/
│   └── kronos/
│       ├── benchmark.hpp    # Multi-threaded benchmarking suite
│       ├── engine.hpp       # Sharded key-value storage engine
│       ├── threadpool.hpp   # Asynchronous worker thread pool
│       └── wal.hpp          # Binary Write-Ahead Log & Group Commit
└── src/
    ├── benchmark.cpp
    ├── engine.cpp
    ├── threadpool.cpp
    ├── wal.cpp
    └── main.cpp             # Entry point & usage examples
```

---

## 🚀 Getting Started

### Prerequisites
* A C++20 compatible compiler (`GCC 11+`, `Clang 13+`, or `MSVC 2022+`)
* `CMake 3.20+`

### Build & Run

```bash
# 1. Clone the repository
git clone https://github.com/yourusername/kronosdb.git
cd kronosdb

# 2. Configure and build in Release mode for accurate benchmarks
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 3. Run the executable
./build/kronos_server
```