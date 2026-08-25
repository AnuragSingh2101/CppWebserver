# Resume Optimization & Interview Defense: C++ HTTP/1.1 Web Server

This document contains ATS-optimized resume bullet points, verified project facts, metrics analysis, and interview preparation questions based on the C++ HTTP/1.1 Multi-Threaded Web Server codebase.

---

## 1. Project Title Options

*   **Multithreaded HTTP/1.1 Web Server** *(Recommended: Covers systems, concurrency, and networking aspects)*
*   **C++ HTTP/1.1 Server & REST API**
*   **High-Performance C++ Web Server**
*   **Concurrent HTTP/1.1 Server with Thread Pool**
*   **Low-Level C++ HTTP Server**

---

## 2. Technology Stack

```text
C++17 | Winsock2 | TCP/IP | HTTP/1.1 | Multithreading | REST API | CMake | Google Test
```

---

## 3. Verified Project Facts

| Category | Verified Fact |
| :--- | :--- |
| **Language** | C++17 configuration (`set(CMAKE_CXX_STANDARD 17)` in `CMakeLists.txt`). |
| **Networking** | Winsock2 TCP socket lifecycle (`WSAStartup`, `socket`, `bind`, `listen`, `accept`, `recv`, `send`, `closesocket`). |
| **HTTP Support** | HTTP/1.1 request-line and header parsing (extracts method, path, headers, query params, and body). Supports HTTP Keep-Alive connection timeout loop. |
| **Concurrency** | Custom `ThreadPool` with worker threads synchronized via `std::mutex` and `std::condition_variable`. |
| **REST API** | Endpoint dispatching in `RouteHandler` for `/api/users` and `/api/info` supporting `GET`, `POST`, `PUT`, `PATCH`, `DELETE`, and `OPTIONS`. |
| **Storage** | In-memory `UserStore` protected by a `std::mutex` for thread-safe user CRUD operations. |
| **MIME Parsing** | MIME type lookup based on file extensions (`.html`, `.css`, `.js`, `.json`, `.png`, etc.) in `Router::getMimeType`. |
| **Security** | Rejection of directory traversal attacks (`..` or `%2e%2e` encoding checks), hidden files block (`.env`), request size limits (max 16KB headers, 1MB body), and duplicate header validation. |
| **Testing** | **16 unit and integration tests** in **5 suites** (`ThreadPoolTest`, `UserStoreTest`, `HttpResponseTest`, `HttpRequestTest`, `RouterTest`) using **Google Test**. |
| **Build System** | CMake configuring and building targets, downloading `nlohmann/json` and `googletest` automatically via `FetchContent`. |

---

## 4. Measured Metrics

| Metric | Value | How it was measured / Evidence |
| :--- | :--- | :--- |
| **Requests/sec** | **NOT MEASURED** | Benchmark script (`benchmark/run_benchmarks.py`) exists but no results are saved in the repo. |
| **Concurrent connections** | **NOT MEASURED** | Benchmark script is implemented but results are not saved. |
| **P99 Latency** | **NOT MEASURED** | Benchmark script is implemented but results are not saved. |
| **Test Count** | **16 tests** | Built and executed [`build/webserver_tests.exe`](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/tests). |
| **Test Pass Rate** | **100%** | All 16 automated tests passed during local verification run. |
| **Lines of Code** | **~2,700 LOC** | Calculated from C++ source files (`src/` and `tests/` directories). |
| **Executable Size** | **~570 KB** | Release build output for `webserver.exe` in the root folder. |

---

## 5. Resume Bullet Point Versions

### Version A — General Software Engineering

*   **Designed and implemented a C++17 HTTP/1.1 server** using Winsock2, establishing low-level TCP connections, custom request parsing, static-file serving, content-type mapping, and directory traversal security filtering. *(27 words)*
*   **Built a custom Thread Pool with a thread-safe task queue** and condition variables, delegating incoming client sockets to worker threads to process requests concurrently and shut down gracefully. *(28 words)*
*   **Engineered a RESTful user management API with thread-safe in-memory storage** and validated all API logic, concurrency safety, and request parsing through a 16-test Google Test suite. *(27 words)*

### Version B — C++ / Systems (Recommended)

*   **Engineered a multi-threaded HTTP/1.1 server in C++17** from scratch using low-level Winsock2 APIs, managing TCP sockets, connection timeouts, and binary-safe file streaming. *(24 words)*
*   **Developed a fixed-size Thread Pool utilizing worker threads**, standard synchronization primitives (`std::mutex`, `std::condition_variable`), and atomic flags to process network tasks concurrently and shut down gracefully. *(28 words)*
*   **Optimized systems reliability** by mitigating directory traversal exploits, capping request sizes (16KB header, 1MB body), and verifying concurrency stability using a 16-test Google Test suite. *(26 words)*

### Version C — Backend Software

*   **Built an HTTP/1.1 server using C++17 and Winsock2**, featuring relative path routing, content MIME detection, and binary file streaming for static asset serving. *(24 words)*
*   **Designed a RESTful User API supporting CRUD operations** (GET, POST, PUT, PATCH, DELETE) with in-memory thread-safe storage and strict JSON schema and email validation. *(25 words)*
*   **Hardened API security** by enforcing request size limits, blocking path traversal, and executing automated unit/integration tests with Google Test to verify routing and parser correctness. *(26 words)*

---

## 6. ATS Keywords

*   **Programming**: C++, C++17, STL, OOP, RAII, Mutex, Atomic, Memory Management
*   **Networking**: Winsock2, TCP/IP, Sockets, HTTP/1.1, HTTP Parsing, Keep-Alive, Sockets API
*   **Concurrency**: Multithreading, Thread Pool, Task Queue, Synchronization, Condition Variable, Race Condition Mitigation
*   **Backend**: REST API, CRUD, JSON Parsing, HTTP Methods, URI Routing, Data Validation, MIME Detection
*   **Testing & Tools**: Unit Testing, Integration Testing, Google Test (GTest), CMake, Git, Benchmarking

---

## 7. Recommended Version

**Recommendation: Version B — C++ / Systems Resume Version**

**Why**: Building a web server from scratch in C++ on Windows using Winsock2 is a **low-level systems and networking challenge**. Recruiters looking at C++ projects expect to see socket mechanics, threading primitives, resource management (RAII), and synchronization. Version B highlights your control over the network card interfaces, thread lifecycles, and OS-level security policies, making it stand out as a highly technical systems project.

---

## 8. Instructions to Collect Missing Metrics

To replace **NOT MEASURED** with real, impressive resume metrics, perform the following steps to execute the benchmark:

### Step 1: Fix Localhost DNS Overhead
Windows DNS resolver adds a substantial overhead when resolving `localhost`. Modify the target URL in [`benchmark/run_benchmarks.py`](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/benchmark/run_benchmarks.py#L10) from:
```python
SERVER_URL = "http://localhost:8080/api/info"
```
to:
```python
SERVER_URL = "http://127.0.0.1:8080/api/info"
```
This forces Python to bypass the DNS lookup and connect directly over loopback.

### Step 2: Run the Benchmark
1.  Start the C++ web server:
    ```bash
    .\\build\\webserver.exe
    ```
2.  In another terminal, run the benchmark script:
    ```bash
    python benchmark/run_benchmarks.py
    ```
3.  Record the **Throughput (Req/Sec)** and **Avg Latency (ms)** printed in the table for each thread count (1, 2, 4, and 8).

### Step 3: Add to your Resume
You can now update the third bullet in your resume:
> "Benchmarked server throughput scaling from **X req/sec** (single thread) to **Y req/sec** (8 worker threads) with an average latency of **Z ms**."

---

## 9. Interview Defense Q&A

### Thread Pool & Concurrency
1.  **Why did you use a Thread Pool instead of spawning a new thread per connection?**
    *   *Defense:* Spawning threads is expensive due to OS allocation overhead and stack reservation. Under high load, spawning a thread per connection leads to thread exhaustion, CPU context-switching overhead, and eventual server crash. A thread pool caps thread creation and reuses existing workers, ensuring predictable resource usage.
2.  **How does your Thread Pool task queue prevent race conditions?**
    *   *Defense:* The queue is shared among all worker threads. We protect it using `std::mutex` and synchronize access with `std::condition_variable`. Before pushing a task, the main thread locks the queue. Workers wait on the condition variable, releasing the mutex while sleeping, and are woken up thread-safely via `notify_one()` when a new task is pushed.

### Winsock2 & TCP
3.  **What is the difference between `listen()` and `accept()`?**
    *   *Defense:* `listen()` puts the socket in a passive listening state, specifying the backlog queue size (max pending TCP connections). It does not establish connections. `accept()` is a blocking call that pulls the first completed connection from the OS backlog queue and returns a new socket descriptor dedicated exclusively to communicating with that specific client.
4.  **Why can `recv()` return partial data, and how does your server handle it?**
    *   *Defense:* TCP is a stream-oriented protocol, not packet-oriented; it does not guarantee that headers arrive in a single block. We handle this in `receiveHttpRequest` by calling `recv()` inside a loop, appending incoming bytes to a buffer, and checking for the double CRLF boundary (`\r\n\r\n`) to identify the end of HTTP headers before proceeding.

### HTTP Parsing
5.  **How does your server handle the request body and verify its completion?**
    *   *Defense:* Once the headers are parsed, the server extracts the `Content-Length` header. It checks the body size currently in the buffer. If it's less than `Content-Length`, it enters a loop calling `recv()` until the bytes received match the specified content length. If `Content-Length` exceeds 1MB, it immediately rejects the request with a `413 Payload Too Large`.
6.  **What is HTTP Keep-Alive, and how did you implement it?**
    *   *Defense:* Keep-alive keeps the TCP socket open after serving a request to allow subsequent requests, avoiding TCP handshake overhead. We check if the client sends `Connection: keep-alive` (or HTTP/1.1 default). If yes, we keep the socket loop active and read the next request, enforcing a receive timeout (`SO_RCVTIMEO`) of 5 seconds to prevent clients from hanging open indefinitely.

### REST API & Thread Safety
7.  **What makes your `UserStore` thread-safe?**
    *   *Defense:* The `UserStore` is a static resource accessed by multiple worker threads concurrently. We protected all database operations (inserts, updates, lookups, and deletions) by securing a `std::mutex` (`storeMutex`) using RAII locks (`std::lock_guard`). This prevents concurrent threads from modifying the vector layout simultaneously, avoiding memory corruption.
8.  **Why does your API return HTTP status code 201 for POST, 204 for DELETE, and 409 for duplicates?**
    *   *Defense:* 201 indicates a resource was successfully created on the server; 204 (No Content) indicates successful execution where no response body needs to be returned; 409 (Conflict) is used when a request violates database integrity constraints, such as duplicate unique emails.

### Security
9.  **How did you prevent directory traversal attacks?**
    *   *Defense:* We sanitize the URL path by resolving it to a canonical representation using `std::filesystem::weakly_canonical` and comparing it with the canonical path of the root directory. If the requested path does not start with the document root prefix, or if the string contains `..` after decoding, we block it and return `403 Forbidden`.
10. **Why did you check for `%2e%2e` instead of just `..` in the request path?**
    *   *Defense:* Percent encoding can be used to bypass naive string matching (e.g. `%2e%2e` represents `..` in URL encoding). Our router runs a decoder first (`Router::urlDecode`) to decode hex representations before performing prefix validations, ensuring that obfuscated directory traversal strings are correctly intercepted.
