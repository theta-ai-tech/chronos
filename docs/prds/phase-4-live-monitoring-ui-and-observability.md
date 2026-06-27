# Phase 4 PRD: Live Monitoring, UI, And Observability

## Goal
Make Chronos usable as a local monitoring and research product by exposing live state, signals, paper positions, and system performance through a UI.

## Scope
- Local web UI
- Replay controls
- Live monitoring views
- Signal review and explanation views
- Latency and pipeline health views
- Alerts for live monitoring

## Requirements
- The UI must support both replay mode and live read-only monitoring mode.
- The UI must show top-of-book, recent depth, recent trades, active signals, and suggested paper positions.
- The UI must show latency histograms, throughput, and queue or pipeline health.
- The UI must let the user select instruments, enable or disable strategies, and tune thresholds.
- The system must support low-burden live monitoring without requiring heavy operational maintenance.

## Deliverables
- Web UI
- Control-plane endpoints
- Replay controls
- Monitoring dashboards
- Signal explanation panel
- Alerting surface for operator review

## Out Of Scope
- Hosted multi-user product
- Real execution workflows

## Exit Criteria
- A local operator can monitor a few instruments live.
- A local operator can replay sessions and inspect strategy behavior.
- System performance and signal behavior are visible in one place.
