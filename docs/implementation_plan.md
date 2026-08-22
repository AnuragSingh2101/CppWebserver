# C++ HTTP Web Server Enhancement Plan

We will systematically extend, refactor, and harden the C++ HTTP/1.1 Web Server on Windows (Winsock2) to transition it from a simple prototype into a highly secure, concurrent, and robust production-ready systems project.

## User Review Required

> [!IMPORTANT]
> **No Thread-Per-Connection:** We will replace the current detached thread model (`std::thread(...).detach()`) with a fixed-size `ThreadPool` to cap system resource consumption and enable thread joining on shutdown.
> **Graceful Shutdown Integration:** On Windows, we will use `SetConsoleCtrlHandler` to capture termination signals (Ctrl+C, Console Close). This requires closing the listening socket synchronously from the signal thread to wake up the blocked main thread in `accept()`.

---

## Proposed Changes

### Component 1: Concurrency & Lifecycle
We will introduce a thread pool and graceful shutdown support to eliminate detached threads.

#### [NEW] [thread_pool.h](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/concurrency/thread_pool.h)
#### [NEW] [thread_pool.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/concurrency/thread_pool.cpp)
*   Implements a classic task queue with worker threads waiting on `std::condition_variable`.
*   Includes safety features for shutdown synchronization.

#### [MODIFY] [main.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/main.cpp)
*   Replaces the accept loop with a thread-pool-based enqueue model.
*   Implements `SetConsoleCtrlHandler` for console control signals.
*   Enables command-line configuration for `--port`, `--threads`, and `--root` using robust numeric parsing.

---

### Component 2: Robust HTTP Parsing & Keep-Alive
We will overhaul network reading to safely buffer bytes, support Keep-Alive, and validate protocol boundaries.

#### [MODIFY] [http_request.h](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/http_request.h)
#### [MODIFY] [http_request.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/http_request.cpp)
*   Case-insensitive header storage and lookups.
*   Validation for duplicate `Host` headers or missing `Host` headers (required by HTTP/1.1).
*   Tracks leftover content in request reading buffers to support consecutive Keep-Alive requests.

#### [MODIFY] [main.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/main.cpp) (Request Reader)
*   Refactors `receiveHttpRequest` to accept a persistence buffer for Keep-Alive.
*   Reads socket data until finding `\r\n\r\n` header delimiters, parses `Content-Length`, and then reads the exact body size.
*   Validates Content-Length: rejects duplicate values, negative numbers, numeric overflow, or bodies exceeding `MAX_BODY_SIZE` (1 MB) with `400 Bad Request` or `413 Payload Too Large`.

#### [NEW] [http_status.h](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/http_status.h)
*   Centralizes status codes (200, 201, 204, 400, 403, 404, 405, 408, 409, 413, 415, 500) and reason phrases.

---

### Component 3: Security & Path Normalization
We will harden path handling to prevent all variants of directory traversal and secure files.

#### [MODIFY] [router.h](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/router.h)
#### [MODIFY] [router.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/router.cpp)
*   Implements URL-decoding helper.
*   Uses `std::filesystem::canonical` and `std::filesystem::weakly_canonical` to resolve paths relative to the document root.
*   Verifies that the target path remains inside the canonicalized public document root.
*   Replaces the basic hardcoded router with a dynamic lookup system supporting standard MIME types: `.html`, `.htm`, `.css`, `.js`, `.json`, `.txt`, `.png`, `.jpg`, `.jpeg`, `.gif`, `.svg`, `.ico`, fallback `application/octet-stream`.

---

### Component 4: REST API & JSON Processing
We will replace the fragile manual JSON parsing with `nlohmann/json` and standardize the API response format.

#### [MODIFY] [route_handler.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/route_handler.cpp)
*   Integrates `nlohmann/json` library (fetched via CMake FetchContent).
*   Implements path-parameter routing `/api/users/{id}` (e.g. `/api/users/5`) while preserving backwards-compatibility with `/api/users?id=5`.
*   Standardizes REST status codes (e.g., returning `201 Created` for POST, `204 No Content` for DELETE, `409 Conflict` for duplicate emails, `405 Method Not Allowed` with an `Allow` header).
*   Validates field types (e.g. name/email must be strings, ID must be a positive integer) and limits string lengths.
*   Standardizes JSON error responses containing specific code identifiers (`INVALID_JSON`, `USER_NOT_FOUND`, etc.).

#### [MODIFY] [user_store.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/user_store.cpp)
*   Implements duplicate email checks and protects internal database operations.

---

### Component 5: Metrics & Logging
We will track server metrics and output structured logs.

#### [NEW] [metrics.h](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/metrics.h)
*   Thread-safe server metrics tracking requests, connection counts, and data volumes.
*   Exposes metrics at `GET /api/info`.

#### [MODIFY] [logger.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/src/logger.cpp)
*   Enhances logging format to output HTTP method, path, status code, body size, and processing duration in milliseconds.

---

### Component 6: Testing & Benchmarking
We will integrate a test suite and create benchmark configurations.

#### [NEW] [tests/http_request_tests.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/tests/http_request_tests.cpp)
#### [NEW] [tests/router_tests.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/tests/router_tests.cpp)
#### [NEW] [tests/user_store_tests.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/tests/user_store_tests.cpp)
#### [NEW] [tests/api_tests.cpp](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/tests/api_tests.cpp)
*   Unit and integration test suites using GoogleTest (integrated in CTest).
*   Includes regression tests for directory traversal (regular, percent-encoded, absolute paths), concurrent request load, malformed JSON, duplicate email, and keep-alive.

#### [NEW] [benchmark/README.md](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/benchmark/README.md)
#### [NEW] [benchmark/results.md](file:///c:/Users/hp/OneDrive/Desktop/cpp/cpp-webserver/benchmark/results.md)
*   Includes results for baseline scaling comparisons (1, 2, 4, 8 worker threads).

---

## Verification Plan

### Automated Tests
*   We will run CTest to compile and execute all tests:
    ```bash
    ctest --test-dir build -C Release --output-on-failure
    ```

### Manual Verification
*   Test with standard web browsers and run `curl` requests targeting user endpoints, invalid paths, and traversal attacks.
*   Validate keep-alive connections via `curl -H "Connection: keep-alive"`.
*   Validate signal interruption handling using `Ctrl+C`.
