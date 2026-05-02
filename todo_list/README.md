# Todo List

A command-line todo list manager with persistent JSON storage.

## Usage

```bash
python todo.py
```

**Example session:**

```
Todo List Manager
Commands: add <title> | done <id> | delete <id> | list | list-all | quit

>>> add Buy milk
Added task #1: Buy milk
>>> add Read a book
Added task #2: Read a book
>>> list
  [ ] 1. Buy milk
  [ ] 2. Read a book
>>> done 1
Task #1 marked as done.
>>> list
  [ ] 2. Read a book
>>> list-all
  [✓] 1. Buy milk
  [ ] 2. Read a book
>>> delete 2
Task #2 deleted.
>>> quit
Goodbye!
```

Tasks are saved to `tasks.json` in the same directory.

## Running Tests

```bash
python -m pytest test_todo.py -v
```
