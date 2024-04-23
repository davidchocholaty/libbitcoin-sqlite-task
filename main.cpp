#include <boost/filesystem.hpp>
#include <iostream>
#include <string>
#include <sqlite3.h>

/* https://www.visual-paradigm.com/features/database-design-with-erd-tools/ */

/**
 * The error codes enumeration for the whole program.
 */
enum error_code
{
    no_error = 0,
    db_create_error = 1,
    table_create_error = 2,
    table_insert_error = 3,
    unknown_error = 4
};

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
    char* err_msg = 0;

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
    char* err_msg = 0;

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

bool insert_table_record(const std::string table_name, const std::string table_columns_names, const std::string columns_values, sqlite3** p_db)
{
    // TODO kontrola jestli uz neexistuje v tabulce
    char* err_msg = 0;
    const std::string insert_record_sql = "INSERT INTO " + table_name + " (" + table_columns_names + ") VALUES (" + columns_values + ");";

    int status = sqlite3_exec(*p_db, insert_record_sql.c_str(), nullptr, nullptr, &err_msg);

    if (status != SQLITE_OK) {
        std::cerr << "Error: inserting record into the " << table_name << " table.\n";
        std::cerr << "Error message: " << err_msg << "\n";
            return false;
    }

    std::cout << "Info: The record was inserted successfully into the " << table_name << " table.\n";
    std::cout << "-----------------------------------------------------------------------\n";

    return true;
}

// TODO database delete and table delete

int main(int argc, char** argv)
{
    sqlite3* p_db = nullptr;
    const std::string db_filename = "dbschema.db";

    bool success = create_database(db_filename, &p_db);

    if (!success)
    {
        return error_code::db_create_error;
    }

    const std::string table_name = "Staff";
    //TODO ze email a phonenum musi byt unique
    const std::string table_columns =
        "ID INTEGER PRIMARY KEY           AUTOINCREMENT," \
        "FirstName          VARCHAR(255)  NOT NULL," \
        "LastName           VARCHAR(255)  NOT NULL," \
        "Address            VARCHAR(255)  NOT NULL," \
        "Salary             INTEGER       NOT NULL," \
        "Email              VARCHAR(320)  NOT NULL," \
        "ProfileImage       VARCHAR(4096)         ," \
        "PhoneNum           VARCHAR(20)   NOT NULL," \
        "TimeZone           VARCHAR(50)            ";

    success = create_table(table_name, table_columns, &p_db);

    if (!success)
    {
        return error_code::table_create_error;
    }

    const std::string table_columns_names = "FirstName, Address, Salary, LastName, Email, ProfileImage, PhoneNum, TimeZone";

    const std::string columns_values = "'Kenneth','3793 Columbia Mine Road',3200,'Prevost','kenneth@hello-world.com','staff/profiles/kenneth/avatar.png','255-48-5875','PST'";

    success = insert_table_record(table_name, table_columns_names, columns_values, &p_db);

    if (!success)
    {
        return error_code::table_insert_error;
    }

    sqlite3_close(p_db);

    return error_code::no_error;
}
