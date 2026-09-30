# 05 — Interfaces, virtual dispatch, factories

**Time:** 1.5 h · **Gringofts link:** `src/infra/Encodable.h`, `src/infra/Decodable.h`,
`src/infra/es/Event.h`, `src/infra/es/EventDecoder.h`,
`src/app_demo/should_be_generated/domain/EventDecoderImpl.cpp`, `ProcessedEvent.h`.

## Java → C++ OOP differences that bite
| Java | C++ |
|---|---|
| methods virtual by default | **non-virtual by default**; write `virtual` in the base |
| `interface Foo` | class with only pure virtuals: `virtual void f() = 0;` |
| `@Override` (optional) | `override` — always write it; the compiler then catches signature typos |
| `final class` / method | `final` (same meaning) |
| GC cleans up subclasses | base **must** have `virtual ~Base() = default;` or deleting via a base pointer is UB |
| `instanceof` + cast | `dynamic_cast<Derived*>(basePtr)` → nullptr if wrong type |
| objects live on heap | polymorphism only works through **pointers or references**; `Base b = derived;` *slices* off the derived part |
| `extends A implements B, C` | `class X : public A, public B, public C` (Gringofts: `class Command : public Encodable, Decodable`) |

## Tasks (`src/events.h`)
Model a slice of Gringofts' event types:
1. `Encodable` interface: make `encodeToString()` pure virtual
   (`virtual std::string encodeToString() const = 0;`). The virtual destructor
   is given — read the comment on it.
2. `Event : public Encodable` — abstract, holds `Type` (uint32) and a timestamp;
   `virtual std::string describe() const` with a default implementation.
3. `IncreasedEvent final : public Event` (type 1) carrying `int value`,
   encodes as `"inc:<value>"`, overrides `describe()` → `"Increased to <value>"`.
4. `ResetEvent final : public Event` (type 2), encodes `"reset"`. Does **not**
   override `describe()` (default is `"Event#<type>"`).
5. `decodeEvent(Type type, const std::string &payload)` →
   `std::unique_ptr<Event>` factory (like `EventDecoderImpl::decodeEventFromString`).
   Throw `std::runtime_error` for an unknown type.

## Experiments
- Remove `virtual` from `~Encodable()` and run the test binary with
  `-DJ2C_SANITIZER=address`. (Look for "new-delete-type-mismatch" / leaks.)
- Misspell an override (`encodeToStrin() const override`) — compiler error. Now
  delete `override` — silently compiles and does the wrong thing. That's why
  cpplint in Gringofts wants `override`.
