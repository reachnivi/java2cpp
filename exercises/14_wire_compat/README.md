# 14 — Wire compatibility: the protobuf encoding and schema evolution

**Time:** 2.5 h · **Gringofts link:** `src/app_demo/should_be_generated/domain/protos/demo.proto`,
`src/infra/es/store/store.proto`, `src/infra/raft/raft.proto`, and the command/event type
constants in `domain/common_types.h`. `IncreaseCommand::encodeToString()` is
`mRequest.SerializeAsString()`: **protobuf bytes are what's stored in the Raft log.**

## Why this matters more than it looks
Anything Gringofts persists (log entries, snapshots, RocksDB values) or sends between nodes
must stay readable when:
- a **new** binary reads data written by an **old** one (normal upgrade),
- an **old** binary reads data written by a **new** one (rollback, or a follower not yet
  upgraded during a rolling deploy),
- an old node receives a new message and **passes it on** (forwarding to the leader).

Protobuf makes all three work, *if* you follow its rules. You implement the encoding once by
hand so the rules stop being magic.

## The wire format (the subset you'll implement)
- A message is a sequence of **fields**: `tag` then value. `tag = (field_number << 3) | wire_type`.
- Wire type **0 = varint**: 7 bits per byte, little-endian groups, high bit = "more bytes follow".
  `300` → `0xAC 0x02`.
- Wire type **2 = length-delimited**: varint length, then that many bytes (strings, bytes, nested messages).
- `sint64` uses **zigzag** before varint so small negatives stay small: `(n << 1) ^ (n >> 63)`.
- Proto3 omits fields that hold the default value (0 / empty) and reads a missing field as the default.
- A decoder that meets a field number it doesn't know **skips it** (it can, because the wire type
  says how long it is) and **keeps the bytes** so it can re-serialise them unchanged.

## Schema
```proto
// v1 — what old nodes run
message IncreaseRequest { uint64 value = 1; string request_id = 2; }
// v2 — adds two fields. Never renumber or reuse 1 or 2.
message IncreaseRequest { uint64 value = 1; string request_id = 2; sint64 delta = 3; string tracking_context = 4; }
```

## Tasks (`src/wire.h`)
1. `putVarint`, `getVarint` (throw `std::runtime_error` on truncated input or >10 bytes), `zigzagEncode/Decode`.
2. `encode(const RequestV1&)` / `decode(std::string_view) -> RequestV1`: skip unknown fields but
   **append their raw bytes to `unknownFields`**, and re-emit them at the end of `encode`.
3. `encode(const RequestV2&)` / `decodeV2`.
4. Decoding a *known* field number with the *wrong* wire type throws: that's what happens when
   someone reuses a field number for a different type, and why you must never do it.

## Rules to take into Gringofts code review
- Never change a field's number or type; never reuse a deleted number (mark it `reserved`).
- New fields must have a safe default meaning "old behaviour".
- The same rules apply to Gringofts' **type constants** (`PROCESSED_EVENT = 6`, `INCREASE_COMMAND = 0`):
  they're persisted in every log entry. Never renumber them; never reuse one.
- Roll-forward **and** roll-back must work: in a rolling upgrade, old and new nodes coexist for a while.
