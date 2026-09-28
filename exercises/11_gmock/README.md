# 11 — GoogleTest & GoogleMock (JUnit + Mockito)

**Time:** 1.5 h · **Gringofts link:** `test/infra/es/ReadonlyCommandEventStoreMock.h`,
`test/infra/es/ReadonlyCommandEventStoreTest.cpp`, `test/infra/mpscqueue/MpscDoubleBufferQueueTest.cpp`,
`test/TestRunner.cc`. Every Gringofts PR is expected to come with tests.

## Map
| JUnit / Mockito | gtest / gmock |
|---|---|
| `@Test void foo()` | `TEST(Suite, Name) { ... }` |
| `@BeforeEach` + fields | fixture: `class FooTest : public ::testing::Test { void SetUp() override; }` + `TEST_F(FooTest, Name)` |
| `assertEquals(a, b)` | `EXPECT_EQ(a, b)` (continues) / `ASSERT_EQ(a, b)` (stops this test) |
| `assertThrows` | `EXPECT_THROW(stmt, Type)` |
| `mock(Foo.class)` | `class MockFoo : public Foo { MOCK_METHOD(Ret, name, (Args), (const, override)); };` |
| `when(m.f(1)).thenReturn(2)` | `EXPECT_CALL(m, f(1)).WillOnce(Return(2));` or `ON_CALL(...)` |
| `verify(m, times(3)).f(any())` | `EXPECT_CALL(m, f(_)).Times(3);` — verified automatically when the mock is destroyed |
| `InOrder` | `::testing::InSequence seq;` |
| `@ParameterizedTest` | `TEST_P` + `INSTANTIATE_TEST_SUITE_P` |

**Mocking requires virtual methods** — which is why Gringofts code depends
on interfaces (`ReadonlyCommandEventStore`, `MpscQueue<T>`, `StateMachine`)
and injects them via constructor as references / smart pointers.

Useful commands:
```bash
./build/ex11 --gtest_filter='PersistService.*'   # run a subset
./build/ex11 --gtest_repeat=100 --gtest_shuffle  # hunt flaky/ordering bugs
```

## Tasks (`src/persist_service.h`)
The test file gives you two mocks (`MockEventStore`, `MockNotifier`). Read
them first — they're the lesson. Then implement `PersistService::submit`:
1. `index = store.lastIndex() + 1`.
2. Call `store.persist(index, payload)` up to `maxAttempts` times until it
   returns `true`.
3. On success call `notifier.onPersisted(commandId, index)`; on exhaustion call
   `notifier.onPersistFailed(commandId, "persist failed after N attempts")`.
4. If `payload` is empty, don't touch the store: `onPersistFailed(id, "empty payload")`.

**Then write two tests yourself** at the bottom of `test.cpp`:
- `maxAttempts = 1` and persist fails → exactly one persist call.
- Use `::testing::InSequence` to assert `lastIndex()` is called *before* `persist()`.
