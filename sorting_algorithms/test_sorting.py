"""Tests for sorting algorithm implementations."""

import pytest
from sorting import bubble_sort, selection_sort, insertion_sort, merge_sort, quick_sort

ALGORITHMS = [bubble_sort, selection_sort, insertion_sort, merge_sort, quick_sort]
ALGORITHM_IDS = ["bubble", "selection", "insertion", "merge", "quick"]


@pytest.mark.parametrize("sort_fn", ALGORITHMS, ids=ALGORITHM_IDS)
class TestSortingAlgorithms:
    def test_typical(self, sort_fn):
        assert sort_fn([64, 25, 12, 22, 11]) == [11, 12, 22, 25, 64]

    def test_already_sorted(self, sort_fn):
        assert sort_fn([1, 2, 3, 4, 5]) == [1, 2, 3, 4, 5]

    def test_reverse_sorted(self, sort_fn):
        assert sort_fn([5, 4, 3, 2, 1]) == [1, 2, 3, 4, 5]

    def test_single_element(self, sort_fn):
        assert sort_fn([42]) == [42]

    def test_empty(self, sort_fn):
        assert sort_fn([]) == []

    def test_duplicates(self, sort_fn):
        assert sort_fn([3, 1, 2, 1, 3]) == [1, 1, 2, 3, 3]

    def test_negative_numbers(self, sort_fn):
        assert sort_fn([-3, 1, -1, 2, 0]) == [-3, -1, 0, 1, 2]

    def test_does_not_mutate_input(self, sort_fn):
        original = [3, 1, 2]
        sort_fn(original)
        assert original == [3, 1, 2]
