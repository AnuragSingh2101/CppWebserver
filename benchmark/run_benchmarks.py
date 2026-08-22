import time
import urllib.request
import threading
import sys
import json

# Python script to benchmark our C++ Web Server
# Sends concurrent requests to the server and measures average latency and throughput.

SERVER_URL = "http://localhost:8080/api/info"
NUM_REQUESTS = 1000
CONCURRENT_THREADS = [1, 2, 4, 8]

def send_requests(url, count, latency_list):
    for _ in range(count):
        start = time.perf_counter()
        try:
            with urllib.request.urlopen(url) as response:
                response.read()
                latency = (time.perf_counter() - start) * 1000  # in ms
                latency_list.append(latency)
        except Exception as e:
            pass

def run_benchmark(threads_count):
    latencies = []
    threads = []
    req_per_thread = NUM_REQUESTS // threads_count

    start_time = time.perf_counter()

    for _ in range(threads_count):
        t = threading.Thread(target=send_requests, args=(SERVER_URL, req_per_thread, latencies))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    total_time = time.perf_counter() - start_time
    throughput = len(latencies) / total_time if total_time > 0 else 0
    avg_latency = sum(latencies) / len(latencies) if latencies else 0

    return throughput, avg_latency

if __name__ == "__main__":
    print(f"Starting benchmark on {SERVER_URL} with {NUM_REQUESTS} total requests...")
    print("| Concurrency (Threads) | Throughput (Req/Sec) | Avg Latency (ms) |")
    print("|-----------------------|----------------------|------------------|")

    results = []
    for c in CONCURRENT_THREADS:
        # Note: server should be running on port 8080!
        try:
            throughput, avg_latency = run_benchmark(c)
            print(f"| {c:<21} | {throughput:<20.2f} | {avg_latency:<16.2f} |")
            results.append({
                "concurrency": c,
                "throughput": throughput,
                "latency": avg_latency
            })
        except Exception as e:
            print(f"| {c:<21} | Error connecting to server!                     |")
            break

    # Save results to a json file
    with open("benchmark/results.json", "w") as f:
        json.dump(results, f, indent=4)
