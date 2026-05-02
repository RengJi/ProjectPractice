"""A command-line todo list manager."""

import json
import os


class TodoList:
    """Manages a list of tasks with add, complete, delete, and list operations."""

    def __init__(self, filepath=None):
        self.filepath = filepath
        self.tasks = []
        if filepath and os.path.exists(filepath):
            self._load()

    # ------------------------------------------------------------------ I/O --

    def _load(self):
        with open(self.filepath, "r") as f:
            self.tasks = json.load(f)

    def _save(self):
        if self.filepath:
            with open(self.filepath, "w") as f:
                json.dump(self.tasks, f, indent=2)

    # ------------------------------------------------------------ Operations --

    def add(self, title):
        """Add a new task and return its id."""
        task_id = (max((t["id"] for t in self.tasks), default=0)) + 1
        self.tasks.append({"id": task_id, "title": title, "done": False})
        self._save()
        return task_id

    def complete(self, task_id):
        """Mark a task as done. Raises ValueError if id not found."""
        task = self._get(task_id)
        task["done"] = True
        self._save()

    def delete(self, task_id):
        """Remove a task by id. Raises ValueError if id not found."""
        self._get(task_id)  # validate existence
        self.tasks = [t for t in self.tasks if t["id"] != task_id]
        self._save()

    def list_tasks(self, show_done=True):
        """Return tasks, optionally filtering out completed ones."""
        if show_done:
            return list(self.tasks)
        return [t for t in self.tasks if not t["done"]]

    # --------------------------------------------------------------- Helpers --

    def _get(self, task_id):
        for task in self.tasks:
            if task["id"] == task_id:
                return task
        raise ValueError(f"Task with id {task_id} not found.")


# ------------------------------------------------------------------- CLI -----


def _print_tasks(tasks):
    if not tasks:
        print("  (no tasks)")
        return
    for t in tasks:
        status = "✓" if t["done"] else " "
        print(f"  [{status}] {t['id']}. {t['title']}")


def main():
    todo = TodoList("tasks.json")

    print("Todo List Manager")
    print("Commands: add <title> | done <id> | delete <id> | list | list-all | quit\n")

    while True:
        try:
            raw = input(">>> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nGoodbye!")
            break

        if not raw:
            continue

        parts = raw.split(maxsplit=1)
        cmd = parts[0].lower()
        arg = parts[1] if len(parts) > 1 else ""

        if cmd in ("quit", "exit", "q"):
            print("Goodbye!")
            break
        elif cmd == "add":
            if not arg:
                print("Usage: add <task title>")
            else:
                task_id = todo.add(arg)
                print(f"Added task #{task_id}: {arg}")
        elif cmd == "done":
            try:
                todo.complete(int(arg))
                print(f"Task #{arg} marked as done.")
            except (ValueError, TypeError) as e:
                print(f"Error: {e}")
        elif cmd == "delete":
            try:
                todo.delete(int(arg))
                print(f"Task #{arg} deleted.")
            except (ValueError, TypeError) as e:
                print(f"Error: {e}")
        elif cmd == "list":
            _print_tasks(todo.list_tasks(show_done=False))
        elif cmd == "list-all":
            _print_tasks(todo.list_tasks(show_done=True))
        else:
            print(f"Unknown command: {cmd}")


if __name__ == "__main__":
    main()
