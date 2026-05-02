"""Tests for LinkedList, Stack, and Queue."""

import pytest
from data_structures import LinkedList, Stack, Queue


# ================================================================ LinkedList ==


class TestLinkedList:
    def test_append_and_to_list(self):
        ll = LinkedList()
        ll.append(1)
        ll.append(2)
        ll.append(3)
        assert ll.to_list() == [1, 2, 3]

    def test_prepend(self):
        ll = LinkedList()
        ll.append(2)
        ll.prepend(1)
        assert ll.to_list() == [1, 2]

    def test_length(self):
        ll = LinkedList()
        for v in range(5):
            ll.append(v)
        assert len(ll) == 5

    def test_delete_middle(self):
        ll = LinkedList()
        for v in [1, 2, 3]:
            ll.append(v)
        ll.delete(2)
        assert ll.to_list() == [1, 3]

    def test_delete_head(self):
        ll = LinkedList()
        ll.append(10)
        ll.append(20)
        ll.delete(10)
        assert ll.to_list() == [20]

    def test_delete_tail(self):
        ll = LinkedList()
        ll.append(10)
        ll.append(20)
        ll.delete(20)
        assert ll.to_list() == [10]

    def test_delete_not_found(self):
        ll = LinkedList()
        ll.append(1)
        with pytest.raises(ValueError):
            ll.delete(99)

    def test_search_found(self):
        ll = LinkedList()
        for v in [5, 10, 15]:
            ll.append(v)
        assert ll.search(10) == 1

    def test_search_not_found(self):
        ll = LinkedList()
        ll.append(1)
        assert ll.search(99) == -1

    def test_empty_list(self):
        ll = LinkedList()
        assert ll.to_list() == []
        assert len(ll) == 0


# =================================================================== Stack ==


class TestStack:
    def test_push_and_peek(self):
        s = Stack()
        s.push(42)
        assert s.peek() == 42

    def test_push_pop_order(self):
        s = Stack()
        for v in [1, 2, 3]:
            s.push(v)
        assert s.pop() == 3
        assert s.pop() == 2

    def test_len(self):
        s = Stack()
        s.push(1)
        s.push(2)
        assert len(s) == 2
        s.pop()
        assert len(s) == 1

    def test_is_empty(self):
        s = Stack()
        assert s.is_empty()
        s.push(1)
        assert not s.is_empty()

    def test_pop_empty_raises(self):
        s = Stack()
        with pytest.raises(IndexError):
            s.pop()

    def test_peek_empty_raises(self):
        s = Stack()
        with pytest.raises(IndexError):
            s.peek()


# =================================================================== Queue ==


class TestQueue:
    def test_enqueue_dequeue_order(self):
        q = Queue()
        q.enqueue("a")
        q.enqueue("b")
        q.enqueue("c")
        assert q.dequeue() == "a"
        assert q.dequeue() == "b"

    def test_peek(self):
        q = Queue()
        q.enqueue(1)
        q.enqueue(2)
        assert q.peek() == 1
        assert len(q) == 2  # peek doesn't remove

    def test_len(self):
        q = Queue()
        q.enqueue(1)
        q.enqueue(2)
        assert len(q) == 2

    def test_is_empty(self):
        q = Queue()
        assert q.is_empty()
        q.enqueue(1)
        assert not q.is_empty()

    def test_dequeue_empty_raises(self):
        q = Queue()
        with pytest.raises(IndexError):
            q.dequeue()

    def test_peek_empty_raises(self):
        q = Queue()
        with pytest.raises(IndexError):
            q.peek()

    def test_interleaved_enqueue_dequeue(self):
        q = Queue()
        q.enqueue(1)
        q.enqueue(2)
        assert q.dequeue() == 1
        q.enqueue(3)
        assert q.dequeue() == 2
        assert q.dequeue() == 3
