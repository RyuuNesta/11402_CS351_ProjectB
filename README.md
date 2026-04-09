# Project B - CSV Mini Database and Query Engine

## Overview
This project involves the design and implementation of a lightweight database management system and query engine capable of reading, processing, and querying data stored in CSV (Comma-Separated Values) format. The goal is to create a mini database system that demonstrates fundamental database concepts including data storage, indexing, filtering, and query execution.

## Project Description
Project B is a comprehensive exercise in building database fundamentals from scratch. Students will develop a functional query engine that can handle CSV files as data sources, providing capabilities similar to a simplified relational database. The system will parse CSV files, load data into memory-efficient structures, and execute user queries with optimized performance.

## Key Objectives
- **Data Loading & Management**: Implement efficient CSV parsing and in-memory data representation
- **Query Processing**: Build a query engine capable of executing SELECT, WHERE, ORDER BY, and JOIN operations
- **Data Filtering & Sorting**: Support filtering records based on conditions and sorting by multiple columns
- **Index Implementation**: Create simple indexing mechanisms to improve query performance
- **Error Handling**: Implement robust error handling for malformed data and invalid queries

## Core Features
1. **CSV File Parsing**: Parse and validate CSV files with proper handling of delimiters and special characters
2. **Column Operations**: Support operations on individual columns including data type detection
3. **Query Execution**: Execute queries with WHERE clauses, ORDER BY, GROUP BY, and aggregate functions
4. **Join Operations**: Implement basic join operations between multiple CSV tables
5. **Performance Optimization**: Apply indexing and caching strategies to optimize query execution time

## Technical Requirements
- Implementation of a data structure to represent tables and records
- Query parser for interpreting SQL-like statements
- Query optimizer for efficient execution plans
- Support for basic data types (STRING, INTEGER, FLOAT, BOOLEAN)
- Memory-efficient storage and retrieval mechanisms

## Expected Outcomes
Upon completion, the system should:
- Successfully load and parse CSV files of varying sizes
- Execute complex queries with multiple conditions and joins
- Return results in a structured format
- Handle edge cases and invalid inputs gracefully
- Demonstrate understanding of database design principles and query optimization

## Technologies & Concepts
- Data structures and algorithms for efficient querying
- File I/O and parsing techniques
- Query optimization and execution planning
- Algorithm complexity analysis (Big O notation)