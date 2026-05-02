"""Tests for the calculator module."""

import pytest
from calculator import add, subtract, multiply, divide, calculate


def test_add():
    assert add(2, 3) == 5
    assert add(-1, 1) == 0
    assert add(0, 0) == 0


def test_subtract():
    assert subtract(5, 3) == 2
    assert subtract(0, 5) == -5


def test_multiply():
    assert multiply(3, 4) == 12
    assert multiply(-2, 5) == -10
    assert multiply(0, 100) == 0


def test_divide():
    assert divide(10, 2) == 5.0
    assert divide(7, 2) == 3.5


def test_divide_by_zero():
    with pytest.raises(ValueError, match="Cannot divide by zero"):
        divide(5, 0)


def test_calculate_addition():
    assert calculate("3 + 4") == 7


def test_calculate_subtraction():
    assert calculate("10 - 6") == 4


def test_calculate_multiplication():
    assert calculate("3 * 5") == 15


def test_calculate_division():
    assert calculate("9 / 3") == 3.0


def test_calculate_invalid_expression():
    with pytest.raises(ValueError):
        calculate("3 4")


def test_calculate_unsupported_operator():
    with pytest.raises(ValueError, match="Unsupported operator"):
        calculate("3 % 4")


def test_calculate_division_by_zero():
    with pytest.raises(ValueError, match="Cannot divide by zero"):
        calculate("5 / 0")
