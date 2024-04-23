#include <boost/filesystem.hpp>
#include <cstdlib> // std::remove
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
    table_deletion_error = 5,
    file_open_error = 6,
    unknown_error = 7
};

bool headers_printed_flag = false;

// Create a callback function  
/* https://videlais.com/2018/12/12/c-with-sqlite3-part-2-creating-tables/ */
// TODO rename parametry
int callback(void* data, int argc, char** argv, char** az_col_name)
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
            sqlite3_free(err_msg);
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
            sqlite3_free(err_msg);
            return false;
        }

        std::cout << "Info: The record was inserted successfully into the " << table_name << " table.\n";
    }
    
    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

int callback_print(void* data, int argc, char** argv, char** az_col_name)
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
        sqlite3_free(err_msg);
        return false;
    }

    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

int callback_salary(void* data, int argc, char** argv, char** az_col_name)
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

    for (int i = 0; i < argc; ++i)
    {
        std::cout << (argv[i] ? argv[i] : "NULL") << " | ";
    }

    std::cout << "\n";
    return 0;
}

bool select_salary_threshold(const std::string table_name, int threshold, sqlite3** p_db)
{
    char* err_msg = nullptr;
    const std::string select_salary_sql = "SELECT * FROM " + table_name + " WHERE Salary >= " + std::to_string(threshold) + ";";    

    int status = sqlite3_exec(*p_db, select_salary_sql.c_str(), callback_salary, nullptr, &err_msg);

    if (status != SQLITE_OK)
    {
        std::cerr << "Error: executing SQL statement failed (salaries query).\n";
        std::cerr << "Error message: " << err_msg << "\n";
        sqlite3_free(err_msg);
        return false;
    }

    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

int callback_last_name(void* data, int argc, char** argv, char** az_col_name)
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

    for (int i = 0; i < argc; ++i)
    {
        std::cout << (argv[i] ? argv[i] : "NULL") << " | ";
    }

    std::cout << "\n";
    return 0;
}

bool select_by_last_name(const std::string table_name, const std::string last_name, sqlite3** p_db)
{
    char* err_msg = nullptr;
    const std::string select_name_sql = "SELECT * FROM " + table_name + " WHERE LastName = '" + last_name + "';";

    int status = sqlite3_exec(*p_db, select_name_sql.c_str(), callback_last_name, nullptr, &err_msg);

    if (status != SQLITE_OK)
    {
        std::cerr << "Error: executing SQL statement failed (last name query).\n";
        std::cerr << "Error message: " << err_msg << "\n";
        sqlite3_free(err_msg);
        return false;
    }

    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

int callback_phone_number(void* data, int argc, char** argv, char** az_col_name)
{
    if (argc > 0 && argv[0] != nullptr)
    {
        *static_cast<int*>(data) = std::stoi(argv[0]);
    }

    return 0;
}

bool update_phone_number(const std::string table_name, const int person_id, const std::string new_phone_number, sqlite3** p_db)
{
    char* err_msg = nullptr;

    const std::string check_phone_sql = "SELECT COUNT(*) FROM " + table_name + " WHERE PhoneNum = '" + new_phone_number + "';";
    int count = 0;

    int status = sqlite3_exec(*p_db, check_phone_sql.c_str(), callback_phone_number, &count, &err_msg);

    if (status != SQLITE_OK)
    {
        std::cerr << "Error: executing SQL statement failed (phone number check).\n";
        std::cerr << "Error message: " << err_msg << "\n";
        sqlite3_free(err_msg);
        return false;
    }

    if (count == 0)
    {
        // The new phone number does not exist in the table, perform the update.
        std::string update_sql = "UPDATE " + table_name + " SET PhoneNum = '" + new_phone_number + "' WHERE ID = " + std::to_string(person_id) + ";";

        status = sqlite3_exec(*p_db, update_sql.c_str(), nullptr, nullptr, &err_msg);

        if (status != SQLITE_OK)
        {
            std::cerr << "Error: executing SQL statement failed (phone number update).\n";
            std::cerr << "Error message: " << err_msg << "\n";
            sqlite3_free(err_msg);
            return false;
        }

        std::cout << "Info: phone number updated successfully.\n";
    }
    else
    {
        // The phone number is already in the table.
        std::cout << "Phone number already exists in the table. Update aborted." << std::endl;
    }

    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

bool drop_table(const std::string table_name, sqlite3** p_db)
{
    char* err_msg = nullptr;
    const std::string drop_sql = "DROP TABLE IF EXISTS " + table_name + ";";

    int status = sqlite3_exec(*p_db, drop_sql.c_str(), nullptr, nullptr, &err_msg);

    if (status != SQLITE_OK)
    {
        std::cerr << "Error: executing SQL statement failed (table drop).\n";
        std::cerr << "Error message: " << err_msg << "\n";
        sqlite3_free(err_msg);
        return false;
    }

    std::cout << "Info: Table dropped successfully.\n";
    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

bool delete_database(const std::string db_filename)
{
    if (std::remove(db_filename.c_str()) == 0)
    {
        std::cout << "Info: Database file '" << db_filename << "' deleted successfully.\n";
    }
    else
    {
        std::cerr << "Error: deleting database file '" << db_filename << "' failed.\n";
        return false;
    }

    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

int run_queries(const std::string table_name, const std::string table_columns_names, sqlite3** p_db)
{
    /* QUERIES */
    /* Staff with a salary greater or equal to 3500. */
    int threshold = 3500;

    // Set the flag to false so the table headers will be printed for the first time.
    headers_printed_flag = false;

    std::cout << "The staff with a salary greater or equal to " << threshold << ":\n\n";
    int success = select_salary_threshold(table_name, threshold, p_db);

    if (!success)
    {
        return error_code::sqlite_generic_error;
    }

    /* Insert a person with the same last name as the person who already is in the table. */
    std::cout << "Insert person Leonard Sloan into the table:\n\n";
    const std::string table_record = "'Leonard','1688 Strawberry Street',2800,'Sloan','leonard@hello-world.com','staff/profiles/leonard/avatar.png','672-48-1451','PST'";
    success = insert_table_record(table_name, table_columns_names, table_record, p_db);

    if (!success)
    {
        return error_code::table_insert_error;
    }

    // Set the flag to false so the table headers will be printed for the first time.
    headers_printed_flag = false;

    success = print_table(table_name, p_db);

    if (!success)
    {
        return error_code::sqlite_generic_error;
    }

    /* Print all person which has the LastName = Sloan. */

    // Set the flag to false so the table headers will be printed for the first time.
    headers_printed_flag = false;

    std::cout << "The staff with a \"Sloan\" last name:\n\n";

    const std::string last_name = "Sloan";
    success = select_by_last_name(table_name, last_name, p_db);

    if (!success)
    {
        return error_code::sqlite_generic_error;
    }

    /* Update the phone number for a person with ID = 1. */
    std::cout << "Update the phone number for a person with ID = 1. New phone number: 666-55-4444:\n\n";

    success = update_phone_number(table_name, 1, "666-55-4444", p_db);

    if (!success)
    {
        return error_code::sqlite_generic_error;
    }

    // Set the flag to false so the table headers will be printed for the first time.
    headers_printed_flag = false;

    success = print_table(table_name, p_db);

    if (!success)
    {
        return error_code::sqlite_generic_error;
    }

    return error_code::no_error;
}

// TODO funkce cleanup ktera provede vsechny uzavreni (db a souboru) a odstraneni tabulky a databaze. Pro validni i chybove ukonceni.
// TODO ulozit do txt souboru finalni presmerovane vystup, at to nemusi spoustet.
// TODO required sqlite a boost do README
int main(int argc, char** argv)
{
    sqlite3* p_db = nullptr;
    const std::string db_filename = "dbschema.db";
    std::ifstream file("../people.csv");

    if (!file.is_open())
    {
        std::cerr << "Error: CSV file opening failed.\n";
        return error_code::file_open_error;
    }

    bool success = create_database(db_filename, &p_db);

    if (!success)
    {
        sqlite3_close(p_db);
        p_db = nullptr;

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
        sqlite3_close(p_db);
        p_db = nullptr;

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
            sqlite3_close(p_db);
            p_db = nullptr;

            file.close();
            return error_code::table_insert_error;
        }
    }

    success = print_table(table_name, &p_db);

    if (!success)
    {
        sqlite3_close(p_db);
        p_db = nullptr;

        file.close();
        return error_code::sqlite_generic_error;
    }

    // Run queries.

    int err = run_queries(table_name, table_columns_names, &p_db);

    if (err != error_code::no_error)
    {
        sqlite3_close(p_db);
        p_db = nullptr;

        file.close();
        return error_code::sqlite_generic_error;
    }

    success = drop_table(table_name, &p_db);
    
    if (!success)
    {
        sqlite3_close(p_db);
        p_db = nullptr;

        file.close();
        return error_code::sqlite_generic_error;
    }

    sqlite3_close(p_db);
    p_db = nullptr;

    success = delete_database(db_filename);

    if (!success)
    {
        file.close();
        return error_code::table_deletion_error;
    }

    file.close();

    return error_code::no_error;
}
