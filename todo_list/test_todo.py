"""Tests for the TodoList class."""

import pytest
from todo import TodoList


@pytest.fixture
def todo():
    """Return a fresh in-memory TodoList (no file persistence)."""
    return TodoList()


def test_add_task(todo):
    task_id = todo.add("Buy groceries")
    assert task_id == 1
    tasks = todo.list_tasks()
    assert len(tasks) == 1
    assert tasks[0]["title"] == "Buy groceries"
    assert tasks[0]["done"] is False


def test_add_multiple_tasks(todo):
    todo.add("Task A")
    todo.add("Task B")
    tasks = todo.list_tasks()
    assert len(tasks) == 2
    assert tasks[1]["id"] == 2


def test_complete_task(todo):
    todo.add("Walk the dog")
    todo.complete(1)
    tasks = todo.list_tasks()
    assert tasks[0]["done"] is True


def test_complete_nonexistent_task(todo):
    with pytest.raises(ValueError, match="not found"):
        todo.complete(99)


def test_delete_task(todo):
    todo.add("Read a book")
    todo.delete(1)
    assert todo.list_tasks() == []


def test_delete_nonexistent_task(todo):
    with pytest.raises(ValueError, match="not found"):
        todo.delete(42)


def test_list_tasks_hides_done(todo):
    todo.add("Task 1")
    todo.add("Task 2")
    todo.complete(1)
    pending = todo.list_tasks(show_done=False)
    assert len(pending) == 1
    assert pending[0]["title"] == "Task 2"


def test_list_all_tasks_includes_done(todo):
    todo.add("Task 1")
    todo.complete(1)
    all_tasks = todo.list_tasks(show_done=True)
    assert len(all_tasks) == 1


def test_id_increments_after_delete(todo):
    todo.add("First")
    todo.add("Second")
    todo.delete(1)
    new_id = todo.add("Third")
    assert new_id == 3
