import csv
import os
import re
import sys
from typing import Any, Dict, List, Optional

TABLE_MAP = {
    "menu": "coffee_menu_items.csv"
}

COMPARISON_OPERATORS = {
    "=": lambda a, b: a == b,
    "!=": lambda a, b: a != b,
    ">": lambda a, b: a > b,
    "<": lambda a, b: a < b,
    ">=": lambda a, b: a >= b,
    "<=": lambda a, b: a <= b,
}

_WHERE_PATTERN = re.compile(
    r"^SELECT\s+(?P<columns>\*|[A-Za-z0-9_,\s\"]+)\s+FROM\s+(?P<table>[A-Za-z0-9_]+)(?:\s+WHERE\s+(?P<condition>.+))?$",
    re.IGNORECASE,
)

_CONDITION_PATTERN = re.compile(
    r"^(?P<column>[A-Za-z0-9_\"]+)\s*(?P<op>=|!=|<=|>=|<|>)\s*(?P<value>.+)$"
)


def normalize_value(value: str) -> Any:
    value = value.strip()
    if value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    if value.startswith("'") and value.endswith("'"):
        return value[1:-1]
    if value.lower() in {"true", "false"}:
        return value.lower() == "true"
    try:
        if "." in value:
            return float(value)
        return int(value)
    except ValueError:
        return value


def split_columns(raw: str) -> List[str]:
    columns = []
    current = []
    in_quotes = False
    quote_char = None
    for ch in raw:
        if ch in {'"', "'"}:
            if not in_quotes:
                in_quotes = True
                quote_char = ch
            elif ch == quote_char:
                in_quotes = False
                quote_char = None
            current.append(ch)
        elif ch == ',' and not in_quotes:
            columns.append(''.join(current).strip())
            current = []
        else:
            current.append(ch)
    if current:
        columns.append(''.join(current).strip())
    return columns


def parse_select(query: str) -> Dict[str, Optional[str]]:
    text = query.strip()
    if not text.lower().startswith("select "):
        raise ValueError("Invalid query format. Use: SELECT <columns> FROM <table> [WHERE <condition>].")

    parts = re.split(r"\bfrom\b", text, flags=re.IGNORECASE, maxsplit=1)
    if len(parts) != 2:
        raise ValueError("Invalid query format. Missing FROM clause.")

    columns_part = parts[0][6:].strip()
    rest = parts[1].strip()

    where_parts = re.split(r"\bwhere\b", rest, flags=re.IGNORECASE, maxsplit=1)
    table = where_parts[0].strip()
    condition = where_parts[1].strip() if len(where_parts) > 1 else None

    if not columns_part or not table:
        raise ValueError("Invalid query format. Missing columns or table name.")

    return {
        "columns": columns_part,
        "table": table,
        "condition": condition,
    }


def parse_condition(condition: str) -> Dict[str, Any]:
    match = re.match(r"^(?P<column>.+?)\s*(?P<op>=|!=|<=|>=|<|>)\s*(?P<value>.+)$", condition.strip())
    if not match:
        raise ValueError("Invalid WHERE clause. Use: column operator value.")
    column = match.group("column").strip().strip('"').strip("'")
    op = match.group("op")
    value = normalize_value(match.group("value").strip())
    return {"column": column, "op": op, "value": value}


BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def load_table(table_name: str) -> List[Dict[str, Any]]:
    filename = TABLE_MAP.get(table_name.lower(), f"{table_name}.csv")
    filepath = os.path.join(BASE_DIR, filename)
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Table '{table_name}' not found: {filepath}")

    with open(filepath, newline='', encoding='utf-8') as csvfile:
        reader = csv.DictReader(csvfile)
        rows = []
        for row in reader:
            parsed = {key: normalize_value(value) for key, value in row.items()}
            rows.append(parsed)
        return rows


def match_row(row: Dict[str, Any], condition: Dict[str, Any]) -> bool:
    if condition is None:
        return True
    column = condition["column"]
    op = condition["op"]
    value = condition["value"]
    if column not in row:
        raise KeyError(f"Column '{column}' not found in row.")
    comparator = COMPARISON_OPERATORS[op]
    return comparator(row[column], value)


def select_rows(rows: List[Dict[str, Any]], columns: str, condition: Optional[Dict[str, Any]]) -> List[Dict[str, Any]]:
    if columns == "*":
        selected_columns = None
    else:
        selected_columns = [col.strip().strip('"').strip("'") for col in split_columns(columns)]

    result = []
    for row in rows:
        if condition is None or match_row(row, condition):
            if selected_columns is None:
                result.append(row)
            else:
                projected = {col: row.get(col) for col in selected_columns}
                result.append(projected)
    return result


def format_rows(rows: List[Dict[str, Any]]) -> str:
    if not rows:
        return "No rows returned."
    headers = list(rows[0].keys())
    lines = [", ".join(headers)]
    for row in rows:
        values = [str(row.get(col, "")) for col in headers]
        lines.append(", ".join(values))
    return "\n".join(lines)


def execute_query(query: str) -> str:
    parsed = parse_select(query)
    rows = load_table(parsed["table"])
    condition = parse_condition(parsed["condition"]) if parsed["condition"] else None
    selected = select_rows(rows, parsed["columns"], condition)
    return format_rows(selected)


def main() -> None:
    if len(sys.argv) == 1:
        print("Usage: python query_engine.py \"SELECT * FROM table [WHERE condition]\"")
        print("Example: python query_engine.py \"SELECT Item, Price (USD) FROM menu WHERE In Stock = True\"")
        return

    query = " ".join(sys.argv[1:])
    try:
        output = execute_query(query)
        print(output)
    except Exception as exc:
        print(f"Error: {exc}")
        sys.exit(1)


if __name__ == "__main__":
    main()
