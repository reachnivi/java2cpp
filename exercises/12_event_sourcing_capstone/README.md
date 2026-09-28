# 12 — Capstone: a mini Gringofts, and your first "feature"

**Time:** 3 h · **Gringofts link:** this is a compressed model of
`src/infra/es/*` + `src/app_demo/*`:

```
 gRPC RequestReceiver ──► BlockingQueue<shared_ptr<Command>> ──► CommandProcessLoop
                                                                   │ verifyCommand()
                                                                   │ Handler::process(state, cmd, &events) -> ProcessHint
                                                                   ▼
                                               CommandEventStore (Raft log: replicated, persisted)
                                                                   │
                                                                   ▼
                                               EventApplyLoop: StateMachine::applyEvent(event)
      recovery on restart = replay every persisted event into a fresh StateMachine
```

| mini (this exercise) | real Gringofts |
|---|---|
| `Command`, `IncreaseCommand` | `src/infra/es/Command.h`, `app_demo/.../domain/IncreaseCommand.h` |
| `Event`, `ProcessedEvent` | `src/infra/es/Event.h`, `app_demo/.../domain/ProcessedEvent.h` |
| `StateMachine::processCommand` | `app_demo/execution/IncreaseHandler.cpp` |
| `StateMachine::applyEvent` | `app_demo/execution/IncreaseApplier.cpp`, `AppStateMachine.cpp` |
| `decodeEvent` | `app_demo/.../domain/EventDecoderImpl.cpp` |
| `EventLog` | `CommandEventStore` / `RaftCommandEventStore` |
| `App::recover` | the replay logic in `EventApplyLoop` on startup |

The `Increase` path is **fully implemented** — read it top to bottom first.
The invariant everything rests on: **`applyEvent` must be deterministic** —
no clocks, randomness, or I/O — because every replica (and every restart)
replays the same events and must reach the identical state.

## Your feature: `DecreaseCommand`
Add a new command the way you would in Gringofts (in `src/mini_es.h`, look for TODOs):
1. `DecreaseCommand::verifyCommand()` — a `value < 0` is rejected with
   `"value must be non-negative"` (anything else: `kVerifiedSuccess`).
2. `DecreasedEvent` — type `2`, carries `newValue`, encodes as its decimal string.
3. `StateMachine::processCommand` for `DecreaseCommand` — mirror the increase
   semantics (idempotent "set to next value"):
   - `value >= current` → `{201, "Duplicated request"}` (already applied)
   - `value < current - 1` → `{400, "Invalid request"}`
   - else emit one `DecreasedEvent{value}` → `{200, "Success"}`
4. `StateMachine::applyEvent` — handle `DecreasedEvent`.
5. `decodeEvent` — decode type `2`.
6. `App::recover(const EventLog &)` — replay the log into a fresh `StateMachine`.

When `ex12` is green, do the same thing for real in Gringofts — see
[`GRINGOFTS_GUIDE.md`](../../GRINGOFTS_GUIDE.md), task G4.
