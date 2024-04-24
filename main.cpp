/**
 * @file    main.cpp
 *
 * @brief   Small SQLite program containing a single table scheme and a few queries.
 *
 * @author  David Chocholaty
 */

#include <boost/filesystem.hpp>
#include <cstdlib> // std::remove
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sqlite3.h>

// The expected number of provided columns to save record into a table.
constexpr int k_expected_cols = 8;
// Indexes of the columns containing the specific attributes.
constexpr int k_first_name_idx = 0;
constexpr int k_last_name_idx = 3;
constexpr int k_phone_num_idx = 6;

// A flag which handles print of the table header only before the first table 
// record is printed.
bool headers_printed_flag = false;

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

/**
 * Function check if the file containing the database scheme already exists.
 * 
 * @param db_filename Name of the file containing the database scheme.
 * @return            True if the database exists, false otherwise.
 */
bool database_exists(const std::string& db_filename)
{
    boost::filesystem::path path_to_db(db_filename);

    return boost::filesystem::exists(path_to_db);
}

/**
 * Function which creates the database scheme.
 * 
 * If the file containing the scheme already exists, this database is used 
 * instead of creating a new one.
 * 
 * @param db_filename Name of the file containing the database scheme.
 * @param p_db        Database connection pointer.
 * @return            True if all sub-tasks were done successfully, false if 
 *                    an error occurs.
 */
bool create_database(const std::string& db_filename, sqlite3** p_db)
{
    // Check if the database already exists. If yes, load the database from the file.
    if (database_exists(db_filename))
    {
        std::cout << "The database \"" << db_filename << "\" already exists. Loading an existing database.\n";

        int status = sqlite3_open(db_filename.c_str(), p_db);

        if (status != SQLITE_OK)
        {
            std::cerr << "Error: loading the database \"" << db_filename << "\" failed.\n";
            return false;
        }

        std::cout << "Info: The database \"" << db_filename << "\" loaded successfully.\n";
    }
    else
    {
        // The database does not exist. Create a new one.
        int status = sqlite3_open(db_filename.c_str(), p_db);
    
        if (status != SQLITE_OK)
        {
            std::cerr << "Error: creating the database \"" << db_filename << "\" failed.\n";
            return false;
        }

        std::cout << "Info: The database \"" << db_filename << "\" created successfully.\n";
    }

    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

/**
 * Function which creates the chosen custom table.
 * 
 * If the table already exists in the database, do not create a new one.
 * 
 * @param table_name    The name of a table to create.
 * @param table_columns Comma-separated list of table columns headers.
 * @param p_db          Database connection pointer.
 * @return              True if all sub-tasks were done successfully, false if 
 *                      an error occurs.
 */
bool create_table(const std::string table_name, const std::string table_columns, sqlite3** p_db)
{
    char* err_msg = nullptr;    

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

        status = sqlite3_exec(*p_db, create_table_sql.c_str(), nullptr, 0, &err_msg);

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

/**
 * Function which parses the comma-separated list of values from a single line.
 * 
 * @param line The line which contains a comma-separated list of values.
 * @return     Returns the input line parsed into a vector.
 */
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

/**
 * The function checks if the records with a person already exist in the table.
 * 
 * The person is checked based on the FirstName, LastName and PhoneNum which 
 * has to be unique in the table, so the person too.
 * 
 * @param table_name     The name of a table in which the person will be searched.
 * @param columns_values Comma-separated list of values in the same order as 
 *                       table headers.
 * @param p_db           Database connection pointer.
 * @return               True if a person already exists, false otherwise.
 */
bool person_exists(const std::string table_name,
                   const std::string columns_values,
                   sqlite3** p_db)
{
    const std::string select_person_sql = "SELECT COUNT(*) FROM " + table_name + \
        " WHERE FirstName = ? AND LastName = ? AND PhoneNum = ?;";
    
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

/**
 * The function inserts a record with a person into the table.
 * 
 * If the person already exists in the table, nothing is done.
 * @param table_name           The name of a table into which the person will 
 *                             be inserted.
 * @param table_columns_values Comma-separated list of table headers.
 * @param columns_values       Comma-separated list of values in the same order
 *                             as table headers.
 * @param p_db                 Database connection pointer.
 * @return                     True if all sub-tasks were done successfully, false if 
 *                             an error occurs.
 */
bool insert_table_record(const std::string table_name, const std::string table_columns_names,
                         const std::string columns_values, sqlite3** p_db)
{
    // Check if the person exists in the table.
    if (person_exists(table_name, columns_values, p_db))
    {
        std::cout << "The inserted person already exists.\n";
    }
    else
    {
        // The person is not in the table, insert the record.
        char* err_msg = nullptr;
        const std::string insert_record_sql = "INSERT INTO " + table_name + " (" + table_columns_names + \
            ") VALUES (" + columns_values + ");";

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

/**
 * The function which prints the results of a query.
 * 
 * It is expected that this function is called only from the callbacks 
 * functions. This function prints before the first record of the table headers
 * and then the table records one by one.
 * 
 * @param argc        Number of arguments.
 * @param argv        The obtained data from the table query (table records).
 * @param az_col_name The list of column headers.
 */
void print_query_result(int argc, char** argv, char** az_col_name)
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
}

/**
 * Callback function for the SQL execution in the print_table function.
 * 
 * @param data        Generic void pointer. For now, it is unused.
 * @param argc        Number of arguments.
 * @param argv        The obtained data from the table query (table records).
 * @param az_col_name The list of column headers.
 * @return            Returns zero status after all sub-tasks are done.
 */
int callback_print(void* data, int argc, char** argv, char** az_col_name)
{
    print_query_result(argc, argv, az_col_name);
    
    return 0;
}

/**
 * The function executes the SQL statement for printing the complete table to stdout.
 * 
 * @param table_name    The name of a table to print.
 * @param p_db          Database connection pointer.
 */
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

/**
 * Callback function for the SQL execution in the select_salary_threshold 
 * function.
 * 
 * @param data        Generic void pointer. For now, it is unused.
 * @param argc        Number of arguments.
 * @param argv        The obtained data from the table query (table records).
 * @param az_col_name The list of column headers.
 * @return            Returns zero status after all sub-tasks are done.
 */
int callback_salary(void* data, int argc, char** argv, char** az_col_name)
{   
    // Print column names.
    print_query_result(argc, argv, az_col_name);

    return 0;
}

/**
 * The function executes the query to obtain records with people, who have a 
 * salary greater or equal to a specific threshold provided by the parameter.
 * 
 * @param table_name    The name of a table.
 * @param threshold     Salary threshold value.
 * @param p_db          Database connection pointer.
 */
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

/**
 * Callback for the select_by_last_name function. It prints the result to stdout.
 * 
 * @param data        Generic void pointer. For now, it is unused.
 * @param argc        Number of arguments.
 * @param argv        The obtained data from the table query (table records).
 * @param az_col_name The list of column headers.
 * @return            Returns zero status after all sub-tasks are done.
 */
int callback_last_name(void* data, int argc, char** argv, char** az_col_name)
{
    // Print column names.
    print_query_result(argc, argv, az_col_name);

    return 0;
}

/**
 * The function runs a query to obtain people who have a specific last name.
 * 
 * @param table_name Name of a table in which the people will be searched.
 * @param last_name  Searched last name.
 * @param p_db       Database connection pointer.
*/
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

/**
 * Callback for the update_phone_number function.
 * 
 * This function returns the count of the people with the phone number. Because
 * the phone number has to be always unique in the database, it can be zero or 
 * just one.
 * 
 * @param data        Generic void pointer. It is used to propagate the count value.
 * @param argc        Number of arguments.
 * @param argv        The obtained data from the table query (table records).
 * @param az_col_name The list of column headers. For now, it is unused.
 * @return            Returns zero status after all sub-tasks are done.
 */
int callback_phone_number(void* data, int argc, char** argv, char** az_col_name)
{
    if (argc > 0 && argv[0] != nullptr)
    {
        *static_cast<int*>(data) = std::stoi(argv[0]);
    }

    return 0;
}

/**
 * Function for executing a query which updates a phone number to a specific 
 * person identified by the primary key (ID).
 * 
 * It is checked if the new phone number is not already in the table. If yes, 
 * the phone number can't be updated because the phone number has to be unique 
 * in the table for each person.
 * 
 * @param table_name       Name of a table in which the person will be searched and 
 *                         in which will be the phone number updated.
 * @param person_id        The identifier of a person in the table.
 * @param new_phone_number The new phone number which replaces the previous one.
 * @param p_db             Database connection pointer.
 */
bool update_phone_number(const std::string table_name,
                         const int person_id,
                         const std::string new_phone_number,
                         sqlite3** p_db)
{
    char* err_msg = nullptr;

    const std::string check_phone_sql = "SELECT COUNT(*) FROM " + table_name + \
        " WHERE PhoneNum = '" + new_phone_number + "';";
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
        std::string update_sql = "UPDATE " + table_name + " SET PhoneNum = '" + new_phone_number + \
            "' WHERE ID = " + std::to_string(person_id) + ";";

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

/**
 * The function which deletes the table from the database.
 *
 * @param table_name Name of a table to delete.
 * @param p_db       Database connection pointer. 
 */
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

/**
 * The function which deletes the whole database (the file *.db from the system).
 * 
 * @param db_filename The name of a file containing the database including the .db extension.
 */
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

/**
 * The primary function for running the custom-created queries.
 * 
 * The implemented queries are as follows:
 * 1. Select and print people with a salary greater or equal to 3500.
 * 2. Insert a new person. This person has the same last name as at least one 
 *    person who already is stored in the table.
 * 3. Print all persons from the table which has the last name mentioned in the
 *    previous point (LastName = Sloan).
 * 4. Update the phone number for the person with a specific identifier (ID = 1).
 * 
 * @param table_name
 */
int run_queries(const std::string table_name,
                const std::string table_columns_names,
                sqlite3** p_db)
{   
    // *********************************************************************
    // 1. Select and print people with a salary greater or equal to 3500.
    // *********************************************************************
    int threshold = 3500;

    // Set the flag to false so the table headers will be printed for the first time.
    headers_printed_flag = false;

    std::cout << "The staff with a salary greater or equal to " << threshold << ":\n\n";
    int success = select_salary_threshold(table_name, threshold, p_db);

    if (!success)
    {
        return error_code::sqlite_generic_error;
    }

    // *********************************************************************
    // 2. Insert a new person. This person has the same last name as at least 
    //    one person who already is stored in the table.
    // *********************************************************************
    std::cout << "Insert person Leonard Sloan into the table:\n\n";
    const std::string table_record =
        "'Leonard',"
        "'1688 Strawberry Street',"
        "2800,"
        "'Sloan',"
        "'leonard@hello-world.com',"
        "'staff/profiles/leonard/avatar.png',"
        "'672-48-1451',"
        "'PST'";
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

    // *********************************************************************
    // 3. Print all persons from the table which has the last name mentioned in
    //    the previous point (LastName = Sloan).
    // *********************************************************************

    // Set the flag to false so the table headers will be printed for the first time.
    headers_printed_flag = false;

    std::cout << "The staff with a \"Sloan\" last name:\n\n";

    const std::string last_name = "Sloan";
    success = select_by_last_name(table_name, last_name, p_db);

    if (!success)
    {
        return error_code::sqlite_generic_error;
    }

    // *********************************************************************
    // 4. Update the phone number for the person with a specific identifier (ID = 1).
    // *********************************************************************
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

/**
 * The function which deletes the table and database validly closes the 
 * database connection and the input file.
 * 
 * @param db_filename Database file name including the .db extension.
 * @param table_name  The name of a table to delete.
 * @param file        The input file.
 * @param p_db        Database connection pointer.
 */
int cleanup(const std::string db_filename,
            const std::string table_name,
            std::ifstream& file,
            sqlite3** p_db)
{
    bool success = false;

    if (table_name.compare("") == 0)
    {
        success = drop_table(table_name, p_db);    

        sqlite3_close(*p_db);
        p_db = nullptr;

        if (!success)
        {
            // Even over the failure try to delete the whole database.
            success = delete_database(db_filename);
            file.close();
            return error_code::sqlite_generic_error;
        }
    }
    else
    {
        sqlite3_close(*p_db);
        p_db = nullptr;
    }
    
    success = delete_database(db_filename);

    if (!success)
    {
        file.close();
        return error_code::table_deletion_error;
    }    

    file.close();
    
    return error_code::no_error;
}

/**
 * Main function of the program.
 * 
 * @param argc  The number of program arguments.
 * @param argv  The list of program arguments.
 * @return      If the program ends correctly, return zero ok status. Otherwise
 *              returns the status value.
*/
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
        // Because of the error ignore the cleanup return code.
        cleanup(db_filename, "", file, &p_db);
        return error_code::db_create_error;
    }

    // Email can have a total length of 320 characters, so VARCHAR(320) data 
    // type is used instead of VARCHAR(255) for the Email column.
    // https://www.mindbaz.com/en/email-deliverability/what-is-the-maximum-size-of-an-mail-address/

    // Linux's maximum path length is 4096 characters, so the VARCHAR(4096) 
    // data type is used instead of VARCHAR(255) for the ProfileImage column.
    // https://unix.stackexchange.com/questions/32795/what-is-the-maximum-allowed-filename-and-folder-size-with-ecryptfs

    // The longest phone number is 15 characters, so the VARCHAR(20) data type 
    // is used including reserving for spaces between numbers.
    // https://www.oreilly.com/library/view/regular-expressions-cookbook/9781449327453/ch04s03.html
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
        // Because of the error ignore the cleanup return code.
        cleanup(db_filename, "", file, &p_db);
        return error_code::table_create_error;
    }

    const std::string table_columns_names =
        "FirstName, Address, Salary, LastName, Email, ProfileImage, PhoneNum, TimeZone";

    std::string table_record;
    while (std::getline(file, table_record))
    {
        std::cout << "Record: " << table_record << "\n";

        success = insert_table_record(table_name, table_columns_names, table_record, &p_db);

        if (!success)
        {
            // Because of the error ignore the cleanup return code.
            cleanup(db_filename, table_name, file, &p_db);
            return error_code::table_insert_error;
        }
    }

    success = print_table(table_name, &p_db);

    if (!success)
    {
        // Because of the error ignore the cleanup return code.
        cleanup(db_filename, table_name, file, &p_db);
        return error_code::sqlite_generic_error;
    }

    // Run queries.
    int err = run_queries(table_name, table_columns_names, &p_db);

    if (err != error_code::no_error)
    {
        // Because of the error ignore the cleanup return code.
        cleanup(db_filename, table_name, file, &p_db);
        return error_code::sqlite_generic_error;
    }

    // Final cleanup.
    int status = cleanup(db_filename, table_name, file, &p_db);

    if (status != error_code::no_error)
    {
        std::cerr << "Error: final cleanup failed.\n";
        return status;
    }

    return error_code::no_error;
}
