#include <Windows.h>
#include <mysql.h>
#include <string>
#include <memory>
#include <stdexcept>
#include <vector>
#include <sstream>

class CheatDatabase {
private:
    MYSQL* conn;
    // change to your host, user, pass, and db
    const char* host = "localhost";
    const char* user = "username"; 
    const char* pass = "yourpass";
    const char* db = "yourdb";
    
public:
    CheatDatabase() {
        conn = mysql_init(nullptr);
        if (!conn) {
            throw std::runtime_error("MySQL init failed");
        }

        if (!mysql_real_connect(conn, host, user, pass, db, 3306, nullptr, 0)) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Connection error: " + error);
        }
    }

    ~CheatDatabase() {
        if (conn) {
            mysql_close(conn);
        }
    }

    bool LogCheatUse(const std::string& cheat_type, const std::string& player_name) {
        std::string query = "INSERT INTO cheat_logs (cheat_type, player_name, timestamp) VALUES ('" + 
                           cheat_type + "', '" + player_name + "', NOW())";
                           
        return mysql_query(conn, query.c_str()) == 0;
    }

    bool UpdateStats(const std::string& player_name, int kills, int deaths) {
        std::string query = "INSERT INTO player_stats (player_name, kills, deaths) VALUES ('" +
                           player_name + "', " + std::to_string(kills) + ", " + 
                           std::to_string(deaths) + ") ON DUPLICATE KEY UPDATE kills=kills+" +
                           std::to_string(kills) + ", deaths=deaths+" + std::to_string(deaths);
                           
        return mysql_query(conn, query.c_str()) == 0;
    }

    bool BanPlayer(const std::string& player_name, const std::string& reason) {
        std::string query = "INSERT INTO banned_players (player_name, reason, ban_date) VALUES ('" +
                           player_name + "', '" + reason + "', NOW())";
                           
        return mysql_query(conn, query.c_str()) == 0;
    }

    bool IsPlayerBanned(const std::string& player_name) {
        std::string query = "SELECT COUNT(*) FROM banned_players WHERE player_name = '" + player_name + "'";
        
        if (mysql_query(conn, query.c_str()) != 0) {
            return false;
        }

        MYSQL_RES* result = mysql_store_result(conn);
        if (!result) return false;

        MYSQL_ROW row = mysql_fetch_row(result);
        bool banned = row && atoi(row[0]) > 0;
        mysql_free_result(result);
        return banned;
    }

    std::vector<std::string> GetTopPlayers(int limit = 10) {
        std::vector<std::string> top_players;
        std::string query = "SELECT player_name, kills FROM player_stats ORDER BY kills DESC LIMIT " + 
                           std::to_string(limit);

        if (mysql_query(conn, query.c_str()) == 0) {
            MYSQL_RES* result = mysql_store_result(conn);
            if (result) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(result))) {
                    std::stringstream ss;
                    ss << row[0] << " (" << row[1] << " kills)";
                    top_players.push_back(ss.str());
                }
                mysql_free_result(result);
            }
        }
        return top_players;
    }

    bool LogHackDetection(const std::string& player_name, const std::string& hack_type, 
                         const std::string& details) {
        std::string query = "INSERT INTO hack_detections (player_name, hack_type, details, detect_time) "
                           "VALUES ('" + player_name + "', '" + hack_type + "', '" + details + "', NOW())";
        return mysql_query(conn, query.c_str()) == 0;
    }

    bool ResetPlayerStats(const std::string& player_name) {
        std::string query = "UPDATE player_stats SET kills=0, deaths=0 WHERE player_name='" + 
                           player_name + "'";
        return mysql_query(conn, query.c_str()) == 0;
    }
};

// global database instance
std::unique_ptr<CheatDatabase> g_Database;

void InitializeDatabase() {
    try {
        g_Database = std::make_unique<CheatDatabase>();
    }
    catch(const std::exception& e) {
        // log error but continue running
        OutputDebugStringA(e.what());
    }
}
