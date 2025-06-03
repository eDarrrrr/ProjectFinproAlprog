#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include <mutex>
#include <vector> 

extern const int PORT;
extern const char* BINARY_FILE;
extern const char* JSON_FILE;

extern std::vector<ScanLog> g_scan_logs;
extern std::mutex g_logs_mutex;

std::string format_timestamp(time_t rawtime);
void save_to_binary();
void load_from_binary();
void export_to_json();
void handle_client(SOCKET client_socket);
void search_logs_by_id(const std::string& student_id);
void display_sorted_logs();

#endif 