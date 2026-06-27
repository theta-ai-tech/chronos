# ADR-0002: Hosting and benchmark environment

- Status: Accepted
- Date: 2026-06-27

## Context

Chronos is local-first: the default system runs on one operator machine without
requiring cloud infrastructure. Two additional needs sit on top of that:

1. **Credible latency evidence.** The project is presented to a quantitative trading
   audience. Latency distributions (p50/p95/p99/p99.9) and tail behavior are part of the
   product. The performance plan already states that noisy shared CI is not a source of
   performance truth — and a general-purpose shared cloud VM is exactly that noise:
   hypervisor jitter, noisy neighbors, and frequency scaling make tail numbers
   meaningless and, worse, make them *look* dishonest to an expert audience.

2. **An always-on demo.** Someone should be able to open a URL and click through the
   web console and a replay without the author present.

These two needs have opposite environment requirements, so they are hosted separately.

## Decision

1. **Benchmark host of record: dedicated bare metal.** All headline latency, throughput,
   and tail numbers are produced on a dedicated, non-virtualized machine — the author's
   own controlled hardware or a bare-metal rental (e.g. Hetzner dedicated, Latitude.sh,
   Equinix Metal). The benchmark environment uses isolated and pinned CPU cores
   (`isolcpus`/`cpuset`, IRQ affinity away from hot cores), the performance CPU governor,
   disabled or characterized turbo/SMT, and no co-tenant workloads. Every benchmark run
   records its reference-environment manifest (hardware, OS, runtime versions, governor,
   isolation settings) alongside the workload manifest, per the performance plan. This
   host is the *only* source of numbers used in any claim about Chronos latency.

2. **Always-on demo: a small cloud VM.** The web console and a packaged replay run on a
   modest cloud VM (or container) for availability. This host serves the showroom: UI,
   replay playback, recorded/illustrative metrics.

3. **The demo is labeled non-authoritative for performance.** The console clearly marks
   when it is running on the demo host and that its live timing is illustrative, not the
   benchmark of record. No number sourced from the demo VM may be presented as a Chronos
   latency result. This mirrors the architecture rule that degraded or non-authoritative
   surfaces must be explicit and may not be dressed up as success.

4. **Local-first remains the default.** Bare-metal and cloud hosting are deployments of
   the same artifact, selected by static configuration; neither introduces a new domain
   meaning, new authority, or distributed-infrastructure assumption. Co-located /
   low-latency-to-venue hosting for live trading is explicitly out of scope here and, if
   ever pursued, is a separate ADR (it changes the threat model and the latency
   semantics).

## Consequences

- Latency claims are reproducible and defensible to a hedge-fund audience because they
  come from a controlled, documented environment rather than a shared VM.
- Two environments must be maintained. The bare-metal host need not be always-on — it is
  spun up for benchmark campaigns and recorded. The cloud VM is always-on but cheap.
- Benchmark automation must capture and version the reference-environment manifest so a
  result can be reproduced and audited, consistent with the performance-budget process.
- The build must produce a deployable artifact that runs in both places with only static
  configuration differences (paths, ports, telemetry destinations, isolation settings).

## Related

- `planning/02-observability/performance-and-slo-method.md` — measurement mechanics,
  reference-environment manifests, regression policy.
- `planning/01-architecture/architecture.md` — local-first constraint, static-config
  restart semantics, "degradation must be visible."
- ADR-0001 — the C++ hot path whose numbers this environment certifies.
