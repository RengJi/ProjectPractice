# Data Structures

Implementations of common data structures from scratch in Python.

## Structures Implemented

| Structure | Description |
|-----------|-------------|
| `LinkedList` | Singly linked list with append, prepend, delete, and search |
| `Stack` | LIFO stack backed by a linked list |
| `Queue` | FIFO queue using two stacks (amortised O(1) operations) |

## Usage

```bash
python data_structures.py
```

**Sample output:**

```
=== Linked List ===
List:         5 -> 10 -> 20 -> 30 -> 40 -> None
Search 30:    index 3
After del 20: 5 -> 10 -> 30 -> 40 -> None

=== Stack ===
Peek:  3
Pop:   3
Size:  2

=== Queue ===
Peek:     a
Dequeue:  a
Dequeue:  b
Size:     1
```

## Running Tests

```bash
python -m pytest test_data_structures.py -v
```
