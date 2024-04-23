#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sqlite3.h>

constexpr int k_expected_cols = 8;
constexpr int k_first_name_idx = 0;
constexpr int k_last_name_idx = 3;
constexpr int k_phone_num_idx = 6;

/**
 * The error codes enumeration for the whole program.
 */
enum error_code
{
    no_error = 0,
    db_create_error = 1,
    table_create_error = 2,
    table_insert_error = 3,
    sqlite_generic_error = 4,
    file_open_error = 5,    
    unknown_error = 6
};

bool headers_printed_flag = false;

// Create a callback function  
/* https://videlais.com/2018/12/12/c-with-sqlite3-part-2-creating-tables/ */
// TODO rename parametry
int callback(void *NotUsed, int argc, char **argv, char **azColName)
{


    // Return successful
    return 0;
}

bool database_exists(const std::string& db_filename)
{
    boost::filesystem::path path_to_db(db_filename);

    return boost::filesystem::exists(path_to_db);
}

bool create_database(const std::string& db_filename, sqlite3** p_db)
{
    char* err_msg = nullptr;

    // Check if the database already exists. If yes, delete the database.
    if (database_exists(db_filename))
    {
        std::cout << "The database \"" << db_filename << "\" already exists. Loading an existing database.\n";

        int status = sqlite3_open(db_filename.c_str(), p_db);

        if (status != SQLITE_OK)
        {
            std::cerr << "Error: loading the database \"" << db_filename << "\" failed.\n";
            sqlite3_close(*p_db);
            return false;
        }

        std::cout << "Info: The database \"" << db_filename << "\" loaded successfully.\n";
    }
    else
    {
        int status = sqlite3_open(db_filename.c_str(), p_db);
    
        if (status != SQLITE_OK)
        {
            std::cerr << "Error: creating the database \"" << db_filename << "\" failed.\n";
            sqlite3_close(*p_db);
            return false;
        }

        std::cout << "Info: The database \"" << db_filename << "\" created successfully.\n";
    }

    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

bool create_table(const std::string table_name, const std::string table_columns, sqlite3** p_db)
{
    char* err_msg = nullptr;

    /* Create SQL statement */
    /*
    * Email can have a total length of 320 characters, so VARCHAR(320) data type is used instead of VARCHAR(255) for the Email column.
    * https://www.mindbaz.com/en/email-deliverability/what-is-the-maximum-size-of-an-mail-address/
    */
    /* Linux's maximum path length is 4096 characters, so the VARCHAR(4096) data type is used instead of VARCHAR(255) for the ProfileImage column.
    * https://unix.stackexchange.com/questions/32795/what-is-the-maximum-allowed-filename-and-folder-size-with-ecryptfs
    */
    /*
    * Longest phone number is 15 character, so the VARCHAR(20) data type is used including reserve for spaces between numbers.
    * https://www.oreilly.com/library/view/regular-expressions-cookbook/9781449327453/ch04s03.html
    */

    sqlite3_stmt* stmt;
    const std::string table_exists_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";

    int status = sqlite3_prepare_v2(*p_db, table_exists_sql.c_str(), -1, &stmt, nullptr);

    if (status != SQLITE_OK)
    {
        std::cerr << "Error: preparing SQL statement failed (table existence check).\n";
        std::cerr << "Error message: " << sqlite3_errmsg(*p_db) << "\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, table_name.c_str(), -1, SQLITE_STATIC);

    status = sqlite3_step(stmt);

    if (status == SQLITE_ROW)
    {
        // The table already exists. Use this table instead of creating a new one.
        sqlite3_finalize(stmt);
        std::cout << "The table \"" << table_name << "\" already exists. The new table was not created.\n";
    }
    else if (status == SQLITE_DONE)
    {
        // The table does not exist: create it.
        sqlite3_finalize(stmt);

        std::string create_table_sql = "CREATE TABLE " + table_name + " (" + table_columns +  ");";

        /* Execute the SQL. */
        status = sqlite3_exec(*p_db, create_table_sql.c_str(), callback, 0, &err_msg);

        if (status != SQLITE_OK)
        {
            std::cerr << "Error: executing SQL statement failed (table creation).\n";
            std::cerr << "Error message: " << err_msg << "\n";
            return false;
        }

        std::cout << "Info: The table was created successfully.\n";
    }
    else
    {
        std::cerr << "Error: executing SQL statement failed (table existence check).\n";
        return false;
    }

    std::cout << "-----------------------------------------------------------------------\n";
    
    return true;
}

std::vector<std::string> parse_csv_line(const std::string& line)
{
    std::vector<std::string> cols;
    std::stringstream ss(line);
    std::string col;

    while (std::getline(ss, col, ','))
    {
        size_t start = col.find_first_not_of(" \t\n\r'");
        size_t end = col.find_last_not_of(" \t\n\r'");

        if (start != std::string::npos && end != std::string::npos) {
            cols.push_back(col.substr(start, end - start + 1));
        } else {
            cols.push_back("");
        }
    }

    return cols;
}

bool person_exists(const std::string table_name, const std::string columns_values, sqlite3** p_db)
{
    const std::string select_person_sql = "SELECT COUNT(*) FROM " + table_name + " WHERE FirstName = ? AND LastName = ? AND PhoneNum = ?;";
    
    sqlite3_stmt* stmt;
    int status = sqlite3_prepare_v2(*p_db, select_person_sql.c_str(), -1, &stmt, nullptr);

    if (status != SQLITE_OK)
    {
        std::cerr << "Error: preparing SQL statement failed (person existence check).\n";
        std::cerr << "Error message: " << sqlite3_errmsg(*p_db) << "\n";
        return false;
    }

    std::vector<std::string> cols = parse_csv_line(columns_values);

    if (cols.size() != k_expected_cols) {
        std::cerr << "Error: unexpected number of columns.\n";
        return false;
    }

    const std::string first_name = cols[k_first_name_idx];
    const std::string last_name = cols[k_last_name_idx];
    const std::string phone_num = cols[k_phone_num_idx];

    // Bind parameters.
    sqlite3_bind_text(stmt, 1, first_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, last_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, phone_num.c_str(), -1, SQLITE_STATIC);

    status = sqlite3_step(stmt);

    if (status != SQLITE_ROW) {
        std::cerr << "Error: executing SQL statement failed (person existence check).\n";
        return false;
    }

    int person_count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return (person_count > 0);
}

bool insert_table_record(const std::string table_name, const std::string table_columns_names, const std::string columns_values, sqlite3** p_db)
{
    // Check if the person exists in the table.
    if (person_exists(table_name, columns_values, p_db))
    {
        std::cout << "The inserted person already exists.\n";
    }
    else
    {
        char* err_msg = nullptr;
        const std::string insert_record_sql = "INSERT INTO " + table_name + " (" + table_columns_names + ") VALUES (" + columns_values + ");";

        int status = sqlite3_exec(*p_db, insert_record_sql.c_str(), nullptr, nullptr, &err_msg);

        if (status != SQLITE_OK) {
            std::cerr << "Error: inserting record into the " << table_name << " table.\n";
            std::cerr << "Error message: " << err_msg << "\n";
            return false;
        }

        std::cout << "Info: The record was inserted successfully into the " << table_name << " table.\n";
    }
    
    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

int callback_print(void *data, int argc, char **argv, char **az_col_name)
{
    // Print column names.
    if (!headers_printed_flag)
    {
        for (int i = 0; i < argc; ++i)
        {
            std::cout << az_col_name[i] << " | ";
        }

        std::cout << "\n\n";        

        headers_printed_flag = true;
    }

    // Print each row.
    for (int i = 0; i < argc; ++i)
    {
        std::cout << (argv[i] ? argv[i] : "NULL") << " | ";
    }

    std::cout << "\n";

    return 0;
}

bool print_table(const std::string table_name, sqlite3** p_db)
{
    char* err_msg = nullptr;

    const std::string table_select_sql = "SELECT * FROM Staff";

    int status = sqlite3_exec(*p_db, table_select_sql.c_str(), callback_print, nullptr, &err_msg);
    if (status != SQLITE_OK)
    {
        std::cerr << "Error: table " << table_name << " select failed.\n";
        std::cerr << "Error message: " << err_msg << "\n";
        return false;
    }

    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

// TODO database delete and table delete

int main(int argc, char** argv)
{
    sqlite3* p_db = nullptr;
    const std::string db_filename = "dbschema.db";
    std::ifstream file("../people.csv");

    if (!file.is_open()) {
        std::cerr << "Error: CSV file opening failed.\n";
        return error_code::file_open_error;
    }

    bool success = create_database(db_filename, &p_db);

    if (!success)
    {
        file.close();
        return error_code::db_create_error;
    }

    const std::string table_name = "Staff";
    const std::string table_columns =
        "ID INTEGER PRIMARY KEY           AUTOINCREMENT  ," \
        "FirstName          VARCHAR(255)  NOT NULL       ," \
        "LastName           VARCHAR(255)  NOT NULL       ," \
        "Address            VARCHAR(255)  NOT NULL       ," \
        "Salary             INTEGER       NOT NULL       ," \
        "Email              VARCHAR(320)  NOT NULL UNIQUE," \
        "ProfileImage       VARCHAR(4096)          UNIQUE," \
        "PhoneNum           VARCHAR(20)   NOT NULL UNIQUE," \
        "TimeZone           VARCHAR(50)                   ";

    success = create_table(table_name, table_columns, &p_db);

    if (!success)
    {
        file.close();
        return error_code::table_create_error;
    }

    const std::string table_columns_names = "FirstName, Address, Salary, LastName, Email, ProfileImage, PhoneNum, TimeZone";

    std::string table_record;
    while (std::getline(file, table_record))
    {
        std::cout << "Record: " << table_record << "\n";

        success = insert_table_record(table_name, table_columns_names, table_record, &p_db);

        if (!success)
        {
            file.close();
            return error_code::table_insert_error;
        }
    }

    success = print_table(table_name, &p_db);

    if (!success) {
        file.close();
        return error_code::sqlite_generic_error;
    }

    sqlite3_close(p_db);
    file.close();

    return error_code::no_error;
}
