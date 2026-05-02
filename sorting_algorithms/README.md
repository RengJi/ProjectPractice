# Sorting Algorithms

Implementations of five classic sorting algorithms in Python.

## Algorithms

| Algorithm | Time (avg) | Time (worst) | Space | Notes |
|-----------|-----------|--------------|-------|-------|
| Bubble Sort | O(n²) | O(n²) | O(1) | Stops early if already sorted |
| Selection Sort | O(n²) | O(n²) | O(1) | |
| Insertion Sort | O(n²) | O(n²) | O(1) | O(n) on nearly sorted input |
| Merge Sort | O(n log n) | O(n log n) | O(n) | Stable, recursive |
| Quick Sort | O(n log n) | O(n²) | O(log n) | In-place, last element pivot |

## Usage

```bash
python sorting.py
```

**Output:**

```
Input:  [64, 25, 12, 22, 11]

Bubble Sort     : [11, 12, 22, 25, 64]
Selection Sort  : [11, 12, 22, 25, 64]
Insertion Sort  : [11, 12, 22, 25, 64]
Merge Sort      : [11, 12, 22, 25, 64]
Quick Sort      : [11, 12, 22, 25, 64]
```

## Running Tests

```bash
python -m pytest test_sorting.py -v
```
