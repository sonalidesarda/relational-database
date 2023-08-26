/**
 * @file SQLExec.cpp - implementation of SQLExec class
 * @author Ana Mendes
 * @author Keerthana Thonupunuri
 * @see "Seattle University, CPSC5300, Spring 2022"
 */
#include "SQLExec.h"
#include "ParseTreeToString.h"
#include "EvalPlan.h"

using namespace std;
using namespace hsql;

// define static data
Tables *SQLExec::tables = nullptr;
Indices *SQLExec::indices = nullptr;

// make query result be printable
ostream &operator<<(ostream &out, const QueryResult &qres)
{
    if (qres.column_names != nullptr)
    {
        for (auto const &column_name : *qres.column_names)
            out << column_name << " ";
        out << endl
            << "+";
        for (unsigned int i = 0; i < qres.column_names->size(); i++)
            out << "----------+";
        out << endl;
        for (auto const &row : *qres.rows)
        {
            for (auto const &column_name : *qres.column_names)
            {
                Value value = row->at(column_name);
                switch (value.data_type)
                {
                case ColumnAttribute::INT:
                    out << value.n;
                    break;
                case ColumnAttribute::TEXT:
                    out << "\"" << value.s << "\"";
                    break;
                case ColumnAttribute::BOOLEAN:
                    out << (value.n == 1 ? "true" : "false");
                    break;
                default:
                    out << "???";
                }
                out << " ";
            }
            out << endl;
        }
    }
    out << qres.message;
    return out;
}

QueryResult::~QueryResult()
{
    delete column_names;
    delete column_attributes;
    delete rows;
}

QueryResult *SQLExec::execute(const SQLStatement *statement)
{
    // initialize _tables table, if not yet present
    if (SQLExec::tables == nullptr)
    {
        SQLExec::tables = new Tables();
    }
    if (SQLExec::indices == nullptr)
    {
        SQLExec::indices = new Indices();
    }

    try
    {
        switch (statement->type())
        {
        case kStmtCreate:
            return create((const CreateStatement *)statement);
        case kStmtDrop:
            return drop((const DropStatement *)statement);
        case kStmtShow:
            return show((const ShowStatement *)statement);
        case kStmtInsert:
            return insert((const InsertStatement *) statement);
        case kStmtDelete:
            return del((const DeleteStatement *) statement);
        case kStmtSelect:
            return select((const SelectStatement *) statement);
        default:
            return new QueryResult("not implemented");
        }
    }
    catch (DbRelationError &e)
    {
        throw SQLExecError(string("DbRelationError: ") + e.what());
    }
}
ColumnAttribute get_column_type(string column, ColumnNames columns, ColumnAttributes column_types) {
    for(uint i = 0; i < columns.size(); i++) {
        if(columns[i] == column) {
            return column_types[i];
        }
    }
    throw SQLExecError("unkown column " + column);
}

QueryResult *SQLExec::insert(const InsertStatement *statement) {
    Identifier table_name = statement->tableName;
    DbRelation &table = SQLExec::tables->get_table(table_name);
    ColumnNames insert_columns;
    if (statement->columns != nullptr) {
        for(const auto &column : *statement->columns) {
            insert_columns.push_back(column);
        }
    } else {
        insert_columns = table.get_column_names();
    }
    std::vector<Expr*> insert_values = *statement->values;

    ValueDict row;
    ColumnNames columns = table.get_column_names();
    ColumnAttributes column_types = table.get_column_attributes();
    for(uint i = 0; i < insert_values.size(); i++) {
        Identifier column = insert_columns[i];
        ColumnAttribute column_type = get_column_type(column, columns, column_types);

        switch(column_type.get_data_type()) {
            case ColumnAttribute::INT:
                row[column] = Value(insert_values[i]->ival);
                break;
            case ColumnAttribute::TEXT:
                row[column] = Value(insert_values[i]->name);
                break;
            default:
                throw SQLExecError("don't know how to handle data type in INSERT");
        }
    }
    Handle insert_handle = table.insert(&row);

    IndexNames index_names = SQLExec::indices->get_index_names(table_name);
    for(const auto index_name : index_names) {
        DbIndex &index = SQLExec::indices->get_index(table_name, index_name);
        index.insert(insert_handle);
    }
    string suffix = "";
    if(index_names.size() > 0) {
        suffix = " and " + to_string(index_names.size()) + " indices";
    }

    return new QueryResult("successfully inserted 1 row into " + table_name + suffix);
}

ValueDict* SQLExec::get_where_conjunction(const Expr *expr) {
    ValueDict* where = new ValueDict();
    if (expr->type != kExprOperator)
        throw DbRelationError("Invalid statement");

    if (expr->opType == Expr::AND) {
        ValueDict* first = get_where_conjunction(expr->expr);
        ValueDict* second = get_where_conjunction(expr->expr2);
        where->insert(first->begin(), first->end());
        where->insert(second->begin(), second->end());
        delete first;
        delete second;
    } else if (expr->opChar == '=') {
        string index = expr->expr->name;
        if (expr->expr2->type == kExprLiteralInt) {
          (*where)[index] = Value(int32_t(expr->expr2->ival));
        } else if (expr->expr2->type == kExprLiteralString) {
            (*where)[index] = Value(expr->expr2->name);
        } else {
            throw DbRelationError("Don't know how to handle " + expr->expr2->type);
        }
    }  
    return where;
}

QueryResult *SQLExec::del(const DeleteStatement *statement) {
    Identifier tableName = statement->tableName;
    DbRelation &table = SQLExec::tables->get_table(tableName);
    EvalPlan *plan = new EvalPlan(table);
    if (statement->expr != nullptr)
        plan = new EvalPlan(get_where_conjunction(statement->expr), plan);
    EvalPlan *optimized = plan->optimize();
    delete plan;
    EvalPipeline pipeline = optimized->pipeline();
    delete optimized;
    
    IndexNames index_names = SQLExec::indices->get_index_names(tableName);
    Handles *handles = pipeline.second;
    uint rows = 0;
    uint indices = index_names.size();
    for (auto const& handle: *handles) {
        for (auto const index_name: index_names) {
            DbIndex &index = SQLExec::indices->get_index(tableName, index_name);
            index.del(handle);
        }
        table.del(handle);
        rows++;
    }
    delete handles;
    return new QueryResult("successfully deleted " + to_string(rows) + " rows from " + tableName + " " + to_string(indices) + " indices");
}

QueryResult *SQLExec::select(const SelectStatement *statement)
{
    Identifier table_name = statement->fromTable->name;
    DbRelation& table = SQLExec::tables->get_table(table_name);

    ColumnNames* col_names = new ColumnNames;
    for(auto const& expr : *statement->selectList)
    {
        if(expr->type == kExprStar)
        {
            for(auto const col : table.get_column_names())
            {
                col_names->push_back(col);
            }
            break;
        }
        else if(expr->type == kExprColumnRef)
            col_names->push_back(expr->name);
        else
            return new QueryResult("Invalid expr");
    }

    EvalPlan* plan = new EvalPlan(table);
    ValueDict* where = new ValueDict;

    if (statement->whereClause != NULL)
    {
        where = get_where_conjunction(statement->whereClause);
        plan = new EvalPlan(where, plan);
    }

    plan = new EvalPlan(col_names, plan);
    EvalPlan* optimize = plan->optimize();
    ValueDicts* rows = optimize->evaluate();
    delete where;

    ColumnAttributes* col_attr = table.get_column_attributes(*col_names);
    return new QueryResult(col_names, col_attr, rows, "successfully returned" + to_string(rows->size()) + " rows");
}

void SQLExec::column_definition(const ColumnDefinition *col, Identifier &column_name, ColumnAttribute &column_attribute)
{
    column_name = col->name;

    if (col->type == hsql::ColumnDefinition::DataType::TEXT)
    {
        column_attribute = ColumnAttribute::DataType::TEXT;
    }
    else if (col->type == hsql::ColumnDefinition::DataType::INT)
    {
        column_attribute = ColumnAttribute::DataType::INT;
    }
    else
    {
        throw new SQLExecError("Column type not supported " + col->type);
    }
}

QueryResult *SQLExec::create(const CreateStatement *statement)
{
    switch (statement->type)
    {
        case CreateStatement::kTable:
            return create_table(statement);
        case CreateStatement::kIndex:
            return create_index(statement);
        default:
            return new QueryResult("not implemented");
    }
}


/**
 * Function to create the table
 * @return the list of tables created with table name after being executed by SQLExec
 * 
 */

QueryResult *SQLExec::create_table(const CreateStatement *statement)
{
    // get table and columns from the sql statement
    Identifier table_name = statement->tableName;
    // add table name to Tables
    ValueDict row;
    row["table_name"] = Value(table_name);
    Handle tableHandle = SQLExec::tables->insert(&row);
    // add columns to Column
    Handles columnHandles;
    try
    {
        DbRelation &columns_table = SQLExec::tables->get_table(Columns::TABLE_NAME);
        try {
            Identifier column_name;
            ColumnAttribute column_attribute;
            // add everything to Column
            // get all columns names and datatypes
            for (ColumnDefinition *column : *statement->columns)
            {
                column_definition(column, column_name, column_attribute);
                string value = column_attribute.get_data_type() == ColumnAttribute::TEXT ? "TEXT" : "INT";
                row["data_type"] = Value(value);
                row["column_name"] = Value(column_name);
                columnHandles.push_back(columns_table.insert(&row)); 
            }

            // now that columns were successfully added, get table and create it
            DbRelation &table = SQLExec::tables->get_table(table_name);
            table.create();
        }
        catch (DbRelationError &e) {
            try {
                // undo insertions into _columns
                for(auto const columnHandle : columnHandles) {
                    columns_table.del(columnHandle);
                }
            }
            catch (DbRelationError &e) {}
            // throw the exception for the next one to catch
            throw;
        }
    }
    catch (DbRelationError &e)
    {
        try
        {
            // undo table insertion
            SQLExec::tables->del(tableHandle);
        }
        catch (DbRelationError &e) {}
        // throw this exception to display error message in SQL shell
        throw;
    }

    return new QueryResult("Created new table " + table_name);
}


/**
 * Function to create index for the tables available
 * @return the created index with index name and type after being executed by SQLExec
 * 
 */


QueryResult *SQLExec::create_index(const CreateStatement *statement) {
    Identifier index_name = statement->indexName;
    Identifier table_name = statement->tableName;

    // get underlying relation
    DbRelation &table = SQLExec::tables->get_table(table_name);

    // check that given columns exist in table
    const ColumnNames &table_columns = table.get_column_names();
    for (auto const &col_name: *statement->indexColumns)
        if (find(table_columns.begin(), table_columns.end(), col_name) == table_columns.end())
            throw SQLExecError(string("Column '") + col_name + "' does not exist in " + table_name);

    // insert a row for every column in index into _indices
    ValueDict row;
    row["table_name"] = Value(table_name);
    row["index_name"] = Value(index_name);
    row["index_type"] = Value(statement->indexType);
    row["is_unique"] = Value(string(statement->indexType) == "BTREE"); // assume HASH is non-unique --
    int seq = 0;
    Handles i_handles;
    try {
        for (auto const &col_name: *statement->indexColumns) {
            row["seq_in_index"] = Value(++seq);
            row["column_name"] = Value(col_name);
            i_handles.push_back(SQLExec::indices->insert(&row));
        }

        DbIndex &index = SQLExec::indices->get_index(table_name, index_name);
        index.create();

    } catch (...) {
        // attempt to remove from _indices
        try {  // if any exception happens in the reversal below, we still want to re-throw the original ex
            for (auto const &handle: i_handles)
                SQLExec::indices->del(handle);
        } catch (...) {}
        throw;  // re-throw the original exception (which should give the client some clue as to why it did
    }
    return new QueryResult("created index " + index_name);
}

QueryResult *SQLExec::show(const ShowStatement *statement)
{
    switch(statement->type) {
        case ShowStatement::kTables: 
            return show_tables();
        case ShowStatement::kColumns:
            return show_columns(statement);
        case ShowStatement::kIndex:
            return show_index(statement);
        default:
            throw new SQLExecError("statement not implemented " + statement->type);
    }
}


/**
 * Function to show all the tables available
 * @return the list of tables with table name after being executed by SQLExec
 * 
 */

QueryResult *SQLExec::show_tables()
{
    ColumnNames *column_names = new ColumnNames;
    ColumnAttributes *column_attributes = new ColumnAttributes;
    ValueDicts *rows = new ValueDicts();
    // tables will only have one column name, which is table_name
    // // attributes will always be TEXT
    SQLExec::tables->get_columns("_tables", *column_names, *column_attributes);
    // use select to get all entries from that table
    Handles *selectResult = SQLExec::tables->select();
    // use project to get all entries from column "table_name"
    for(const Handle handle : *selectResult) {
        ValueDict *row = SQLExec::tables->project(handle, column_names);
        // "_tables" and "_columns" is in the list too, filter out
        Identifier column_name = row->at("table_name").s;
        if (column_name != Tables::TABLE_NAME && column_name != Columns::TABLE_NAME && column_name != Indices::TABLE_NAME){
            rows->push_back(row);
        }
        else {
            delete row;
        }
    }
    delete selectResult;
    // message should contain the number of records returned.
    string message = "successfully returned " + to_string(rows->size()) + " rows";
    return new QueryResult(column_names, column_attributes, rows, message);
}

/**
 * Function to show all the columns available
 * @return the list of columns with column name after being executed by SQLExec
 * 
 */

QueryResult *SQLExec::show_columns(const ShowStatement *statement)
{
    // Construct the QueryResult
    ColumnNames *column_names = new ColumnNames;
    column_names->push_back("table_name");
    column_names->push_back("column_name");
    column_names->push_back("data_type");
    ColumnAttributes *column_attributes = new ColumnAttributes;

    // The middle ColumnAttribute is Class type. The third one is DataType
    column_attributes->push_back(ColumnAttribute(ColumnAttribute::TEXT));

    // First ValueDicts to store the data
    ValueDicts *rows = new ValueDicts();

    // Second ValueDicts to locate the table
    ValueDict where;
    where["table_name"] = Value(statement->tableName);

    // A different method to get the column name
    Handles *handles = SQLExec::tables->get_table(Columns::TABLE_NAME).select(&where);
    int count = handles->size();

    // Check not in schema_tables.SCHEMA_TABLES
    for (auto const &handle : *handles)
    {
        DbRelation &table = SQLExec::tables->get_table(Columns::TABLE_NAME);
        ValueDict *row = table.project(handle, column_names);
        rows->push_back(row);
    }

    delete handles;
    return new QueryResult(column_names, column_attributes, rows, " successfully returned " + to_string(count) + " rows");
}


/**
 * Function to show all the index available
 * @return the index for the table name after being executed by SQLExec
 * 
 */

QueryResult *SQLExec::show_index(const ShowStatement *statement) {
    ColumnNames *column_names = new ColumnNames;
    ColumnAttributes *column_attributes = new ColumnAttributes;

    column_names->push_back("table_name");
    column_attributes->push_back(ColumnAttribute(ColumnAttribute::TEXT));

    column_names->push_back("index_name");
    column_attributes->push_back(ColumnAttribute(ColumnAttribute::TEXT));

    column_names->push_back("column_name");
    column_attributes->push_back(ColumnAttribute(ColumnAttribute::TEXT));

    column_names->push_back("seq_in_index");
    column_attributes->push_back(ColumnAttribute(ColumnAttribute::INT));

    column_names->push_back("index_type");
    column_attributes->push_back(ColumnAttribute(ColumnAttribute::TEXT));

    column_names->push_back("is_unique");
    column_attributes->push_back(ColumnAttribute(ColumnAttribute::BOOLEAN));

    ValueDict where;
    where["table_name"] = Value(string(statement->tableName));
    Handles *handles = SQLExec::indices->select(&where);
    u_long n = handles->size();

    ValueDicts *rows = new ValueDicts;
    for (auto const &handle : *handles)
    {
        ValueDict *row = SQLExec::indices->project(handle, column_names);
        rows->push_back(row);
    }
    delete handles;
    return new QueryResult(column_names, column_attributes, rows,
                           "successfully returned " + to_string(n) + " rows");
}

// DROP ...
QueryResult *SQLExec::drop(const hsql::DropStatement *statement) {
    switch (statement->type)
    {
        case DropStatement::kTable:
            return drop_table(statement);
        case DropStatement::kIndex:
            return drop_index(statement);
        default:
            return new QueryResult("not implemented");
    }
}


/**
 * Function to drop all the tables available
 * @return the statement dropped table after being executed by SQLExec
 * 
 */
QueryResult *SQLExec::drop_table(const DropStatement *statement)
{
    if (statement->type != hsql::DropStatement::kTable)
    {
        return new QueryResult("Cannot drop a schema table!");
    }

    Identifier table_name = statement->name;

    // Check the table is not a schema table
    if (table_name == Tables::TABLE_NAME || table_name == Columns::TABLE_NAME)
        throw SQLExecError("Cannot drop a schema table!");

    // before dropping table, drop all indices of that table
    ValueDict index_where;
    index_where["table_name"] = Value(table_name);
    Handles *indexHandles = SQLExec::indices->select(&index_where);
    for(auto const &handle : *indexHandles) {
        SQLExec::indices->del(handle);
    }

    // get the table
    DbRelation &table = SQLExec::tables->get_table(table_name);

    // remove table
    table.drop();

    // remove from _columns schema
    ValueDict where;
    where["table_name"] = Value(table_name);

    DbRelation &columns = SQLExec::tables->get_table(Columns::TABLE_NAME);
    Handles *handles = columns.select(&where);
    for (auto const &handle : *handles)
    {
        columns.del(handle);
    }

    // finally, remove from table schema
    Handles *tableHandles = SQLExec::tables->select(&where);
    SQLExec::tables->del(*tableHandles->begin()); // expect only one row
    
    delete indexHandles;
    delete handles;
    delete tableHandles;

    return new QueryResult(std::string("dropped ") + table_name);
}

QueryResult *SQLExec::drop_index(const DropStatement *statement) {
    Identifier name = statement->name;
    Identifier indexName = statement->indexName;

    // drop index
    DbIndex &index = SQLExec::indices->get_index(name, indexName);
    index.drop();

    // remove rows from _indices for this index
    ValueDict where;
    where["table_name"] = Value(name);
    where["index_name"] = Value(indexName);
    Handles *handles = SQLExec::indices->select(&where);
    
    for (auto const &handle : *handles)
        SQLExec::indices->del(handle);
    delete handles;

    return new QueryResult("dropped index " + indexName + " from " + name); 
}

/**
 * Helper function to parse queries to run tests.
 * @return result of query after being executed by SQLExec
 */
QueryResult *parser_helper(string query) {
    SQLParserResult *parse = SQLParser::parseSQLString(query);
    if (!parse->isValid()) {
        cout << "invalid SQL: " << query << endl;
        cout << parse->errorMsg() << endl;
    } else {
        for (uint i = 0; i < parse->size(); ++i) {
            const SQLStatement *statement = parse->getStatement(i);
            try {
                QueryResult *result = SQLExec::execute(statement);
                delete parse;
                return result;
        } catch (SQLExecError &e) {
                delete parse;
                cout << "Error: " << e.what() << endl;
            }
        }
    }
    delete parse;
    return nullptr;
}

/**
 * Testing function for table and column functionality.
 * @return true if the tests all succeeded
 */
bool test_table_functionality() {
    string show_tables = "show tables";
    string create_table = "create table foo (id int, data text, x integer, y integer, z integer)";
    string show_columns = "show columns from foo";
    string drop_tables = "drop table foo";
    // verify show tables when no tables return 0 rows
    QueryResult *qr_show_no_tables = parser_helper(show_tables);
    ValueDicts *show_no_tables_rows = qr_show_no_tables->get_rows();
    if (show_no_tables_rows->size() != 0) {
        delete qr_show_no_tables;
        return false;
    }
    delete qr_show_no_tables;
    cout << endl << "show tables with no tables ok" << endl;
    // verify create table works, show tables will return 1 row
    QueryResult *qr_create_table = parser_helper(create_table);
    QueryResult *qr_show_tables = parser_helper(show_tables);
    ValueDicts *show_tables_rows = qr_show_tables->get_rows();
    if (show_tables_rows->size() != 1) {
        delete qr_create_table;
        delete qr_show_tables;
        return false;
    }
    delete qr_create_table;
    delete qr_show_tables;
    cout << "create table ok" << endl;
    // verify show columns works, returns 5 rows for foo
    QueryResult *qr_show_columns = parser_helper(show_columns);
    ValueDicts *show_columns_rows = qr_show_columns->get_rows();
    if (show_columns_rows->size() != 5) {
        delete qr_show_columns;
        return false;
    }
    delete qr_show_columns;
    cout << "show columns ok" << endl;
    // verify drop table works, show tables will return 0 rows
    QueryResult *qr_drop_table = parser_helper(drop_tables);
    QueryResult *qr_show_tables_drop = parser_helper(show_tables);
    ValueDicts *show_tables_drop_rows = qr_show_tables_drop->get_rows();
    if (show_tables_drop_rows->size() != 0) {
        delete qr_drop_table;
        delete qr_show_tables_drop;
        return false;
    }
    delete qr_drop_table;
    delete qr_show_tables_drop;
    cout << "drop table ok" << endl;

    return true;
}

/**
 * Testing function for index functionality.
 * @return true if the tests all succeeded
 */
bool test_index_functionality() {
    string create_table = "create table test (x int, y int, z int)";
    string show_index = "show index from test";
    string create_index = "create index fx on test (x,y)";
    string drop_index = "drop index fx from test";
    string drop_tables = "drop table test";
    // create table for testing
    QueryResult *qr_create_table = parser_helper(create_table);
    // verify show indices when no indices return 0 rows
    QueryResult *qr_show_no_indice = parser_helper(show_index);
    ValueDicts *show_no_indice_rows = qr_show_no_indice->get_rows();
    if (show_no_indice_rows->size() != 0) {
         delete qr_show_no_indice;
        delete qr_create_table;
        return false;
    }
    delete qr_show_no_indice;
    delete qr_create_table;
    cout << "show index with no index ok" << endl;
    // verify create indix works, show index will return 1 row
    QueryResult *qr_create_index = parser_helper(create_index);
    QueryResult *qr_show_index = parser_helper(show_index);
    ValueDicts *show_index_rows = qr_show_index->get_rows();
    if (show_index_rows->size() != 2) {
        delete qr_create_index;
        delete qr_show_index;
        return false;
    }
    delete qr_create_index;
    delete qr_show_index;
    cout << "create index ok" << endl;
    // verify drop index works, show index will return 0 rows
    QueryResult *qr_drop_index = parser_helper(drop_index);
    QueryResult *qr_show_index_drop = parser_helper(show_index);
    ValueDicts *show_index_drop_rows = qr_show_index_drop->get_rows();
    if (show_index_drop_rows->size() != 0) {
        delete qr_drop_index;
        delete qr_show_index_drop;
        return false;
    }
    delete qr_drop_index;
    delete qr_show_index_drop;
    cout << "drop index ok" << endl;
    // delete table for testing
    QueryResult *qr_drop_table = parser_helper(drop_tables);
    delete qr_drop_table;

    return true;
}

/**
 * Testing function for SQL Exec.
 * @return true if the tests all succeeded
 */
bool test_sql_exec() {
    if (!test_table_functionality())
        return assertion_failure("_tables and _columns tests failed");
    cout << "_tables and _columns tests ok" << endl;

    if (!test_index_functionality())
        return assertion_failure("_indices tests failed");
    cout << "_indices tests ok" << endl;

    return true;
}