# SYSC4001 Assignment 3 — Part 2

This repository contains the full solution for SYSC4001 Assignment 3, Part 2.  
The assignment is split into three components:

- **Part 2.a:** Process creation and shared memory (no synchronization)
- **Part 2.b:** Semaphore-based synchronization added to Part 2.a
- **Part 2.c:** Report on deadlock/livelock behavior

Each part is contained in its own folder for clean organization.

## Folder Structure

```
SYSC4001_A3_P2/
│
├── part2a/              # Part 2a source code and exam files
├── part2b/              # Part 2b source code and exam files
└── README.md            # This file
```

## How to Compile & Run

### Part 2a
```
cd part2a
gcc -Wall -O2 marker_101300152_101280677.c -o part2a
./part2a <num_TAs> rubric.txt exam_list.txt
```

### Part 2b (with semaphores)
```
cd part2b/part2b
gcc -Wall -O2 -DUSE_SEMAPHORES marker_101300152_101280677.c -o part2b -pthread
./part2b <num_TAs> rubric.txt exam_list.txt
```

## Contributors
- **Joodi Al-Asaad - 101300152**
- **Kemal Sogut - 101280677**
