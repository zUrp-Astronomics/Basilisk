# Design doctrine

[Français](DOCTRINE.md)

The criterion is a device doing its job at three in the morning in a field, without its author on call. Engineering effort goes into predictable behavior, including lost communication, failed storage and interrupted motion.

## State the invariant

Before changing code, identify the invariant, its owner and the events that can invalidate it. Derived data needs an identified source and an explicit lifetime. Each boundary must establish what its consumers may rely on; repeating the same validation does not establish that contract.

Representations should prevent contradictory states as far as the language permits. Missing or stale data is not a plausible measurement. An uncertain transmission is not a definite rejection. Admitting a move is not reaching its target.

## Pay for simplicity

Remove, then simplify, then shorten. An abstraction waits for its second concrete use; a dependency must justify the code it brings. The firmware depends on ESP-IDF; development tools have their own dependencies.

A guard defends a named, demonstrable case. Passing tests after its removal do not prove it unnecessary: the tests may miss that case. Complexity paid for by a real incident carries knowledge that must survive a redesign.

## Keep the boundaries honest

The USB contract is [PROTOCOL.md](PROTOCOL.md). The `bench_core` transmit allowlist is the last barrier before the lens; it is not an optional diagnostic setting. The Samyang AF 135 Custom exception is explicit, bounded and documented. A read path must not claim that a proprietary firmware can never cause persistent effects.

Internal contracts live near the code: lifetime, allowed calls, value origins and hardware context. Comments explain decisions the statements alone cannot express. They do not narrate the agent's session.

## Verify without inflating the evidence

Make one coherent change, run relevant checks and record readable results. Suites, simulations and mutation checks seek faults and blind spots; they do not replace hardware measurements. Private static analysis does not become an observation because a simulator reproduces it.

A known defect stays named until addressed. Reports identify the revision they assess. Engineering discipline is visible in those contracts and checks; it needs neither heroic comparisons nor an invented certification.
