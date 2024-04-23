#include <iostream> 
#include <sqlite3.h>

/**
 * The error codes enumeration for the whole program.
 */
enum ErrorCode {
    NoError = 0,
    DBOpenError = 1,                
    UnknownError = 2
};

int main(int argc, char** argv) 
{
    sqlite3* pDb = nullptr;
    const char* dBFilename = "dbschema.db";

    int status = sqlite3_open(dBFilename, &pDb);
    
    if (status != SQLITE_OK) {
        std::cerr << "Error: opening the database \"" << dBFilename << "\" failed.\n";
        return ErrorCode::DBOpenError;
    }

    std::cout << "Info: The database \"" << dBFilename << "\" opened successfully.\n";

    //std::string create

    return ErrorCode::NoError;
}
