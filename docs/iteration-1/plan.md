# AI Smart Airport: Iteration 1 Plan

## 1. Project Context

AI Smart Airport is a real-time simulation of an airport that manages air traffic and passenger flow. Aircraft and passengers compete for shared, limited resources: check-in counters, passport control and security lanes, boarding gates and runways. The system decides how those resources are allocated, first with simple routing policies, later with real-time scheduling algorithms (RM, DM, EDF) and AI-based decision making.

- **Back end:** the simulation core, written in C (C11) with POSIX threads and built with CMake.
- **Front end:** a separate 2D GUI that shows the state of the back end. It is planned in its own document.

## 2. Process

Development follows the **SWE process group of Automotive SPICE (ASPICE)**, applied as an **iterative V-model**. Each iteration runs the whole V on a small slice of the system:

| Left side (this plan) | Right side (verified by) |
| --- | --- |
| Software requirements specification (SRS) | System tests |
| Software architecture description (SAD) | Integration tests |
| Software detailed design (SDD), then implementation | Unit tests |

This document covers the left side of the V for iteration 1. The tests are planned in a separate test plan, and trace back to the IDs used here. ASPICE sets the process, not the document format: the SRS, SAD and SDD below are our own lightweight documents, and every requirement, architecture element and design decision has an ID so that it can be traced and verified.

## 3. Iteration 1 Scope

- **Component:** passenger flow management and queue modelling.
- **In scope:** departing passengers only.
- **Deferred:** arriving passengers, the aircraft side (runways, gates, taxiing) and the GUI front end. Flight departure times are mocked until the aircraft model replaces them.

## 4. Repository Baseline

What the repository already provides, and what it means for this iteration.

| Item | Current state | Consequence for the plan |
| --- | --- | --- |
| `src/queue.[ch]` | Generic ring-buffer FIFO of fixed-size items over a caller-provided buffer, with an internal mutex and non-blocking push and pop (returning full or empty). Capacity is at most 255 items. | Reused as the lane FIFO (SDD-17). It has no blocking wait and no way to inspect or lock several queues together (see open items). |
| Build | CMake 3.22, C11. Strict warning set including `-Wconversion`, `-Waggregate-return` and, on GCC, `-Wstack-usage=1024`. Options for ASan/UBSan, TSan and valgrind (memcheck, helgrind, drd), with presets `debug`, `asan`, `tsan`, `valgrind` and `helgrind`. | SRS-C-03. Large objects must be static, and no function returns a struct by value. |
| Tests | Unity and CMock (v2.7.0) through `add_cmock_test`, 60 s default timeout. No test registered yet. | One unit test target per module, with neighbouring modules mocked. |
| Application target | Not defined yet. The CMake file only has a placeholder, taken when `BUILD_TESTING=OFF`. | To be defined in this iteration (open items). |
| Style guide | `.agents/STYLES.md`: tabs, K&R braces, Doxygen on every function, banner-style file sections, `STATIC` macro for testability, an adopted MISRA C:2012 subset, British English comments. | SRS-C-02. |
| README | Title only. | To be filled in. |

## 5. Software Requirements Specification (SRS)

Queue parameters (lanes, service times, generation profiles) describe the behaviour of the modelled system and are not under its control. The time constraints are the deadlines the system must meet. The two are linked: if arrivals outpace a queue's service capacity, its deadlines are missed.

### 5.1 Requirements

| ID | Category | Requirement |
| --- | --- | --- |
| SRS-F-01 | Functional | The system shall model the four departing-passenger queuing stages (check-in, passport control, security, boarding). Each stage consists of lanes with the parameters in 5.2, and each lane is a FIFO queue with one server (M/M/1). |
| SRS-F-02 | Functional | A passenger shall go through these stages in order: check-in, passport control, security, gate lounge, boarding. The gate lounge is a holding area, not a queue. |
| SRS-F-03 | Functional | The system shall create the passengers of each flight, with their flight built in, according to the generation profiles in 5.4, and shall route them to a check-in lane of that flight. |
| SRS-F-04 | Functional | The system shall enforce the time constraints in 5.3. |
| SRS-F-05 | Functional | The system shall run on a simulated clock whose speed relative to real time is configurable at startup. |
| SRS-F-06 | Functional | The system shall record, per flight, the number of passengers checked in, boarded, missed at check-in and missed at the gate, and, per queue, the average waiting time and the maximum length, and shall report them at the end of the run. |
| SRS-F-07 | Functional | The system shall write a timestamped event log of passenger and flight events (stage entry and exit, deadline events). |
| SRS-F-08 | Functional | All random draws (departure times, arrival times, service times) shall be reproducible from a seed given at startup, independently of thread scheduling. |
| SRS-F-09 | Functional | Queue parameters, time constraints, generation profiles and the range of N shall be configurable at startup without recompiling. |
| SRS-F-10 | Functional | A passenger entering a stage shall be routed to one of its lanes by a routing policy. The policy shall be replaceable. The default policy is to join the shortest line (proposed). |
| SRS-C-01 | Constraint | The back end shall be implemented in C (C11) with POSIX threads and built with CMake. |
| SRS-C-02 | Constraint | The code shall follow the project style guide (`.agents/STYLES.md`), including its adopted MISRA C:2012 subset. Violations are flagged, and accepted deviations are logged in the guide. |
| SRS-C-03 | Constraint | The code shall build without warnings under the project's warning set, and its unit tests shall pass under the `asan`, `tsan` and `valgrind` presets. |

### 5.2 Queue parameters

| Queue | Scope | Lanes (c) | Mean service time | Max throughput |
| --- | --- | --- | --- | --- |
| Check-in | per flight | 3 | 5 min | 36 per hour per flight |
| Passport control | shared | 6 | 3 min | 120 per hour |
| Security | shared | 4 | 20 s | 720 per hour |
| Boarding | per flight | 1 | 10 s | 360 per hour per flight |

Service time is the time spent at the server, excluding waiting, and is exponentially distributed. The values are assumptions from the team's experience, to be tuned. The 20 s security value is consistent with the roughly 14 s per passenger per lane implied by Wang (2017, Journal of Applied Mathematics and Physics).

### 5.3 Time constraints

T is the scheduled departure time of the flight.

| ID | Queue | Event | Time |
| --- | --- | --- | --- |
| SRS-T-01 | Check-in | opens | T-180 min |
| SRS-T-02 | Check-in | closes (including bag drop) | T-60 min |
| SRS-T-03 | Boarding | opens | T-40 min |
| SRS-T-04 | Boarding | gate closes | T-15 min |

Check-in is active from T-180 to T-60, and boarding service from T-40 to T-15. Passport control and security have no window of their own. The values are typical industry timings (check-in opens 2 to 3 hours before departure, closes 45 to 60 minutes before for domestic and 60 or more for international flights, boarding opens 30 to 45 minutes before, the gate closes 15 to 20 minutes before). Sources: Travel Sentry, Transpovia, Airclaim, LABUSA Travel. Exact cutoffs vary by airline.

### 5.4 Passenger generation profiles

These are the generation profiles of the passengers of a flight, expressed as expected shares of its N registered passengers, so they scale with flight size. They are applied by the passenger generator, not by the queues.

| Queue | Phase | Window | Expected share of N | Expected rate (share of N per hour) |
| --- | --- | --- | --- | --- |
| Check-in | early | T-180 to T-150 | 12.5% | 25% |
| Check-in | rush | T-150 to T-90 | 75% | 75% |
| Check-in | late | T-90 to T-60 | 12.5% | 25% |
| Boarding | early line-up | T-50 to T-40 | 66.7% | 400% |
| Boarding | trickle | T-40 to T-15 | 33.3% | 80% |

Example for N = 48: check-in 12, 36 and 12 passengers per hour; boarding 192 and 38 per hour. The check-in peak equals the capacity of 3 counters (36 per hour), so a queue builds during the rush and drains afterwards. Passport control and security have no external arrival rate (see SAD-01).

## 6. Software Architecture Description (SAD)

| ID | Element | Decision |
| --- | --- | --- |
| SAD-01 | Stage chaining | Check-in lanes feed passport control, and passport control lanes feed security. Each passenger leaving a lane is routed to a lane of the next stage, so the arrivals of passport control and security are the departures of the previous stage. |
| SAD-02 | Passenger generation | A passenger generator, separate from the queues, creates each passenger with its flight built in and routes it to a check-in lane of that flight. Queues never generate passengers. The generation profiles of 5.4 are applied by the generator. |
| SAD-03 | Lanes and routing | Every lane is a monitor (mutex and condition variable) holding a FIFO, served by one server thread that blocks while the lane is empty. Each stage has a router that picks the lane by the routing policy of SRS-F-10. Choosing a lane and joining it is one critical section, because passport control and security are shared by up to 4 flights. |
| SAD-04 | Gate lounge and boarding | The gate lounge is a plain buffer without queue behaviour. Boarding is a single lane per flight. A passenger who has been through boarding leaves the system and is only counted. |
| SAD-05 | Threads | One server thread per lane. The generation of passengers, the release of passengers from the gate lounge and the application of the time constraints of 5.3 run in generator and flight-timeline threads; their number (one per flight or a single one) is to be decided. One global clock thread (SAD-06). The main thread initialises the system. |
| SAD-06 | Global clock thread | A global clock thread provides the simulated time and synchronises the simulation. How the other threads interact with it is not decided yet. |

## 7. Software Detailed Design (SDD)

| ID | Kind | Decision |
| --- | --- | --- |
| SDD-01 | Simplification | Passengers are individuals; there are no groups or families. |
| SDD-02 | Simplification | There is no walking time between queues. |
| SDD-03 | Design decision | A flight holds: identifier, scheduled departure time T, registered passengers N, state (scheduled, check-in open, boarding, gate closed), counters (checked in, boarded, missed at check-in, missed at gate), and references to its check-in lanes, gate lounge and boarding lane. A passenger holds: identifier, flight identifier, drawn arrival times, drawn service time per stage, entry and exit timestamps per stage, and status. Proposed fields, finalised during iteration 1. |
| SDD-04 | Design decision | A flight is active from check-in opening (T-180) until the gate closes (T-15). At most 4 flights are active at any time. The airport has 12 pre-defined check-in lanes, allocated dynamically to flights at runtime, 3 per active flight. This also bounds the gate lounges and boarding lanes at 4 each. |
| SDD-05 | Design decision | Each flight has its own gate lounge, a plain buffer sized to hold the maximum number of passengers of a single flight, so it never blocks anyone. Passengers are drawn from it at random to join the boarding lane. |
| SDD-06 | Design decision | Because N is fixed, each passenger's check-in arrival time and each of the flight's N boarding join times are drawn from the profiles in 5.4 (pick a phase with its expected share, then a uniform time inside it). The count per phase is random and the total is exactly N. |
| SDD-07 | Design decision | At each boarding join time, a randomly chosen passenger from the gate lounge joins the boarding lane. If the lounge is empty at that time, the join waits for the next passenger to enter the lounge. |
| SDD-08 | Design decision | A passenger who has not completed check-in service by T-60, or boarding service by T-15, has missed the flight. A passenger already in service at the deadline is allowed to finish. A missed passenger is sent back only after security; what sending back means is to be defined in the design. The stage where the deadline was missed is recorded. Rebooking on the next flight is not modelled. |
| SDD-09 | Simplification | Whether a registered passenger makes the flight depends only on the queue simulation and randomness. |
| SDD-10 | Design decision | All random draws of a passenger (check-in arrival time and service times at each stage) and of its flight (boarding join times, choice of the lounge passenger) come from a per-flight generator seeded from the global seed. Results then do not depend on thread scheduling. |
| SDD-11 | Design decision | N is drawn uniformly from the integers 30 to 60 per flight (proposed, configurable). At N = 48 the check-in peak matches the counter capacity, so larger flights stress check-in. |
| SDD-12 | Design decision | Mock departure times are drawn at random, subject to at most 4 active flights, over a configurable horizon (proposed default: 6 simulated hours). |
| SDD-13 | Design decision | The default speed factor is 80 (one simulated hour per 45 real seconds), configurable. |
| SDD-14 | Design decision | The event log is a CSV file with one line per event: simulated timestamp, event type, passenger identifier, flight identifier, queue. |
| SDD-15 | Design decision | Static allocation is the default. Lanes, lounges, passenger and flight records, and log buffers are file-scope arrays sized by compile-time maxima (for example `MAX_N`); start-up rejects any configuration (SRS-F-09) that exceeds them, and nothing is allocated dynamically after start-up. Large objects are never locals, which `-Wstack-usage=1024` enforces on GCC. |
| SDD-16 | Design decision | A passenger who has finished boarding or has been sent back is counted in the flight's metrics and no longer modelled. Its record is not freed but returned to a fixed pool, in line with SDD-15. |
| SDD-17 | Design decision | Each lane FIFO is an instance of the existing queue module (`src/queue.[ch]`) over a static buffer of passenger-pool indices. That module holds at most 255 items per queue, so the worst case (all passengers of 4 active flights in one lane, 4 × `MAX_N` = 240) is checked at compile time with `_Static_assert`. |

## 8. Open Items

- **To confirm by the team:** the values proposed in SDD-11 to SDD-13 (N range, mock departure horizon, speed factor) and the default routing policy of SRS-F-10 (shortest line).
- **To decide in the SAD:** the generator and flight-timeline threads (one per flight, or a single one), and how threads interact with the global clock thread.
- **To define in the SDD:**
  - What sending a missed passenger back means, including passengers still waiting at check-in when it closes.
  - Pool sizes. They must also cover passengers of a closed flight who are still in the shared queues after T-15, and stay within the 255-item limit of the queue module (or the module's capacity type is widened).
  - How lanes use the queue module, which has an internal mutex, never blocks, and cannot be inspected or locked across several queues. The lanes need a blocking wait while empty (SAD-03), a way to read the queue length, and an atomic choose-and-join across the lanes of a stage.
  - The application target and entry point, since the CMake file defines none yet.
- **To define later:** tuning of service times and generation profiles after the first runs. An optional luggage queue at check-in is deferred: bag drop is already part of the 5-minute check-in service time.
- **Deferred:** the GUI front end, planned in its own document, and the test plan for the right side of the V.
