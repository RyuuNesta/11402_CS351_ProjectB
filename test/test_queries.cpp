#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#include <optional>
#include <sstream>

// ── Embedded implementation ───────────────────────────────────────────────────

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

std::vector<std::string> splitCSVLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ','))
        fields.push_back(trim(field));
    return fields;
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

std::optional<Condition> parseWhereClause(std::istringstream& ss) {
    std::string token;
    if (!(ss >> token) || toUpper(token) != "WHERE")
        return std::nullopt;

    std::string rest;
    std::getline(ss, rest);
    rest = trim(rest);

    for (const auto& op : {"<=", ">=", "!=", "<", ">", "="}) {
        auto pos = rest.find(op);
        if (pos != std::string::npos) {
            Condition cond;
            cond.column = trim(rest.substr(0, pos));
            cond.op     = op;
            cond.value  = trim(rest.substr(pos + std::string(op).size()));
            if (cond.value.size() >= 2 &&
                ((cond.value.front() == '"' && cond.value.back() == '"') ||
                 (cond.value.front() == '\'' && cond.value.back() == '\'')))
                cond.value = cond.value.substr(1, cond.value.size() - 2);
            return cond;
        }
    }
    throw std::runtime_error("Invalid WHERE clause: " + rest);
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

// SELECT

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
    q.where = parseWhereClause(ss);
    return q;
}

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

// INSERT

struct InsertQuery {
    std::string tableName;
    std::vector<std::string> values;
};

InsertQuery parseInsert(const std::string& query) {
    std::istringstream ss(query);
    std::string token;
    InsertQuery q;

    ss >> token;
    if (toUpper(token) != "INSERT")
        throw std::runtime_error("Expected INSERT");

    ss >> token;
    if (toUpper(token) != "INTO")
        throw std::runtime_error("Expected INTO after INSERT");

    ss >> q.tableName;

    ss >> token;
    if (toUpper(token) != "VALUES")
        throw std::runtime_error("Expected VALUES after table name");

    std::string rest;
    std::getline(ss, rest);
    rest = trim(rest);

    if (rest.empty() || rest.front() != '(' || rest.back() != ')')
        throw std::runtime_error("Expected VALUES (v1, v2, ...)");

    rest = rest.substr(1, rest.size() - 2);
    q.values = splitCSVLine(rest);
    for (auto& v : q.values)
        if (v.size() >= 2 &&
            ((v.front() == '"' && v.back() == '"') ||
             (v.front() == '\'' && v.back() == '\'')))
            v = v.substr(1, v.size() - 2);

    return q;
}

void executeInsert(const InsertQuery& query,
                   std::unordered_map<std::string, Table>& db) {
    auto it = db.find(query.tableName);
    if (it == db.end())
        throw std::runtime_error("Table not found: " + query.tableName);

    Table& table = it->second;
    if (query.values.size() != table.columns.size())
        throw std::runtime_error(
            "Expected " + std::to_string(table.columns.size()) +
            " values, got " + std::to_string(query.values.size()));

    table.rows.push_back(query.values);
}

// DELETE

struct DeleteQuery {
    std::string tableName;
    std::optional<Condition> where;
};

DeleteQuery parseDelete(const std::string& query) {
    std::istringstream ss(query);
    std::string token;
    DeleteQuery q;

    ss >> token;
    if (toUpper(token) != "DELETE")
        throw std::runtime_error("Expected DELETE");

    ss >> token;
    if (toUpper(token) != "FROM")
        throw std::runtime_error("Expected FROM after DELETE");

    ss >> q.tableName;
    q.where = parseWhereClause(ss);
    return q;
}

int executeDelete(const DeleteQuery& query,
                  std::unordered_map<std::string, Table>& db) {
    auto it = db.find(query.tableName);
    if (it == db.end())
        throw std::runtime_error("Table not found: " + query.tableName);

    Table& table = it->second;
    int before = (int)table.rows.size();

    if (!query.where) {
        table.rows.clear();
    } else {
        table.rows.erase(
            std::remove_if(table.rows.begin(), table.rows.end(),
                [&](const std::vector<std::string>& row) {
                    return matchRow(row, table.columns, *query.where);
                }),
            table.rows.end());
    }

    return before - (int)table.rows.size();
}

// ── Test Fixture ──────────────────────────────────────────────────────────────

class QueryTest : public ::testing::Test {
protected:
    std::unordered_map<std::string, Table> db;

    void SetUp() override {
        Table menu;
        menu.name    = "menu";
        menu.columns = {"Item", "Category", "Price", "In Stock"};
        menu.rows    = {
            {"Espresso",   "Coffee", "3.0", "True"},
            {"Latte",      "Coffee", "4.5", "True"},
            {"Cappuccino", "Coffee", "4.5", "True"},
            {"Americano",  "Coffee", "3.5", "True"},
            {"Green Tea",  "Tea",    "2.5", "True"},
            {"Croissant",  "Pastry", "3.0", "False"},
        };
        db["menu"] = menu;
    }
};

// ── SELECT Tests ──────────────────────────────────────────────────────────────

TEST_F(QueryTest, Select_StarReturnsAllRowsAndColumns) {
    auto result = executeSelect(parseSelect("SELECT * FROM menu"), db);
    EXPECT_EQ(result.size(), 6u);
    EXPECT_EQ(result[0].size(), 4u);
}

TEST_F(QueryTest, Select_SpecificColumnsProjectsCorrectly) {
    auto result = executeSelect(parseSelect("SELECT Item, Category FROM menu"), db);
    EXPECT_EQ(result.size(), 6u);
    EXPECT_EQ(result[0].size(), 2u);
    EXPECT_EQ(result[0][0], "Espresso");
    EXPECT_EQ(result[0][1], "Coffee");
}

TEST_F(QueryTest, Select_WhereEqualsFiltersRows) {
    auto result = executeSelect(parseSelect("SELECT * FROM menu WHERE Category = Tea"), db);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0][0], "Green Tea");
}

TEST_F(QueryTest, Select_WhereGreaterThanFiltersNumeric) {
    auto result = executeSelect(parseSelect("SELECT Item FROM menu WHERE Price > 4.0"), db);
    EXPECT_EQ(result.size(), 2u);
}

TEST_F(QueryTest, Select_NonExistentTableThrows) {
    EXPECT_THROW(executeSelect(parseSelect("SELECT * FROM drinks"), db), std::runtime_error);
}

TEST_F(QueryTest, Select_NonExistentColumnThrows) {
    EXPECT_THROW(executeSelect(parseSelect("SELECT Calories FROM menu"), db), std::runtime_error);
}

// ── INSERT Tests ──────────────────────────────────────────────────────────────

TEST_F(QueryTest, Insert_AddsRowToTable) {
    executeInsert(parseInsert("INSERT INTO menu VALUES (Mocha, Coffee, 5.0, True)"), db);
    EXPECT_EQ(db["menu"].rows.size(), 7u);
}

TEST_F(QueryTest, Insert_NewRowAppearsInSelect) {
    executeInsert(parseInsert("INSERT INTO menu VALUES (Mocha, Coffee, 5.0, True)"), db);
    auto result = executeSelect(parseSelect("SELECT * FROM menu WHERE Item = Mocha"), db);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0][0], "Mocha");
}

TEST_F(QueryTest, Insert_WrongColumnCountThrows) {
    EXPECT_THROW(
        executeInsert(parseInsert("INSERT INTO menu VALUES (Mocha, Coffee)"), db),
        std::runtime_error);
}

TEST_F(QueryTest, Insert_NonExistentTableThrows) {
    EXPECT_THROW(
        executeInsert(parseInsert("INSERT INTO drinks VALUES (Water, Cold, 1.0, True)"), db),
        std::runtime_error);
}

// ── DELETE Tests ──────────────────────────────────────────────────────────────

TEST_F(QueryTest, Delete_WithWhereRemovesMatchingRows) {
    int deleted = executeDelete(parseDelete("DELETE FROM menu WHERE Category = Pastry"), db);
    EXPECT_EQ(deleted, 1);
    EXPECT_EQ(db["menu"].rows.size(), 5u);
}

TEST_F(QueryTest, Delete_RemovedRowDoesNotAppearInSelect) {
    executeDelete(parseDelete("DELETE FROM menu WHERE Item = Espresso"), db);
    auto result = executeSelect(parseSelect("SELECT * FROM menu WHERE Item = Espresso"), db);
    EXPECT_EQ(result.size(), 0u);
}

TEST_F(QueryTest, Delete_WithoutWhereRemovesAllRows) {
    int deleted = executeDelete(parseDelete("DELETE FROM menu"), db);
    EXPECT_EQ(deleted, 6);
    EXPECT_EQ(db["menu"].rows.size(), 0u);
}

TEST_F(QueryTest, Delete_NonExistentTableThrows) {
    EXPECT_THROW(
        executeDelete(parseDelete("DELETE FROM drinks"), db),
        std::runtime_error);
}
