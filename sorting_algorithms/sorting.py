"""Classic sorting algorithm implementations.

Algorithms:
- Bubble Sort    O(n²) average/worst
- Selection Sort O(n²) average/worst
- Insertion Sort O(n²) worst, O(n) best (nearly sorted input)
- Merge Sort     O(n log n) guaranteed
- Quick Sort     O(n log n) average, O(n²) worst
"""


def bubble_sort(arr):
    """Sort in-place using bubble sort and return the list."""
    a = list(arr)
    n = len(a)
    for i in range(n):
        swapped = False
        for j in range(0, n - i - 1):
            if a[j] > a[j + 1]:
                a[j], a[j + 1] = a[j + 1], a[j]
                swapped = True
        if not swapped:
            break
    return a


def selection_sort(arr):
    """Sort using selection sort and return the list."""
    a = list(arr)
    n = len(a)
    for i in range(n):
        min_idx = i
        for j in range(i + 1, n):
            if a[j] < a[min_idx]:
                min_idx = j
        a[i], a[min_idx] = a[min_idx], a[i]
    return a


def insertion_sort(arr):
    """Sort using insertion sort and return the list."""
    a = list(arr)
    for i in range(1, len(a)):
        key = a[i]
        j = i - 1
        while j >= 0 and a[j] > key:
            a[j + 1] = a[j]
            j -= 1
        a[j + 1] = key
    return a


def merge_sort(arr):
    """Sort using merge sort and return a new sorted list."""
    if len(arr) <= 1:
        return list(arr)
    mid = len(arr) // 2
    left = merge_sort(arr[:mid])
    right = merge_sort(arr[mid:])
    return _merge(left, right)


def _merge(left, right):
    result = []
    i = j = 0
    while i < len(left) and j < len(right):
        if left[i] <= right[j]:
            result.append(left[i])
            i += 1
        else:
            result.append(right[j])
            j += 1
    result.extend(left[i:])
    result.extend(right[j:])
    return result


def quick_sort(arr):
    """Sort using quick sort and return a new sorted list."""
    a = list(arr)
    _quick_sort(a, 0, len(a) - 1)
    return a


def _quick_sort(a, low, high):
    if low < high:
        pivot_idx = _partition(a, low, high)
        _quick_sort(a, low, pivot_idx - 1)
        _quick_sort(a, pivot_idx + 1, high)


def _partition(a, low, high):
    pivot = a[high]
    i = low - 1
    for j in range(low, high):
        if a[j] <= pivot:
            i += 1
            a[i], a[j] = a[j], a[i]
    a[i + 1], a[high] = a[high], a[i + 1]
    return i + 1


# ------------------------------------------------------------------- Demo ----

_ALGORITHMS = {
    "Bubble Sort": bubble_sort,
    "Selection Sort": selection_sort,
    "Insertion Sort": insertion_sort,
    "Merge Sort": merge_sort,
    "Quick Sort": quick_sort,
}


def main():
    sample = [64, 25, 12, 22, 11]
    print(f"Input:  {sample}\n")
    for name, func in _ALGORITHMS.items():
        print(f"{name:<16}: {func(sample)}")


if __name__ == "__main__":
    main()
