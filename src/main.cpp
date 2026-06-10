#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#include <optional>

// ── Utilities ─────────────────────────────────────────────────────────────────

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

// ── CSV Storage ───────────────────────────────────────────────────────────────

struct Table {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

std::vector<std::string> splitCSVLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ','))
        fields.push_back(trim(field));
    return fields;
}

Table loadCSV(const std::string& filepath, const std::string& tableName) {
    std::ifstream file(filepath);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + filepath);

    Table table;
    table.name = tableName;

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (trim(line).empty()) continue;
        auto fields = splitCSVLine(line);
        if (firstLine) {
            table.columns = fields;
            firstLine = false;
        } else {
            table.rows.push_back(fields);
        }
    }
    return table;
}

// ── Query Parser ──────────────────────────────────────────────────────────────

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

    // Collect everything between SELECT and FROM
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

    // Optional WHERE clause
    if (ss >> token && toUpper(token) == "WHERE") {
        Condition cond;
        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);

        // Split on operator (order matters: <=, >=, != before <, >, =)
        for (const auto& op : {"<=", ">=", "!=", "<", ">", "="}) {
            auto pos = rest.find(op);
            if (pos != std::string::npos) {
                cond.column = trim(rest.substr(0, pos));
                cond.op     = op;
                cond.value  = trim(rest.substr(pos + std::string(op).size()));
                // Strip surrounding quotes from value
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

// ── Query Executor ────────────────────────────────────────────────────────────

bool matchRow(const std::vector<std::string>& row,
              const std::vector<std::string>& columns,
              const Condition& cond) {
    auto it = std::find(columns.begin(), columns.end(), cond.column);
    if (it == columns.end())
        throw std::runtime_error("Column not found: " + cond.column);
    int idx = (int)(it - columns.begin());
    const std::string& cell = (idx < (int)row.size()) ? row[idx] : "";

    // Try numeric comparison
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

    // Fall back to string comparison
    if (cond.op == "=")  return cell == cond.value;
    if (cond.op == "!=") return cell != cond.value;
    if (cond.op == "<")  return cell <  cond.value;
    if (cond.op == ">")  return cell >  cond.value;
    if (cond.op == "<=") return cell <= cond.value;
    if (cond.op == ">=") return cell >= cond.value;
    return false;
}

void executeSelect(const SelectQuery& query,
                   const std::unordered_map<std::string, Table>& db) {
    auto it = db.find(query.tableName);
    if (it == db.end())
        throw std::runtime_error("Table not found: " + query.tableName);

    const Table& table = it->second;

    std::vector<int> colIndices;
    std::vector<std::string> colNames;

    if (query.selectAll) {
        for (int i = 0; i < (int)table.columns.size(); ++i)
            colIndices.push_back(i);
        colNames = table.columns;
    } else {
        for (const auto& col : query.columns) {
            auto cit = std::find(table.columns.begin(), table.columns.end(), col);
            if (cit == table.columns.end())
                throw std::runtime_error("Column not found: " + col);
            colIndices.push_back((int)(cit - table.columns.begin()));
            colNames.push_back(col);
        }
    }

    // Filter rows
    std::vector<const std::vector<std::string>*> filtered;
    for (const auto& row : table.rows) {
        if (!query.where || matchRow(row, table.columns, *query.where))
            filtered.push_back(&row);
    }

    // Calculate column widths
    std::vector<int> widths(colNames.size());
    for (size_t i = 0; i < colNames.size(); ++i)
        widths[i] = (int)colNames[i].size();
    for (const auto* row : filtered) {
        for (size_t i = 0; i < colIndices.size(); ++i) {
            int idx = colIndices[i];
            int len = (idx < (int)row->size()) ? (int)(*row)[idx].size() : 0;
            widths[i] = std::max(widths[i], len);
        }
    }

    std::string sep = "+";
    for (int w : widths) sep += std::string(w + 2, '-') + "+";

    std::cout << sep << "\n";
    for (size_t i = 0; i < colNames.size(); ++i)
        std::cout << "| " << colNames[i]
                  << std::string(widths[i] - colNames[i].size(), ' ') << " ";
    std::cout << "|\n" << sep << "\n";

    for (const auto* row : filtered) {
        for (size_t i = 0; i < colIndices.size(); ++i) {
            int idx = colIndices[i];
            std::string val = (idx < (int)row->size()) ? (*row)[idx] : "";
            std::cout << "| " << val
                      << std::string(widths[i] - val.size(), ' ') << " ";
        }
        std::cout << "|\n";
    }
    std::cout << sep << "\n";
    std::cout << filtered.size() << " row(s) returned.\n";
}

// ── REPL ──────────────────────────────────────────────────────────────────────

int main() {
    std::unordered_map<std::string, Table> db;
    try {
        db["menu"] = loadCSV("data/coffee_menu_items.csv", "menu");
    } catch (const std::exception& e) {
        std::cerr << "Warning: " << e.what() << "\n";
    }

    std::cout << "CSV Query Engine\n";
    std::cout << "Loaded tables: ";
    for (auto& kv : db) std::cout << kv.first << " ";
    std::cout << "\nType a SELECT query or 'exit' to quit.\n\n";

    std::string line;
    while (true) {
        std::cout << ">> ";
        if (!std::getline(std::cin, line)) break;
        line = trim(line);
        if (line.empty()) continue;
        if (toUpper(line) == "EXIT") break;

        try {
            SelectQuery q = parseSelect(line);
            executeSelect(q, db);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
        std::cout << "\n";
    }

    return 0;
}
