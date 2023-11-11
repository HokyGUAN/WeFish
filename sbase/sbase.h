#include <iostream>
#include <vector>
#include <sqlite3.h>


class SBase {
public:
    SBase();
    ~SBase() = default;

    bool Initialise(const std::string &db_filepath);
    bool CreateTable(void);
    bool Insert(int id, std::string passw, std::string name, std::string icon);
    bool ReadByID(int id, const std::string read, std::string *return_value);
    bool UpdateByID(int id, const std::string update, const std::string value);
    int Count(void);
    std::string GenerateInviteCode(void);
    std::string GetInviteCode(void);
    bool ChangeInviteCode(std::string new_code);
    int GetInviteCodeTimes(void);
    bool UpdateInviteCodeTimes(int times);


private:
    bool OpenDatabase(void);
    bool ExecuteSqlQuery(const std::string &sqlquery,
                        std::vector<std::string> *return_rows = {});

private:
    sqlite3* connection_;
    std::string database_location_;
};