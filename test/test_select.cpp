#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#include <optional>
#include <sstream>

// ── Embedded implementation (mirrors src/main.cpp) ───────────────────────────

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

struct Table {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

struct Condition {
    std::string column;
    std::string op;
    std::string value;
};

struct SelectQuery {
    bool selectAll = false;
    std::vector<std::string> columns;
    std::string tableName;
    std::optional<Condition> where;
};

SelectQuery parseSelect(const std::string& query) {
    std::istringstream ss(query);
    std::string token;
    SelectQuery q;

    ss >> token;
    if (toUpper(token) != "SELECT")
        throw std::runtime_error("Expected SELECT");

    std::string columnSection;
    while (ss >> token && toUpper(token) != "FROM")
        columnSection += token + " ";
    if (toUpper(token) != "FROM")
        throw std::runtime_error("Expected FROM");

    columnSection = trim(columnSection);
    if (columnSection.empty())
        throw std::runtime_error("Expected column list after SELECT");

    if (columnSection == "*") {
        q.selectAll = true;
    } else {
        std::replace(columnSection.begin(), columnSection.end(), ',', ' ');
        std::istringstream colStream(columnSection);
        std::string col;
        while (colStream >> col)
            q.columns.push_back(col);
    }

    if (!(ss >> token))
        throw std::runtime_error("Expected table name after FROM");
    q.tableName = token;

    if (ss >> token && toUpper(token) == "WHERE") {
        Condition cond;
        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);

        for (const auto& op : {"<=", ">=", "!=", "<", ">", "="}) {
            auto pos = rest.find(op);
            if (pos != std::string::npos) {
                cond.column = trim(rest.substr(0, pos));
                cond.op     = op;
                cond.value  = trim(rest.substr(pos + std::string(op).size()));
                if (cond.value.size() >= 2 &&
                    ((cond.value.front() == '"' && cond.value.back() == '"') ||
                     (cond.value.front() == '\'' && cond.value.back() == '\'')))
                    cond.value = cond.value.substr(1, cond.value.size() - 2);
                q.where = cond;
                break;
            }
        }
        if (!q.where)
            throw std::runtime_error("Invalid WHERE clause: " + rest);
    }

    return q;
}

bool matchRow(const std::vector<std::string>& row,
              const std::vector<std::string>& columns,
              const Condition& cond) {
    auto it = std::find(columns.begin(), columns.end(), cond.column);
    if (it == columns.end())
        throw std::runtime_error("Column not found: " + cond.column);
    int idx = (int)(it - columns.begin());
    const std::string& cell = (idx < (int)row.size()) ? row[idx] : "";

    try {
        double cellNum  = std::stod(cell);
        double valueNum = std::stod(cond.value);
        if (cond.op == "=")  return cellNum == valueNum;
        if (cond.op == "!=") return cellNum != valueNum;
        if (cond.op == "<")  return cellNum <  valueNum;
        if (cond.op == ">")  return cellNum >  valueNum;
        if (cond.op == "<=") return cellNum <= valueNum;
        if (cond.op == ">=") return cellNum >= valueNum;
    } catch (...) {}

    if (cond.op == "=")  return cell == cond.value;
    if (cond.op == "!=") return cell != cond.value;
    if (cond.op == "<")  return cell <  cond.value;
    if (cond.op == ">")  return cell >  cond.value;
    if (cond.op == "<=") return cell <= cond.value;
    if (cond.op == ">=") return cell >= cond.value;
    return false;
}

// Returns projected rows (as strings) for assertion
std::vector<std::vector<std::string>> executeSelect(
    const SelectQuery& query,
    const std::unordered_map<std::string, Table>& db) {

    auto it = db.find(query.tableName);
    if (it == db.end())
        throw std::runtime_error("Table not found: " + query.tableName);

    const Table& table = it->second;

    std::vector<int> colIndices;
    if (query.selectAll) {
        for (int i = 0; i < (int)table.columns.size(); ++i)
            colIndices.push_back(i);
    } else {
        for (const auto& col : query.columns) {
            auto cit = std::find(table.columns.begin(), table.columns.end(), col);
            if (cit == table.columns.end())
                throw std::runtime_error("Column not found: " + col);
            colIndices.push_back((int)(cit - table.columns.begin()));
        }
    }

    std::vector<std::vector<std::string>> result;
    for (const auto& row : table.rows) {
        if (query.where && !matchRow(row, table.columns, *query.where))
            continue;
        std::vector<std::string> projected;
        for (int idx : colIndices)
            projected.push_back((idx < (int)row.size()) ? row[idx] : "");
        result.push_back(projected);
    }
    return result;
}

// ── Test Fixture ──────────────────────────────────────────────────────────────

class SelectTest : public ::testing::Test {
protected:
    std::unordered_map<std::string, Table> db;

    void SetUp() override {
        Table menu;
        menu.name    = "menu";
        menu.columns = {"Item", "Category", "Price", "In Stock"};
        menu.rows    = {
            {"Espresso",   "Coffee",   "3.0", "True"},
            {"Latte",      "Coffee",   "4.5", "True"},
            {"Cappuccino", "Coffee",   "4.5", "True"},
            {"Americano",  "Coffee",   "3.5", "True"},
            {"Green Tea",  "Tea",      "2.5", "True"},
            {"Croissant",  "Pastry",   "3.0", "False"},
        };
        db["menu"] = menu;
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(SelectTest, SelectStarReturnsAllRowsAndColumns) {
    auto q   = parseSelect("SELECT * FROM menu");
    auto res = executeSelect(q, db);
    EXPECT_EQ(res.size(), 6u);
    EXPECT_EQ(res[0].size(), 4u);
}

TEST_F(SelectTest, SelectSpecificColumns) {
    auto q   = parseSelect("SELECT Item, Category FROM menu");
    auto res = executeSelect(q, db);
    EXPECT_EQ(res.size(), 6u);
    EXPECT_EQ(res[0].size(), 2u);
    EXPECT_EQ(res[0][0], "Espresso");
    EXPECT_EQ(res[0][1], "Coffee");
}

TEST_F(SelectTest, SelectSingleColumn) {
    auto q   = parseSelect("SELECT Item FROM menu");
    auto res = executeSelect(q, db);
    EXPECT_EQ(res.size(), 6u);
    EXPECT_EQ(res[0].size(), 1u);
}

TEST_F(SelectTest, WhereEqualsFiltersRows) {
    auto q   = parseSelect("SELECT * FROM menu WHERE Category = Tea");
    auto res = executeSelect(q, db);
    EXPECT_EQ(res.size(), 1u);
    EXPECT_EQ(res[0][0], "Green Tea");
}

TEST_F(SelectTest, WhereGreaterThanFiltersNumeric) {
    auto q   = parseSelect("SELECT Item FROM menu WHERE Price > 4.0");
    auto res = executeSelect(q, db);
    EXPECT_EQ(res.size(), 2u);
}

TEST_F(SelectTest, WhereLessThanOrEqual) {
    auto q   = parseSelect("SELECT Item FROM menu WHERE Price <= 3.0");
    auto res = executeSelect(q, db);
    EXPECT_EQ(res.size(), 3u);
}

TEST_F(SelectTest, NonExistentTableThrows) {
    auto q = parseSelect("SELECT * FROM drinks");
    EXPECT_THROW(executeSelect(q, db), std::runtime_error);
}

TEST_F(SelectTest, NonExistentColumnThrows) {
    auto q = parseSelect("SELECT Calories FROM menu");
    EXPECT_THROW(executeSelect(q, db), std::runtime_error);
}

TEST_F(SelectTest, MissingFromThrows) {
    EXPECT_THROW(parseSelect("SELECT Item"), std::runtime_error);
}

TEST_F(SelectTest, EmptyInputThrows) {
    EXPECT_THROW(parseSelect(""), std::runtime_error);
}

TEST_F(SelectTest, NotSelectThrows) {
    EXPECT_THROW(parseSelect("INSERT INTO menu VALUES (x)"), std::runtime_error);
}
