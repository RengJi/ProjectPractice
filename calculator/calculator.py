"""A simple command-line calculator supporting basic arithmetic operations."""


def add(a, b):
    return a + b


def subtract(a, b):
    return a - b


def multiply(a, b):
    return a * b


def divide(a, b):
    if b == 0:
        raise ValueError("Cannot divide by zero.")
    return a / b


def calculate(expression):
    """Parse and evaluate a simple two-operand expression like '3 + 4'."""
    parts = expression.strip().split()
    if len(parts) != 3:
        raise ValueError("Enter an expression in the form: <number> <operator> <number>")

    try:
        a = float(parts[0])
        b = float(parts[2])
    except ValueError:
        raise ValueError("Operands must be numbers.")

    operator = parts[1]
    ops = {"+": add, "-": subtract, "*": multiply, "/": divide}
    if operator not in ops:
        raise ValueError(f"Unsupported operator '{operator}'. Use +, -, *, /.")

    return ops[operator](a, b)


def main():
    print("Simple Calculator")
    print("Enter an expression like: 3 + 4  or  10 / 2")
    print("Type 'quit' to exit.\n")

    while True:
        user_input = input(">>> ").strip()
        if user_input.lower() in ("quit", "exit", "q"):
            print("Goodbye!")
            break
        try:
            result = calculate(user_input)
            # Display as integer when there is no fractional part
            if result == int(result):
                print(int(result))
            else:
                print(result)
        except ValueError as e:
            print(f"Error: {e}")


if __name__ == "__main__":
    main()
