"""Common data structure implementations from scratch.

Includes:
- Node          – building block used by LinkedList and Stack
- LinkedList    – singly linked list
- Stack         – LIFO stack backed by a linked list
- Queue         – FIFO queue backed by two stacks
"""


# ---------------------------------------------------------------- Node -------


class Node:
    """A single node holding a value and a reference to the next node."""

    def __init__(self, value):
        self.value = value
        self.next = None


# ------------------------------------------------------------- LinkedList ----


class LinkedList:
    """Singly linked list with append, prepend, delete, search, and traversal."""

    def __init__(self):
        self.head = None
        self._length = 0

    # ---------------------------------------------------------- Mutation ----

    def append(self, value):
        """Add a node at the tail (O(n))."""
        new_node = Node(value)
        if self.head is None:
            self.head = new_node
        else:
            current = self.head
            while current.next:
                current = current.next
            current.next = new_node
        self._length += 1

    def prepend(self, value):
        """Add a node at the head (O(1))."""
        new_node = Node(value)
        new_node.next = self.head
        self.head = new_node
        self._length += 1

    def delete(self, value):
        """Remove the first node with the given value. Raises ValueError if not found."""
        if self.head is None:
            raise ValueError(f"{value!r} not found in the list.")

        if self.head.value == value:
            self.head = self.head.next
            self._length -= 1
            return

        current = self.head
        while current.next:
            if current.next.value == value:
                current.next = current.next.next
                self._length -= 1
                return
            current = current.next

        raise ValueError(f"{value!r} not found in the list.")

    # ------------------------------------------------------------ Query -----

    def search(self, value):
        """Return the index (0-based) of the first node with the given value, or -1."""
        current = self.head
        index = 0
        while current:
            if current.value == value:
                return index
            current = current.next
            index += 1
        return -1

    def to_list(self):
        """Return all values as a Python list."""
        result = []
        current = self.head
        while current:
            result.append(current.value)
            current = current.next
        return result

    def __len__(self):
        return self._length

    def __repr__(self):
        return " -> ".join(str(v) for v in self.to_list()) + " -> None"


# -------------------------------------------------------------- Stack -------


class Stack:
    """LIFO stack backed by a singly linked list."""

    def __init__(self):
        self._head = None
        self._size = 0

    def push(self, value):
        """Push a value onto the top of the stack (O(1))."""
        node = Node(value)
        node.next = self._head
        self._head = node
        self._size += 1

    def pop(self):
        """Remove and return the top value. Raises IndexError if empty."""
        if self.is_empty():
            raise IndexError("pop from empty stack")
        value = self._head.value
        self._head = self._head.next
        self._size -= 1
        return value

    def peek(self):
        """Return the top value without removing it. Raises IndexError if empty."""
        if self.is_empty():
            raise IndexError("peek at empty stack")
        return self._head.value

    def is_empty(self):
        return self._size == 0

    def __len__(self):
        return self._size


# -------------------------------------------------------------- Queue -------


class Queue:
    """FIFO queue implemented with two stacks (amortised O(1) operations)."""

    def __init__(self):
        self._inbox = Stack()   # receives new items
        self._outbox = Stack()  # serves dequeue requests

    def enqueue(self, value):
        """Add a value to the back of the queue."""
        self._inbox.push(value)

    def dequeue(self):
        """Remove and return the front value. Raises IndexError if empty."""
        if self.is_empty():
            raise IndexError("dequeue from empty queue")
        if self._outbox.is_empty():
            while not self._inbox.is_empty():
                self._outbox.push(self._inbox.pop())
        return self._outbox.pop()

    def peek(self):
        """Return the front value without removing it. Raises IndexError if empty."""
        if self.is_empty():
            raise IndexError("peek at empty queue")
        if self._outbox.is_empty():
            while not self._inbox.is_empty():
                self._outbox.push(self._inbox.pop())
        return self._outbox.peek()

    def is_empty(self):
        return self._inbox.is_empty() and self._outbox.is_empty()

    def __len__(self):
        return len(self._inbox) + len(self._outbox)


# ---------------------------------------------------------------- Demo -------


def main():
    print("=== Linked List ===")
    ll = LinkedList()
    for v in [10, 20, 30, 40]:
        ll.append(v)
    ll.prepend(5)
    print(f"List:        {ll}")
    print(f"Search 30:   index {ll.search(30)}")
    ll.delete(20)
    print(f"After del 20:{ll}")

    print("\n=== Stack ===")
    stack = Stack()
    for v in [1, 2, 3]:
        stack.push(v)
    print(f"Peek:  {stack.peek()}")
    print(f"Pop:   {stack.pop()}")
    print(f"Size:  {len(stack)}")

    print("\n=== Queue ===")
    queue = Queue()
    for v in ["a", "b", "c"]:
        queue.enqueue(v)
    print(f"Peek:     {queue.peek()}")
    print(f"Dequeue:  {queue.dequeue()}")
    print(f"Dequeue:  {queue.dequeue()}")
    print(f"Size:     {len(queue)}")


if __name__ == "__main__":
    main()
