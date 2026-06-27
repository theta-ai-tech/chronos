# Designing a low‑latency C++ engine side project with a Python API and web UI

## What “low‑latency credibility” looks like in a portfolio project
A side project will read as “low‑latency” (rather than “a normal web app with finance flavour”) when it demonstrates *determinism, measurement discipline, and systems thinking*—not just fast code. In practice that means you can point to (a) a clearly defined hot path, (b) evidence that you understand contention and tail latency, and (c) concrete benchmarking/profiling artefacts.

The strongest signal is an architecture where the latency‑critical logic is deliberately kept in a small, testable, allocation‑controlled core that processes events predictably. The canonical illustration of this style is the entity["organization","LMAX","retail trading platform"] approach described by entity["people","Martin Fowler","software author"]: a single‑threaded in‑memory “business logic processor” surrounded by a lock‑avoiding queueing/concurrency mechanism (the Disruptor pattern), achieving very high throughput with low and predictable latency. citeturn0search0turn0search17

For a low‑latency engineering portfolio, you want to show you can *measure* and *explain* latency, not just claim it. A credible project typically includes (1) microbenchmarks for core data structures and matching logic, (2) system‑level profiling, and (3) latency distributions (p50/p95/p99/p99.9) under controlled load. The entity["organization","Linux Foundation","open-source consortium"] ecosystem provides the standard toolchain: `perf` (hardware/software performance counters) and related profiling workflows are the baseline in many performance‑sensitive environments. citeturn5search3turn5search7

Finally, the project should show that you understand that low latency is *end‑to‑end*: kernel/user transitions, wakeups, CPU frequency states, and scheduler interference can dwarf algorithmic improvements. Practical tuning guides focus on reproducible benchmarking and reducing jitter (variance), not only improving averages. citeturn5search6turn5search18


## Candidate project themes that fit your goals
Given your background (15 years in banking; experience at entity["company","Bloomberg","financial data firm"] and entity["company","JPMorgan Chase","global banking group"]) and your explicit goal (keep C++ sharp while using Python for productivity), you want a project whose hot path naturally belongs in C++ and whose surrounding layers naturally belong in Python + web.

Three themes map well to that:

**Exchange / matching‑engine simulator (recommended baseline).**  
A central‑limit‑order‑book simulator forces you to implement data structures and event processing patterns that are recognisably “low latency”: order book, matching rules, event replay, sequencing, and a performance harness. A large fraction of real markets use price‑time priority (FIFO within price). An academic description of this rule is explicit: orders are prioritised first by best price and then by arrival time at that price, following FIFO. citeturn14view0

**Crypto execution & market‑data lab (good for real feeds; higher operational risk).**  
This focuses on feed handling, reconnect logic, throttling, and execution orchestration. entity["company","Coinbase","crypto exchange, us"]’s Advanced Trade offering explicitly supports programmatic trading and order management via REST and real‑time market data via WebSockets, which is convenient for building a realistic “live mode” without institutional connectivity. citeturn1search2turn1search10  
The trade‑off: you can easily end up spending most of your time on exchange‑specific details (rate limits, auth, reconnection edge cases) instead of the C++ latency core.

**Betting exchange market‑making / trading simulator (excellent microstructure; jurisdiction dependent).**  
A betting exchange behaves like a limit order book (back/lay prices and sizes), and the event stream is naturally suited for an event‑driven engine and a real‑time UI. entity["company","Betfair","betting exchange, uk"] documents an Exchange Stream API positioned as low‑latency access to market data (subscribe to and track market/price/order changes). citeturn1search7turn1search11  
The trade‑off: access, account constraints, and market availability vary by country and can be a distraction.

Across all three, the best strategy for a side project is to **start with a self‑contained simulator** (so you can benchmark deterministically), then add *optional* “live connectors” once the engine is solid.


## Recommended project design
The design below is intentionally “portfolio‑shaped”: you’ll end up with something interviewers in low‑latency roles can reason about quickly, while still being useful for your own hypothesis testing.

**Project concept: a deterministic exchange emulator + strategy lab**

At its centre is a C++ engine that implements a **central limit order book (CLOB)** with **price‑time priority** matching. In academic treatment, the matching rule is straightforward: sort by price (best price first), then FIFO for orders at the same price level, and incoming marketable flow executes against highest‑priority resting liquidity. citeturn14view0turn9view1

Around that, you build two modes:

* **Replay/backtest mode (deterministic):** feed the engine an event log (historical market data events, or synthetic flow), let strategies react, produce fills, and record results.
* **Live/paper mode (non‑deterministic):** ingest streaming market data from a source like Coinbase WebSockets (or a betting exchange stream), generate orders, and send them via a gateway.

A key design trick is to make the engine **event‑sourced**: all state changes derive from an ordered event stream, and you can persist/replay the stream to reproduce results. This aligns with a proven low‑latency architecture pattern emphasising in‑memory processing and event sourcing for correctness plus performance. citeturn0search0turn0search17

To make this “feel” like low latency rather than a toy, scope the engine around a *tight hot path*:

* **Input:** a normalised “event” type (new order, cancel, replace, trade print, book snapshot delta).
* **Core:** order validation (risk gates), matching, trade generation, order state updates.
* **Output:** deterministic acknowledgements/fills + incremental book updates.

And to keep it useful for hypothesis testing, make it easy to put strategies on top:

* In replay mode, strategies can run in Python (fast iteration).
* If a strategy becomes performance‑critical, you can port it to C++ or keep the “signal” in Python but enforce that execution decisions are turned into a compact, low‑overhead message to the C++ engine (so Python stays off the hot path by design).

image_group{"layout":"carousel","aspect_ratio":"16:9","query":["central limit order book diagram price time priority FIFO","matching engine architecture diagram low latency trading system","order book depth ladder UI example","event driven trading system architecture diagram"] ,"num_per_query":1}

A useful real‑world comparison point (not as a template, but as validation that your separation makes sense) is research literature on matching engines that explicitly separates order‑generation environments from the matching engine and feed components. For example, the CoinTossX system description highlights binary message encodings over UDP transport (via Aeron) and a separation between the environment generating orders and the order‑book/matching components, to preserve realistic asynchronicity between events. citeturn11view0turn2search7


## Architecture and tech stack blueprint
The stack you described—web app + REST API + Python backend + C++ engine—can be put together in a way that preserves low‑latency learning goals (without turning the whole thing into a distributed-systems science project).

### Web UI and control plane
Use the web tier for *control and observability*, not for the hot path:

* **Web UI:** shows order book depth, trades tape, strategy state, P&L, and—critically—latency histograms.
* **Data push:** WebSockets are the right tool for real‑time UI updates because they provide a full‑duplex channel over a single TCP connection after an HTTP upgrade handshake. That “server can push without polling” property is exactly what you want for streaming book updates. citeturn3search14turn3search5

### Python backend (orchestrator + REST/WS gateway)
For Python, use an ASGI framework so you can support REST plus WebSockets cleanly:

* **FastAPI** positions itself as a high‑performance Python web framework for building APIs using type hints, which is a good match for a small, well‑typed control plane. citeturn3search0
* **Uvicorn** is an ASGI server that explicitly supports HTTP and WebSockets and exists to provide a low‑level server interface for async frameworks; this matches the “gateway” role. citeturn3search1turn3search7

Python responsibilities that fit your goal profile:
* REST endpoints for configuration (instruments, tick size, risk limits, strategy selection).
* Auth, API key management for external venues, persistence, admin actions (start/stop replay).
* WebSocket fan‑out to the UI (book deltas, trades, metrics).

### C++ engine (latency‑critical core)
For the engine layer you want a clear boundary and a small “surface area”:

* **Networking / I/O:** libraries with asynchronous models are typical for performance and clarity; `Boost.Asio` is explicitly a cross‑platform C++ library for network and low‑level I/O programming providing a consistent asynchronous model. citeturn2search2turn2search17
* **Event processing model:** the “single writer + ring buffer” approach is widely referenced in low‑latency architectures, and the Disruptor documentation emphasises reduced fixed costs, improved throughput, and reduced latency by structuring consumer dependency graphs over a single ring buffer rather than lock‑heavy queues. citeturn0search17turn0search0

### Python↔C++ integration options
You have two pragmatic choices, and you can support both with minimal duplication:

**In‑process (Python calls C++ for simulation/backtest).**  
Use `pybind11` to expose the engine as a Python module. The official documentation describes it as a lightweight, header‑only approach to expose C++ types in Python (and interoperate with Python). citeturn0search22turn0search1  
This is great for fast iteration, but you should design your API so Python makes *coarse* calls (e.g., batch events), because crossing the language boundary has overhead and is only worthwhile when substantial work happens in C++. citeturn6search17turn0search22

**Out‑of‑process (C++ daemon; Python as gateway/controller).**  
For a “production‑shaped” architecture, run the engine as its own process. For IPC:
* **gRPC** is built on HTTP/2’s long‑lived connections and supports streaming RPC patterns; its own engineering materials describe this as a basis for robust, performant inter‑service communication, and its documentation includes explicit performance considerations (e.g., concurrent streams and queuing). citeturn2search1turn2search5
* **ZeroMQ** is a brokerless messaging library with a minimalist messaging philosophy; it’s frequently used for concurrent/distributed messaging patterns without standing up a broker. citeturn2search0

For a side project, the strongest narrative is: **pybind11 for deterministic replay/backtest**, and **gRPC (or ZeroMQ) for live mode**, so you demonstrate both “quant research glue” and “production‑like service boundaries”.


## Performance engineering and how to prove it
To make the project genuinely useful for low‑latency roles, treat “measurement” as a first‑class subsystem.

### Microbenchmarks and regression control
Use a microbenchmark harness for the hot path data structures and algorithms (e.g., add order, cancel, match, best bid/ask updates). The entity["company","Google","technology company"] `benchmark` library provides a standard microbenchmark framework with a documented user guide and reproducible benchmark execution. citeturn0search3turn0search6  
A good portfolio outcome is a benchmark suite that runs in CI and fails if latency regressions exceed thresholds.

### Profiling and hardware counters
`perf` is explicitly designed as a front end to Linux performance counters, covering CPU/PMU and tracepoints, and Red Hat’s documentation frames it as a tool to measure/record hardware and software events. citeturn5search3turn5search7  
In low‑latency interviews, being able to show “I used perf to confirm cache misses/branch mispredicts correlate with p99 spikes” is often more compelling than raw throughput numbers.

### OS and system‑level tuning (optional, but high signal)
Low‑latency tuning guides emphasise that the goal is predictable behaviour. One detailed public guide focuses on tuning AMD64/x86_64 Linux systems for low‑latency workloads and accurate benchmarking, explicitly calling out kernel‑bypass networking and benchmark reproducibility as target scenarios. citeturn5search6turn15search10  
entity["company","Intel","semiconductor company"] similarly frames latency tuning as both hardware configuration and application tuning, highlighting the need for checklists and systematic configuration. citeturn5search18

If you want a credible “advanced step”, include a documented experiment set:
* baseline vs CPU pinning / isolation,
* baseline vs different I/O strategies,
* baseline vs allocation‑free object pools.

For CPU isolation, Red Hat’s real‑time documentation describes isolating CPUs by removing user threads, unbound kernel threads, and interrupts from specific cores (via IRQ affinity), and automating this with tuned profiles. citeturn15search2

### Network and I/O experimentation path
You don’t need kernel bypass to make this project valuable, but you can design an “upgrade path”:

* **io_uring:** the Linux man page describes it as an async I/O interface using shared ring buffers between user and kernel space for efficient I/O submission/completion. citeturn0search2turn0search11  
  The original io_uring introductory paper (by Jens Axboe) presents it as the newer Linux I/O interface and explains why it exists and how it works at a high level. citeturn9view0
* **DPDK / kernel bypass (stretch goal):** cloud documentation describes DPDK as bypassing the kernel network stack with user‑space packet processing to achieve low latency and consistent performance for packet‑intensive applications. citeturn15search4turn15search0

A practical portfolio approach is: keep the first version on TCP + epoll/Asio, and add io_uring as an optional transport backend once your core engine is stable and benchmarked.


## Roadmap and portfolio-ready deliverables
The best way to keep this feasible as a side project is to build in layers where every layer adds a “demonstrable asset” (benchmarks, UI, docs), not just features.

### Engine-first milestones
Build the C++ core as if it will be reused:

1) **CLOB + matching engine with price‑time priority (FIFO).**  
Ground the implementation in explicit behaviour: price priority first, then FIFO within the same price, as described in the literature. citeturn14view0

2) **Deterministic replay harness + event log.**  
Use an append‑only event stream so runs are reproducible (a pattern strongly associated with the LMAX-style event‑sourced architecture). citeturn0search0turn0search17

3) **Benchmark suite + perf profile scripts.**  
Document how to run `google/benchmark` and how to collect `perf` counters/flame graphs. citeturn0search6turn5search3

### Python lab milestones
Once the engine is stable:

4) **Python strategy interface (batch event processing).**  
Expose “submit batch of events → receive fills + deltas” rather than per‑event calls to reduce boundary overhead, consistent with the reality that crossing Python/C++ boundaries has non‑trivial conversion and call costs. citeturn0search22turn6search17

5) **Backtesting realism features: bias and cost model hooks.**  
Even if this is primarily a systems project, adding basic “avoid fooling yourself” mechanics makes it more useful for hypothesis testing:
* Look‑ahead bias occurs when backtests use information that would only be available after the decision point, and survivorship bias can overstate performance by excluding delisted/bankrupt assets. citeturn9view2
* Your architecture should make transaction cost and slippage models pluggable (even if simplistic), because backtest results can be materially distorted otherwise. citeturn4search9turn4search4

### Web + observability milestones
6) **FastAPI control plane + WebSocket streaming UI.**  
Use FastAPI for REST configuration and Uvicorn to serve the ASGI app, streaming updates to the UI via WebSockets (full‑duplex channel suitable for real‑time events). citeturn3search0turn3search1turn3search14

7) **Latency telemetry dashboard.**  
This is where the project becomes “low‑latency credible”: display end‑to‑end (event ingest → match → publish) latency distributions and correlate spikes with counter metrics captured via `perf`. citeturn5search7turn5search3

### Optional “live connector” milestones
8) **Crypto or betting connector as a plug‑in.**  
If you choose crypto first: Coinbase’s Advanced Trade docs explicitly support REST + WebSocket for trading and market data, making it a straightforward venue to add a live feed. citeturn1search2turn1search10  
If you choose betting: Betfair positions its exchange streaming as low‑latency and suitable for efficiently tracking market updates. citeturn1search7turn1search11  
Treat the connector as *non‑critical path* and keep the C++ engine interface stable.

### Safety, controls, and “real trading system shape”
Even for a personal project, adding risk controls makes your design more realistic and aligns with regulatory expectations around automated access:

* entity["organization","U.S. Securities and Exchange Commission","us securities regulator"] Rule 15c3‑5 was adopted to ensure broker‑dealers implement risk management controls for market access, motivated by the risks created by automated electronic trading and direct/sponsored access. citeturn9view3
* entity["organization","ESMA","eu securities regulator"]’s 2026 supervisory briefing emphasises governance, testing, and pre‑trade controls under MiFID II as key supervisory focus areas for algorithmic trading. citeturn9view4
* The entity["organization","Financial Conduct Authority","uk regulator"] handbook includes dedicated systems-and-controls content for algorithmic trading. citeturn4search3turn4search14

You can mirror this “shape” without building a full compliance framework by implementing:
* pre‑trade limits (max order size, max notional, max order rate),
* kill switch,
* mandatory dry‑run/paper mode for new strategies,
* audit log of all actions/events.

Those features both protect you and make the system read as “production‑adjacent” rather than purely academic.